// WallScout - induction balance (IB) detektor betonfalhoz
// - TX: Timer1 hardveres OC1A (D9) ad ~4,5 kHz-es negyszoget a soros rezgokorre
// - RX: A0, az ADC-t Timer1 inditja, tengelyenkent 4 minta / TX periodus
//       -> szinkron demodulalas (I/Q), 20 ms (1 halozati periodus) integralas
// - (az A1 50 Hz-es elo vezetek antenna kikommentezve)
#include <Arduino.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

constexpr uint8_t PIN_BTN1 = 2;  // rovid: kalibracio, hosszu: nullazo mod
constexpr uint8_t PIN_BTN2 = 4;  // rovid: vas betanitas, hosszu: auto hangolas
constexpr uint8_t PIN_BUZ = 3;   // passziv piezo (tone)

// Timer1 OCR1A = N, TX frekvencia = 500000 / N Hz
// L_TX = 960 uH, L_RX = 931 uH, C = 1 uF -> f0 ~ 7,042 kHz -> N ~ 97
constexpr uint8_t N_MIN = 71, N_MAX = 115, N_DEF = 97;
constexpr uint8_t CLIP_COUNT =
    5;  // ennyi telített mintától jelez TELITES-t (keretenként)

// Érzékenység beállítások
constexpr float THR_SIGMA =
    3.0f;  // küszöb = THR_SIGMA x zajszórás (kisebb = érzékenyebb)
constexpr float THR_MIN = 0.3f;   // a küszöb alsó határa
constexpr float BAR_DIV = 15.0f;  // a FEM csík tele a küszöb ennyiszeresénél
                                  // (kisebb = hamarabb telik)
constexpr float SMOOTH = 0.3f;  // I/Q simítás (nagyobb = gyorsabb, de zajosabb)
constexpr float DRIFT_DIV =
    128.0f;  // lassú driftkövetés a nullpontnál (nagyobb = lassabb követés)

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

struct Frame {
  int32_t i, q;
  uint32_t s, ss;
  uint8_t n;
  bool clip;
};

static Frame rxF;
static volatile uint8_t rxSeq = 0;
// static Frame acF;
// static volatile uint8_t acSeq = 0;

static uint16_t halfN = N_DEF / 2;
static uint8_t frameCycles = 91;
static uint16_t nSamp = 0;
static int32_t aI, aQ;
static uint8_t aClip;  // telített minták száma a keretben
// static uint8_t curMode = 0;      // 0 = RX (A0), 1 = AC antenna (A1)
// static uint8_t frameIdx = 0;     // 3 RX keret : 1 AC keret
// static uint32_t aS, aSS, grp;
// static uint8_t grpN;

EMPTY_INTERRUPT(TIMER1_COMPB_vect);  // csak a trigger flag torlese miatt kell

ISR(ADC_vect) {
  uint16_t v = ADC;
  uint16_t t = TCNT1;
  bool lvl = PINB & _BV(PINB1);  // az OC1A (TX) aktualis szintje
  // a mintavetel helyzete: fel/le szamlalas x TX szint -> 0..3, idoben egymas
  // utan
  uint8_t ph = (t > halfN ? 1 : 0) + (lvl ? 0 : 2);
  if ((v < 8 || v > 1015) && aClip < 255) aClip++;

  switch (ph) {
    case 0:
      aI += v;
      break;
    case 2:
      aI -= v;
      break;
    case 1:
      aQ += v;
      break;
    default:
      aQ -= v;
      break;
  }
  /*
  if (curMode == 1) {
    grp += v;                                    // 4 minta osszege: a TX es
  harmonikusai kiesnek if (++grpN == 4) { aS += grp; aSS += grp * grp; grp = 0;
      grpN = 0;
    }
  }
  */

  if (++nSamp >= (uint16_t)frameCycles * 4) {
    rxF.i = aI;
    rxF.q = aQ;
    rxF.n = frameCycles;
    rxF.clip = aClip >= CLIP_COUNT;
    rxSeq++;
    nSamp = 0;
    aI = aQ = 0;
    aClip = 0;
    /*
    frameIdx = (frameIdx + 1) & 3;
    uint8_t nm = (frameIdx == 3) ? 1 : 0;
    if (nm != curMode) {
      curMode = nm;
      ADMUX = _BV(REFS0) | nm;
    }
    */
  }
}

static uint8_t skipRx = 0;

static void setN(uint8_t n) {
  uint8_t c = (10000 + n / 2) / n;  // ciklus / 20 ms
  noInterrupts();
  OCR1A = n;
  OCR1B = n / 2;
  halfN = n / 2;
  frameCycles = c;
  TCNT1 = 0;
  nSamp = 0;
  aI = aQ = 0;
  aClip = 0;
  ADMUX = _BV(REFS0);
  interrupts();
  skipRx = 2;
}

static void startHardware(uint8_t n) {
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);
  TCCR1A = _BV(COM1A0) |
           _BV(WGM10);  // mod 9: fazishelyes PWM, TOP=OCR1A, OC1A toggle
  TCCR1B = _BV(WGM13);
  TIMSK1 = _BV(OCIE1B);
  ADMUX = _BV(REFS0);
  ADCSRB = _BV(ADTS2) | _BV(ADTS0);  // trigger: Timer1 compare match B
  ADCSRA =
      _BV(ADEN) | _BV(ADATE) | _BV(ADIE) | _BV(ADPS2) |
      _BV(ADPS0);  // clk/32 = 500 kHz, 26 us/konverzio < 45 us mintakoz (N=90)
  setN(n);
  TCCR1B |= _BV(CS11);  // clk/8
}

static bool pollRx(float& i, float& q, bool& clip) {
  static uint8_t seen = 0;
  if (rxSeq == seen) return false;
  Frame f;
  noInterrupts();
  f = rxF;
  seen = rxSeq;
  interrupts();
  if (skipRx) {
    skipRx--;
    return false;
  }
  i = (float)f.i / f.n;
  q = (float)f.q / f.n;
  clip = f.clip;
  return true;
}

/*
static bool pollAc(float &rms) {
  static uint8_t seen = 0;
  if (acSeq == seen) return false;
  Frame f;
  noInterrupts();
  f = acF;
  seen = acSeq;
  interrupts();
  int64_t n = f.n;
  int64_t num = n * (int64_t)f.ss - (int64_t)f.s * (int64_t)f.s;
  float var = num > 0 ? (float)num / (float)(n * n) : 0;
  rms = sqrtf(var) / 4.0f;
  return true;
}
*/

// ---- beallitasok ----
struct Cfg {
  uint8_t magic;
  uint8_t n;
  uint8_t taught;
  float ang;
};
static Cfg cfg;

static void loadCfg() {
  EEPROM.get(0, cfg);
  if (cfg.magic != 0x5B || cfg.n < N_MIN || cfg.n > N_MAX) {
    cfg.magic = 0x5B;
    cfg.n = N_DEF;
    cfg.taught = 0;
    cfg.ang = 0;
  }
}
static void saveCfg() { EEPROM.put(0, cfg); }

static float bI = 0, bQ = 0, thr = 1;
static float vI = 0, vQ = 0;
// static float acBase = 0, acThr = 1, acD = 0;
static bool firstRx = true;

static void showMsg(const char* a, const char* b = nullptr,
                    const char* c = nullptr) {
  oled.firstPage();
  do {
    oled.setFont(u8g2_font_7x13B_tr);
    oled.drawStr(4, 11, a);
    oled.setFont(u8g2_font_6x10_tr);
    if (b) oled.drawStr(4, 29, b);
    if (c) oled.drawStr(4, 43, c);
  } while (oled.nextPage());
}

static void calibrate() {
  showMsg("KALIBRALAS", "Ne mozgasd a", "detektort");
  float sI = 0, sQ = 0, sM2 = 0;
  uint8_t nr = 0;
  uint32_t t0 = millis();
  float i, q;
  bool clip;
  while (nr < 50 && millis() - t0 < 8000) {
    if (pollRx(i, q, clip)) {
      sI += i;
      sQ += q;
      sM2 += i * i + q * q;
      nr++;
    }
  }
  if (nr < 10) {
    showMsg("HIBA", "Nincs RX jel", "Ellenorizd a TX-et");
    delay(2000);
    return;
  }
  bI = sI / nr;
  bQ = sQ / nr;
  float var = sM2 / nr - (bI * bI + bQ * bQ);
  thr = fmaxf(THR_SIGMA * sqrtf(fmaxf(var, 0)), THR_MIN);
  /*
  if (na > 3) {
    acBase = sA / na;
    float av = sA2 / na - acBase * acBase;
    acThr = fmaxf(5.0f * sqrtf(fmaxf(av, 0)), 0.4f);
  }
  */
  vI = bI;
  vQ = bQ;
  firstRx = false;
}

static void autoTune() {
  showMsg("HANGOLAS", "Tarts femet a", "tekercs fole!");
  delay(3000);
  uint8_t bestN = cfg.n;
  float best = -1;
  char buf[20];
  for (uint8_t n = N_MIN; n <= N_MAX; n++) {
    setN(n);
    float acc = 0, i, q;
    bool clip;
    uint8_t got = 0;
    uint32_t t0 = millis();
    while (got < 3 && millis() - t0 < 600) {
      if (pollRx(i, q, clip)) {
        acc += sqrtf(i * i + q * q);
        got++;
      }
    }
    if (got && acc / got > best) {
      best = acc / got;
      bestN = n;
    }
    if ((n - N_MIN) % 6 == 0) {
      snprintf(buf, sizeof buf, "%u Hz", (unsigned)(500000UL / n));
      showMsg("HANGOLAS", buf);
    }
  }
  cfg.n = bestN;
  saveCfg();
  setN(bestN);
  snprintf(buf, sizeof buf, "f = %u Hz", (unsigned)(500000UL / bestN));
  showMsg("KESZ", buf, "Kalibralj ujra!");
  delay(1500);
}

// ---- gombok ----
static uint8_t readButton(uint8_t pin, uint32_t& pressedAt) {
  bool down = digitalRead(pin) == LOW;
  uint32_t now = millis();
  if (down && !pressedAt) {
    pressedAt = now ? now : 1;
    return 0;
  }
  if (!down && pressedAt) {
    uint32_t d = now - pressedAt;
    pressedAt = 0;
    if (d < 30) return 0;
    return d >= 1200 ? 2 : 1;
  }
  return 0;
}

static uint32_t nextBeep = 0, beepOff = 0;
static void updateBeeper(float level, bool active, uint16_t len) {
  uint32_t now = millis();
  if (beepOff && (int32_t)(now - beepOff) >= 0) {
    noTone(PIN_BUZ);
    beepOff = 0;
  }
  if (!active) return;
  if ((int32_t)(now - nextBeep) >= 0) {
    tone(PIN_BUZ, 2700);
    beepOff = now + len;
    nextBeep = now + (uint32_t)(600 - 550 * level);
  }
}

void setup() {
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BUZ, OUTPUT);
  Serial.begin(115200);
  oled.begin();
  Wire.setClock(400000);
  loadCfg();
  startHardware(cfg.n);
  delay(1500);
  calibrate();
}

void loop() {
  static uint32_t b1At = 0, b2At = 0, lastDraw = 0, clipUntil = 0;
  static bool nullMode = false;
  static float m = 0, mRaw = 0, ang = 0;
  static bool ferrous = false;
  float i, q;
  bool clip;

  uint8_t b1 = readButton(PIN_BTN1, b1At);
  uint8_t b2 = readButton(PIN_BTN2, b2At);
  if (b1 == 1) {
    nullMode = false;
    calibrate();
  }
  if (b1 == 2) nullMode = !nullMode;
  if (b2 == 1) {
    if (m > 3 * thr) {
      cfg.ang = ang;
      cfg.taught = 1;
      saveCfg();
      showMsg("VAS BETANITVA");
      delay(1000);
    } else {
      showMsg("TUL KEVES JEL", "Vasat a tekercshez");
      delay(1500);
    }
  }
  if (b2 == 2) {
    autoTune();
    calibrate();
  }

  while (pollRx(i, q, clip)) {
    if (firstRx) {
      vI = i;
      vQ = q;
      firstRx = false;
    }
    vI += SMOOTH * (i - vI);
    vQ += SMOOTH * (q - vQ);
    float dI = vI - bI, dQ = vQ - bQ;
    m = sqrtf(dI * dI + dQ * dQ);
    mRaw = sqrtf(vI * vI + vQ * vQ);
    if (m < thr) {
      bI += (vI - bI) / DRIFT_DIV;
      bQ += (vQ - bQ) / DRIFT_DIV;
    }  // lassu drift-kovetes
    ang = atan2f(dQ, dI);
    ferrous = cosf(ang - cfg.ang) > 0;
    if (clip) clipUntil = millis() + 1000;
  }
  /*
  while (pollAc(r)) {
    float d = r - acBase;
    acD += 0.5f * (d - acD);
    if (fabsf(d) < acThr) acBase += (r - acBase) / 32.0f;
  }
  */

  bool mAct = m > thr;
  // bool aAct = acD > acThr;
  float mLevel = fminf(m / (thr * BAR_DIV), 1.0f);
  // float aLevel = fminf(fmaxf(acD, 0) / (acThr * 8), 1.0f);
  float nLevel = fminf(sqrtf(mRaw / 800.0f), 1.0f);

  if (millis() - lastDraw >= 200) {
    lastDraw = millis();
    char buf[24];
    oled.firstPage();
    do {
      oled.setFont(u8g2_font_6x10_tr);
      if (nullMode) {
        oled.drawStr(0, 9, "NULLAZAS");
        oled.drawFrame(0, 14, 128, 14);
        oled.drawBox(2, 16, (uint8_t)(124 * nLevel), 10);
        snprintf(buf, sizeof buf, "RX = %d", (int)mRaw);
        oled.drawStr(0, 44, buf);
        oled.drawStr(0, 58, "Mozgasd a tekercset");
      } else {
        oled.drawStr(0, 9, "FEM");
        oled.drawFrame(24, 1, 104, 9);
        oled.drawBox(26, 3, (uint8_t)(100 * mLevel), 5);
        // oled.drawStr(0, 31, "AC");
        // oled.drawFrame(24, 23, 104, 9);
        // oled.drawBox(26, 25, (uint8_t)(100 * aLevel), 5);
        snprintf(buf, sizeof buf, "m=%d", (int)m);
        oled.drawStr(0, 26, buf);
        snprintf(buf, sizeof buf, "f=%uHz", (unsigned)(500000UL / cfg.n));
        oled.drawStr(0, 40, buf);
        oled.setFont(u8g2_font_7x13B_tr);
        const char* txt = "-";
        if (millis() < clipUntil) txt = "TELITES!";
        // else if (aAct) txt = "ELO VEZETEK!";
        else if (mAct)
          txt = !cfg.taught ? "FEM (tanitsd be)"
                            : (ferrous ? "VAS" : "NEM-VAS (Cu/Al)");
        oled.drawStr(0, 63, txt);
      }
    } while (oled.nextPage());
  }

  if (nullMode) updateBeeper(nLevel, true, 15);
  // else if (aAct) updateBeeper(1.0f, true, 25);
  else
    updateBeeper(mLevel, mAct, ferrous || !cfg.taught ? 25 : 70);
}

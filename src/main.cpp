// Pulzusindukcios betonvas-kereso - Arduino Nano (ATmega328P), DD tekercs (Tx + Rx)
// Impulzus + mintavetelezes: egyetlen szal, megszakitas-tiltott idokritikus resz.
// Kozben a Timer1 szabadon fut (0,5 us/tick), a csipogo a Timer2 hardveres kimenete (D11).

#include <Arduino.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <math.h>
#include <stdlib.h>

// ---- Labkiosztas ----
// D4  = gomb: nullazas / DD beallito   A4/A5 = OLED I2C
// D5  = gomb: kalibralas               D11 = csipogo (OC2A, BC547-en keresztul)
// D2  = IRLZ44N gate (47R soros)       A1 = akku osztó (100k/33k)
// A0  = jel (U1D kimenet, x100)        A2 = U1A kimenet (x10), tartalek
// A3  = nyers Rx jel (R15 10k-n at), DD beallito mod (csatolas-meres)
static const uint8_t PIN_BTN_ZERO = 4;
static const uint8_t PIN_BTN_CAL = 5;
#define GATE_BIT PD2

// ---- Idozites (us) - oszcilloszkoppal finomhangolando ----
static const uint16_t T_ON = 60;        // gate bekapcsolt ido
static const uint16_t T_PERIOD = 1000;  // impulzus periodus (1 kHz)
static const uint16_t T_SAMPLE1 = 60;   // 1. mintavetel a kikapcsolas utan
static const uint16_t T_SAMPLE2 = 600;  // referencia mintavetel (alapvonal)
static const uint16_t N_PULSES = 100;   // impulzus / meres (kb. 100 ms)
static const uint16_t TICKS_PER_US = 2; // Timer1: 16 MHz / 8

// ---- Erzekelesi / kijelzesi parameterek (ADC egyseg, 0..1023) ----
static const float DETECT_TH = 1.5f;     // ennel nagyobb jel = talalat
static const float FULL_SCALE = 60.0f;   // kalibralatlan oszlop teljes kitereses
static const float SETUP_FULL = 500.0f;  // DD beallito mod: log skala teljes kitereses
static const float SETUP_GAIN_RATIO = 10.0f;  // A0 (x100) / A2 (U1A kimenet, x10)
static const float CAL_DEPTH_CM = 5.0f;  // kalibralasi melyseg
static const float BATT_DIV = (100.0f + 33.0f) / 33.0f;
static const uint16_t ZERO_UPDATES = 15; // inditasi nullazas hossza (x ~140 ms)
static const uint32_t LONG_PRESS_MS = 1000;

#define USE_WATCHDOG 1

// ---- Csipogo: Timer2 CTC, OC2A (D11) hardveres billentes, megszakitas nelkul ----
static void beepOff() {
  TCCR2A = 0;
  TCCR2B = 0;
  PORTB &= ~_BV(PB3);
}

static void beepOn(uint16_t f) {
  f = constrain(f, 250, 4000);
  OCR2A = (uint8_t)(62500UL / f - 1);  // 16 MHz / (2 * 128 * f)
  TCCR2A = _BV(COM2A0) | _BV(WGM21);
  TCCR2B = _BV(CS22) | _BV(CS20);
}

// ---- ADC ----
static inline uint16_t adcRead() {
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) {
  }
  return ADC;
}

static void adcFast() { ADCSRA = _BV(ADEN) | _BV(ADPS2); }                         // /16 = 1 MHz
static void adcSlow() { ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); }  // /128

// A mintavetel a konverzio indulasa utan ~1,5 ADC orajelnel tortenik.
static void pulse(uint16_t &s1, uint16_t &s2) {
  const uint8_t sreg = SREG;
  cli();
  const uint16_t t0 = TCNT1;
  PORTD |= _BV(GATE_BIT);
  while ((uint16_t)(TCNT1 - t0) < T_ON * TICKS_PER_US) {
  }
  PORTD &= ~_BV(GATE_BIT);
  const uint16_t toff = TCNT1;
  while ((uint16_t)(TCNT1 - toff) < T_SAMPLE1 * TICKS_PER_US) {
  }
  s1 = adcRead();
  while ((uint16_t)(TCNT1 - toff) < T_SAMPLE2 * TICKS_PER_US) {
  }
  s2 = adcRead();
  SREG = sreg;
}

static bool g_zeroSeen = false, g_calSeen = false;

// DD beallitas: az Rx nyers feszultsege a Tx bekapcsolt allapotaban (M * dI/dt, elojeles),
// es nyugalmi alapvonal. Az atfedes nullpontjan a kulonbseg elojelet valt.
static const uint16_t T_COUPLE = 40;  // mintavetel a bekapcsolas utan
static void pulseCoupling(uint16_t &s1, uint16_t &s2) {
  const uint8_t sreg = SREG;
  cli();
  const uint16_t t0 = TCNT1;
  PORTD |= _BV(GATE_BIT);
  while ((uint16_t)(TCNT1 - t0) < T_COUPLE * TICKS_PER_US) {
  }
  s1 = adcRead();
  while ((uint16_t)(TCNT1 - t0) < T_ON * TICKS_PER_US) {
  }
  PORTD &= ~_BV(GATE_BIT);
  const uint16_t toff = TCNT1;
  while ((uint16_t)(TCNT1 - toff) < T_SAMPLE2 * TICKS_PER_US) {
  }
  s2 = adcRead();
  SREG = sreg;
}

// Gombok mintavetelezese a meres kozben (kulonben a rovid nyomasok elvesznenek)
static inline void pollButtons() {
  if (!(PIND & _BV(PIN_BTN_ZERO))) g_zeroSeen = true;
  if (!(PIND & _BV(PIN_BTN_CAL))) g_calSeen = true;
}

static void measure(uint32_t &sum1, uint32_t &sum2, uint8_t ch) {
  sum1 = 0;
  sum2 = 0;
  g_zeroSeen = g_calSeen = false;
  ADMUX = _BV(REFS0) | ch;
  delayMicroseconds(300);
  (void)adcRead();
  for (uint16_t i = 0; i < N_PULSES; i++) {
    const uint16_t t0 = TCNT1;
    uint16_t a, b;
    if (ch == 2) {
      pulseCoupling(a, b);
    } else {
      pulse(a, b);
    }
    sum1 += a;
    sum2 += b;
    pollButtons();
    while ((uint16_t)(TCNT1 - t0) < T_PERIOD * TICKS_PER_US) {
    }
  }
}

// Vcc a belso 1,1 V-os referenciabol, akku az A1-en (Vcc-re aranyosan)
static void readSupply(float &vbat) {
  adcSlow();
  ADMUX = _BV(REFS0) | 0x0E;
  delayMicroseconds(1000);
  (void)adcRead();
  uint32_t s = 0;
  for (uint8_t i = 0; i < 8; i++) s += adcRead();
  const float vcc = 1.1f * 1023.0f * 8.0f / (float)s;
  ADMUX = _BV(REFS0) | 1;
  delayMicroseconds(300);
  (void)adcRead();
  s = 0;
  for (uint8_t i = 0; i < 8; i++) s += adcRead();
  vbat = (s / 8.0f) * vcc / 1023.0f * BATT_DIV;
  ADMUX = _BV(REFS0);
  adcFast();
  delayMicroseconds(300);
  (void)adcRead();
}

// ---- Kijelzo ----
static U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0);
static float g_zero = 0, g_calK = 0;  // g_calK: S ~ K / d^3

static void saveCal() { EEPROM.put(0, g_calK); }

static float fullScale() {
  return g_calK > 0 ? g_calK / (CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM) * 1.5f : FULL_SCALE;
}

static void drawHeader(const char *title, float vbat) {
  char b[8];
  dtostrf(vbat, 3, 1, b);
  strcat(b, "V");
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(0, 9, title);
  oled.drawStr(98, 9, b);
}

static void drawNormal(const char *msg, float mag, float vbat, bool overload, bool det) {
  char num[10], dep[12] = "";
  dtostrf(mag, 1, 0, num);
  if (g_calK > 0 && det && mag > 0) {
    dtostrf(cbrt(g_calK / mag), 1, 1, dep);
    strcat(dep, " cm");
  }
  const int w = (int)(constrain(mag / fullScale(), 0.0f, 1.0f) * 124);
  oled.firstPage();
  do {
    drawHeader("PI Scout", vbat);
    if (msg) {
      oled.setFont(u8g2_font_6x10_tr);
      oled.drawStr(0, 34, msg);
    } else if (overload) {
      oled.setFont(u8g2_font_10x20_tr);
      oled.drawStr(0, 36, "TULVEZERLES");
    } else {
      oled.setFont(u8g2_font_logisoso24_tn);
      oled.drawStr(0, 40, num);
      oled.setFont(u8g2_font_6x10_tr);
      if (dep[0]) {
        oled.drawStr(80, 34, dep);
      } else if (g_calK <= 0) {
        oled.drawStr(70, 34, "nincs kal.");
      }
      oled.drawFrame(0, 48, 128, 14);
      oled.drawBox(2, 50, w, 10);
    }
  } while (oled.nextPage());
}

// DD beallito mod: elojeles S1 - S2, kulon sorban a minimum es a nyers S1 (telites-ellenorzes)
static void drawSetup(float d, float best, float s1, bool overload, bool dcFault, uint8_t ch, float vbat) {
  char num[10], bst[10], s1s[8], line[24] = "min ";
  dtostrf(overload ? s1 - 512.0f : d, 1, 1, num);
  if (best < 1e8f) {
    dtostrf(best, 1, 1, bst);
  } else {
    strcpy(bst, "--");
  }
  dtostrf(s1, 1, 0, s1s);
  strcat(line, bst);
  strcat(line, "  S1 ");
  strcat(line, s1s);
  const float a = fabsf(overload ? 512.0f : d);
  const float v = overload ? s1 - 512.0f : d;
  const float frac = constrain(log(1.0f + a) / log(1.0f + SETUP_FULL), 0.0f, 1.0f);
  const int half = (int)(frac * 61) * (v < 0 ? -1 : 1);
  const bool ok = !overload && a < DETECT_TH;
  oled.firstPage();
  do {
    drawHeader(dcFault ? "DC HIBA!" : (overload ? "TULVEZERLES" : "DD beallitas"), vbat);
    oled.setFont(u8g2_font_10x20_tr);
    oled.drawStr(0, 31, num);
    oled.setFont(u8g2_font_6x10_tr);
    if (ok) oled.drawStr(90, 28, "NULL OK");
    oled.drawStr(0, 44, line);
    oled.drawFrame(0, 48, 128, 14);
    oled.drawVLine(63, 46, 18);
    if (half >= 0) {
      oled.drawBox(64, 50, half, 10);
    } else {
      oled.drawBox(64 + half, 50, -half, 10);
    }
  } while (oled.nextPage());
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_ZERO, INPUT_PULLUP);
  pinMode(PIN_BTN_CAL, INPUT_PULLUP);
  DDRD |= _BV(GATE_BIT);
  PORTD &= ~_BV(GATE_BIT);
  DDRB |= _BV(PB3);
  beepOff();

  TCCR1A = 0;
  TCCR1B = _BV(CS11);  // normal mod, /8
  ADMUX = _BV(REFS0);
  DIDR0 |= _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D);
  adcFast();

  EEPROM.get(0, g_calK);
  if (!(g_calK > 0 && g_calK < 1e9f)) g_calK = 0;

  Wire.begin();
  Wire.setWireTimeout(3000, true);
  oled.begin();
  oled.setBusClock(400000);

#if USE_WATCHDOG
  wdt_enable(WDTO_1S);  // lefagyaskor a gate-et a 10k lehuzo tartja alacsonyan
#endif
}

static bool g_setupMode = false;

void loop() {
  static uint32_t calPressStart = 0, zeroPressStart = 0, confirmUntil = 0, flashUntil = 0;
  static uint16_t zeroCount = 0;
  static float zeroAcc = 0, zeroSq = 0, best = 1e9f, runTh = DETECT_TH;
  static bool detOn = false;
  static bool calDone = false, zeroLong = false;
  bool &setupMode = g_setupMode;
  const uint8_t ch = setupMode ? 2 : 0;  // beallito mod: A2 (U1A x10, csatolas-meres)

  uint32_t s1, s2;
  measure(s1, s2, ch);
  wdt_reset();
  // Valodi DC-ellenorzes: impulzus nelkul, a telitesbol visszaterve (3 ms)
  delay(3);
  uint16_t idleLevel = 0;
  {
    uint32_t t = 0;
    for (uint8_t i = 0; i < 8; i++) t += adcRead();
    idleLevel = t / 8;
  }
  float vbat;
  readSupply(vbat);

  // A beallito mod nyers (x1) vagy U1A (x10) jelet mer; a kijelzett ertek x100-as egyenertekesre skalazott
  const float gainScale = setupMode ? SETUP_GAIN_RATIO : 1.0f;
  const float meanS1 = (float)s1 / N_PULSES;
  const float rawDiff = (float)((int32_t)s1 - (int32_t)s2) / N_PULSES;
  float diff = gainScale * rawDiff;
  {
    // Simitas (EMA): az ADC-zajt elnyomja; csatornavaltaskor vagy nagy ugrasnal ujrainditja
    static float smooth = 0;
    static uint8_t smoothCh = 255;
    const float jump = setupMode ? 8.0f * gainScale : 60.0f;
    if (ch != smoothCh || fabsf(diff - smooth) > jump) {
      smooth = diff;
      smoothCh = ch;
    } else {
      smooth += (setupMode ? 0.15f : 0.3f) * (diff - smooth);
    }
    diff = smooth;
  }
  const bool overload = meanS1 > 950 || meanS1 < 70;
  const uint32_t now = millis();

  // Nullazas gomb: rovid = ujranullazas, hosszu = DD beallito mod ki/be
  if (g_zeroSeen && !zeroPressStart) {
    zeroPressStart = now;
    zeroLong = false;
  }
  if (g_zeroSeen && !zeroLong && now - zeroPressStart > LONG_PRESS_MS) {
    zeroLong = true;
    setupMode = !setupMode;
    best = 1e9f;
    zeroCount = 0;
    zeroAcc = 0;
    confirmUntil = now + 150;
    return;  // a kovetkezo meres mar az uj csatornan fut
  }
  if (!g_zeroSeen && zeroPressStart) {
    if (!zeroLong) {
      if (setupMode) {
        best = 1e9f;
      } else {
        zeroCount = 0;
        zeroAcc = 0;
      }
    }
    zeroPressStart = 0;
  }

  if (g_calSeen && !calPressStart) {
    calPressStart = now;
    calDone = false;
  }
  if (!g_calSeen) calPressStart = 0;

  uint16_t freq = 0;
  float mag = 0, sig = 0;
  bool det = false;
  const char *msg = nullptr;

  if (setupMode) {
    const float a = fabsf(diff);
    if (overload) {
      freq = 250;  // telitesben: mély, folyamatos hang, a minimum nem frissül
    } else {
      if (a < best) best = a;
      if (a >= DETECT_TH) {
        freq = 300 + (uint16_t)(constrain(log(1.0f + a) / log(1.0f + SETUP_FULL), 0.0f, 1.0f) * 2700);
      }
    }
  } else if (zeroCount < ZERO_UPDATES) {
    if (zeroCount == 0) zeroSq = 0;
    zeroAcc += diff;
    zeroSq += diff * diff;
    zeroCount++;
    if (zeroCount == ZERO_UPDATES) {
      g_zero = zeroAcc / ZERO_UPDATES;
      const float var = zeroSq / ZERO_UPDATES - g_zero * g_zero;
      runTh = max(DETECT_TH, 5.0f * sqrtf(max(var, 0.0f)));  // zajhoz igazodo kuszob
      detOn = false;
    }
    msg = "Nullazas - legben!";
  } else {
    sig = diff - g_zero;
    mag = fabsf(sig);
    if (mag < runTh) g_zero += 0.01f * sig;  // lassu driftkovetes
    detOn = mag >= (detOn ? 0.6f * runTh : runTh);  // hiszteresis
    det = detOn && !overload;

    const bool calHeld = g_calSeen && calPressStart && !calDone;
    if (calHeld && now - calPressStart > LONG_PRESS_MS && det) {
      g_calK = mag * CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM;
      saveCal();
      calDone = true;
      confirmUntil = now + 300;
      flashUntil = now + 1500;
    } else if (calHeld) {
      msg = now - calPressStart > LONG_PRESS_MS ? (overload ? "Tulvezerles!" : "Nincs jel! Vas 5cm") : "Kalibralas...";
    } else if (now < flashUntil) {
      msg = "Kalibralva (5 cm)";
    }
    if (det) freq = 800 + (uint16_t)(constrain(mag / fullScale(), 0.0f, 1.0f) * 2200);
  }

  if (now < confirmUntil) freq = 3000;
  if (freq) {
    beepOn(freq);
  } else {
    beepOff();
  }

  if (setupMode) {
    drawSetup(diff, best, meanS1, overload, idleLevel > 900 || idleLevel < 120, ch, vbat);
  } else {
    drawNormal(msg, mag, vbat, overload, det);
  }

  Serial.print(setupMode ? F("setup") : F("run"));
  if (setupMode) {
    Serial.print(F(" ch=A"));
    Serial.print(ch);
  }
  Serial.print(F(" diff="));
  Serial.print(diff, 2);
  Serial.print(F(" s1="));
  Serial.print(meanS1, 0);
  Serial.print(F(" s2="));
  Serial.print((float)s2 / N_PULSES, 0);
  Serial.print(F(" idle="));
  Serial.print(idleLevel);
  Serial.print(F(" zero="));
  Serial.print(g_zero, 2);
  Serial.print(F(" sig="));
  Serial.print(sig, 2);
  Serial.print(F(" vbat="));
  Serial.println(vbat, 2);
}

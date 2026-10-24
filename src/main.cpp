// Pulzusindukcios betonvas-kereso - Raspberry Pi Pico (RP2040)
// core1: impulzus + mintavetelezes (idokritikus), core0: kijelzo, gombok, hang.

#include <Arduino.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <hardware/watchdog.h>

// ---- Labkiosztas ----
static const uint PIN_GATE = 2;   // IRLZ44N gate (47R soros)
static const uint PIN_SDA = 4;
static const uint PIN_SCL = 5;
static const uint PIN_BTN_ZERO = 10;  // nullazas
static const uint PIN_BTN_CAL = 11;   // kalibralas (hosszan nyomva)
static const uint PIN_BUZZ = 15;      // passziv csipogo (BC547-en keresztul)
// ADC0 = GP26 (jel), ADC1 = GP27 (akku osztó 100k/33k)

// ---- Idozites (us) - oszcilloszkoppal finomhangolando ----
static const uint32_t T_ON = 60;       // gate bekapcsolt ido
static const uint32_t T_PERIOD = 1000; // impulzus periodus (1 kHz)
static const uint32_t T_SAMPLE1 = 50;  // 1. mintavetel a kikapcsolas utan
static const uint32_t T_SAMPLE2 = 600; // referencia mintavetel (alapvonal)
static const int N_SAMPLES = 4;        // minta / mintavetelezesi pont
static const int N_PULSES = 100;       // impulzus / meres (100 ms)

// ---- Erzekelesi / kijelzesi parameterek ----
static const float DETECT_TH = 2.0f;    // ADC egyseg, ennel nagyobb jel = talalat
static const float FULL_SCALE = 100.0f; // kalibralatlan oszlop teljes kitereses
static const float CAL_DEPTH_CM = 5.0f; // kalibralasi melyseg
static const float BATT_DIV = (100.0f + 33.0f) / 33.0f;
static const int ZERO_UPDATES = 15;     // inditasi nullazas hossza (x100 ms)

// ---- Core1 -> core0 megosztott adatok ----
static volatile float g_diff = 0;   // (S1 - S2) atlag, ADC egysegben
static volatile float g_s1 = 0;     // S1 atlag (tulvezerles-ellenorzes)
static volatile float g_vbat = 0;
static volatile uint32_t g_seq = 0;

static inline uint32_t now_us() { return timer_hw->timerawl; }

static inline uint32_t sampleBurst() {
  uint32_t sum = 0;
  for (int i = 0; i < N_SAMPLES; i++) {
    adc_hw->cs |= ADC_CS_START_ONCE_BITS;
    while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    }
    sum += adc_hw->result;
  }
  return sum;
}

static void __not_in_flash_func(pulse)(uint32_t &s1, uint32_t &s2) {
  uint32_t t0 = now_us();
  gpio_put(PIN_GATE, 1);
  while (now_us() - t0 < T_ON) {
  }
  gpio_put(PIN_GATE, 0);
  uint32_t toff = now_us();
  while (now_us() - toff < T_SAMPLE1) {
  }
  s1 = sampleBurst();
  while (now_us() - toff < T_SAMPLE2) {
  }
  s2 = sampleBurst();
}

void setup1() {
  gpio_init(PIN_GATE);
  gpio_set_dir(PIN_GATE, GPIO_OUT);
  gpio_put(PIN_GATE, 0);
  adc_init();
  adc_gpio_init(26);
  adc_gpio_init(27);
  adc_select_input(0);
}

void loop1() {
  static uint32_t next = now_us();
  uint64_t sum1 = 0, sum2 = 0;
  for (int i = 0; i < N_PULSES; i++) {
    while ((int32_t)(now_us() - next) < 0) {
    }
    next += T_PERIOD;
    if ((int32_t)(now_us() - next) > (int32_t)T_PERIOD) next = now_us();  // lemaradas: nincs sorozat-impulzus
    uint32_t a, b;
    pulse(a, b);
    sum1 += a;
    sum2 += b;
  }
  adc_select_input(1);
  sleep_us(5);
  (void)adc_read();
  uint32_t vb = 0;
  for (int i = 0; i < 8; i++) vb += adc_read();
  adc_select_input(0);

  const float n = (float)N_SAMPLES * N_PULSES;
  g_s1 = sum1 / n;
  g_diff = ((float)sum1 - (float)sum2) / n;
  g_vbat = (vb / 8.0f) * 3.3f / 4095.0f * BATT_DIV;
  g_seq = g_seq + 1;
  watchdog_update();
}

// ---- Core0 ----
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0);
static float g_zero = 0, g_calK = 0;  // g_calK: S ~ K / d^3

static void saveCal() {
  EEPROM.put(0, g_calK);
  EEPROM.commit();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_ZERO, INPUT_PULLUP);
  pinMode(PIN_BTN_CAL, INPUT_PULLUP);
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_BUZZ, LOW);
  EEPROM.begin(256);
  EEPROM.get(0, g_calK);
  if (!(g_calK > 0 && g_calK < 1e9f)) g_calK = 0;
  Wire.setSDA(PIN_SDA);
  Wire.setSCL(PIN_SCL);
  oled.begin();
  oled.setBusClock(400000);
  oled.setFont(u8g2_font_6x10_tf);
  watchdog_enable(1000, 1);  // core1 etetja; lefagyaskor a gate lehuz
}

static float fullScale() {
  return g_calK > 0 ? g_calK / (CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM) * 1.5f : FULL_SCALE;
}

static void draw(const char *msg, float mag, float vbat, bool overload, bool det) {
  char b[24];
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  snprintf(b, sizeof b, "%.1fV", vbat);
  oled.drawStr(0, 9, "PI Scout");
  oled.drawStr(98, 9, b);
  if (msg) {
    oled.drawStr(0, 34, msg);
  } else if (overload) {
    oled.setFont(u8g2_font_10x20_tf);
    oled.drawStr(0, 36, "TULVEZERLES");
  } else {
    oled.setFont(u8g2_font_logisoso24_tn);
    snprintf(b, sizeof b, "%.0f", mag);
    oled.drawStr(0, 40, b);
    oled.setFont(u8g2_font_6x10_tf);
    if (g_calK > 0 && det) {
      snprintf(b, sizeof b, "~%.1f cm", cbrtf(g_calK / mag));
      oled.drawStr(80, 34, b);
    } else if (g_calK <= 0) {
      oled.drawStr(70, 34, "nincs kal.");
    }
    float f = constrain(mag / fullScale(), 0.0f, 1.0f);
    oled.drawFrame(0, 48, 128, 14);
    oled.drawBox(2, 50, (int)(f * 124), 10);
  }
  oled.sendBuffer();
}

void loop() {
  static uint32_t lastSeq = 0, calPressStart = 0;
  static int zeroCount = 0;
  static float zeroAcc = 0;
  static bool zeroPrev = false, calDone = false;

  bool bz = !digitalRead(PIN_BTN_ZERO);
  if (bz && !zeroPrev) {
    zeroCount = 0;
    zeroAcc = 0;
  }
  zeroPrev = bz;

  bool bc = !digitalRead(PIN_BTN_CAL);
  if (bc && calPressStart == 0) {
    calPressStart = millis();
    calDone = false;
  }
  if (!bc) calPressStart = 0;

  if (g_seq == lastSeq) return;
  lastSeq = g_seq;
  float diff = g_diff, vbat = g_vbat;
  bool overload = g_s1 > 4000 || g_s1 < 20;

  if (zeroCount < ZERO_UPDATES) {
    zeroAcc += diff;
    zeroCount++;
    if (zeroCount == ZERO_UPDATES) g_zero = zeroAcc / ZERO_UPDATES;
    noTone(PIN_BUZZ);
    draw("Nullazas - legben!", 0, vbat, false, false);
    return;
  }

  float sig = diff - g_zero;
  float mag = fabsf(sig);
  if (mag < DETECT_TH) g_zero += 0.01f * sig;  // lassu driftkovetes

  bool det = mag >= DETECT_TH && !overload;

  if (bc && !calDone && calPressStart && millis() - calPressStart > 1000 && det) {
    g_calK = mag * CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM;
    saveCal();
    calDone = true;
    tone(PIN_BUZZ, 3000, 300);
  } else if (det) {
    tone(PIN_BUZZ, 800 + (int)(constrain(mag / fullScale(), 0.0f, 1.0f) * 2200));
  } else {
    noTone(PIN_BUZZ);
  }

  draw(nullptr, mag, vbat, overload, det);
  Serial.printf("diff=%.2f s1=%.0f zero=%.2f sig=%.2f vbat=%.2f\n", diff, g_s1, g_zero, sig, vbat);
}

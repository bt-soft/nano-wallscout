// Pulzusindukcios betonvas-kereso - Arduino Nano (ATmega328P), DD tekercs (Tx + Rx)
// Impulzus + mintavetelezes: egyetlen szal, megszakitas-tiltott idokritikus resz.
// Kozben a Timer1 szabadon fut (0,5 us/tick), a csipogo a Timer2 hardveres kimenete (D11).
//
// Modulok: config.h (beallitasok), sensor (meres), beeper (csipogo),
//          calibration (melyseg), display (OLED).

#include <Arduino.h>
#include <avr/wdt.h>
#include <math.h>

#include "beeper.h"
#include "calibration.h"
#include "config.h"
#include "display.h"
#include "sensor.h"

// A beallito mod is a normal (x100) jelutat meri, nullazatlanul
static const uint8_t CHANNEL = 0;

// Egy meresi kor eredmenye
struct Measurement {
  float s1Mean, s2Mean;  // az S1 / S2 minta atlaga (ADC egyseg)
  float diff;            // S1 - S2, simitva
  uint16_t idle;         // A0 szintje impulzus nelkul
  bool overload;         // az erosito telitesben van
  bool dcFault;          // a bemenet nyugalmi DC szintje hibas
  float vbat;
};

// A hang / kijelzes bemenete, az uzemmod logikaja tolti ki
struct Outcome {
  uint16_t freq = 0;  // csipogo frekvencia, 0 = nem szol
  float mag = 0;      // nullazott jel abszolut erteke
  float sig = 0;      // nullazott jel (elojeles)
  bool det = false;   // talalat
  const char* msg = nullptr;
};

struct State {
  bool setupMode = false;
  // gombok
  uint32_t calPressStart = 0, zeroPressStart = 0;
  bool calDone = false, zeroLong = false;
  // visszajelzes
  uint32_t confirmUntil = 0, flashUntil = 0;
  // nullazas
  uint16_t zeroCount = 0;
  float zeroAcc = 0, zeroSq = 0, zero = 0;
  float runTh = DETECT_TH;  // zajhoz igazodo erzekelesi kuszob
  bool detOn = false;
  // DD beallito mod: eddigi legkisebb abszolut atallas
  float best = 1e9f;
  // simitas
  float smooth = 0;
  uint8_t smoothCh = 255;
};

static State st;

void setup() {
  Serial.begin(115200);
  sensorInit();
  beepInit();
  calLoad();
  displayInit();

#if USE_WATCHDOG
  wdt_enable(WDTO_1S);  // lefagyaskor a gate-et a 10k lehuzo tartja alacsonyan
#endif
}

// ---- Meres ----

static float smoothDiff(float diff) {
  if (CHANNEL != st.smoothCh || fabsf(diff - st.smooth) > SMOOTH_JUMP) {
    st.smooth = diff;
    st.smoothCh = CHANNEL;
  } else {
    st.smooth += (st.setupMode ? SMOOTH_SETUP : SMOOTH_RUN) * (diff - st.smooth);
  }
  return st.smooth;
}

static Measurement acquire() {
  Measurement m;
  uint32_t s1, s2;
  measure(s1, s2, CHANNEL);
  wdt_reset();
  delay(3);  // DC-ellenorzes: impulzus nelkul, a telitesbol visszaterve
  m.idle = readIdleLevel();
  m.vbat = readBattery();

  m.s1Mean = (float)s1 / N_PULSES;
  m.s2Mean = (float)s2 / N_PULSES;
  m.diff = smoothDiff((float)((int32_t)s1 - (int32_t)s2) / N_PULSES);
  m.overload = m.s1Mean > OVERLOAD_HIGH || m.s1Mean < OVERLOAD_LOW;
  m.dcFault = m.idle > IDLE_HIGH || m.idle < IDLE_LOW;
  return m;
}

// ---- Gombok ----

static void restartZeroing() {
  st.zeroCount = 0;
  st.zeroAcc = 0;
}

// NULLAZAS gomb: rovid = ujranullazas (beallito modban kilepes), hosszu = beallito modba lepes.
// Igaz, ha modvaltas tortent, es a ciklus hatralevo resze kihagyando.
static bool handleZeroButton(uint32_t now) {
  if (g_zeroSeen && !st.zeroPressStart) {
    st.zeroPressStart = now;
    st.zeroLong = false;
  }
  if (g_zeroSeen && !st.setupMode && !st.zeroLong && now - st.zeroPressStart > LONG_PRESS_MS) {
    st.zeroLong = true;
    st.setupMode = true;
    st.best = 1e9f;
    restartZeroing();
    st.confirmUntil = now + 150;
    return true;  // a kovetkezo meres mar az uj modban fut
  }
  if (!g_zeroSeen && st.zeroPressStart) {
    if (!st.zeroLong) {
      // beallito modban barmilyen nyomas kilep es ujranullaz
      if (st.setupMode) {
        st.setupMode = false;
        st.confirmUntil = now + 150;
      }
      restartZeroing();
    }
    st.zeroPressStart = 0;
  }
  return false;
}

// KALIBRALAS gomb: a nyomas kezdete rogzitodik; beallito modban a D5 a min torlese
static void handleCalButton(uint32_t now) {
  if (g_calSeen && !st.calPressStart) {
    st.calPressStart = now;
    st.calDone = false;
    if (st.setupMode) st.best = 1e9f;
  }
  if (!g_calSeen) st.calPressStart = 0;
}

// ---- Uzemmodok ----

static uint16_t setupFreq(float a) {
  const float frac = constrain(log(1.0f + a) / log(1.0f + SETUP_FULL), 0.0f, 1.0f);
  return 300 + (uint16_t)(frac * 2700);
}

static void runSetupMode(const Measurement& m, Outcome& out) {
  const float a = fabsf(m.diff);
  if (m.overload) {
    out.freq = 250;  // telitesben: mely, folyamatos hang, a minimum nem frissul
    return;
  }
  if (a < st.best) st.best = a;
  if (a >= DETECT_TH) out.freq = setupFreq(a);
}

static void runZeroing(const Measurement& m, Outcome& out) {
  if (st.zeroCount == 0) st.zeroSq = 0;
  st.zeroAcc += m.diff;
  st.zeroSq += m.diff * m.diff;
  st.zeroCount++;
  if (st.zeroCount == ZERO_UPDATES) {
    st.zero = st.zeroAcc / ZERO_UPDATES;
    const float var = st.zeroSq / ZERO_UPDATES - st.zero * st.zero;
    st.runTh = max(DETECT_TH, 5.0f * sqrtf(max(var, 0.0f)));  // zajhoz igazodo kuszob
    st.detOn = false;
  }
  out.msg = "Nullazas - legben!";
}

static void handleCalibration(const Measurement& m, uint32_t now, Outcome& out) {
  const bool calHeld = g_calSeen && st.calPressStart && !st.calDone;
  const bool longPress = now - st.calPressStart > LONG_PRESS_MS;
  if (calHeld && longPress && out.det) {
    calSet(calKFromSignal(out.mag));
    st.calDone = true;
    st.confirmUntil = now + 300;
    st.flashUntil = now + 1500;
  } else if (calHeld) {
    out.msg = longPress ? (m.overload ? "Tulvezerles!" : "Nincs jel! Vas 5cm") : "Kalibralas...";
  } else if (now < st.flashUntil) {
    out.msg = "Kalibralva (5 cm)";
  }
}

static void runDetect(const Measurement& m, uint32_t now, Outcome& out) {
  out.sig = m.diff - st.zero;
  out.mag = fabsf(out.sig);
  if (out.mag < st.runTh) st.zero += DRIFT_FOLLOW * out.sig;       // lassu driftkovetes
  st.detOn = out.mag >= (st.detOn ? 0.6f * st.runTh : st.runTh);  // hiszteresis
  out.det = st.detOn && !m.overload;

  handleCalibration(m, now, out);
  if (out.det) out.freq = 800 + (uint16_t)(constrain(out.mag / calFullScale(), 0.0f, 1.0f) * 2200);
}

// ---- Kimenetek ----

static void updateBeeper(uint16_t freq, uint32_t now) {
  if (now < st.confirmUntil) freq = 3000;
  if (freq) {
    beepOn(freq);
  } else {
    beepOff();
  }
}

static void logSerial(const Measurement& m, const Outcome& out) {
  Serial.print(st.setupMode ? F("setup") : F("run"));
  if (st.setupMode) {
    Serial.print(F(" ch=A"));
    Serial.print(CHANNEL);
  }
  Serial.print(F(" diff="));
  Serial.print(m.diff, 2);
  Serial.print(F(" s1="));
  Serial.print(m.s1Mean, 0);
  Serial.print(F(" s2="));
  Serial.print(m.s2Mean, 0);
  Serial.print(F(" idle="));
  Serial.print(m.idle);
  Serial.print(F(" zero="));
  Serial.print(st.zero, 2);
  Serial.print(F(" sig="));
  Serial.print(out.sig, 2);
  Serial.print(F(" vbat="));
  Serial.println(m.vbat, 2);
}

void loop() {
  const Measurement m = acquire();
  const uint32_t now = millis();

  if (handleZeroButton(now)) return;
  handleCalButton(now);

  Outcome out;
  if (st.setupMode) {
    runSetupMode(m, out);
  } else if (st.zeroCount < ZERO_UPDATES) {
    runZeroing(m, out);
  } else {
    runDetect(m, now, out);
  }

  updateBeeper(out.freq, now);
  if (st.setupMode) {
    drawSetup(m.diff, st.best, m.s1Mean, m.overload, m.dcFault, m.vbat);
  } else {
    drawNormal(out.msg, out.mag, m.vbat, m.overload, out.det);
  }
  logSerial(m, out);
}

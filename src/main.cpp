/**
 * @file Arduino Nano-alapú, DD-tekercses betonvas-kereső vezérlőprogramja.
 *
 * Az impulzusmérés egyetlen szálon fut; a kritikus mintavételezés alatt
 * a megszakítások tiltva vannak. A Timer1 időalapja 0,5 µs, a hangjelzőt
 * a D11-es Timer2-kimenet vezérli.
 *
 * A konfigurációt a config.h, a mérést a sensor, a hangjelzést a beeper,
 * a mélységkalibrálást a calibration, a kijelzést pedig a display kezeli.
 */

#include <Arduino.h>
#include <avr/wdt.h>
#include <math.h>

#include "beeper.h"
#include "calibration.h"
#include "config.h"
#include "display.h"
#include "sensor.h"

/**
 * @brief A mérőjel ADC-csatornája; a beállító mód is ezt a jelutat használja.
 */
static const uint8_t CHANNEL = 0;

/**
 * @brief Egy mérési ciklus összesített eredménye.
 */
struct Measurement {
  /** Az S1- és S2-minták átlagai ADC-egységben. */
  float s1Mean, s2Mean;
  /** Az S1 − S2 különbség simított értéke. */
  float diff;
  /** Az A0 bemenet impulzus nélküli szintje. */
  uint16_t idle;
  /** Igaz, ha az erősítő túlvezérlésben van. */
  bool overload;
  /** Igaz, ha a bemenet nyugalmi DC-szintje hibás. */
  bool dcFault;
  /** Az akkumulátor mért feszültsége voltban. */
  float vbat;
};

/**
 * @brief Az üzemmód-logika által előállított hang- és kijelzési adatok.
 */
struct Outcome {
  /** A hangjelző frekvenciája hertzben; nulla esetén néma. */
  uint16_t freq = 0;
  /** A nullázott jel abszolút értéke. */
  float mag = 0;
  /** A nullázott, előjeles jel. */
  float sig = 0;
  /** Igaz, ha találat történt. */
  bool det = false;
  /** Opcionális, a kijelzőn megjelenő állapotüzenet. */
  const char* msg = nullptr;
};

/**
 * @brief A főciklus állapotát, a gombokat és a simítás adatait tárolja.
 */
struct State {
  /** Igaz, ha a DD-beállító mód aktív. */
  bool setupMode = false;
  /** A kalibráló- és nullázógomb lenyomásának kezdőideje. */
  uint32_t calPressStart = 0, zeroPressStart = 0;
  /** A kalibrálás befejezett és a hosszú gombnyomás észlelt állapota. */
  bool calDone = false, zeroLong = false;
  /** A hangjelzés és a kalibrálási visszajelzés lejárati ideje. */
  uint32_t confirmUntil = 0, flashUntil = 0;
  /** A nullázáshoz összegyűjtött minták száma és összesítése. */
  uint16_t zeroCount = 0;
  float zeroAcc = 0, zeroSq = 0, zero = 0;
  /** A zajszinthez igazított futási küszöb. */
  float runTh = DETECT_TH;
  /** A hiszterézises találatjelzés aktuális állapota. */
  bool detOn = false;
  /** Az aktuális találat előjele; ellenkező előjelnél a jel el van nyomva. */
  int8_t signalPolarity = 0;
  /** A küszöb alatti állapot kezdete az előjelzár feloldásához. */
  uint32_t polarityQuietSince = 0;
  /** Az előző simított különbség és az egymás utáni állandó mérések száma. */
  float prevDiff = 0;
  uint16_t steadyCount = 0;
  /** A DD-beállító módban eddig mért legkisebb abszolút áthallás. */
  float best = 1e9f;
  /** Az exponenciálisan simított mérési érték és annak csatornája. */
  float smooth = 0;
  uint8_t smoothCh = 255;
};

/** @brief A program globális mérési és üzemmód-állapota. */
static State st;

/**
 * @brief Elindítja a soros portot, az érzékelőt, a hangjelzőt és a kijelzőt.
 * @return Nincs visszatérési érték.
 */
void setup() {
  Serial.begin(115200);
  sensorInit();
  beepInit();
  calLoad();
  displayInit();

#if USE_WATCHDOG
  wdt_enable(WDTO_1S);
#endif
}

/**
 * @brief Simítja az S1 − S2 különbséget exponenciális átlagolással.
 * @param diff Az új, simítandó mérési különbség.
 * @return A simított különbség.
 */
static float smoothDiff(float diff) {
  if (CHANNEL != st.smoothCh || fabsf(diff - st.smooth) > SMOOTH_JUMP) {
    st.smooth = diff;
    st.smoothCh = CHANNEL;
  } else {
    st.smooth += (st.setupMode ? SMOOTH_SETUP : SMOOTH_RUN) * (diff - st.smooth);
  }
  return st.smooth;
}

/**
 * @brief Elvégzi az impulzusmérést, és kiszámítja a mérés állapotjelzőit.
 * @return Az átlagolt mintákat, a nyugalmi szintet és az akkufeszültséget tartalmazó mérés.
 */
static Measurement acquire() {
  Measurement m;
  uint32_t s1, s2;
  measure(s1, s2, CHANNEL);
  wdt_reset();
  delay(3);
  m.idle = readIdleLevel();
  m.vbat = readBattery();

  m.s1Mean = (float)s1 / N_PULSES;
  m.s2Mean = (float)s2 / N_PULSES;
  m.diff = smoothDiff((float)((int32_t)s1 - (int32_t)s2) / N_PULSES);
  m.overload = m.s1Mean > OVERLOAD_HIGH || m.s1Mean < OVERLOAD_LOW;
  m.dcFault = m.idle > IDLE_HIGH || m.idle < IDLE_LOW;
  return m;
}

/**
 * @brief Újrakezdi az indulási vagy kézi nullázás mintagyűjtését.
 * @return Nincs visszatérési érték.
 */
static void restartZeroing() {
  st.zeroCount = 0;
  st.zeroAcc = 0;
  st.signalPolarity = 0;
  st.polarityQuietSince = 0;
  st.steadyCount = 0;
  st.detOn = false;
}

/**
 * @brief Kezeli a nullázógomb rövid és hosszú megnyomását.
 * @param now Az aktuális idő milliszekundumban.
 * @return Igaz, ha üzemmódváltás történt, ezért a mérési ciklust ki kell hagyni.
 */
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
    return true;
  }
  if (!g_zeroSeen && st.zeroPressStart) {
    if (!st.zeroLong) {
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

/**
 * @brief Rögzíti a kalibrálógomb állapotát, és törli a beállító mód minimumát.
 * @param now Az aktuális idő milliszekundumban.
 * @return Nincs visszatérési érték.
 */
static void handleCalButton(uint32_t now) {
  if (g_calSeen && !st.calPressStart) {
    st.calPressStart = now;
    st.calDone = false;
    if (st.setupMode) st.best = 1e9f;
  }
  if (!g_calSeen) st.calPressStart = 0;
}

/**
 * @brief A DD-beállító mód áthallásához hangfrekvenciát rendel.
 * @param a Az áthallás abszolút értéke.
 * @return A hangjelző frekvenciája hertzben.
 */
static uint16_t setupFreq(float a) {
  const float frac = constrain(log(1.0f + a) / log(1.0f + SETUP_FULL), 0.0f, 1.0f);
  return 300 + (uint16_t)(frac * 2700);
}

/**
 * @brief Frissíti a DD-beállító mód minimumát és hangjelzését.
 * @param m Az aktuális mérési eredmény.
 * @param out A mód által frissítendő hang- és kijelzési adatok.
 * @return Nincs visszatérési érték.
 */
static void runSetupMode(const Measurement& m, Outcome& out) {
  const float a = fabsf(m.diff);
  if (m.overload) {
    out.freq = 250;
    return;
  }
  if (a < st.best) st.best = a;
  if (a >= DETECT_TH) out.freq = setupFreq(a);
}

/**
 * @brief Összegyűjti a nullázási mintákat, és meghatározza a zajküszöböt.
 * @param m Az aktuális mérési eredmény.
 * @param out A nullázási állapotüzenet kimenete.
 * @return Nincs visszatérési érték.
 */
static void runZeroing(const Measurement& m, Outcome& out) {
  if (st.zeroCount == 0) st.zeroSq = 0;
  st.zeroAcc += m.diff;
  st.zeroSq += m.diff * m.diff;
  st.zeroCount++;
  if (st.zeroCount == ZERO_UPDATES) {
    st.zero = st.zeroAcc / ZERO_UPDATES;
    const float var = st.zeroSq / ZERO_UPDATES - st.zero * st.zero;
    st.runTh = max(DETECT_TH, 5.0f * sqrtf(max(var, 0.0f)));
    st.detOn = false;
  }
  out.msg = "Nullazas - legben!";
}

/**
 * @brief Elvégzi a gombnyomásra indított mélységkalibrálást.
 * @param m Az aktuális mérési eredmény.
 * @param now Az aktuális idő milliszekundumban.
 * @param out A kalibrálás kijelzési és hangjelzési adatai.
 * @return Nincs visszatérési érték.
 */
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

/**
 * @brief Kiszámítja a nullázott jelet, és frissíti a találat állapotát.
 * @param m Az aktuális mérési eredmény.
 * @param now Az aktuális idő milliszekundumban.
 * @param out A találat, a hang és a kijelzés kimeneti adatai.
 * @return Nincs visszatérési érték.
 */
static void runDetect(const Measurement& m, uint32_t now, Outcome& out) {
  out.sig = m.diff - st.zero;
  const float rawSig = out.sig;
  const float rawMag = fabsf(rawSig);

  if (st.signalPolarity == 0 && rawMag >= st.runTh) {
    st.signalPolarity = rawSig < 0 ? -1 : 1;
  }
  if (st.signalPolarity != 0) {
    if (rawMag < st.runTh) {
      if (!st.polarityQuietSince) st.polarityQuietSince = now;
      if (now - st.polarityQuietSince >= POLARITY_RELEASE_MS) {
        st.signalPolarity = 0;
        st.polarityQuietSince = 0;
      }
    } else {
      st.polarityQuietSince = 0;
    }
    if (st.signalPolarity != 0 && rawSig * st.signalPolarity < 0) out.sig = 0;
  }

  // Hosszan álló jel nem mozgó fém, hanem nullpont-eltolódás: lassan újranullázunk.
  const bool steady = fabsf(m.diff - st.prevDiff) < 0.3f * st.runTh;
  st.prevDiff = m.diff;
  if (steady) {
    if (st.steadyCount < 255) st.steadyCount++;
  } else {
    st.steadyCount = 0;
  }
  const bool rezero = st.steadyCount >= STEADY_CYCLES;
  if (rezero) {
    st.zero += REZERO_FOLLOW * rawSig;
    out.sig = m.diff - st.zero;
    if (fabsf(out.sig) < st.runTh) st.signalPolarity = 0;
  }

  out.mag = fabsf(out.sig);
  if (!rezero && out.mag < st.runTh) st.zero += DRIFT_FOLLOW * out.sig;
  st.detOn = out.mag >= (st.detOn ? 0.6f * st.runTh : st.runTh);
  out.det = st.detOn && !m.overload;

  handleCalibration(m, now, out);
  if (out.det) out.freq = 800 + (uint16_t)(constrain(out.mag / calFullScale(), 0.0f, 1.0f) * 2200);
}

/**
 * @brief Frissíti a hangjelző állapotát és frekvenciáját.
 * @param freq A kért hangfrekvencia hertzben; nulla esetén a hangjelző kikapcsol.
 * @param now Az aktuális idő milliszekundumban.
 * @return Nincs visszatérési érték.
 */
static void updateBeeper(uint16_t freq, uint32_t now) {
  if (now < st.confirmUntil) freq = 3000;
  if (freq) {
    beepOn(freq);
  } else {
    beepOff();
  }
}

/**
 * @brief Kiírja a mérési eredményeket a soros portra.
 * @param m Az aktuális mérési eredmény.
 * @param out Az üzemmód kimeneti adatai.
 * @return Nincs visszatérési érték.
 */
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

/**
 * @brief Végrehajtja a mérési főciklust és frissíti a kijelzőket.
 * @return Nincs visszatérési érték.
 */
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

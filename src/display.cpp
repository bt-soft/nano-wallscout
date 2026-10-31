#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

#include "calibration.h"
#include "config.h"
#include "display.h"

// OLED-kijelző kezelése U8g2 könyvtárral.
static U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0);

/**
 * @brief Elindítja az I²C-buszt, és inicializálja az OLED-kijelzőt.
 *
 */
void displayInit() {
    Wire.begin();
    Wire.setWireTimeout(3000, true); // I²C busz időkorlát beállítása 3000 ms-ra.
    oled.begin();
    oled.setBusClock(400000); // I²C busz órajele 400 kHz-re állítva.
}

/**
 * @brief Kikapcsolja az OLED-kijelzőt és lezárja az I²C-buszt.
 */
void displaySleep() {
    oled.setPowerSave(1);
    Wire.end();
}

/**
 * @brief Kirajzolja a fejléc címét és az akkumulátor feszültségét.
 *
 * @param title A fejlécben megjelenő cím.
 * @param vbat Az akkumulátor feszültsége voltban.
 *
 */
static void drawHeader(const char *title, float vbat) {
    char b[8];
    dtostrf(vbat, 3, 1, b);
    strcat(b, "V");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(0, 9, title);
    oled.drawStr(98, 9, b);
}

/**
 * @brief Kirajzolja a vas helyzetét jelző kis ikont (9x9 pixel).
 *
 * @param x Az ikon bal szélének X koordinátája.
 * @param y Az ikon tetejének Y koordinátája.
 * @param orient +1: párhuzamos ('|'), -1: merőleges ('-'), 0: nincs ikon.
 */
static void drawOrientation(int x, int y, int8_t orient) {
    if (orient > 0) {
        oled.drawBox(x + 3, y, 3, 9);
    } else if (orient < 0) {
        oled.drawBox(x, y + 3, 9, 3);
    }
}

/**
 * @brief Kirajzolja a normál keresési képernyőt a jel és a kalibrációs adatok alapján.
 *
 * @param msg Opcionális állapotüzenet; null értéknél a mérési adat jelenik meg.
 * @param mag A jel abszolút értéke.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param det Igaz, ha a jel találatot jelez.
 *
 */
void drawNormal(const char *msg, float mag, float vbat, bool overload, bool det) {
    char num[10], dep[12] = "";
    dtostrf(mag, 1, 0, num);
    if (calIsSet() && det && mag > 0) {
        dtostrf(calDepthCm(mag), 1, 1, dep);
        strcat(dep, " cm");
    }

    const int w = (int)(constrain(mag / calFullScale(), 0.0f, 1.0f) * 124);

    oled.firstPage();

    do {
        drawHeader("Kereses", vbat);
        if (msg) {
            oled.setFont(u8g2_font_6x10_tr);
            oled.drawStr(0, 34, msg);

        } else if (overload) {
            oled.setFont(u8g2_font_6x10_tr);
            oled.drawStr(0, 34, "TULVEZERLES");

        } else {
            oled.setFont(u8g2_font_logisoso24_tn);
            oled.drawStr(0, 40, num);
            oled.setFont(u8g2_font_6x10_tr);
            if (dep[0]) {
                oled.drawStr(80, 34, dep);
            } else if (!calIsSet()) {
                oled.drawStr(70, 34, "nincs kal.");
            }
            oled.drawFrame(0, 48, 128, 14);
            oled.drawBox(2, 50, w, 10);
        }
    } while (oled.nextPage());
}

/**
 * @brief Kirajzolja a tanítási képernyőt a jel és a kalibrációs adatok alapján.
 *
 * @param msg Opcionális állapotüzenet; null értéknél a mérési adat jelenik meg.
 * @param mag A jel abszolút értéke.
 * @param threshold A tanítási küszöbérték.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param orient A vas helyzete: +1 párhuzamos, -1 merőleges, 0 ismeretlen.
 *
 */
void drawTeach(const char *msg, float mag, float threshold, float vbat, bool overload, int8_t orient) {
    char num[10], dep[14] = "";
    dtostrf(mag, 1, 1, num);
    if (calIsSet() && mag >= threshold) {
        dtostrf(calDepthCm(mag), 1, 1, dep);
        strcat(dep, " cm");
    }

    const int w = (int)(constrain(mag / calFullScale(), 0.0f, 1.0f) * 124);

    oled.firstPage();

    do {
        drawHeader("Tanitas 5cm", vbat);
        oled.setFont(u8g2_font_6x10_tr);
        if (overload) {
            oled.drawStr(0, 20, "TULVEZERLES");
        } else if (msg) {
            oled.drawStr(0, 20, msg);
        } else {
            oled.drawStr(0, 20, "Vas 5cm, gomb: rogzit");
        }
        oled.setFont(u8g2_font_logisoso24_tn);
        oled.drawStr(0, 46, num);
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(80, 40, dep[0] ? dep : (calIsSet() ? "-- cm" : "nincs kal."));
        if (!overload && mag >= threshold) {
            drawOrientation(112, 21, orient);
        }
        oled.drawFrame(0, 50, 128, 12);
        oled.drawBox(2, 52, w, 8);
    } while (oled.nextPage());
}

/**
 * @brief Kirajzolja a gördülő jelgrafikont és a pöttyözött küszöbvonalat.
 *
 * @param graph A kirajzolandó minták.
 * @param top A grafikon területének teteje (pixel).
 * @param bottom A grafikon alapvonala (pixel).
 */
static void drawGraph(const SignalGraph &graph, int top, int bottom) {
    oled.drawHLine(0, bottom, 128);

    const int height = bottom - top;

    // Pöttyözött küszöbvonal
    const int thresholdY = bottom - height / 4;
    for (int x = 0; x < 128; x += 3) {
        oled.drawPixel(x, thresholdY);
    }

    int prevX = 0, prevY = 0;
    for (uint8_t i = 0; i < graph.count; ++i) {
        const int x = 128 - graph.count * 4 + i * 4;
        const int y = bottom - (int)graph.at(i) * height / 32;
        oled.drawPixel(x, y);
        if (i > 0) {
            oled.drawLine(prevX, prevY, x, y);
        }
        prevX = x;
        prevY = y;
    }
}

/**
 * @brief A vas helyzete a jel előjele alapján; 0, ha hiba van vagy a jel a küszöb alatt marad.
 *
 * @param sig Az előjeles jel.
 * @param threshold A detektálási küszöbérték.
 * @param fault Igaz, ha hiba történt (DC hiba vagy túlvezérlés).
 *
 * @return A vas helyzete: 0, ha hiba van vagy a jel a küszöb alatt marad; 1, ha a jel pozitív; -1, ha a jel negatív.
 */
static int8_t orientOf(float sig, float threshold, bool fault) { return (fault || fabsf(sig) < threshold) ? 0 : (sig > 0 ? 1 : -1); }

/**
 * @brief Kirajzolja a Jel-Ido képernyőt: az előjeles jelet, a vas helyzetét és a gördülő grafikont.
 *
 * @param sig Az előjeles, nullázott jel.
 * @param threshold A detektálási küszöbérték.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha DC hiba történt.
 * @param graph A grafikon mintái.
 */
void drawSignalTime(float sig, float threshold, float vbat, bool overload, bool dcFault, const SignalGraph &graph) {
    char num[10];
    dtostrf(sig, 1, 0, num); // Egész számra kerekítve
    if (strcmp(num, "-0") == 0) {
        num[0] = '0';
        num[1] = '\0';
    }

    const bool fault = dcFault || overload;
    const char *status = dcFault ? "DC HIBA!" : "TULVEZERLES";

    oled.firstPage();

    do {
        drawHeader("Jel-Ido", vbat);
        if (fault) {
            oled.setFont(u8g2_font_6x10_tr);
            oled.drawStr(128 - oled.getStrWidth(status), 20, status);
        }
        oled.setFont(u8g2_font_helvB12_tn);
        oled.drawStr(0, 22, num);
        drawOrientation(116, 14, orientOf(sig, threshold, fault));
        drawGraph(graph, 24, 63);
    } while (oled.nextPage());
}

#if ENABLE_CENTER_FINDER_SCREEN
/**
 * @brief Kirajzolja a középkereső képernyőt: a jelet, a csúcs- és visszatérési értéket, a kitérés-sávot és a grafikont.
 *
 * @param msg Opcionális állapotüzenet.
 * @param sig Az előjeles, nullázott jel.
 * @param threshold A detektálási küszöbérték.
 * @param peak A csúcsérték.
 * @param returnPeak A visszatérési csúcsérték.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha DC hiba történt.
 * @param stage A középkereső aktuális szakasza.
 * @param graph A grafikon mintái.
 */
void drawCenterFinder(const char *msg, float sig, float threshold, float peak, float returnPeak, float vbat, bool overload, bool dcFault, CenterStage stage, const SignalGraph &graph) {
    char num[10], peakText[8], returnText[8];
    dtostrf(fabsf(sig), 1, 1, num);
    dtostrf(peak, 1, 1, peakText);
    dtostrf(returnPeak, 1, 1, returnText);

    const int half = (int)(constrain(sig / (threshold * 4.0f), -1.0f, 1.0f) * 58.0f);
    const char *status = dcFault ? "DC HIBA!" : (overload ? "TULVEZERLES" : (msg ? msg : centerStageLabel(stage)));

    oled.firstPage();

    do {
        drawHeader("Kozep keres", vbat);
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(0, 20, status);
        drawOrientation(116, 14, orientOf(sig, threshold, dcFault || overload));

        oled.setFont(u8g2_font_logisoso24_tn);
        oled.drawStr(0, 42, num);
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(75, 30, "P:");
        oled.drawStr(88, 30, peakText);
        oled.drawStr(75, 41, "R:");
        oled.drawStr(88, 41, returnText);

        oled.drawFrame(2, 45, 124, 6);
        oled.drawVLine(63, 44, 8);
        if (half >= 0) {
            oled.drawBox(64, 46, half, 4);
        } else {
            oled.drawBox(64 + half, 46, -half, 4);
        }
        drawGraph(graph, 53, 63);
    } while (oled.nextPage());
}
#endif // ENABLE_CENTER_FINDER_SCREEN

/**
 * @brief A DD-beállító áthallásának logaritmikus aránya a teljes skálához képest.
 * @param a Az áthallás abszolút értéke.
 * @return Az arány 0 és 1 között.
 */
float setupFraction(float a) {
    static const float logFull = log(1.0f + SETUP_FULL);
    return constrain(log(1.0f + a) / logFull, 0.0f, 1.0f);
}

/**
 * @brief A DD-beállító képernyőt és az előjeles áthallásmérőt rajzolja ki.
 *
 * @param diff Az S1 és S2 minták előjeles különbsége.
 * @param best Az eddig mért legkisebb abszolút áthallás.
 * @param s1 Az S1-minta átlagos nyers ADC-értéke.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha a bemenet nyugalmi DC-szintje hibás.
 * @param vbat Az akkumulátor feszültsége voltban.
 *
 */
void drawDDSetup(float diff, float best, float s1, bool overload, bool dcFault, float vbat) {
    char num[10], bst[10], s1s[8], line[24] = "min ";
    const float v = overload ? s1 - 512.0f : diff;
    dtostrf(v, 1, 1, num);
    if (best < 1e8f) {
        dtostrf(best, 1, 1, bst);
    } else {
        strcpy(bst, "--");
    }
    dtostrf(s1, 1, 0, s1s);
    strcat(line, bst);
    strcat(line, "  S1 ");
    strcat(line, s1s);
    const float a = fabsf(overload ? 512.0f : diff);
    const float frac = setupFraction(a);
    const int half = (int)(frac * 61) * (v < 0 ? -1 : 1);
    const bool ok = !overload && a < DETECT_TH;

    oled.firstPage();

    do {
        drawHeader(dcFault ? "DC HIBA!" : (overload ? "TULVEZERLES" : "DD beallitas"), vbat);

        oled.setFont(u8g2_font_helvB12_tn);
        oled.drawStr(0, 30, num);
        oled.setFont(u8g2_font_6x10_tr);

        if (ok) {
            oled.drawStr(128 - oled.getStrWidth("NULL OK"), 28, "NULL OK");
        }
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

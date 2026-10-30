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
    Wire.setWireTimeout(3000, true);
    oled.begin();
    oled.setBusClock(400000);
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
            } else if (!calIsSet()) {
                oled.drawStr(70, 34, "nincs kal.");
            }
            oled.drawFrame(0, 48, 128, 14);
            oled.drawBox(2, 50, w, 10);
        }
    } while (oled.nextPage());
}

void drawTeach(const char *msg, float mag, float threshold, float vbat, bool overload) {
    char num[10], dep[14] = "";
    dtostrf(mag, 1, 1, num);
    if (calIsSet() && mag >= threshold) {
        dtostrf(calDepthCm(mag), 1, 1, dep);
        strcat(dep, " cm");
    }

    const int w = (int)(constrain(mag / calFullScale(), 0.0f, 1.0f) * 124);

    oled.firstPage();

    do {
        drawHeader("TANITAS 5cm", vbat);
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
        oled.drawFrame(0, 50, 128, 12);
        oled.drawBox(2, 52, w, 8);
    } while (oled.nextPage());
}

void drawCenter(const char *msg, float sig, float threshold, float peak, float returnPeak, float vbat, bool overload, bool dcFault, bool historyMode, CenterStage stage, const int8_t *history, uint8_t historyCount,
                uint8_t historyNext) {
    char num[10], peakText[8], returnText[8];
    dtostrf(fabsf(sig), 1, 1, num);
    dtostrf(peak, 1, 1, peakText);
    dtostrf(returnPeak, 1, 1, returnText);

    const int half = (int)(constrain(sig / (threshold * 4.0f), -1.0f, 1.0f) * 58.0f);

    oled.firstPage();

    do {
        drawHeader(historyMode ? "HISTORIKUS" : "CENTER SCAN", vbat);
        oled.setFont(u8g2_font_6x10_tr);
        const char *status = dcFault ? "DC HIBA!" : (overload ? "TULVEZERLES" : (msg ? msg : centerStageLabel(stage)));
        if (!historyMode || dcFault || overload) {
            oled.drawStr(historyMode ? 128 - oled.getStrWidth(status) : 0, 20, status);
        }

        int graphTop, graphBottom;
        if (historyMode) {
            oled.setFont(u8g2_font_helvB12_tr);
            dtostrf(sig, 1, 0, num);
            oled.drawStr(0, 22, num);
            graphTop = 24;    // A grafikon teteje a sáv felett
            graphBottom = 63; // A grafikon alja a sáv felett
        } else {
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
            graphTop = 53;    // A grafikon teteje a sáv alatt
            graphBottom = 63; // A grafikon alja a sáv alatt
        }

        oled.drawHLine(0, graphBottom, 128);
        const int graphHeight = graphBottom - graphTop;
        const int thresholdY = graphBottom - graphHeight / 4;
        for (int x = 0; x < 128; x += 3) {
            oled.drawPixel(x, thresholdY);
        }
        for (uint8_t i = 0; i < historyCount; ++i) {
            const uint8_t index = historyCount < GRAPH_HISTORY_SIZE ? i : (historyNext + i) % GRAPH_HISTORY_SIZE;
            const int x = 128 - historyCount * 4 + i * 4;
            const int y = graphBottom - (int)history[index] * graphHeight / 32;
            oled.drawPixel(x, y);
            if (i > 0) {
                const uint8_t previous = historyCount < GRAPH_HISTORY_SIZE ? i - 1 : (historyNext + i - 1) % GRAPH_HISTORY_SIZE;
                const int previousX = x - 4;
                const int previousY = graphBottom - (int)history[previous] * graphHeight / 32;
                oled.drawLine(previousX, previousY, x, y);
            }
        }
    } while (oled.nextPage());
}

/**
 * @brief Kirajzolja a DD-beállító képernyőt és az előjeles áthallásmérőt.
 *
 * @param diff Az S1 és S2 minták előjeles különbsége.
 * @param best Az eddig mért legkisebb abszolút áthallás.
 * @param s1 Az S1-minta átlagos nyers ADC-értéke.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha a bemenet nyugalmi DC-szintje hibás.
 * @param vbat Az akkumulátor feszültsége voltban.
 *
 */
void drawSetup(float diff, float best, float s1, bool overload, bool dcFault, float vbat) {
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
    const float frac = constrain(log(1.0f + a) / log(1.0f + SETUP_FULL), 0.0f, 1.0f);
    const int half = (int)(frac * 61) * (v < 0 ? -1 : 1);
    const bool ok = !overload && a < DETECT_TH;

    oled.firstPage();

    do {
        drawHeader(dcFault ? "DC HIBA!" : (overload ? "TULVEZERLES" : "DD beallitas"), vbat);

        oled.setFont(u8g2_font_10x20_tr);
        oled.drawStr(0, 31, num);
        oled.setFont(u8g2_font_6x10_tr);

        if (ok) {
            oled.drawStr(90, 28, "NULL OK");
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

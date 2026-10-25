#include "display.h"

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

#include "calibration.h"
#include "config.h"

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0);

/**
 * @brief Elindítja az I²C-buszt, és inicializálja az OLED-kijelzőt.
 * @return Nincs visszatérési érték.
 */
void displayInit() {
  Wire.begin();
  Wire.setWireTimeout(3000, true);
  oled.begin();
  oled.setBusClock(400000);
}

/**
 * @brief Kirajzolja a fejléc címét és az akkumulátor feszültségét.
 * @param title A fejlécben megjelenő cím.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @return Nincs visszatérési érték.
 */
static void drawHeader(const char* title, float vbat) {
  char b[8];
  dtostrf(vbat, 3, 1, b);
  strcat(b, "V");
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(0, 9, title);
  oled.drawStr(98, 9, b);
}

/**
 * @brief Kirajzolja a normál keresési képernyőt.
 * @param msg Opcionális állapotüzenet; null értéknél a mérési adat jelenik meg.
 * @param mag A jel abszolút értéke.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param det Igaz, ha a jel találatot jelez.
 * @return Nincs visszatérési érték.
 */
void drawNormal(const char* msg, float mag, float vbat, bool overload, bool det) {
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

/**
 * @brief Kirajzolja a DD-beállító képernyőt és az előjeles áthallásmérőt.
 * @param diff Az S1 és S2 minták előjeles különbsége.
 * @param best Az eddig mért legkisebb abszolút áthallás.
 * @param s1 Az S1-minta átlagos nyers ADC-értéke.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha a bemenet nyugalmi DC-szintje hibás.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @return Nincs visszatérési érték.
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

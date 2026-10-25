#include "calibration.h"

#include <EEPROM.h>
#include <math.h>

#include "config.h"

static float g_calK = 0;

static const float CAL_DEPTH_CUBED = CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM;

void calLoad() {
  EEPROM.get(0, g_calK);
  if (!(g_calK > 0 && g_calK < 1e9f)) g_calK = 0;
}

bool calIsSet() { return g_calK > 0; }

void calSet(float k) {
  g_calK = k;
  EEPROM.put(0, g_calK);
}

float calDepthCm(float mag) { return cbrt(g_calK / mag); }

float calFullScale() { return g_calK > 0 ? g_calK / CAL_DEPTH_CUBED * 1.5f : FULL_SCALE; }

float calKFromSignal(float mag) { return mag * CAL_DEPTH_CUBED; }

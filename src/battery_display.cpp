// battery_display.cpp
#include "battery_display.h"
#include "shared_config.h"
#include <Arduino.h>
#include "menu.h"
#include "DEV_Config.h"
// #include "clock.h"

#define NUM_ADC_SAMPLE       20         // Samples per read
#define BATTERY_SAMPLE_PERIOD_MS 500   // Sample battery every 1s
#define BATTERY_FULL_VOL     1345
#define BATTERY_DEFICIT_VOL  1185
#define BATTERY_BUFFER_SIZE  10         // Rolling average buffer size

static int32_t batteryVoltBuffer[BATTERY_BUFFER_SIZE] = {0};
static uint8_t bufferIndex = 0;
static int32_t averagedMillivolts = 0;
static bool isCharging = false;
int cycles = 0;
const int initialStart = 8;
const int initialEnd = 352;


typedef struct {
  int millivolts;
  int percent;
} BatteryPoint;

const BatteryPoint batteryCurve[] = {
  {1358, 100},
  {1327, 90},
  {1306, 80},
  {1285, 70},
  {1272, 60},
  {1258, 50},
  {1241, 40},
  {1231, 30},
  {1219, 20},
  {1195, 0}   // cutoff point
};

// bool isCharging = false;
int batteryX = CANVAS_CENTER_X;
int batteryY = CANVAS_CENTER_Y + 100;
int32_t batteryPercent = 0;

int32_t battery_level_percent(void) {
  int mv = averagedMillivolts;
  if (mv >= batteryCurve[0].millivolts) return 100;
  if (mv <= batteryCurve[sizeof(batteryCurve)/sizeof(batteryCurve[0]) - 1].millivolts) return 0;

  for (int i = 0; i < sizeof(batteryCurve)/sizeof(batteryCurve[0]) - 1; ++i) {
    int v1 = batteryCurve[i].millivolts;
    int v2 = batteryCurve[i+1].millivolts;
    int p1 = batteryCurve[i].percent;
    int p2 = batteryCurve[i+1].percent;

    if (mv <= v1 && mv >= v2) {
      float ratio = (float)(mv - v2) / (float)(v1 - v2);
      return (int32_t)(p2 + ratio * (p1 - p2));
    }
  }

  return 0;
}

void batteryMonitorTask(void *pvParameters) {
  int32_t previousAverage = 0;

  while (true) {
    // Read current voltage (averaged over NUM_ADC_SAMPLE)
    int32_t mvolts = 0;
    for (int i = 0; i < NUM_ADC_SAMPLE; i++) {
      // mvolts += analogReadMilliVolts(D0);
      mvolts += DEC_ADC_Read();
    }
    mvolts /= NUM_ADC_SAMPLE;

    // Serial.println(mvolts);

    // Store in circular buffer
    batteryVoltBuffer[bufferIndex] = mvolts;
    bufferIndex = (bufferIndex + 1) % BATTERY_BUFFER_SIZE;

    // Compute average
    int32_t sum = 0;
    for (int i = 0; i < BATTERY_BUFFER_SIZE; i++) {
      sum += batteryVoltBuffer[i];
    }
    averagedMillivolts = sum / BATTERY_BUFFER_SIZE;

    // Detect charging by voltage trend
    isCharging = (averagedMillivolts > previousAverage + 2) || averagedMillivolts > BATTERY_FULL_VOL + 15;  // Allow small tolerance
    previousAverage = averagedMillivolts;

    vTaskDelay(pdMS_TO_TICKS(BATTERY_SAMPLE_PERIOD_MS));
  }
}

void getBatteryArcAngles(int batteryPercent, int& outStart, int& outEnd) {
  const int centerAngle = 180;
  const int minSweep = 2;  // Minimum degrees between start and end at 0%

  // Linear interpolation factor (0.0 to 1.0)
  float fraction = batteryPercent / 100.0f;

  // Move both angles toward center, but limit with min sweep
  int startTarget = centerAngle - (minSweep / 2);
  int endTarget   = centerAngle + (minSweep / 2);

  outStart = static_cast<int>(initialStart + (startTarget - initialStart) * (1.0f - fraction));
  outEnd   = static_cast<int>(initialEnd + (endTarget - initialEnd) * (1.0f - fraction));

  outStart = (outStart + 360) % 360;
  outEnd   = (outEnd + 360) % 360;
}

void drawBatteryBar() {
  uint16_t arcColor = (batteryPercent > 49) ? 0x07FF : (batteryPercent > 19) ? 0xF81F : 0xFD20;
  int startAngle, endAngle;
  getBatteryArcAngles(batteryPercent, startAngle, endAngle);

  menuSprite.drawSmoothArc(CANVAS_CENTER_X, CANVAS_CENTER_Y, CENTER_X - 4, CENTER_X - 8,
                              initialStart, initialEnd,
                              TFT_DARKGREY, TFT_BLACK, true);
  menuSprite.drawSmoothArc(CANVAS_CENTER_X, CANVAS_CENTER_Y, CENTER_X - 4, CENTER_X - 8,
                              startAngle, endAngle,
                              arcColor, TFT_BLACK, true);

  // bool usbConnected = Serial;
  if (isCharging) { // show charging symbol
    menuSprite.fillTriangle(batteryX + 1, batteryY + 8, batteryX - 3, batteryY + 14, batteryX + 1, batteryY + 14, TFT_WHITE);
    menuSprite.fillTriangle(batteryX, batteryY + 13, batteryX + 4, batteryY + 13, batteryX, batteryY + 19, TFT_WHITE);
  }
}

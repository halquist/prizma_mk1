// battery_display.h
#pragma once

#include <cstdint>  // for int32_t

void batteryMonitorTask(void *pvParameters);
int32_t battery_level_percent(void);
void drawBatteryBar();
// int getBatteryEndAngle(int batteryPercent);
// void renderMenuButton(void);

extern int batteryArcStartAngle;

#include "fps_monitor.h"
#include <Arduino.h>

#ifdef FPS_MONITOR

static uint32_t frameCount = 0;
static uint32_t windowStartMs = 0;

static const char* modeLabel(RenderMode mode) {
  switch (mode) {
    case MODE_MENU:       return "menu";
    case MODE_FRACTAL:    return "fractal";
    case MODE_VISUALIZER: return "visualizer";
    case MODE_OUTRUN:     return "outrun";
    case MODE_SHADER:     return "shader";
    case MODE_DEEP_END:   return "deep_end";
    case MODE_FLASHLIGHT: return "flashlight";
    default:              return "unknown";
  }
}

void fpsMonitorReset(void) {
  frameCount = 0;
  windowStartMs = millis();
}

void fpsMonitorTick(RenderMode mode) {
  if (windowStartMs == 0) {
    fpsMonitorReset();
  }

  frameCount++;
  uint32_t now = millis();
  uint32_t elapsed = now - windowStartMs;
  if (elapsed < 1000) {
    return;
  }

  float fps = frameCount * 1000.0f / elapsed;
  Serial.printf("FPS [%s]: %.1f\n", modeLabel(mode), fps);
  frameCount = 0;
  windowStartMs = now;
}

#else

void fpsMonitorReset(void) {}
void fpsMonitorTick(RenderMode) {}

#endif

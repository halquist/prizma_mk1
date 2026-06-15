// fractal_renderer.cpp
#include "fractal_renderer.h"
#include "shared_config.h"
#include "palette_manager.h"
#include "battery_display.h"
#include "fractal_region_detection.h"
#include <Arduino.h>
#include <math.h>
#include "menu.h"

float zoomExponent = 0.0f;
float centerX = -1.471f;
float centerY = 0.00001f;
float targetCenterX = -1.471;
float targetCenterY = 0.00001;
bool zoomingIn = true;
float MAX_ZOOM = 11.4f;
float MIN_ZOOM = 1.0f;
float rotationAngle = 0.0f;  // In radians
const float panSpeed = 0.04f;
// Continuous-rate constant equivalent to panSpeed per frame at 60 fps.
// Using this with deltaSec gives frame-rate-independent pan convergence.
const float panK = -logf(1.0f - panSpeed) * 60.0f;
bool zoomingEnabled = false;
const float ZOOM_EXP_SPEED_PER_SEC = 0.25f;
uint32_t lastZoomUpdateTime = 0;
bool rotationCenterIsMiddle = (random(0,2) == 2);  // true = middle, false = bottom-right
float rotationMin = 0.95f;
float rotationMax = 1.95f;
float rotationSpeed = rotationCenterIsMiddle ? rotationMin : rotationMax;

bool fractalWasPaused = false;

int mandelbrotPoint(float real, float imag, int iterCap) {
  // Fast rejection: skip iteration for points inside the main cardioid or period-2 bulb.
  float q = (real - 0.25f) * (real - 0.25f) + imag * imag;
  if (q * (q + (real - 0.25f)) < 0.25f * imag * imag) return iterCap;
  if ((real + 1.0f) * (real + 1.0f) + imag * imag < 0.0625f) return iterCap;

  float zReal = 0.0f, zImag = 0.0f;
  float zReal2 = 0.0f, zImag2 = 0.0f;
  int iter = 0;

  while ((zReal2 + zImag2 <= 4.0f) && iter < iterCap) {
    zImag = 2.0f * zReal * zImag + imag;
    zReal = zReal2 - zImag2 + real;
    zReal2 = zReal * zReal;
    zImag2 = zImag * zImag;
    iter++;
  }
  return iter;
}

void mandelbrotLine(uint8_t* line, int y, float minX, float maxX, float minY, float maxY, int iterCap, int radiusLimit, float cosA, float sinA) {
  const int halfW = SCREEN_WIDTH / 2;
  const int halfH = SCREEN_HEIGHT / 2;
  const float zoomFactor = (maxX - minX) / 2.0f;
  const float cx = centerX;
  const float cy = centerY;

  const float invHalfW = 1.0f / (halfW - 1);
  const float invHalfH = 1.0f / (halfH - 1);

  float anchorX = rotationCenterIsMiddle ? (halfW / 2.0f) : halfW;
  float anchorY = rotationCenterIsMiddle ? (halfH / 2.0f) : halfH;

  for (int px = 0; px < halfW; px++) {
    int dx = px - anchorX;
    int dy = y - anchorY;

    if (dx * dx + dy * dy > radiusLimit * radiusLimit) {
      line[px] = 0;
      continue;
    }

    float nx = dx * invHalfW;
    float ny = dy * invHalfH;
    float rx = nx * cosA - ny * sinA;
    float ry = nx * sinA + ny * cosA;

    float real = cx + rx * zoomFactor;
    float imag = cy + ry * zoomFactor;

    int iter = mandelbrotPoint(real, imag, iterCap);
    line[px] = (iter == iterCap) ? 0 : iter % PALETTE_SIZE;
  }
}


void updateZoomAndCenter() {
  uint32_t now = millis();
  float deltaSec = (now - lastZoomUpdateTime) / 1000.0f;
  lastZoomUpdateTime = now;

  float dx = targetCenterX - centerX;
  float dy = targetCenterY - centerY;
  const float PAN_EPSILON = 0.0000001f;
  const float ZOOM_PAN_THRESHOLD = 0.05f;

  if (zoomingIn && (fabs(dx) > PAN_EPSILON || fabs(dy) > PAN_EPSILON)) {
    // Time-based exponential lerp — converges at the same real-world speed
    // regardless of frame rate.
    float alpha = 1.0f - expf(-panK * deltaSec);
    centerX += dx * alpha;
    centerY += dy * alpha;
  }

  rotationAngle = rotationSpeed * sinf(zoomExponent * 0.8f);  // radiansrotationAngle = 0.4f * zoomExponent;  // Adjust multiplier for rotation speed

  if (fabs(dx) <= ZOOM_PAN_THRESHOLD && fabs(dy) <= ZOOM_PAN_THRESHOLD) {
    zoomingEnabled = true;
  }

  if (zoomingIn && zoomingEnabled) {
    zoomExponent += deltaSec * ZOOM_EXP_SPEED_PER_SEC;
    if (zoomExponent >= MAX_ZOOM) {
      zoomExponent = MAX_ZOOM;
      zoomingIn = false;
    }
  } else if (!zoomingIn) {
    zoomExponent -= deltaSec * ZOOM_EXP_SPEED_PER_SEC;
    if (zoomExponent <= MIN_ZOOM) {
      zoomExponent = MIN_ZOOM;
      zoomingIn = true;
      zoomingEnabled = false;
      regionSearchPending = true;  // kick off background search, non-blocking

      // Randomly pick rotation center
      rotationCenterIsMiddle = (random(0, 2) == 1);
      rotationSpeed = rotationCenterIsMiddle ? rotationMin : rotationMax;
    }
  }
}

// uint16_t invert565(uint16_t color) {
//     uint8_t r = 31 - ((color >> 11) & 0x1F);
//     uint8_t g = 63 - ((color >> 5) & 0x3F);
//     uint8_t b = 31 - (color & 0x1F);
//     return (r << 11) | (g << 5) | b;
// }

void mandelbrotGenerator() {
  if (fractalWasPaused) {
    // reset zoom update time so no accumulated delta
    lastZoomUpdateTime = millis();
    fractalWasPaused = false;
  }

  float zoomFactor = 1.0f / powf(2.9f, zoomExponent);
  float minX = centerX - zoomFactor;
  float maxX = centerX + zoomFactor;
  float minY = centerY - zoomFactor;
  float maxY = centerY + zoomFactor;

  float zoomNorm = (zoomExponent - MIN_ZOOM) / (MAX_ZOOM - MIN_ZOOM);
  zoomNorm = constrain(zoomNorm, 0.0f, 1.0f);
  int iterCap = MIN_ITER + (int)(zoomNorm * (MAX_ITER - MIN_ITER));

  int radiusLimit = (SCREEN_WIDTH / 2) + 2;

  const float cosA = cosf(rotationAngle);
  const float sinA = sinf(rotationAngle);

  for (int y = 0; y < SCREEN_HEIGHT / 2; y++) {
    uint8_t* line = renderFB + y * (SCREEN_WIDTH / 2);
    mandelbrotLine(line, y, minX, maxX, minY, maxY, iterCap, radiusLimit, cosA, sinA);
    if ((y & 15) == 0) vTaskDelay(1);
  }
  updateZoomAndCenter();
  updatePaletteShift(zoomExponent);
}

void fractalRender() {
  const int radiusLimit = 121;
  const int radiusSq = radiusLimit * radiusLimit;

  for (int y = 0; y < SCREEN_HEIGHT / 2; y++) {
    uint8_t* row = displayFB + y * (SCREEN_WIDTH / 2);
    int ym = SCREEN_HEIGHT - 1 - y;

    for (int x = 0; x < SCREEN_WIDTH / 2; x++) {
      int xm = SCREEN_WIDTH - 1 - x;
      int dx = x - CENTER_X;
      int dy = y - CENTER_Y;

      uint8_t paletteIndex = row[x];
      uint16_t color = (dx*dx + dy*dy <= radiusSq)
                       ? ((paletteIndex == 0) ? 0 : palette[paletteIndex])
                       : 0;

      fractalFramebuffer[y  * SCREEN_WIDTH + x]  = color;
      fractalFramebuffer[y  * SCREEN_WIDTH + xm] = color;
      fractalFramebuffer[ym * SCREEN_WIDTH + x]  = color;
      fractalFramebuffer[ym * SCREEN_WIDTH + xm] = color;
    }
    if ((y & 31) == 0) vTaskDelay(1);
  }

  if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, fractalFramebuffer);
    xSemaphoreGive(displayMutex);
  }
}

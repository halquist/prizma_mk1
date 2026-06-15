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
bool zoomingEnabled = false;
const float ZOOM_EXP_SPEED_PER_SEC = 0.25f;
const float PAN_DURATION_SEC = 5.0f;  // total eased pan time after picking a new point
uint32_t lastZoomUpdateTime = 0;
bool rotationCenterIsMiddle = (random(0, 2) == 1);
float rotationMin = 0.95f;
float rotationMax = 1.95f;
float rotationSpeed = rotationCenterIsMiddle ? rotationMin : rotationMax;

bool fractalWasPaused = false;

static bool panActive = false;
static float panStartX = 0.0f;
static float panStartY = 0.0f;
static float panElapsed = 0.0f;
// 0 = rotation anchor at quadrant center, 1 = anchor at quadrant corner (mirrored to screen center).
static float anchorBlend = 0.0f;
static float anchorBlendStart = 0.0f;
static float anchorBlendTarget = 0.0f;
static float rotationSpeedStart = 0.0f;
static float rotationSpeedTarget = 0.0f;

static float easeInOut(float t) {
  t = constrain(t, 0.0f, 1.0f);
  // Smoothstep: zero velocity at start and end, peak in the middle.
  return t * t * (3.0f - 2.0f * t);
}

void beginPanToTarget(bool transitionRotationAnchor) {
  panStartX = centerX;
  panStartY = centerY;
  panElapsed = 0.0f;
  panActive = true;

  anchorBlendStart = anchorBlend;
  rotationSpeedStart = rotationSpeed;

  if (!transitionRotationAnchor) {
    anchorBlend = rotationCenterIsMiddle ? 0.0f : 1.0f;
    anchorBlendStart = anchorBlend;
    rotationSpeedStart = rotationSpeed;
  }

  if (transitionRotationAnchor) {
    bool newMiddle = (random(0, 2) == 1);
    anchorBlendTarget = newMiddle ? 0.0f : 1.0f;
    rotationSpeedTarget = newMiddle ? rotationMin : rotationMax;
    rotationCenterIsMiddle = newMiddle;
  } else {
    anchorBlendTarget = anchorBlendStart;
    rotationSpeedTarget = rotationSpeedStart;
  }
}

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

  const float anchorMiddleX = halfW * 0.5f;
  const float anchorMiddleY = halfH * 0.5f;
  const float anchorCornerX = (float)halfW;
  const float anchorCornerY = (float)halfH;
  const float anchorX = anchorMiddleX + (anchorCornerX - anchorMiddleX) * anchorBlend;
  const float anchorY = anchorMiddleY + (anchorCornerY - anchorMiddleY) * anchorBlend;

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

  // Eased pan at full zoom-out: accelerate from rest, decelerate into the target.
  if (zoomingIn && !zoomingEnabled && panActive) {
    panElapsed += deltaSec;
    float t = panElapsed / PAN_DURATION_SEC;
    float eased = easeInOut(t);
    centerX = panStartX + (targetCenterX - panStartX) * eased;
    centerY = panStartY + (targetCenterY - panStartY) * eased;
    anchorBlend = anchorBlendStart + (anchorBlendTarget - anchorBlendStart) * eased;
    rotationSpeed = rotationSpeedStart + (rotationSpeedTarget - rotationSpeedStart) * eased;

    if (t >= 1.0f) {
      centerX = targetCenterX;
      centerY = targetCenterY;
      anchorBlend = anchorBlendTarget;
      rotationSpeed = rotationSpeedTarget;
      panActive = false;
      zoomingEnabled = true;
    }
  }

  dx = targetCenterX - centerX;
  dy = targetCenterY - centerY;

  rotationAngle = rotationSpeed * sinf(zoomExponent * 0.8f);  // radiansrotationAngle = 0.4f * zoomExponent;  // Adjust multiplier for rotation speed

  // If already at the target (e.g. first boot with no travel), allow zoom immediately.
  if (!panActive && zoomingIn && !zoomingEnabled) {
    const float ZOOM_PAN_THRESHOLD = 0.05f;
    if (fabs(dx) <= ZOOM_PAN_THRESHOLD && fabs(dy) <= ZOOM_PAN_THRESHOLD) {
      zoomingEnabled = true;
    }
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
      findFractalEdgePoint(0);
      beginPanToTarget(true);
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

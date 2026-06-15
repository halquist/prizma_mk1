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

// mandelbrotLine — incremental (DDA) coordinate stepping.
// dreal/dimag are the per-pixel steps in complex-plane coords (constant for a given frame).
// Precomputing them in mandelbrotGenerator avoids 6 muls per pixel here.
void mandelbrotLine(uint8_t* line, int y, float cx, float cy,
                    int iterCap, int radiusLimit,
                    float cosA, float sinA,
                    float dreal, float dimag,
                    float anchorX, float anchorY,
                    float invHalfW, float invHalfH, float zoomFactor) {
  const int halfW = SCREEN_WIDTH / 2;

  // --- Precompute valid pixel range for this row (one sqrtf replaces 120 dx²+dy² checks) ---
  const int dyInt = y - (int)anchorY;
  const int dySq  = dyInt * dyInt;
  const int radSq = radiusLimit * radiusLimit;
  const int maxDxSq = radSq - dySq;

  if (maxDxSq < 0) {
    // Entire row is outside the circle — blank it and return.
    memset(line, 0, halfW);
    return;
  }

  const int xRange = (int)sqrtf((float)maxDxSq);
  const int xStart = max(0, (int)anchorX - xRange);
  const int xEnd   = min(halfW - 1, (int)anchorX + xRange);

  // Zero the out-of-circle edge pixels.
  if (xStart > 0)            memset(line, 0, xStart);
  if (xEnd < halfW - 1)      memset(line + xEnd + 1, 0, halfW - 1 - xEnd);

  // --- Compute starting complex coords at px = xStart ---
  // real(px) = cx + [ (px - anchorX)*invHalfW * cosA - (y - anchorY)*invHalfH * sinA ] * zoomFactor
  const float ny_row   = (float)dyInt * invHalfH;
  const float rowConstR = (ny_row * (-sinA)) * zoomFactor;  // -sinA*ny term
  const float rowConstI = (ny_row *   cosA)  * zoomFactor;  //  cosA*ny term
  const float nx_start  = (float)(xStart - (int)anchorX) * invHalfW;
  float real = cx + nx_start * cosA * zoomFactor + rowConstR;
  float imag = cy + nx_start * sinA * zoomFactor + rowConstI;

  // --- Inner loop: only active pixels, no per-pixel circle check, no muls ---
  for (int px = xStart; px <= xEnd; px++, real += dreal, imag += dimag) {
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

// Worker task: renders the top half of the quarter buffer on Core 0 while the
// producer renders the bottom half on Core 1.
void fractalWorkerTask(void* pvParameters) {
  for (;;) {
    xSemaphoreTake(workerStartSem, portMAX_DELAY);
    const FractalWorkerParams& p = workerParams;
    const int halfW = SCREEN_WIDTH / 2;
    for (int y = p.yStart; y < p.yEnd; y++) {
      uint8_t* line = renderFB + y * halfW;
      mandelbrotLine(line, y, p.cx, p.cy,
                     p.iterCap, p.radiusLimit,
                     p.cosA, p.sinA,
                     p.dreal, p.dimag,
                     p.anchorX, p.anchorY,
                     p.invHalfW, p.invHalfH, p.zoomFactor);
      if ((y & 15) == 0) vTaskDelay(1);
    }
    xSemaphoreGive(workerDoneSem);
  }
}

void mandelbrotGenerator() {
  if (fractalWasPaused) {
    lastZoomUpdateTime = millis();
    fractalWasPaused = false;
  }

  const int halfW = SCREEN_WIDTH / 2;
  const int halfH = SCREEN_HEIGHT / 2;
  const int halfHalf = halfH / 2;  // line split point between cores

  float zoomFactor = 1.0f / powf(2.9f, zoomExponent);

  float zoomNorm = (zoomExponent - MIN_ZOOM) / (MAX_ZOOM - MIN_ZOOM);
  zoomNorm = constrain(zoomNorm, 0.0f, 1.0f);
  int iterCap = MIN_ITER + (int)(zoomNorm * (MAX_ITER - MIN_ITER));

  int radiusLimit = (SCREEN_WIDTH / 2) + 2;

  const float cosA = cosf(rotationAngle);
  const float sinA = sinf(rotationAngle);

  const float anchorX = halfW * 0.5f + (halfW - halfW * 0.5f) * anchorBlend;
  const float anchorY = halfH * 0.5f + (halfH - halfH * 0.5f) * anchorBlend;

  const float invHalfW = 1.0f / (halfW - 1);
  const float invHalfH = 1.0f / (halfH - 1);

  // Per-pixel complex-plane step (constant for the entire frame).
  const float dreal = invHalfW * cosA * zoomFactor;
  const float dimag = invHalfW * sinA * zoomFactor;

  // Hand the top half of lines (0..halfHalf-1) to the worker on Core 0.
  workerParams = { centerX, centerY,
                   cosA, sinA,
                   dreal, dimag,
                   anchorX, anchorY,
                   invHalfW, invHalfH, zoomFactor,
                   iterCap, radiusLimit,
                   0, halfHalf };
  xSemaphoreGive(workerStartSem);  // start Core 0 worker

  // Render bottom half of lines (halfHalf..halfH-1) on Core 1 (this task).
  for (int y = halfHalf; y < halfH; y++) {
    uint8_t* line = renderFB + y * halfW;
    mandelbrotLine(line, y, centerX, centerY,
                   iterCap, radiusLimit,
                   cosA, sinA,
                   dreal, dimag,
                   anchorX, anchorY,
                   invHalfW, invHalfH, zoomFactor);
    if ((y & 15) == 0) vTaskDelay(1);
  }

  // Wait for Core 0 to finish before swapping buffers.
  xSemaphoreTake(workerDoneSem, portMAX_DELAY);

  updateZoomAndCenter();
  updatePaletteShift(zoomExponent);
}

void fractalRender() {
  // Build each full 240-pixel row into a stack buffer (fast internal SRAM), then
  // memcpy it to PSRAM for both the top and mirrored bottom rows. This gives the
  // PSRAM controller sequential burst writes instead of 4 scattered stores per pixel.
  uint16_t rowBuf[SCREEN_WIDTH];

  const int halfW  = SCREEN_WIDTH / 2;
  const int halfH  = SCREEN_HEIGHT / 2;
  const int radSq  = 121 * 121;

  for (int y = 0; y < halfH; y++) {
    const uint8_t* row = displayFB + y * halfW;
    const int ym  = SCREEN_HEIGHT - 1 - y;
    const int dy  = y - CENTER_Y;
    const int dySq = dy * dy;

    if (dySq > radSq) {
      // Entire row is outside the circle — blank both rows.
      memset(rowBuf, 0, sizeof(rowBuf));
    } else {
      const int maxDxSq = radSq - dySq;
      for (int x = 0; x < halfW; x++) {
        const int dx = x - CENTER_X;
        uint8_t idx = row[x];
        uint16_t color = (dx*dx <= maxDxSq)
                         ? ((idx == 0) ? 0 : palette[idx])
                         : 0;
        rowBuf[x]               = color;   // left half
        rowBuf[SCREEN_WIDTH-1-x] = color;  // right half (mirror)
      }
    }

    memcpy(&fractalFramebuffer[y  * SCREEN_WIDTH], rowBuf, SCREEN_WIDTH * sizeof(uint16_t));
    memcpy(&fractalFramebuffer[ym * SCREEN_WIDTH], rowBuf, SCREEN_WIDTH * sizeof(uint16_t));

    if ((y & 31) == 0) vTaskDelay(1);
  }

  if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, fractalFramebuffer);
    xSemaphoreGive(displayMutex);
  }
}

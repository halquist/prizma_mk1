#include "shared_config.h"
#include "fractal_renderer.h"
#include "fractal_region_detection.h"
#include <Arduino.h>

#define VERIFY_PREVIEW_SIZE 32
uint8_t verifyPreview[VERIFY_PREVIEW_SIZE * VERIFY_PREVIEW_SIZE];

float MIN_DETAIL_SCORE = 0.5f;

float randomFloat(float minVal, float maxVal) {
  return minVal + ((float)random() / (float)RAND_MAX) * (maxVal - minVal);
}

float evaluateDetailScore(float x, float y, int iterCap) {
  float step = 0.002f;
  float centerVal = mandelbrotPoint(x, y, iterCap);
  float dx = mandelbrotPoint(x + step, y, iterCap) - mandelbrotPoint(x - step, y, iterCap);
  float dy = mandelbrotPoint(x, y + step, iterCap) - mandelbrotPoint(x, y - step, iterCap);
  float grad = sqrtf(dx * dx + dy * dy);
  float score = grad * logf(2.0f + centerVal);
  if (centerVal > iterCap * 0.9f && centerVal < iterCap) {
    score *= 1.5f;  // Boost score if close to escaping
  }
  return score;
}

float zoomAwareDetailScore(float cx, float cy, float zoomFactor, int iterCap) {
  const int gridSize = 8;
  float minX = cx - zoomFactor;
  float maxX = cx + zoomFactor;
  float minY = cy - zoomFactor;
  float maxY = cy + zoomFactor;

  int unique = 0, total = 0;
  int histogram[256] = {0};

  for (int y = 0; y < gridSize; y++) {
    float imag = minY + (maxY - minY) * y / gridSize;
    for (int x = 0; x < gridSize; x++) {
      float real = minX + (maxX - minX) * x / gridSize;
      int iter = mandelbrotPoint(real, imag, iterCap);
      uint8_t val = (iter == iterCap) ? 255 : iter;
      histogram[val]++;
      total++;
    }
  }

  for (int i = 0; i < 256; i++) if (histogram[i]) unique++;

  return (float)unique / total;  // higher is better
}

bool verifyZoomTarget(float cx, float cy, float zoomFactor) {
  const float zoom = MAX_ZOOM - 0.5f;
  const int iterCap = MAX_ITER * 1.5f;
  const int W = VERIFY_PREVIEW_SIZE;
  const int H = VERIFY_PREVIEW_SIZE;
  float minX = cx - zoomFactor;
  float maxX = cx + zoomFactor;
  float minY = cy - zoomFactor;
  float maxY = cy + zoomFactor;

  int total = 0, nonZero = 0, unique = 0, edgePixels = 0;
  int histogram[256] = {0};

  // Pass 1: generate preview and histogram
  for (int y = 0; y < H; y++) {
    float imag = minY + y * (maxY - minY) / H;
    for (int x = 0; x < W; x++) {
      float real = minX + x * (maxX - minX) / W;
      int iter = mandelbrotPoint(real, imag, iterCap);
      uint8_t val = (iter == iterCap) ? 255 : iter;
      verifyPreview[y * W + x] = val;
      histogram[val]++;
      if (val != 255 && val != 0) nonZero++;
      total++;
    }
  }

  // Histogram uniqueness
  for (int i = 0; i < 256; ++i)
    if (histogram[i] > 0) unique++;

  // Edge pixel estimation via gradient magnitude
  for (int y = 1; y < H - 1; y++) {
    for (int x = 1; x < W - 1; x++) {
      uint8_t c = verifyPreview[y * W + x];
      uint8_t l = verifyPreview[y * W + (x - 1)];
      uint8_t r = verifyPreview[y * W + (x + 1)];
      uint8_t t = verifyPreview[(y - 1) * W + x];
      uint8_t b = verifyPreview[(y + 1) * W + x];

      int dx = abs(l - r);
      int dy = abs(t - b);
      if (dx + dy > 10) edgePixels++;  // Threshold can be tuned
    }
  }

  float fillRatio = (float)nonZero / total;
  float edgeDensity = (float)edgePixels / total;

  // Debug log
  // Serial.print("VERIFY @ (");
  // Serial.print(cx, 6); Serial.print(", ");
  // Serial.print(cy, 6); Serial.print("): fill=");
  // Serial.print(fillRatio, 2); Serial.print(" unique=");
  // Serial.print(unique); Serial.print(" edge=");
  // Serial.println(edgeDensity, 2);

  if (fillRatio < 0.20f) {
    // Serial.println("❌ Rejected: low fill");
    return false;
  }
  if (unique < 5) {
    // Serial.println("❌ Rejected: low uniqueness");
    return false;
  }
  if (edgeDensity < 0.05f) {
    // Serial.println("❌ Rejected: low edge density");
    return false;
  }

  // Serial.println("✅ Accepted zoom target");
  return true;
}

void findFractalEdgePoint(int retryDepth) {
  const int maxRetryDepth = 6;
  const int coarseSamples = 1000;
  const int fineSamples = 1000;
  const float fineRadius = 0.02f;
  const int coarseIterCap = MIN_ITER;
  const int fineIterCap = MAX_ITER;
  float zoomFactor = 1.0f / powf(2.9f, MAX_ZOOM);

  bool useAltLocalDetail = (random(0, 2) == 1);

  // Keep the top 5 coarse candidates instead of just the single best.
  // Randomly selecting among them prevents the same dominant boundary
  // region from winning every cycle.
  const int TOP_N = 5;
  float topX[TOP_N] = {}, topY[TOP_N] = {};
  float topScore[TOP_N] = { -1, -1, -1, -1, -1 };
  int topCount = 0;

  // Coarse scan
  for (int i = 0; i < coarseSamples; ++i) {
    float x = randomFloat(-2.0f, 1.0f);
    float y = randomFloat(-1.5f, 1.5f);
    float score = evaluateDetailScore(x, y, coarseIterCap);
    if (score <= MIN_DETAIL_SCORE) continue;

    // Insert into top-N (insertion sort on the small fixed array)
    if (topCount < TOP_N || score > topScore[TOP_N - 1]) {
      int pos = topCount < TOP_N ? topCount : TOP_N - 1;
      // Find insertion position
      while (pos > 0 && score > topScore[pos - 1]) pos--;
      // Shift down
      int last = (topCount < TOP_N) ? topCount : TOP_N - 1;
      for (int j = last; j > pos; j--) {
        topX[j] = topX[j - 1];
        topY[j] = topY[j - 1];
        topScore[j] = topScore[j - 1];
      }
      topX[pos] = x;
      topY[pos] = y;
      topScore[pos] = score;
      if (topCount < TOP_N) topCount++;
    }
  }

  // Randomly pick one of the top candidates (uniform — all are already
  // above MIN_DETAIL_SCORE and represent genuinely interesting regions).
  int pick = (topCount > 0) ? random(0, topCount) : 0;
  float bestCoarseX = (topCount > 0) ? topX[pick] : 0.0f;
  float bestCoarseY = (topCount > 0) ? topY[pick] : 0.0f;

  float bestFineX = bestCoarseX;
  float bestFineY = bestCoarseY;
  float bestFineScore = -1.0f;

  // Fine scan — uniform disk around the coarse point (full 360°).
  // Previously only searched upper-left (−dx, −dy), missing 3/4 of the
  // refinement neighborhood.
  for (int i = 0; i < fineSamples; ++i) {
    float angle = randomFloat(0.0f, 6.28318f);
    float radius = randomFloat(0.0f, fineRadius);
    float x = bestCoarseX + cosf(angle) * radius;
    float y = bestCoarseY + sinf(angle) * radius;

    float score = useAltLocalDetail ? zoomAwareDetailScore(x, y, zoomFactor, fineIterCap) : evaluateDetailScore(x, y, fineIterCap);

    if (score > bestFineScore && score > MIN_DETAIL_SCORE && verifyZoomTarget(x, y, zoomFactor)) {
      bestFineScore = score;
      bestFineX = x;
      bestFineY = y;
    }
  }

  if (bestFineScore > 0.0f) {
    targetCenterX = bestFineX;
    targetCenterY = bestFineY;
  } else if (retryDepth < maxRetryDepth) {
    findFractalEdgePoint(retryDepth + 1);
  } else {
    targetCenterX = bestCoarseX;
    targetCenterY = bestCoarseY;
  }
}


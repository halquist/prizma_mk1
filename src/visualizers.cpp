#include "shared_config.h"
#include "palette_manager.h"
#include "visualizers.h"
#include "outrun.h"

TFT_eSprite visSprite = TFT_eSprite(&tft);

enum ShapeType { SHAPE_FILL_CIRCLE, SHAPE_DRAW_CIRCLE  };
// enum ShapeType { SHAPE_FILL_CIRCLE, SHAPE_DRAW_CIRCLE, SHAPE_TRIANGLE, SHAPE_SQUARE, SHAPE_ROUNDED_SQUARE };
ShapeType shapeType1 = SHAPE_FILL_CIRCLE;
ShapeType shapeType2 = SHAPE_FILL_CIRCLE;

int symmetryCount = 6;  // start at 6-way symmetry

const int minRadius = 3;
const int maxRadius = 30;
const float minSpeed = 0.5f;  // minimum allowed speed in any direction
const float maxSpeed = 6.5f;  // minimum allowed speed in any direction
float orbitSpeedBase = 0.05f;
float orbitSpeedAmplitude = 0.02f;


// Global state for first set
const float radiusSpeed = 0.01f;  // speed of pulsation
float baseX = SCREEN_WIDTH / random(1, 5);
float baseY = SCREEN_HEIGHT / random(1, 5);
float velocityX = 2.3f + random(-10, 10) * 0.01f;
float velocityY = 2.5f + random(-10, 10) * 0.01f;
float orbitPhase = 0.0f;
float orbitPhaseModulator = 0.0f;
float orbitRadiusBase = 10.0f;
float orbitRadiusAmplitude = 5.0f;
float radiusPhase = 0.0f;
bool doubleIt = true;
bool psychIt = false;

// Global state for second set
const float radiusSpeed2 = 0.02f;  // speed of pulsation
float baseX2 = SCREEN_WIDTH / random(1, 5);
float baseY2 = SCREEN_HEIGHT / random(1, 5);
float velocityX2 = -2.1f - random(-10, 10) * 0.01f;
float velocityY2 = 2.4f + random(-10, 10) * 0.01f;
float orbitPhase2 = 0.0f;
float orbitPhaseModulator2 = 0.0f;
float orbitRadiusBase2 = 15.0f;
float orbitRadiusAmplitude2 = 4.0f;
float radiusPhase2 = 0.0f;
bool doubleIt2 = true;

// variables for hexTernity
float hexPulsePhase = 0.0f;
float hexRotationPhase = 0.0f;
float hexPulsePhase2 = 0.0f;
float hexRotationPhase2 = 0.0f;

enum VisualizerMode {
  VISUALIZER_LASER_LOTUS,
  VISUALIZER_DUAL_RAYS,
  VISUALIZER_HEXWAVE,
  // VISUALIZER_SERPINSKI,
  NUM_VISUALIZERS
};

VisualizerMode currentVisualizer = VISUALIZER_LASER_LOTUS;

void cycleVisualizer(bool reverse) {
  int progress = reverse ? -1 : 1;
  currentVisualizer = (VisualizerMode)((currentVisualizer + NUM_VISUALIZERS + progress) % NUM_VISUALIZERS);
}

void randomizeVisualizer() {
  VisualizerMode previous = currentVisualizer;
  do {
    currentVisualizer = (VisualizerMode)(random(NUM_VISUALIZERS));
  } while (currentVisualizer == previous);  // avoid repeating
}

void renderVisualizerFrame() {
  switch (currentVisualizer) {
    case VISUALIZER_LASER_LOTUS:
      laserLotus();
      break;
    case VISUALIZER_DUAL_RAYS:
      hexDualDynamicRayLinesWithLocalSpin();
      break;
    case VISUALIZER_HEXWAVE:
      hexwave();
      break;
    // case VISUALIZER_SERPINSKI:
    //   serpinski();
    //   break;
  }
}

void drawRotatedSierpinskiTriangle(int cx, int cy, int x, int y, int size, int depth, int baseColor, float angle) {
  int safeColor = baseColor % PALETTE_SIZE;

  // Apply rotation around center point (cx, cy)
  auto rotate = [&](int px, int py) -> std::pair<int, int> {
    float s = sin(angle);
    float c = cos(angle);

    float dx = px - cx;
    float dy = py - cy;

    int rx = cx + dx * c - dy * s;
    int ry = cy + dx * s + dy * c;
    return {rx, ry};
  };

  if (depth == 0) {
    int x0 = x;
    int y0 = y + size;
    int x1 = x + size / 2;
    int y1 = y;
    int x2 = x - size / 2;
    int y2 = y;

    auto [rx0, ry0] = rotate(x0, y0);
    auto [rx1, ry1] = rotate(x1, y1);
    auto [rx2, ry2] = rotate(x2, y2);

    visSprite.fillTriangle(rx0, ry0, rx1, ry1, rx2, ry2, palette[safeColor]);
    return;
  }

  int half = size / 2;

  drawRotatedSierpinskiTriangle(cx, cy, x, y, half, depth - 1, baseColor, angle);
  drawRotatedSierpinskiTriangle(cx, cy, x - half / 2, y + half, half, depth - 1, baseColor + 40, angle);
  drawRotatedSierpinskiTriangle(cx, cy, x + half / 2, y + half, half, depth - 1, baseColor + 80, angle);
}

void serpinski() {
  float t = millis() * 0.001f;

  // Base angular velocities modulated
  float dynamicSpeed1 = 0.3f + 0.2f * sinf(t * 0.4f);
  float dynamicSpeed2 = 0.25f + 0.15f * cosf(t * 0.3f);

  float angle1 = t * dynamicSpeed1;
  float angle2 = -t * dynamicSpeed2;

  // Add jitter to angles
  angle1 += 0.05f * sinf(t * 1.3f + 5.0f);
  angle2 += 0.04f * cosf(t * 1.7f + 7.0f);

  int fullSize = SCREEN_WIDTH;
  float h = 0.866f * fullSize;

  // Spiral radius with jitter
  float spiralRadius = 10 + 10 * sinf(t * 0.2f);
  spiralRadius += 2.0f * sinf(t * 0.9f); // jitter

  int cx1 = CENTER_X + cosf(t * 0.3f) * spiralRadius + 3.0f * sinf(t * 1.1f);
  int cy1 = CENTER_Y + sinf(t * 0.4f) * spiralRadius + 3.0f * cosf(t * 0.8f);
  int topX1 = cx1;
  int topY1 = cy1 - (int)(h / 2.0f);

  float spiralRadius2 = 10 + 10 * sinf(t * 0.3f);
  spiralRadius2 += 2.0f * cosf(t * 0.7f); // jitter

  int cx2 = CENTER_X - cosf(t * 0.25f) * spiralRadius2 + 2.5f * cosf(t * 1.2f);
  int cy2 = CENTER_Y - sinf(t * 0.35f) * spiralRadius2 + 2.5f * sinf(t * 0.6f);
  int topX2 = cx2;
  int topY2 = cy2 - (int)(h / 2.0f);

  float scale = 0.9f + 0.1f * sinf(t * 0.6f);
  int scaledSize = fullSize * scale;
  float scale2 = 0.9f + 0.1f * sinf(t * 0.5f);
  int scaledSize2 = fullSize * scale2;

  int baseColor = 1 + (int)(20 * sinf(t * 0.3f));

  // Main triangles
  drawRotatedSierpinskiTriangle(cx1, cy1, topX1, topY1, scaledSize, 4, 1 + baseColor, angle1);
  // drawRotatedSierpinskiTriangle(cx2, cy2, topX2, topY2, scaledSize2, 4, 100 + baseColor, angle2);

  // X-axis mirrored
  int m_cx1 = 2 * CENTER_X - cx1;
  int m_topX1 = 2 * CENTER_X - topX1;
  // drawRotatedSierpinskiTriangle(m_cx1, cy1, m_topX1, topY1, scaledSize, 4, 1 + baseColor, -angle1);

  int m_cx2 = 2 * CENTER_X - cx2;
  int m_topX2 = 2 * CENTER_X - topX2;
  // drawRotatedSierpinskiTriangle(m_cx2, cy2, m_topX2, topY2, scaledSize2, 4, 100 + baseColor, -angle2);

  float paletteShiftFactor = 0.1f + pow(random(0, 1001) / 1000.0f, 2) * 7.9f;
  updatePaletteShift(paletteShiftFactor);
}





int fadeFrameCounter = 0;
const int fadeInterval = 2;  // fade every N frames
float fadePhase = 0.0f;
const float fadeSpeed = 0.01f;
const uint8_t minAlpha = 10;
const uint8_t maxAlpha = 36;
// uint16_t fadeColor = palette[0];  // or any color


#define MAX_ACTIVE_CIRCLES 60  // increased capacity for more layers

struct MovingCircle {
  float angle;
  float distance;
  float speed;
  float radius;
  float growth;
  uint16_t colorOffset;
  bool active;
};

MovingCircle circles[MAX_ACTIVE_CIRCLES];

void spawnSymmetricCircles() {
  int slots[4] = {-1, -1, -1, -1};
  int found = 0;

  for (int i = 0; i < MAX_ACTIVE_CIRCLES && found < 4; i++) {
    if (!circles[i].active) {
      slots[found++] = i;
    }
  }

  if (found < 4) return;

  float baseAngle = random(0, 1000) / 1000.0f * TWO_PI;
  float baseSpeed = 2.0f + random(0, 1000) / 1000.0f * 3.0f;
  float baseGrowth = 0.2f + random(0, 1000) / 1000.0f * 0.5f;

  float angles[4] = {
    baseAngle,
    PI - baseAngle,
    -baseAngle,
    PI + baseAngle
  };

  int randomColorOffset = random(0, 256);

  for (int j = 0; j < 4; j++) {
    int idx = slots[j];
    circles[idx].angle = angles[j];
    circles[idx].distance = 0;
    circles[idx].speed = baseSpeed * (0.9f + 0.1f * j);
    circles[idx].radius = 2.0f;
    circles[idx].growth = baseGrowth * (0.9f + 0.1f * j);
    circles[idx].colorOffset = randomColorOffset;
    circles[idx].active = true;
  }
}

void updateAndDrawCircles() {
  for (int i = 0; i < MAX_ACTIVE_CIRCLES; i++) {
    if (circles[i].active) {
      circles[i].distance += circles[i].speed;
      circles[i].radius += circles[i].growth;

      int x = CENTER_X + cos(circles[i].angle) * circles[i].distance;
      int y = CENTER_Y + sin(circles[i].angle) * circles[i].distance;

      visSprite.fillSmoothCircle(x, y, (int)circles[i].radius, palette[circles[i].colorOffset],  palette[circles[i].colorOffset]);

      if (circles[i].distance > CENTER_X + 80) {
        circles[i].active = false;
      }
    }
  }
}

void hexwave() {
  if (random(0, 3) == 0) {
    spawnSymmetricCircles();
  }

  updateAndDrawCircles();

  float paletteShiftFactor = 0.1f + pow(random(0, 1001) / 1000.0f, 2) * 7.9f;
  updatePaletteShift(paletteShiftFactor);
}

// void hexwave() {
//   hexPulsePhase += 0.3f;
//   hexRotationPhase += 0.03f;

//   int baseRadius = 15;
//   int layers = 7;
//   int outerRadius = 0;

//   for (int layer = 1; layer <= layers; layer++) {
//     int paletteIndex = 1;
//     float scale = 1.0f + 0.2f * sin(hexPulsePhase + layer);
//     int radius = baseRadius * layer * scale;

//     if (layer == layers) {
//       outerRadius = radius;
//     }

//     for (int i = 0; i < 6; i++) {
//       float angle1 = TWO_PI * i / 6 - PI / 2 + hexRotationPhase;
//       float angle2 = TWO_PI * (i + 1) / 6 - PI / 2 + hexRotationPhase;

//       int x1 = CENTER_X + cos(angle1) * radius;
//       int y1 = CENTER_Y + sin(angle1) * radius;
//       int x2 = CENTER_X + cos(angle2) * radius;
//       int y2 = CENTER_Y + sin(angle2) * radius;

//       visSprite.drawWedgeLine(x1, y1, x2, y2, 3, 3, palette[paletteIndex]);
//     }
//   }

//   // Randomly trigger new circle (low probability per frame)
//   if (random(0, 25) < 5) {  // ~5% chance per frame
//     spawnCircle();
//   }

//   // Update and draw all active circles
//   updateAndDrawCircles(outerRadius);

//   updatePaletteShift(1.0);
// }


void drawShape(TFT_eSprite& sprite, ShapeType type, int x, int y, int size, uint16_t color) {
  switch (type) {
    case SHAPE_DRAW_CIRCLE:
      sprite.drawSmoothCircle(x, y, size, color, color);
      break;
    case SHAPE_FILL_CIRCLE:
      sprite.fillSmoothCircle(x, y, size, color, color);
      break;
    // case SHAPE_FILL_ELLIPSE:
    //   sprite.fillEllipse(x, y, size, size * 2, color);
    //   break;
    // case SHAPE_TRIANGLE: {
    //   int half = size;
    //   sprite.fillTriangle(x, y - half, x - half, y + half, x + half, y + half, color);
    //   break;
    // }
    // case SHAPE_SQUARE:
    //   sprite.fillRect(x - size, y - size, size * 2, size * 2, color);
    //   break;
    // case SHAPE_ROUNDED_SQUARE:
    //   sprite.fillSmoothRoundRect(x - size, y - size, size * 2, size * 2, size / 2, color, color);
    //   break;
  }
}

void laserLotus() {
  if (random(250) == 0) {
    doubleIt = (random(10) > 3);
  }
  if (random(250) == 0) {
    doubleIt2 = (random(10) > 3);
  }
  if (random(400) == 0) {
    symmetryCount = 5 + random(0, 5);  // random symmetry 4..9
  }

  orbitPhaseModulator += 0.01f;
  if (orbitPhaseModulator >= 2 * PI) orbitPhaseModulator -= 2 * PI;

  float dynamicOrbitRadius = orbitRadiusBase + orbitRadiusAmplitude * sinf(orbitPhaseModulator);
  float dynamicOrbitSpeed = orbitSpeedBase + orbitSpeedAmplitude * sinf(orbitPhaseModulator * 0.5f);

  orbitPhaseModulator2 += 0.01f;
  if (orbitPhaseModulator2 >= 2 * PI) orbitPhaseModulator2 -= 2 * PI;

  float dynamicOrbitRadius2 = orbitRadiusBase2 + orbitRadiusAmplitude2 * sinf(orbitPhaseModulator2);
  float dynamicOrbitSpeed2 = orbitSpeedBase + orbitSpeedAmplitude * sinf(orbitPhaseModulator2 * 0.5f);

  // --- FIRST SET ---
  baseX += velocityX;
  baseY += velocityY;
  if (baseX - maxRadius <= 0 || baseX + maxRadius >= SCREEN_WIDTH) {
    velocityX = -velocityX + random(-10, 10) * 0.03f;
    if (abs(velocityX) < minSpeed) velocityX = (velocityX < 0) ? -minSpeed : minSpeed;
    if (abs(velocityX) > maxSpeed) velocityX = (velocityX < 0) ? -maxSpeed : maxSpeed;
  }
  if (baseY - maxRadius <= 0 || baseY + maxRadius >= SCREEN_WIDTH) {
    velocityY = -velocityY + random(-10, 10) * 0.03f;
    if (abs(velocityY) < minSpeed) velocityY = (velocityY < 0) ? -minSpeed : minSpeed;
    if (abs(velocityY) > maxSpeed) velocityY = (velocityY < 0) ? -maxSpeed : maxSpeed;
  }

  if (baseX - maxRadius <= 0) {
    baseX = maxRadius;
    velocityX = abs(velocityX);  // ensure bouncing right
  }
  else if (baseX + maxRadius >= SCREEN_WIDTH) {
    baseX = SCREEN_WIDTH - maxRadius;
    velocityX = -abs(velocityX);  // ensure bouncing left
  }
  if (baseY - maxRadius <= 0) {
    baseY = maxRadius;
    velocityY = abs(velocityY);  // ensure bouncing right
  }
  else if (baseY + maxRadius >= SCREEN_WIDTH) {
    baseY = SCREEN_WIDTH - maxRadius;
    velocityY = -abs(velocityY);  // ensure bouncing left
  }

  orbitPhase += dynamicOrbitSpeed;
  if (orbitPhase >= 2 * PI) orbitPhase -= 2 * PI;

  float offsetX = cosf(orbitPhase) * dynamicOrbitRadius;
  float offsetY = sinf(orbitPhase) * dynamicOrbitRadius;
  float circleX = baseX + offsetX;
  float circleY = baseY + offsetY;

  float radius = minRadius + (maxRadius - minRadius) * (0.5f + 0.5f * sinf(radiusPhase));
  radiusPhase += radiusSpeed;
  if (radiusPhase >= 2 * PI) radiusPhase -= 2 * PI;

  float dx = circleX - CENTER_X;
  float dy = circleY - CENTER_Y;
  for (int i = 0; i < symmetryCount; i++) {
    float angle = i * (2 * PI / symmetryCount);
    int xr = (int)(cos(angle) * dx - sin(angle) * dy + CENTER_X);
    int yr = (int)(sin(angle) * dx + cos(angle) * dy + CENTER_Y);
    drawShape(visSprite, shapeType1, xr, yr, (int)radius, palette[1]);
    if (doubleIt) {
      drawShape(visSprite, shapeType1, SCREEN_WIDTH - xr, yr, (int)radius, palette[1]);
    }
  }

  // --- SECOND SET ---
  baseX2 += velocityX2;
  baseY2 += velocityY2;
  if (baseX2 - maxRadius <= 0 || baseX2 + maxRadius >= SCREEN_WIDTH) {
    velocityX2 = -velocityX2 + random(-10, 10) * 0.03f;
    if (abs(velocityX2) < minSpeed) velocityX2 = (velocityX2 < 0) ? -minSpeed : minSpeed;
    if (abs(velocityX2) > maxSpeed) velocityX2 = (velocityX2 < 0) ? -maxSpeed : maxSpeed;
  }
  if (baseY2 - maxRadius <= 0 || baseY2 + maxRadius >= SCREEN_WIDTH) {
    velocityY2 = -velocityY2 + random(-10, 10) * 0.03f;
    if (abs(velocityY2) < minSpeed) velocityY2 = (velocityY2 < 0) ? -minSpeed : minSpeed;
    if (abs(velocityY2) > maxSpeed) velocityY2 = (velocityY2 < 0) ? -maxSpeed : maxSpeed;
  }

  if (baseX2 - maxRadius <= 0) {
    baseX2 = maxRadius;
    velocityX2 = abs(velocityX2);  // ensure bouncing right
  } else if (baseX2 + maxRadius >= SCREEN_WIDTH) {
    baseX2 = SCREEN_WIDTH - maxRadius;
    velocityX2 = -abs(velocityX2);  // ensure bouncing left
  }
  if (baseY2 - maxRadius <= 0) {
    baseY2 = maxRadius;
    velocityY2 = abs(velocityY2);  // ensure bouncing right
  } else if (baseY2 + maxRadius >= SCREEN_WIDTH) {
    baseY2 = SCREEN_WIDTH - maxRadius;
    velocityY2 = -abs(velocityY2);  // ensure bouncing left
  }

  orbitPhase2 += dynamicOrbitSpeed2;
  if (orbitPhase2 >= 2 * PI) orbitPhase2 -= 2 * PI;

  float offsetX2 = cosf(orbitPhase2) * dynamicOrbitRadius2;
  float offsetY2 = sinf(orbitPhase2) * dynamicOrbitRadius2;
  float circleX2 = baseX2 + offsetX2;
  float circleY2 = baseY2 + offsetY2;

  float radius2 = minRadius + (maxRadius - minRadius) * (0.5f + 0.5f * sinf(radiusPhase2));
  radiusPhase2 += radiusSpeed2;
  if (radiusPhase2 >= 2 * PI) radiusPhase2 -= 2 * PI;

  float dx2 = circleX2 - CENTER_X;
  float dy2 = circleY2 - CENTER_Y;
  for (int i = 0; i < symmetryCount; i++) {
    float angle = i * (2 * PI / symmetryCount);
    int xr = (int)(cos(angle) * dx2 - sin(angle) * dy2 + CENTER_X);
    int yr = (int)(sin(angle) * dx2 + cos(angle) * dy2 + CENTER_Y);
    drawShape(visSprite, shapeType2, xr, yr, (int)radius2, palette[126]);
    if (doubleIt2) {
      drawShape(visSprite, shapeType2, SCREEN_WIDTH - xr, yr, (int)radius2, palette[126]);
    }
  }

  updatePaletteShift(1.0);
}

void hexDualDynamicRayLinesWithLocalSpin() {
  static int currentNumWedges = random(3, 10);  // start value
  static int currentNumWedges2 = random(3, 10);  // start value
  const float minLength = 10.0f;
  const float maxRadius = CENTER_X;  // ~120 px on 240×240
  const float baseWidth = 2.0f;
  static int numRenders = random(1,3);

  // Check if it's time to change wedge count 
  if (random(1000) == 0) {
    currentNumWedges = random(3, 10);  // random between 3 and 9
  }
  if (random(1000) == 0) {
    currentNumWedges2 = random(3, 10);  // random between 3 and 9
  }

  // Dynamic speeds
  float dynamicPulseSpeed = 0.02f + 0.06f * (0.5f + 0.5f * sin(hexPulsePhase * 0.1f));
  float dynamicRotationSpeed = 0.01f + 0.02f * (0.5f + 0.5f * cos(hexPulsePhase * 0.07f));
  float dynamicRotationSpeed2 = 0.01f + 0.02f * (0.5f + 0.5f * cos(hexPulsePhase * 0.08f));

  hexPulsePhase += dynamicPulseSpeed;
  hexRotationPhase += dynamicRotationSpeed;
  hexRotationPhase2 += dynamicRotationSpeed2;

  // --- Random toggle for local spin (every ~5 seconds)
  static bool enableLocalSpin = false;
  if (random(1000) == 0) {
    enableLocalSpin = random(0, 2) == 1;  // randomly true or false
  }

  // --- Random toggle for double render
  if (random(1000) == 0) {
    numRenders = random(1, 3);  // randomly 1 or 2
  }

  for (int set = 0; set < numRenders; set++) {
    bool reverseRotation = (set == 1);
    int activeNumWedges = set == 1 ? currentNumWedges2 : currentNumWedges;
    float setRotationPhase = reverseRotation ? -hexRotationPhase2 : hexRotationPhase;
    float phaseOffset = (set == 1) ? (TWO_PI / (2 * activeNumWedges)) : 0;
    int paletteShift = (set == 1) ? 128 : 0;

    for (int i = 0; i < activeNumWedges; i++) {
      float angle = TWO_PI * i / activeNumWedges + setRotationPhase + phaseOffset;

      // Pulsing radii
      float mod1 = 0.5f + 0.5f * sin(hexPulsePhase + i + (set == 1 ? PI : 0));
      float mod2 = 0.5f + 0.5f * cos(hexPulsePhase + i + 1.0f + (set == 1 ? PI : 0));

      float r1 = mod1 * maxRadius;
      float r2 = mod2 * maxRadius;

      if (fabs(r2 - r1) < minLength) {
        if (r1 < r2) r2 = r1 + minLength;
        else r1 = r2 + minLength;
        r1 = fmin(r1, maxRadius);
        r2 = fmin(r2, maxRadius);
      }

      // Global positions
      int x1 = CENTER_X + cos(angle) * r1;
      int y1 = CENTER_Y + sin(angle) * r1;
      int x2 = CENTER_X + cos(angle) * r2;
      int y2 = CENTER_Y + sin(angle) * r2;

      // Apply local spin if active
      int finalX1 = x1, finalY1 = y1, finalX2 = x2, finalY2 = y2;
      if (enableLocalSpin) {
        float midX = (x1 + x2) / 2.0f;
        float midY = (y1 + y2) / 2.0f;

        float dx1 = x1 - midX;
        float dy1 = y1 - midY;
        float dx2 = x2 - midX;
        float dy2 = y2 - midY;

        float localSpinAngle = sin(hexPulsePhase * 0.5f + i + set * PI) * 0.5f;  // small swing ±0.5 rad
        float cosA = cos(localSpinAngle);
        float sinA = sin(localSpinAngle);

        float rotatedDx1 = dx1 * cosA - dy1 * sinA;
        float rotatedDy1 = dx1 * sinA + dy1 * cosA;
        float rotatedDx2 = dx2 * cosA - dy2 * sinA;
        float rotatedDy2 = dx2 * sinA + dy2 * cosA;

        finalX1 = midX + rotatedDx1;
        finalY1 = midY + rotatedDy1;
        finalX2 = midX + rotatedDx2;
        finalY2 = midY + rotatedDy2;
      }

      int paletteIndex = (i * (256 / activeNumWedges) + paletteShift) % 256;

      visSprite.drawWedgeLine(finalX1, finalY1, finalX2, finalY2, baseWidth, baseWidth, palette[paletteIndex], palette[paletteIndex]);
      visSprite.drawWedgeLine(SCREEN_WIDTH - finalX1, finalY1, SCREEN_WIDTH - finalX2, finalY2, baseWidth, baseWidth, palette[paletteIndex], palette[paletteIndex]);
      visSprite.drawWedgeLine(finalX1, SCREEN_HEIGHT - finalY1, finalX2, SCREEN_HEIGHT - finalY2, baseWidth, baseWidth, palette[paletteIndex], palette[paletteIndex]);
      visSprite.drawWedgeLine(SCREEN_WIDTH - finalX1, SCREEN_HEIGHT - finalY1, SCREEN_WIDTH - finalX2, SCREEN_HEIGHT - finalY2, baseWidth, baseWidth, palette[paletteIndex], palette[paletteIndex]);
    }
  }

  // Weighted random palette shift factor (0.1–8.0, skewed low)
  float paletteShiftFactor = 0.1f + pow(random(0, 1001) / 1000.0f, 2) * 7.9f;
  updatePaletteShift(paletteShiftFactor);
}

#include <Arduino.h>
#include "menu.h"
#include "shared_config.h"
#include <esp_sleep.h>
#include "CST816S.h"
#include "DEV_Config.h"
#include "battery_display.h"
#include "visualizers.h"
#include "pixelFont24.h"
#include "pixelFont48.h"
#include "palette_manager.h"

SemaphoreHandle_t menuDrawSemaphore;

TFT_eSprite menuSprite = TFT_eSprite(&tft);

gpio_num_t touchInterruptPin = GPIO_NUM_5;

// enum ViewLocation { TL_VIEW, TC_VIEW, TR_VIEW, CL_VIEW, CC_VIEW, CR_VIEW, BL_VIEW, BC_VIEW, BR_VIEW  };
// ViewLocation currentView = CC_VIEW;

int viewX = SCREEN_WIDTH;  // current viewport X offset
int viewY = SCREEN_HEIGHT;  // current viewport Y offset

int currentRow = 1; // center row (0 = top, 1 = middle, 2 = bottom)
int currentCol = 1; // center col (0 = left, 1 = center, 2 = right)
int targetCol = currentCol;
int targetRow = currentRow;

int powerButtonX = CANVAS_CENTER_X + SCREEN_WIDTH;
int powerButtonY = CANVAS_CENTER_Y;
int fractalMenuButtonX = CANVAS_CENTER_X - SCREEN_WIDTH;
int fractalMenuButtonY = CANVAS_CENTER_Y;
int shaderMenuButtonX = CANVAS_CENTER_X - SCREEN_WIDTH;
int shaderMenuButtonY = CANVAS_CENTER_Y - SCREEN_HEIGHT;
int deepEndMenuButtonX = CANVAS_CENTER_X - SCREEN_WIDTH;
int deepEndMenuButtonY = CANVAS_CENTER_Y + SCREEN_HEIGHT;
int visMenuButtonX = CANVAS_CENTER_X;
int visMenuButtonY = CANVAS_CENTER_Y - SCREEN_HEIGHT;
int outrunMenuButtonX = CANVAS_CENTER_X;
int outrunMenuButtonY = CANVAS_CENTER_Y + SCREEN_HEIGHT;
int flashlightMenuButtonX = CANVAS_CENTER_X + SCREEN_WIDTH;
int flashlightMenuButtonY = CANVAS_CENTER_Y - SCREEN_HEIGHT;
int shuffleMenuButtonX = CANVAS_CENTER_X + SCREEN_WIDTH;
int shuffleMenuButtonY = CANVAS_CENTER_Y + SCREEN_HEIGHT;
int basePowerButtonRadius = 30;
const int arrowTouchRadius = 30;
bool screenTouched = false;
unsigned long lastSwipeTime = 0;  // time when last swipe was processed
const unsigned long swipeCooldown = 600;  // cooldown in milliseconds (e.g., 500 ms)
bool powerButtonPressed = false;
// bool menuSpriteReady = false;
// bool showMenu = false;
// int showMenuDelay = 0;
// int showMenuDelayLength = 160;
int bigTextYOffset = -24;
int smallTextYOffset = -12;

bool showLogoText = true;
int logoTextTimeout = 60;
int logoTextTimeoutCounter = 0;

const int arrowSize = 8;  // size of arrowhead

const int arrowLeftX = 16;
const int arrowLeftY = CENTER_Y;

const int arrowRightX = SCREEN_WIDTH - 16;
const int arrowRightY = CENTER_Y;

const int arrowUpX = CENTER_X;
const int arrowUpY = 16;

const int arrowDownX = CENTER_X;
const int arrowDownY = SCREEN_HEIGHT - 16;


// for logo
const int numPetals = 6;
const int baseRadius = 34;  // distance from center to petal
const int basePetalSize = 16;

float pulsePhase = 0.0f;
float rotationPhase = 0.0f;

void renderMenuFrame(uint32_t now) {
  if (flashlightActive) {
    menuSprite.fillSprite(TFT_WHITE);
  } else {
    menuSprite.fillSprite(TFT_BLACK);
    drawMenu(now);     
  }
}

void drawMenu(uint32_t now) {
  drawNavigationArrows();
  if ((currentCol == 1 && currentRow == 1) || (targetCol == 1 && targetRow == 1)) {
    // if (showLogoText && logoTextTimeoutCounter < logoTextTimeout) {
    //   logoTextTimeoutCounter++;
      menuSprite.setTextColor(TFT_WHITE);
      menuSprite.setFreeFont(&pixelFont48pt);
      menuSprite.drawString("PRIZMA", CANVAS_CENTER_X, CANVAS_CENTER_Y + bigTextYOffset);
      menuSprite.setFreeFont(&pixelFont24pt);
      menuSprite.drawString("VISION", CANVAS_CENTER_X, CANVAS_CENTER_Y + smallTextYOffset + 24);
    // } else {
    //   showLogoText = false;
    //   // drawHexagonLogo();
    //   drawCoolHexagonButton();
    // }
    drawBatteryBar();
  }
  if ((currentCol == 2 && currentRow == 1) || (targetCol == 2 && targetRow == 1)) {
    drawPowerButton();
  }
  if ((currentCol == 0 && currentRow == 1) || (targetCol == 0 && targetRow == 1)) {
    drawFractalButton();
  }
  if ((currentCol == 1 && currentRow == 0) || (targetCol == 1 && targetRow == 0)) {
    drawVisButton();
  }
  if ((currentCol == 1 && currentRow == 2) || (targetCol == 1 && targetRow == 2)) {
    drawOutrunButton();
  }
  if ((currentCol == 0 && currentRow == 0) || (targetCol == 0 && targetRow == 0)) {
    drawShaderButton(now);
  }
  if ((currentCol == 0 && currentRow == 2) || (targetCol == 0 && targetRow == 2)) {
    drawDeepEndButton(now);
  }
  if ((currentCol == 2 && currentRow == 0) || (targetCol == 2 && targetRow == 0)) {
    drawFlashlightButton();
  }
  if ((currentCol == 2 && currentRow == 2) || (targetCol == 2 && targetRow == 2)) {
    drawShuffleButton();
  }
}

void animateScroll(int startX, int targetX, int startY, int targetY) {
  const int steps = 20; // number of animation frames
  if (steps == 0) return;
  // const int delayMs = 15;

  for (int i = 1; i <= steps; i++) {
    viewX = startX + (targetX - startX) * i / steps;
    viewY = startY + (targetY - startY) * i / steps;
    menuSprite.pushSprite(-viewX, -viewY);
    // delay(delayMs);
  }
}

void handleSwipe(String direction, uint32_t now) {
  targetCol = currentCol;
  targetRow = currentRow;

  if (direction == "SWIPE LEFT" && currentRow < 2) {
    targetRow++;  // move down in grid
  } else if (direction == "SWIPE RIGHT" && currentRow > 0) {
    targetRow--;  // move up in grid
  } else if (direction == "SWIPE UP" && currentCol > 0 ) {
    targetCol--;  // move left in grid
  } else if (direction == "SWIPE DOWN" && currentCol < 2) {
    targetCol++;  // move right in grid
  } else {
    // No movement; exit early
    return;
  }

  int startX = currentCol * SCREEN_WIDTH;
  int startY = currentRow * SCREEN_HEIGHT;
  int targetX = targetCol * SCREEN_WIDTH;
  int targetY = targetRow * SCREEN_HEIGHT;

  drawMenu(now);
  animateScroll(startX, targetX, startY, targetY);

  // Update current positions AFTER animation
  currentCol = targetCol;
  currentRow = targetRow;
}

void enterSleepMode() {
  digitalWrite(LCD_BL_PIN, LOW);
  // tft.fillScreen(TFT_BLACK);
  // vTaskEndScheduler();
  delay(3000);

  // Enable touchpad as wakeup source
  esp_sleep_enable_ext1_wakeup(1ULL << touchInterruptPin, ESP_EXT1_WAKEUP_ALL_LOW);

  esp_deep_sleep_start();
  // prevent further code from running
  while (true) { /* halt */ }
}

// Called after waking from deep sleep
void disableTouchpadWakeup() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
}

void handleMenuTouch(float touchX, float touchY, String gesture, uint32_t now) {
  unsigned long currentTime = millis();
  // String gesture = touch.gesture();
  // Serial.println(gesture);

  if (gesture.startsWith("SWIPE ")) {
    if (currentTime - lastSwipeTime >= swipeCooldown) {
      handleSwipe(gesture, now);
      lastSwipeTime = currentTime;
    } 
  }

  //--- Arrow Navigation Touches ---
  if (gesture == "SINGLE CLICK") {
    // LEFT ARROW
    if (touchX >= arrowLeftX - arrowTouchRadius &&
        touchX <= arrowLeftX + arrowTouchRadius &&
        touchY >= arrowLeftY - arrowTouchRadius &&
        touchY <= arrowLeftY + arrowTouchRadius) {
      handleSwipe("SWIPE RIGHT", now);  // go left
      return;
    }

    // RIGHT ARROW
    if (touchX >= arrowRightX - arrowTouchRadius &&
        touchX <= arrowRightX + arrowTouchRadius &&
        touchY >= arrowRightY - arrowTouchRadius &&
        touchY <= arrowRightY + arrowTouchRadius) {
      handleSwipe("SWIPE LEFT", now);  // go right
      return;
    }

    // UP ARROW
    if (touchX >= arrowUpX - arrowTouchRadius &&
        touchX <= arrowUpX + arrowTouchRadius &&
        touchY >= arrowUpY - arrowTouchRadius &&
        touchY <= arrowUpY + arrowTouchRadius) {
      handleSwipe("SWIPE DOWN", now);  // go up
      return;
    }

    // DOWN ARROW
    if (touchX >= arrowDownX - arrowTouchRadius &&
        touchX <= arrowDownX + arrowTouchRadius &&
        touchY >= arrowDownY - arrowTouchRadius &&
        touchY <= arrowDownY + arrowTouchRadius) {
      handleSwipe("SWIPE UP", now);  // go down
      return;
    }
  }


  if (touchX >= powerButtonX - basePowerButtonRadius - viewX && touchX <= powerButtonX + basePowerButtonRadius - viewX && // handles power button touch
      touchY >= powerButtonY - basePowerButtonRadius - viewY && touchY <= powerButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    powerButtonPressed = true;
    enterSleepMode();
  }

  if (touchX >= fractalMenuButtonX - basePowerButtonRadius - viewX && touchX <= fractalMenuButtonX + basePowerButtonRadius - viewX && // handles fractal button touch
      touchY >= fractalMenuButtonY - basePowerButtonRadius - viewY && touchY <= fractalMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    currentMode = MODE_FRACTAL;
    rendererActive = true;
    touchCooldownActive = true;
  }
  
  if (touchX >= visMenuButtonX - basePowerButtonRadius - viewX && touchX <= visMenuButtonX + basePowerButtonRadius - viewX && // handles visualizer button touch
      touchY >= visMenuButtonY - basePowerButtonRadius - viewY && touchY <= visMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    visSprite.fillScreen(TFT_BLACK);  
    currentMode = MODE_VISUALIZER;
    touchCooldownActive = true;
  }

  if (touchX >= outrunMenuButtonX - basePowerButtonRadius - viewX && touchX <= outrunMenuButtonX + basePowerButtonRadius - viewX && // handles visualizer button touch
      touchY >= outrunMenuButtonY - basePowerButtonRadius - viewY && touchY <= outrunMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    visSprite.fillScreen(TFT_BLACK);  
    currentMode = MODE_OUTRUN;
    touchCooldownActive = true;
  }

  if (touchX >= shaderMenuButtonX - basePowerButtonRadius - viewX && touchX <= shaderMenuButtonX + basePowerButtonRadius - viewX && // handles visualizer button touch
      touchY >= shaderMenuButtonY - basePowerButtonRadius - viewY && touchY <= shaderMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    currentMode = MODE_SHADER;
    rendererActive = true;
    touchCooldownActive = true;
  }

  if (touchX >= deepEndMenuButtonX - basePowerButtonRadius - viewX && touchX <= deepEndMenuButtonX + basePowerButtonRadius - viewX && // handles visualizer button touch
      touchY >= deepEndMenuButtonY - basePowerButtonRadius - viewY && touchY <= deepEndMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    currentMode = MODE_DEEP_END;
    rendererActive = true;
    touchCooldownActive = true;
  }
  
  if (touchX >= flashlightMenuButtonX - basePowerButtonRadius - viewX && touchX <= flashlightMenuButtonX + basePowerButtonRadius - viewX && // handles visualizer button touch
      touchY >= flashlightMenuButtonY - basePowerButtonRadius - viewY && touchY <= flashlightMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    currentMode = MODE_FLASHLIGHT;
    touchCooldownActive = true;
  }

  if (touchX >= shuffleMenuButtonX - basePowerButtonRadius - viewX && touchX <= shuffleMenuButtonX + basePowerButtonRadius - viewX && // handles visualizer button touch
      touchY >= shuffleMenuButtonY - basePowerButtonRadius - viewY && touchY <= shuffleMenuButtonY + basePowerButtonRadius - viewY && gesture == "SINGLE CLICK") {
    shuffleActive = true;
    touchCooldownActive = true;
    shuffle();
  }
}

void drawNavigationArrows() {
  int chevronOffset = 4;
  // LEFT
  if (currentCol > 0) {
    menuSprite.fillTriangle(
      viewX + arrowLeftX, viewY + arrowLeftY,
      viewX + arrowLeftX + arrowSize, viewY + arrowLeftY - arrowSize,
      viewX + arrowLeftX + arrowSize, viewY + arrowLeftY + arrowSize,
      TFT_DARKGREY
    );
    menuSprite.fillTriangle(
      viewX + arrowLeftX + chevronOffset, viewY + arrowLeftY,
      viewX + arrowLeftX + arrowSize, viewY + arrowLeftY - chevronOffset,
      viewX + arrowLeftX + arrowSize, viewY + arrowLeftY + chevronOffset,
      TFT_BLACK
    );
  }

  // RIGHT
  if (currentCol < 2) {
    menuSprite.fillTriangle(
      viewX + arrowRightX, viewY + arrowRightY,
      viewX + arrowRightX - arrowSize, viewY + arrowRightY - arrowSize,
      viewX + arrowRightX - arrowSize, viewY + arrowRightY + arrowSize,
      TFT_DARKGREY
    );
    menuSprite.fillTriangle(
      viewX + arrowRightX - chevronOffset, viewY + arrowRightY,
      viewX + arrowRightX - arrowSize, viewY + arrowRightY - chevronOffset,
      viewX + arrowRightX - arrowSize, viewY + arrowRightY + chevronOffset,
      TFT_BLACK
    );
  }

  // UP
  if (currentRow > 0) {
    menuSprite.fillTriangle(
      viewX + arrowUpX, viewY + arrowUpY,
      viewX + arrowUpX - arrowSize, viewY + arrowUpY + arrowSize,
      viewX + arrowUpX + arrowSize, viewY + arrowUpY + arrowSize,
      TFT_DARKGREY
    );
    menuSprite.fillTriangle(
      viewX + arrowUpX, viewY + arrowUpY + chevronOffset,
      viewX + arrowUpX - chevronOffset, viewY + arrowUpY + arrowSize,
      viewX + arrowUpX + chevronOffset, viewY + arrowUpY + arrowSize,
      TFT_BLACK
    );
  }

  // DOWN
  if (currentRow < 2) {
    menuSprite.fillTriangle(
      viewX + arrowDownX, viewY + arrowDownY,
      viewX + arrowDownX - arrowSize, viewY + arrowDownY - arrowSize,
      viewX + arrowDownX + arrowSize, viewY + arrowDownY - arrowSize,
      TFT_DARKGREY
    );
    menuSprite.fillTriangle(
      viewX + arrowDownX, viewY + arrowDownY - chevronOffset,
      viewX + arrowDownX - chevronOffset, viewY + arrowDownY - arrowSize,
      viewX + arrowDownX + chevronOffset, viewY + arrowDownY - arrowSize,
      TFT_BLACK
    );
  }
}

void drawPowerButton() {
  menuSprite.drawSmoothArc(powerButtonX, powerButtonY, 15, 20,
                                210, 150,
                                TFT_WHITE, TFT_BLACK, true);
  menuSprite.drawWedgeLine(powerButtonX, powerButtonY - 6, powerButtonX, powerButtonY - 18, 3, 3, TFT_WHITE, TFT_BLACK);
}

void drawSierpinskiTriangle(int x, int y, int size, int depth, int baseColor) {
  int safeColor = baseColor % PALETTE_SIZE;
  // Serial.println(safeColor);
  if (depth == 0) {
    // Use depth-based offset from baseColor
    menuSprite.fillTriangle(
      SCREEN_WIDTH - x, SCREEN_HEIGHT * 3 - y + size,                       // top bottom
      SCREEN_WIDTH - x + size / 2, SCREEN_HEIGHT * 3 - y,     // top right
      SCREEN_WIDTH - x - size / 2, SCREEN_HEIGHT * 3 - y,     // top left
      palette[safeColor]
    );
    return;
  }

  int half = size / 2;

  drawSierpinskiTriangle(x, y, half, depth - 1, baseColor);
  drawSierpinskiTriangle(x - half / 2, y + half, half, depth - 1, baseColor + 40);
  drawSierpinskiTriangle(x + half / 2, y + half, half, depth - 1, baseColor + 80);
}

void drawFractalButton() {
  drawSierpinskiTriangle(fractalMenuButtonX, fractalMenuButtonY - 50, 100, 4, 1);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&pixelFont24pt);
  menuSprite.drawString("FRACTALUX", fractalMenuButtonX, fractalMenuButtonY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
  updatePaletteShift(1.0);
}

void drawShuffleButton() {
  menuSprite.fillRectHGradient(shuffleMenuButtonX - 30, shuffleMenuButtonY - 30, 60, 60, palette[1], palette[122]);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&pixelFont24pt);
  menuSprite.drawString("SHUFFLE", shuffleMenuButtonX, shuffleMenuButtonY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
  updatePaletteShift(1.0);
}

void drawShaderButton(uint32_t now) {
  const int centerX = shaderMenuButtonX;
  const int centerY = shaderMenuButtonY;
  const int radiusInner = 20;
  const int radiusOuter = 34;
  const int arms = 12;  // symmetry count
  const float speed = 0.001f;

  float t = now * speed;

  // Optional: clear the region first (small circle)
  menuSprite.fillCircle(centerX, centerY, radiusOuter + 2, TFT_BLACK);

  for (int i = 0; i < arms; i++) {
    float angle = t + (TWO_PI / arms) * i;
    float x0 = centerX + cosf(angle) * radiusInner;
    float y0 = centerY + sinf(angle) * radiusInner;
    float x1 = centerX + cosf(angle) * radiusOuter;
    float y1 = centerY + sinf(angle) * radiusOuter;
    int paletteIndex = (17 * i) & 0xFF;
    menuSprite.drawWedgeLine((int)x0, (int)y0, (int)x1, (int)y1, 2, 2, palette[paletteIndex], TFT_BLACK);
  }

  // glowing core
  // menuSprite.fillSmoothCircle(centerX, centerY, radiusInner - 5, palette[120], TFT_BLACK);

  // Label
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&pixelFont24pt);
  menuSprite.drawString("NEOSHADE", centerX, centerY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
  updatePaletteShift(1.0);
}

// void drawDeepEndButton() {
//   menuSprite.fillRectVGradient(deepEndMenuButtonX - 30, deepEndMenuButtonY - 30, 60, 60, palette[1], palette[122]);
//   menuSprite.setTextColor(TFT_WHITE);
//   menuSprite.setFreeFont(&pixelFont24pt);
//   menuSprite.drawString("DEEP END", deepEndMenuButtonX, deepEndMenuButtonY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
//   updatePaletteShift(1.0);
// }

void drawDeepEndButton(uint32_t now) {
  const int centerX = deepEndMenuButtonX;
  const int centerY = deepEndMenuButtonY;
  const int logoWidth = 60;
  const int logoHeight = 60;
  const int lines = 6;
  const int amplitude = 3;
  const float freq = 0.3f;
  const float speed = 0.005f;

  int x0 = centerX - logoWidth / 2;
  int y0 = centerY - logoHeight / 2;

  for (int i = 0; i < lines; i++) {
    int yBase = y0 + i * (logoHeight / lines);
    for (int x = 0; x < logoWidth - 1; x++) {
      float t = now * speed;
      float phase = i * 0.7f + t;
      int y1 = yBase + (int)(sinf((x + t * 30) * freq + phase) * amplitude);
      int y2 = yBase + (int)(sinf((x + 1 + t * 30) * freq + phase) * amplitude);
      menuSprite.drawLine(x0 + x, y1, x0 + x + 1, y2, TFT_CYAN);
    }
  }

  // Label
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&pixelFont24pt);
  menuSprite.drawString("DEEP END", centerX, centerY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
}

void drawFlashlightButton() {
  const int centerX = flashlightMenuButtonX;
  const int centerY = flashlightMenuButtonY;
  const int radius = 20;             // Circle radius
  const int rayCount = 6;            // Number of rays
  const int rayGap = 8;              // Gap between circle edge and ray start
  const int rayLength = 6;           // Length of each ray

  // Draw center circle
  menuSprite.fillSmoothCircle(centerX, centerY, radius, TFT_WHITE, TFT_BLACK);

  // Draw radial rays
  for (int i = 0; i < rayCount; i++) {
    float angle = i * (2.0f * PI / rayCount);
    float sinA = sinf(angle);
    float cosA = cosf(angle);

    int xStart = centerX + (radius + rayGap) * cosA;
    int yStart = centerY + (radius + rayGap) * sinA;
    int xEnd   = centerX + (radius + rayGap + rayLength) * cosA;
    int yEnd   = centerY + (radius + rayGap + rayLength) * sinA;

    menuSprite.drawWedgeLine(xStart, yStart, xEnd, yEnd, 2, 2, TFT_WHITE, TFT_BLACK);
  }

  // Draw label
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&pixelFont24pt);
  menuSprite.drawString("FLASHLITE", centerX, centerY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
}

void drawOutrunButton() {
  pulsePhase += 0.3f;
  int baseRadius = 7;
  int layers = 6;  // number of nested hexagons

  int outerRadius = 0;

  for (int layer = 1; layer <= layers; layer++) {
    int paletteIndex = 1;
    float scale = 1.0f + 0.2f * sin(pulsePhase + layer);  // pulse size
    int radius = baseRadius * layer * scale;

    if (layer == layers) {
      outerRadius = radius;  // save outermost radius
      paletteIndex = 122;
    }

    // Draw hexagon outline
    for (int i = 0; i < 6; i++) {
      float angle1 = TWO_PI * i / 6 - PI / 2;
      float angle2 = TWO_PI * (i + 1) / 6 - PI / 2;

      int x1 = outrunMenuButtonX + cos(angle1) * radius;
      int y1 = outrunMenuButtonY + sin(angle1) * radius;
      int x2 = outrunMenuButtonX + cos(angle2) * radius;
      int y2 = outrunMenuButtonY + sin(angle2) * radius;

      menuSprite.drawWedgeLine(x1, y1, x2, y2, 1, 1, palette[paletteIndex]);
    }

    menuSprite.setTextColor(TFT_WHITE);
    menuSprite.setFreeFont(&pixelFont24pt);
    menuSprite.drawString("R U N O U T", outrunMenuButtonX, outrunMenuButtonY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
  }

  // Add 3 radial lines from center to points on outermost hexagon (every 120°)
  for (int i = 0; i < 3; i++) {
    float angle = TWO_PI * i / 3 - PI / 2 + PI / 3;  // start top, 120° apart

    int outerX = outrunMenuButtonX + cos(angle) * outerRadius;
    int outerY = outrunMenuButtonY + sin(angle) * outerRadius;

    menuSprite.drawWedgeLine(outrunMenuButtonX, outrunMenuButtonY, outerX, outerY, 1, 1, palette[122]);
  }

  updatePaletteShift(1.0);
}


void drawVisButton() {
  pulsePhase += 0.2f;     // pulsing speed
  rotationPhase += 0.03f; // rotation speed

  for (int i = 0; i < numPetals; i++) {
    float angle = TWO_PI * i / numPetals + rotationPhase;

    float phaseOffset = (i % 2 == 0) ? 0.0f : PI;

    // Set min and max scale
    float minScale = (i % 2 == 0) ? 0.6f : 0.8f;  // even petals start smaller
    float maxScale = 1.2f;

    // Map sine (-1 to 1) → (0 to 1)
    float sineNormalized = 0.5f * (1.0f + sin(pulsePhase + phaseOffset));

    // Interpolate scale between min and max
    float scale = minScale + (maxScale - minScale) * sineNormalized;

    int x = visMenuButtonX + cos(angle) * baseRadius;
    int y = visMenuButtonY + sin(angle) * baseRadius;

    int petalSize = (int)(basePetalSize * scale);
    int paletteIndex = (i + 1) * 40;
    menuSprite.fillSmoothCircle(x, y, petalSize, palette[paletteIndex], TFT_BLACK);
  }
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setFreeFont(&pixelFont24pt);
  menuSprite.drawString("CHROMA POP", visMenuButtonX, visMenuButtonY + smallTextYOffset - (SCREEN_HEIGHT / 2) + 50);
  updatePaletteShift(1.0);
}

void drawHexagonLogo() {
  pulsePhase += 0.2f;     // pulsing speed
  // rotationPhase += 0.03f; // rotation speed
  int hexRadius = 26;
  float baseScale = 1.0f;
  float pulseAmplitude = 0.30f;  // ±18% scale change

  int x[6], y[6];

  // Calculate rotated outer hexagon points
  for (int i = 0; i < 6; i++) {
    float angle = TWO_PI * i / 6 - PI / 2 + rotationPhase;  // apply rotation
    x[i] = CANVAS_CENTER_X + cos(angle) * hexRadius;
    y[i] = CANVAS_CENTER_Y + sin(angle) * hexRadius;
  }

  auto drawPulsingTriangle = [&](int i1, int i2, float panelPhase, uint16_t color) {
    float scale = baseScale + pulseAmplitude * sin(pulsePhase + panelPhase);

    float cx = CANVAS_CENTER_X;
    float cy = CANVAS_CENTER_Y;

    // Scaled outer points (scaled outward from center)
    int sx1 = cx + (x[i1] - cx) * scale;
    int sy1 = cy + (y[i1] - cy) * scale;
    int sx2 = cx + (x[i2] - cx) * scale;
    int sy2 = cy + (y[i2] - cy) * scale;

    menuSprite.fillTriangle(sx1, sy1, sx2, sy2, cx, cy, color);
  };

  // Panel 1 (white)
  drawPulsingTriangle(1, 2, 0.0f, TFT_WHITE);
  drawPulsingTriangle(2, 3, 0.0f, TFT_WHITE);

  // Panel 2 (light grey, phase offset 120°)
  drawPulsingTriangle(3, 4, TWO_PI / 3, TFT_LIGHTGREY);
  drawPulsingTriangle(4, 5, TWO_PI / 3, TFT_LIGHTGREY);

  // Panel 3 (dark grey, phase offset 240°)
  drawPulsingTriangle(5, 0, TWO_PI * 2 / 3, TFT_DARKGREY);
  drawPulsingTriangle(0, 1, TWO_PI * 2 / 3, TFT_DARKGREY);

  for (int i = 0; i < 3; i++) {
    // Add 60° (PI/3) offset to land between regions
    float angle = rotationPhase + i * (TWO_PI / 3) - (TWO_PI / 12);
    int endX = CANVAS_CENTER_X + cos(angle) * hexRadius * 1.2f;
    int endY = CANVAS_CENTER_Y + sin(angle) * hexRadius * 1.2f;

    menuSprite.drawWedgeLine(CANVAS_CENTER_X, CANVAS_CENTER_Y, endX, endY, 3, 3, TFT_BLACK, TFT_BLACK);
  }
}





void hideMenuBar() {
  if (menuSprite.created()) menuSprite.deleteSprite();
  // if (clockMask != nullptr) {
  //     free(clockMask);
  //     clockMask = nullptr;
  // }
  showMenu = false;
  // menuSpriteReady = false;
}

#include "shared_config.h"
#include "palette_manager.h"
#include "pixelFont24.h"
#include "outrun.h"

TFT_eSprite outrunSprite = TFT_eSprite(&tft);

enum GameState { PLAYING, GAME_OVER };
GameState gameState = PLAYING;

unsigned long gameOverTime = 0;  // time when game over triggered
unsigned long restartDelay = 3000;  // 3 seconds before allowing restart
unsigned long quitDelay = 1000;  // 1 seconds before allowing quit

unsigned long gameStartTime = 0;
int playerScore = 0;
unsigned long lastScoreUpdateTime = 0;

#define QUIT_RADIUS 30
#define QUIT_X CENTER_X + 60
#define QUIT_Y CENTER_Y - 50

float horizonY = CENTER_Y + 20;

unsigned long levelStartTime = 0;
int currentLevel = 1;
float moveSpeed = 0.02f;  // base forward speed
int spawnChance = 8;     // 1 in X frames
int numGridLines = 10;    // starts at 10
#define MAX_GRID_LINES 15

int vehicleBaseX = CENTER_X;  // base horizontal center
int vehicleBaseY = SCREEN_HEIGHT - 10;  // near bottom of screen
int vehicleWidth = 20;  // width of the triangle base
int vehicleHeight = 30; // height (distance from tip to base)

struct Pyramid {
  float depth;            // 0.0 (horizon) to 1.0 (bottom)
  float baseXOffset;      // horizontal trajectory
  float spinPhase;        // rotation animation
  bool active;
};

#define MAX_PYRAMIDS 30
Pyramid pyramids[MAX_PYRAMIDS];


#define NUM_MOUNTAIN_POINTS 29

int mountainX[NUM_MOUNTAIN_POINTS] = {
    0, 15, 30, 45, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240,
    260, 275, 290, 305, 320, 340, 360, 380, 400, 420, 440, 460, 480, 500, 520
};

int mountainY[NUM_MOUNTAIN_POINTS];

float mountainScrollOffset = 0.0f;

bool gridLinesInit = false;

struct GridLine {
  float depth;  // 0.0 (horizon) to 1.0 (bottom)
};

GridLine gridLines[MAX_GRID_LINES];

#define NUM_STARS 40

struct Star {
  int x, y;
  bool isCross;   // true = cross star, false = point star
  float twinklePhase;
};

Star stars[NUM_STARS];

float gridBottomShift = 0.0f;  // current horizontal offset of bottom points
float gridShiftTarget = 0.0f;  // desired target offset
unsigned long lastOutrunTouchTime = 0;

void initGame() {
  // Reset game state
  for (int i = 0; i < MAX_PYRAMIDS; i++) {
      pyramids[i].active = false;
  }
  gridBottomShift = 0.0f;
  gridShiftTarget = 0.0f;
  lastOutrunTouchTime = millis();
  playerScore = 0;
  gameStartTime = millis();
  lastScoreUpdateTime = gameStartTime;
  gameState = PLAYING;
  levelStartTime = millis();
  currentLevel = 1;
  moveSpeed = 0.02f;
  spawnChance = 8;
  numGridLines = 10;
  initGridLines();
}

void handleOutrunTouch(float touchX, float touchY, String gesture) {
  lastOutrunTouchTime = millis();  // record current time on touch

  if (gameState == GAME_OVER && millis() - gameOverTime > quitDelay) {
    // Serial.println("TOUCHED when game over");

    float dx = touchX - QUIT_X;
    float dy = SCREEN_HEIGHT - touchY - QUIT_Y;
    if (dx * dx + dy * dy <= (QUIT_RADIUS + 5) * (QUIT_RADIUS + 5)) {
      // Serial.println("QUIT TOUCHED");

      currentMode = MODE_MENU;
      initGame();
    }
  }

  if (touchY < CENTER_Y) {  // right side
    gridShiftTarget = -90.0f;  // shift left
  } else if (touchY >= CENTER_Y) {  // left side
    gridShiftTarget = 90.0f;   // shift right
  }
}

void resetOutrunTouch() {
  gridShiftTarget = 0.0f;  // smoothly return to center when no input
}

// Spawn function for pyramids
void spawnPyramid() {
  for (int i = 0; i < MAX_PYRAMIDS; i++) {
    if (!pyramids[i].active) {
      pyramids[i].depth = 0.0f;

      float r = random(0, 1000) / 1000.0f;
      if (r < 0.1f) {
        // Edge spawn
        if (random(0, 2) == 0) {
          pyramids[i].baseXOffset = -SCREEN_WIDTH / 2 - random(0, SCREEN_WIDTH / 2);
        } else {
          pyramids[i].baseXOffset = SCREEN_WIDTH / 2 + random(0, SCREEN_WIDTH / 2);
        }
      } else if (r < 0.2f) {
        // Center spawn
        pyramids[i].baseXOffset = random(-SCREEN_WIDTH / 6, SCREEN_WIDTH / 6);
      } else if (r < 0.5f) {
        // Center region bias
        pyramids[i].baseXOffset = random(-SCREEN_WIDTH / 4, SCREEN_WIDTH / 4);
      } else {
        // Full spread
        pyramids[i].baseXOffset = random(-SCREEN_WIDTH / 2, SCREEN_WIDTH / 2);
      }

      pyramids[i].spinPhase = random(0, 1000) / 1000.0f * TWO_PI;
      pyramids[i].active = true;
      break;
    }
  }
}

// Update and render pyramids
void updateAndDrawPyramids() {
  float centerX = CENTER_X;
  float bottomY = SCREEN_HEIGHT;
  float pyramidSpeed = moveSpeed;
  float spreadStrength = 2.0f;

  float activeShift = 0.0f;
  if (gridShiftTarget < 0) activeShift = -4.0f;
  else if (gridShiftTarget > 0) activeShift = 4.0f;

  for (int i = 0; i < MAX_PYRAMIDS; i++) {
    if (pyramids[i].active) {
      pyramids[i].depth += pyramidSpeed;

      if (pyramids[i].depth >= 1.0f) {
        pyramids[i].active = false;
        continue;
      }

      if (gridShiftTarget != 0.0f) {
        pyramids[i].baseXOffset += activeShift;
      }

      float depth = pyramids[i].depth;
      float screenY = horizonY + pow(depth, 2.0f) * (bottomY - horizonY);
      float spread = 1.0f + pow(depth, 2.0f) * spreadStrength;
      float screenX = centerX + pyramids[i].baseXOffset * spread;
      float size = 3.0f + 10.0f * depth;
      float spin = pyramids[i].spinPhase + millis() * 0.002f;

      // Calculate triangle corners
      float angle = spin;
      float r = size;
      int x1 = screenX + cos(angle) * r;
      int y1 = screenY + sin(angle) * r;
      int x2 = screenX + cos(angle + TWO_PI / 3) * r;
      int y2 = screenY + sin(angle + TWO_PI / 3) * r;
      int x3 = screenX + cos(angle + 2 * TWO_PI / 3) * r;
      int y3 = screenY + sin(angle + 2 * TWO_PI / 3) * r;

      // Offset target point below triangle center
      int offsetY = 0.4f * size;
      int tx = screenX;
      int ty = screenY + offsetY;

      outrunSprite.fillTriangle(x1, y1, x2, y2, x3, y3, palette[120]);
      outrunSprite.drawTriangle(x1, y1, x2, y2, x3, y3, palette[180]);

      // Lines to offset center
      outrunSprite.drawLine(x1, y1, tx, ty, palette[180]);
      outrunSprite.drawLine(x2, y2, tx, ty, palette[180]);
      outrunSprite.drawLine(x3, y3, tx, ty, palette[180]);

      // Vehicle bounding box (same logic as drawVehicle)
      int baseX = CENTER_X + (int)(-gridBottomShift * 0.2f);
      int baseY = SCREEN_HEIGHT - 8;
      int carWidth = 28;
      int carHeight = 22;

      float maxDrift = 4.0f;
      float driftOffset = maxDrift * (-gridBottomShift / 60.0f);

      float bounce = 2.0f * sinf(millis() * 0.005f);

      int vehicleLeft = baseX - carWidth / 2 + (int)driftOffset;
      int vehicleRight = baseX + carWidth / 2 + (int)driftOffset;
      int vehicleTop = baseY - carHeight + (int)bounce;
      int vehicleBottom = baseY + (int)bounce;
      // Collision check near bottom
      if (screenY + size > vehicleTop - 10) {
        if (screenX + size > vehicleLeft && screenX - size < vehicleRight &&
            screenY + size > vehicleTop && screenY - size < vehicleBottom - 8) {

          gameState = GAME_OVER;
          gameOverTime = millis();

          for (int j = 0; j < MAX_PYRAMIDS; j++) pyramids[j].active = false;

          return;
        }
      }
    }
  }
}

void initGridLines() {
  for (int i = 0; i < numGridLines; i++) {
    gridLines[i].depth = (float)i / numGridLines;
  }
}

void initStars() {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = random(0, SCREEN_WIDTH);
    stars[i].y = random(0, SCREEN_HEIGHT / 2);  // upper half = sky
    stars[i].isCross = random(0, 4) == 0;  // ~25% are cross stars
    stars[i].twinklePhase = random(0, 1000) / 1000.0f * TWO_PI;
  }
}

void drawStars() {
  uint16_t baseColor = TFT_WHITE;

  for (int i = 0; i < NUM_STARS; i++) {
    float twinkle = 0.6f + 0.4f * sin(millis() * 0.002f + stars[i].twinklePhase);

    uint8_t brightness = (uint8_t)(255 * twinkle);
    uint16_t starColor = tft.color565(brightness, brightness, brightness);

    int x = stars[i].x;
    int y = stars[i].y;

    if (stars[i].isCross) {
      // Cross-shaped star
      outrunSprite.drawPixel(x, y, starColor);
      if (x > 0) outrunSprite.drawPixel(x - 1, y, starColor);
      if (x < SCREEN_WIDTH - 1) outrunSprite.drawPixel(x + 1, y, starColor);
      if (y > 0) outrunSprite.drawPixel(x, y - 1, starColor);
      if (y < SCREEN_HEIGHT - 1) outrunSprite.drawPixel(x, y + 1, starColor);
    } else {
      // Single-point star
      outrunSprite.drawPixel(x, y, starColor);
    }
  }
}

void drawRetroSun(float centerX, float centerY, float radius) {
  // uint16_t sunColor = 0xFC40;  // bright orange/yellow (adjust as desired)
  uint16_t sunColor = palette[200];  

  outrunSprite.fillSmoothCircle((int)centerX, (int)centerY, (int)radius, sunColor, TFT_BLACK);

  outrunSprite.fillRect((int)centerX - (int)radius, (int)centerY + 30, (int)radius * 2, (int)radius * 2, TFT_BLACK);

  // Black wide stripes over the sun, avoiding top/bottom edges
  int stripeCount = 6;

  // Define vertical margin (percent of radius)
  float verticalMargin = 0.30f * radius;

  // Compute vertical range for stripes (excluding margins)
  float innerTopY = centerY - radius + verticalMargin;
  float innerBottomY = centerY + radius - verticalMargin;
  float totalStripeArea = innerBottomY - innerTopY;
  float stripeSpacing = totalStripeArea / (stripeCount - 1);  // evenly space stripes

  for (int i = 0; i < stripeCount; i++) {
    float y = innerTopY + i * stripeSpacing;

    // Calculate half-width at this Y
    float halfWidth = sqrt(radius * radius - (y - centerY) * (y - centerY));

    float startX = centerX - halfWidth;
    float endX = centerX + halfWidth;

    // Calculate progressive width:
    // Top line = thinnest, bottom line = thickest
    float minWidth = 1.0f;       // top stripe width
    float maxWidth = 5.0f;       // bottom stripe width
    float t = (float)i / (stripeCount - 1);  // 0.0 at top, 1.0 at bottom
    float stripeWidth = minWidth + t * (maxWidth - minWidth);

    outrunSprite.drawWideLine(
      startX, y,    // start point
      endX, y,      // end point
      stripeWidth,
      TFT_BLACK,
      TFT_BLACK
    );
  }
}

void initShortMountains() {
    int baseY = (int)horizonY;

    // Hand-designed vertical offsets (repeatable pattern)
    int yOffsets[NUM_MOUNTAIN_POINTS] = {
        0, -10, -5, -15, -7, -20, -12, -23, -13, -16,
        -8, -14, -6, 0,   // first half
        0, -12, -7, -18, -9, -22, -11, -20, -10, -16, -8, -12, -5, 0  // second half
    };

    for (int i = 0; i < NUM_MOUNTAIN_POINTS; i++) {
        mountainY[i] = baseY + yOffsets[i];
    }
}

void drawMountainGrid() {
  uint16_t neonBlue = palette[50];
  int totalWidth = mountainX[NUM_MOUNTAIN_POINTS - 1];

  for (int section = 0; section < 2; section++) {
    int offset = section * totalWidth;

    for (int i = 0; i < NUM_MOUNTAIN_POINTS - 1; i++) {
        int x1 = mountainX[i] - (int)mountainScrollOffset + offset;
        int y1 = mountainY[i];
        int x2 = mountainX[i + 1] - (int)mountainScrollOffset + offset;
        int y2 = mountainY[i + 1];

        // Only draw if segment is on screen
        if (x2 >= 0 && x1 <= SCREEN_WIDTH) {
            // diagonal mesh lines
            outrunSprite.drawLine(x1, y1, x2, horizonY, neonBlue);

            // top ridge outline
            outrunSprite.drawLine(x1, y1, x2, y2, neonBlue);
        }
    }
  }
}


void updateMountainScroll() {
    float mountainScrollSpeed = 0.3f;  // adjust speed here

    if (gridShiftTarget < 0) {  // moving right → scroll right
        mountainScrollOffset += mountainScrollSpeed;
    } else if (gridShiftTarget > 0) {  // moving left → scroll left
        mountainScrollOffset -= mountainScrollSpeed;
    }

    // Wrap offset within the total width
    int totalWidth = mountainX[NUM_MOUNTAIN_POINTS - 1];
    if (mountainScrollOffset < 0) {
        mountainScrollOffset += totalWidth;
    } else if (mountainScrollOffset >= totalWidth) {
        mountainScrollOffset -= totalWidth;
    }
}

void drawMountainSilhouette() {
    int totalWidth = mountainX[NUM_MOUNTAIN_POINTS - 1];

    for (int section = 0; section < 2; section++) {
        int offset = section * totalWidth;

        for (int i = 0; i < NUM_MOUNTAIN_POINTS - 1; i++) {
            int x1 = mountainX[i] - (int)mountainScrollOffset + offset;
            int y1 = mountainY[i];
            int x2 = mountainX[i + 1] - (int)mountainScrollOffset + offset;
            int y2 = mountainY[i + 1];

            // Only draw if segment is on screen
            if (x2 >= 0 && x1 <= SCREEN_WIDTH) {
                outrunSprite.fillTriangle(
                    x1, y1,
                    x2, y2,
                    x1, horizonY,
                    TFT_BLACK
                );
                outrunSprite.fillTriangle(
                    x2, y2,
                    x1, horizonY,
                    x2, horizonY,
                    TFT_BLACK
                );
            }
        }
    }
}

void updateAndDrawGrid() {
  // uint16_t neonPink = 0xF81F;
  uint16_t neonPink = palette[1];

  float bottomY = SCREEN_HEIGHT;
  float centerX = CENTER_X;
  float speed = moveSpeed > 0.04f ? 0.04f : moveSpeed;

  // Draw retro sun centered at horizon
  drawRetroSun(centerX, horizonY - 30, SCREEN_WIDTH * 0.2f);

  // Draw neon blue mountains on the horizon (after sun, before grid)
  drawMountainSilhouette();
  drawMountainGrid();

  // Draw grid horizontal lines
  for (int i = 0; i < numGridLines; i++) {
    gridLines[i].depth += speed;

    if (gridLines[i].depth >= 1.0f) {
      gridLines[i].depth -= 1.0f;
    }

    float normalizedDepth = gridLines[i].depth;
    float screenY = horizonY + pow(normalizedDepth, 2.0f) * (bottomY - horizonY);

    outrunSprite.drawFastHLine(0, (int)screenY, SCREEN_WIDTH, neonPink);
  }

  // Draw fixed horizon line
  outrunSprite.drawFastHLine(0, (int)horizonY, SCREEN_WIDTH, neonPink);

  // Draw vertical angled grid lines
  int numVerticalLines = 16;
  float spacing = SCREEN_WIDTH / (float)numVerticalLines;

  for (int i = -numVerticalLines / 2; i <= numVerticalLines / 2; i++) {
    float xTop = centerX + i * spacing;
    float angleOutward = i * 0.3f;

    // Original bottom X (angled outward)
    float xBottomBase = xTop + angleOutward * (bottomY - horizonY);

    // Apply dynamic shift offset (from touch)
    float xBottomShifted = xBottomBase + gridBottomShift;

    outrunSprite.drawLine(
        (int)xTop, (int)horizonY,
        (int)xBottomShifted, (int)bottomY,
        neonPink
    );
  }
}

void drawVehicle() {
  // Base and size
  int baseX = CENTER_X + (int)(-gridBottomShift * 0.2f);
  int baseY = SCREEN_HEIGHT - 8;
  int carWidth = 28;
  int carHeight = 22;

  float maxDrift = 4.0f;
  float driftOffset = maxDrift * (-gridBottomShift / 60.0f);

  float bounce = 2.0f * sinf(millis() * 0.005f);

  // Car bounding box
  int leftX = baseX - carWidth / 2 + (int)driftOffset;
  int rightX = baseX + carWidth / 2 + (int)driftOffset;
  int topY = baseY - carHeight + (int)bounce;
  int bottomY = baseY + (int)bounce;

  // Roof bottom trapezoid (sloped)
  int roofBottomRearY = topY + 4;
  int roofBottomFrontY = roofBottomRearY + 6;
  int roofBottomFrontLeft = leftX + 1;
  int roofBottomFrontRight = rightX - 1;
  int roofBottomRearLeft = roofBottomFrontLeft + 2;
  int roofBottomRearRight = roofBottomFrontRight - 2;

  // Rear window trapezoid
  int roofTopBottomY = roofBottomRearY;
  int roofTopTopY = roofTopBottomY + 12;
  int roofTopBottomLeft = roofBottomRearLeft + 2;
  int roofTopBottomRight = roofBottomRearRight - 2;
  int roofTopTopLeft = roofTopBottomLeft + 4;
  int roofTopTopRight = roofTopBottomRight - 4;

  // Bumper
  int bumperHeight = 4;
  int bumperTopY = bottomY - bumperHeight;

  uint16_t bodyColor = palette[200];
  uint16_t roofColor = palette[205];
  uint16_t bumperColor = palette[194];
  uint16_t tailLightColor = palette[100];

  // Draw rear tires
  outrunSprite.fillRect(leftX + 1, roofTopTopY + 4, 5, 7, roofColor);
  outrunSprite.fillRect(rightX - 5, roofTopTopY + 4, 5, 7, roofColor);

  // Draw body rectangle
  outrunSprite.fillRect(leftX, roofBottomRearY + 7, carWidth + 1, 10, bodyColor);

  // Draw hood
  outrunSprite.fillRoundRect(leftX + 4, topY - 1, carWidth - 7, 8, 3, bodyColor);

  // Draw roof bottom trapezoid
  outrunSprite.fillTriangle(roofBottomFrontLeft, roofBottomFrontY,
                            roofBottomFrontRight, roofBottomFrontY,
                            roofBottomRearLeft, roofBottomRearY,
                            bodyColor);
  outrunSprite.fillTriangle(roofBottomFrontRight, roofBottomFrontY,
                            roofBottomRearLeft, roofBottomRearY,
                            roofBottomRearRight, roofBottomRearY,
                            bodyColor);

  // Draw side mirrors
  outrunSprite.fillEllipse(roofBottomFrontRight + 1, roofBottomFrontY - 5,
                            3, 2,
                            TFT_BLACK);
  outrunSprite.fillEllipse(roofBottomFrontLeft - 1, roofBottomFrontY - 5,
                            3, 2,
                            TFT_BLACK);
  outrunSprite.drawEllipse(roofBottomFrontRight + 1, roofBottomFrontY - 5,
                            3, 2,
                            bodyColor);
  outrunSprite.drawEllipse(roofBottomFrontLeft - 1, roofBottomFrontY - 5,
                            3, 2,
                            bodyColor);

  // Draw side windows (black)
  outrunSprite.fillTriangle(roofBottomFrontLeft + 2, roofBottomFrontY + 2,
                            roofBottomFrontLeft + 5, roofBottomFrontY + 4,
                            roofBottomRearLeft, roofBottomRearY + 1,
                            TFT_BLACK);
  outrunSprite.fillTriangle(roofBottomFrontRight - 2, roofBottomFrontY + 2,
                          roofBottomFrontRight - 5, roofBottomFrontY + 4,
                          roofBottomRearRight, roofBottomRearY + 1,
                          TFT_BLACK);

  // Draw roof top trapezoid 
  outrunSprite.fillTriangle(roofTopTopLeft, roofTopTopY,
                            roofTopTopRight, roofTopTopY,
                            roofTopBottomLeft, roofTopBottomY,
                            roofColor);
  outrunSprite.fillTriangle(roofTopTopRight, roofTopTopY,
                            roofTopBottomLeft, roofTopBottomY,
                            roofTopBottomRight, roofTopBottomY,
                            roofColor);

  // Draw back lines
  outrunSprite.drawLine(roofTopTopLeft - 1, roofTopTopY - 6,
                            roofTopTopRight + 1, roofTopTopY - 6,
                            TFT_BLACK);
  outrunSprite.drawLine(roofTopTopLeft, roofTopTopY - 4,
                            roofTopTopRight, roofTopTopY - 4,
                            TFT_BLACK);
  outrunSprite.drawLine(roofTopTopLeft + 1, roofTopTopY - 2,
                            roofTopTopRight - 1, roofTopTopY - 2,
                            TFT_BLACK);

  // Draw spoiler
  outrunSprite.fillRect(leftX + 3, roofTopTopY + 1,
                            carWidth - 5, 1,
                            roofColor);

  // Draw bumper
  outrunSprite.fillRoundRect(leftX, bumperTopY, 29, 8, 3, bumperColor);



  // Draw tail lights
  outrunSprite.fillRoundRect(leftX + 2, bottomY - 2, 8, 3, 2, tailLightColor);
  outrunSprite.fillRoundRect(rightX - 9, bottomY - 2, 8, 3, 2, tailLightColor);
  
  // Draw bumper line
  outrunSprite.drawRect(roofTopTopLeft, roofTopTopY + 4,
                            11, 3,
                            roofColor);
}

void drawQuitButton() {
  // Draw circle
  outrunSprite.fillCircle(QUIT_X, QUIT_Y, QUIT_RADIUS, palette[100]);
  outrunSprite.drawCircle(QUIT_X, QUIT_Y, QUIT_RADIUS, palette[200]);

  // Draw 'X'
  outrunSprite.drawWedgeLine(QUIT_X - 10, QUIT_Y - 10, QUIT_X + 10, QUIT_Y + 10, 2, 2, palette[200]);
  outrunSprite.drawWedgeLine(QUIT_X - 10, QUIT_Y + 10, QUIT_X + 10, QUIT_Y - 10, 2, 2, palette[200]);
}

void outrunVisualizerLoop() {
  outrunSprite.fillScreen(TFT_BLACK);

  if (!gridLinesInit) {
    gridLinesInit = true;
    initGridLines();
    initStars();
    initShortMountains();
  }

  if (millis() - lastOutrunTouchTime > 20) {
    resetOutrunTouch();
  }

  // Smoothly approach target
  float shiftSpeed = 2.0f;  // adjust for faster/slower response
  gridBottomShift += (gridShiftTarget - gridBottomShift) * 0.1f * shiftSpeed;

  // Occasionally spawn a pyramid (~1 in 10 frames)
  if (random(0, spawnChance) == 0) {
    spawnPyramid();
  }


  updateMountainScroll();
  drawStars();
  updateAndDrawGrid();
  if (gameState == PLAYING) {
    updateAndDrawPyramids();
  }
  drawVehicle();
  updatePaletteShift(1.0f, true);

  if (gameState == PLAYING) {
      unsigned long currentTime = millis();

      // Update score every 100 ms → 10 points per second
      if (currentTime - lastScoreUpdateTime >= 100) {
          playerScore += (1 * currentLevel);  // 1 point per 100 ms → 10 points/sec
          lastScoreUpdateTime = currentTime;
      }
  }

  outrunSprite.setTextColor(TFT_WHITE);
  outrunSprite.setTextDatum(CC_DATUM);
  outrunSprite.setFreeFont(&pixelFont24pt);
  outrunSprite.drawString(String(playerScore), CENTER_X, 20);

  if (gameState == GAME_OVER) {
    outrunSprite.setTextColor(TFT_WHITE);
    outrunSprite.setTextDatum(CC_DATUM);
    outrunSprite.setFreeFont(&pixelFont24pt);
    outrunSprite.drawString("LOVE IS LOST", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 24);
    drawQuitButton();

    // After delay, allow restart on touch
    if (millis() - gameOverTime > restartDelay && touch.available()) {
        gameState = PLAYING;
        initGame();
    }

    return;  // skip rest of loop during game over
  }

  if (millis() - levelStartTime >= 30000) {  // 30 seconds
    currentLevel++;
    levelStartTime = millis();

    moveSpeed += 0.005f;

    if (spawnChance > 2) {  // don’t go below 1-in-2 chance
        spawnChance--;
    }

    numGridLines += 2;  // increase line density
    if (numGridLines > MAX_GRID_LINES) numGridLines = MAX_GRID_LINES;

    initGridLines();
  }
}
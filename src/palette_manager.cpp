// palette_manager.cpp
#include "palette_manager.h"
#include "shared_config.h"
#include <math.h>

static const ColorProfile palettePresets[] = {
  { 0.0f, 0.33f, 0.67f },  // rainbow
  { 0.0f, 0.1f,  0.2f  },  // fire
  { 0.5f, 0.7f,  0.9f  },  // ocean
  { 0.0f, 0.5f,  0.75f },  // cool tech
  { 0.25f, 0.70f, 0.10f }, // toxic plasma
  { 0.12f, 0.52f, 0.92f }, // cyber neural
  { 0.35f, 0.60f, 0.10f }, // alien world
  { 0.00f, 0.20f, 0.45f }, // sunset
  { 0.10f, 0.65f, 0.85f }, // electric fizz
  { 0.00f, 0.00f, 0.00f }, // monochrome
  { 0.15f, 0.55f, 0.85f }, // blue/gold
  { 0.08f, 0.33f, 0.66f }, // copper-rust-sapphire
  { 0.3f, 0.45f, 0.5f  },  // cauliflower muted
  { 0.0f, 0.5f, 0.7f  }    // ultrasharp deep
};

static const ColorProfile outrunPalettePresets[] = {
  { 0.0f, 0.33f, 0.67f },  // rainbow 0 good
  { 0.5f, 0.7f,  0.9f  },  // ocean 2 good
  { 0.0f, 0.5f,  0.75f },  // cool tech 3 good 
  { 0.25f, 0.70f, 0.10f }, // toxic plasma 4 - pretty good
  { 0.12f, 0.52f, 0.92f }, // cyber neural 5 good
  { 0.35f, 0.60f, 0.10f }, // alien world 6 good
  { 0.10f, 0.65f, 0.85f }, // electric fizz 8 good
  { 0.15f, 0.55f, 0.85f }, // blue/gold 9 good
  { 0.3f, 0.45f, 0.5f  },  // cauliflower muted 11 good
};

uint16_t palette[PALETTE_SIZE];  // Use hue values 0–255

static int lastProfileIndex = 0;
static ColorProfile currentProfile;
static ColorProfile targetProfile;
static float profileBlendT = 1.0f;
static const float profileBlendSpeed = 0.01f;
static float currentSpacing = 1.0f;
static float targetSpacing = 1.0f;
static const float spacingLerpSpeed = 0.01f;
int paletteDirection = 1;
float paletteFloat = 0.0f;
float minSpeed = 1.0f;
float maxSpeed = 5.0f;
float colorCycleBase = random(minSpeed * 100, maxSpeed * 100) / 100.0f;
bool firstOutrunPick = true;

ColorProfile lerpProfile(const ColorProfile& a, const ColorProfile& b, float t) {
  ColorProfile result;
  result.phaseR = a.phaseR + t * (b.phaseR - a.phaseR);
  result.phaseG = a.phaseG + t * (b.phaseG - a.phaseG);
  result.phaseB = a.phaseB + t * (b.phaseB - a.phaseB);
  return result;
}

uint16_t cosinePalette(float t, float phaseR, float phaseG, float phaseB) {
  float r = 0.5f + 0.5f * cosf(6.28318f * (t + phaseR));
  float g = 0.5f + 0.5f * cosf(6.28318f * (t + phaseG));
  float b = 0.5f + 0.5f * cosf(6.28318f * (t + phaseB));
  return tft.color565((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
}

void generateShiftedPalette(float offset) {
  ColorProfile blended = lerpProfile(currentProfile, targetProfile, profileBlendT);
  for (int i = 0; i < PALETTE_SIZE; i++) {
    float t = fmodf((offset + i * currentSpacing) / PALETTE_SIZE, 1.0f);
    palette[i] = cosinePalette(t, blended.phaseR, blended.phaseG, blended.phaseB);
  }
  if (profileBlendT < 1.0f) {
    profileBlendT += profileBlendSpeed;
    if (profileBlendT > 1.0f) profileBlendT = 1.0f;
  }
}

void updatePaletteShift(float zoom, bool isOutrun, bool isShader) {  float scaled = powf(logf(zoom + 1.0f), 1.09f);
  int zoomStep = max(1, (int)scaled);

  // Occasionally reverse direction
  if (random(400) == 0) {
    paletteDirection *= -1;
  }

  if (isOutrun && firstOutrunPick) {
    pickNewFadeProfile(isOutrun);
    firstOutrunPick = false;
  } else if (random(400) == 0 && profileBlendT >= 1.0f) {
    pickNewFadeProfile(isOutrun);
  }

  float time = millis() / 1000.0f;  // time in seconds

  float oscillation = 0.5f * (1.0f + sinf(time * 0.1f));  // 0 to 1 range

  float colorCycleSpeed = colorCycleBase * (0.8f + 0.4f * oscillation);  // ±20% range
  if (isOutrun) {
    colorCycleSpeed = 0.3f * (0.8f + 0.4f * oscillation);  // ±20% range
  }
  paletteFloat += paletteDirection * zoomStep * colorCycleSpeed;

  if (paletteFloat >= 256.0f) paletteFloat = fmodf(paletteFloat, 256.0f);
  if (paletteFloat < 0.0f) paletteFloat += 256.0f;

  if (isShader) {
    targetSpacing = 1.0f;
  }

  if (fabs(currentSpacing - targetSpacing) > 0.01f) {
    currentSpacing += (targetSpacing - currentSpacing) * spacingLerpSpeed;
  } else {
    currentSpacing = targetSpacing;
  }

  // Only regenerate when offset moved by at least half an index or during a
  // profile blend transition (which changes colors independently of offset).
  static float lastGeneratedOffset = -999.0f;
  static float lastGeneratedSpacing = -999.0f;
  bool offsetChanged = fabsf(paletteFloat - lastGeneratedOffset) >= 0.5f;
  bool spacingChanged = fabsf(currentSpacing - lastGeneratedSpacing) >= 0.01f;
  bool blending = (profileBlendT < 1.0f);
  if (offsetChanged || spacingChanged || blending) {
    lastGeneratedOffset = paletteFloat;
    lastGeneratedSpacing = currentSpacing;
    generateShiftedPalette(paletteFloat);
  }
}

void pickNewFadeProfile(bool isOutrun) {
  int newIndex;
  if (isOutrun) {
    newIndex = random(sizeof(outrunPalettePresets) / sizeof(ColorProfile));
    Serial.print("OUTRUN ");
    Serial.println(newIndex);
  } else {
    do {
      newIndex = random(sizeof(palettePresets) / sizeof(ColorProfile));
    } while (newIndex == lastProfileIndex);
  }

  lastProfileIndex = newIndex;
  currentProfile = lerpProfile(currentProfile, targetProfile, profileBlendT);
  // Serial.println(newIndex);
  if (isOutrun) {
    targetProfile = outrunPalettePresets[newIndex];
  } else {
    targetProfile = palettePresets[newIndex];
  }
  profileBlendT = 0.0f;
  targetSpacing = 1 + random(6);
}

void initPalette() {
  paletteFloat = random(0, PALETTE_SIZE);
  paletteDirection = (random(2) * 2) - 1;
  int index = random(sizeof(palettePresets) / sizeof(ColorProfile));
  currentProfile = palettePresets[index];
  pickNewFadeProfile(false);
}

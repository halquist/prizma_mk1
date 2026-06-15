#include "shader.h"
#include "shared_config.h"
#include <math.h>
#include "vec_math.h"
#include "palette_manager.h"

#define DOWNSCALE_FACTOR 2  // 2 = 120x120, 4 = 60x60
const int LOW_W = SCREEN_WIDTH / DOWNSCALE_FACTOR;
const int LOW_H = SCREEN_HEIGHT / DOWNSCALE_FACTOR;

uint16_t lowResBuffer[LOW_W * LOW_H];  // ~28 KB for 120x120

enum ShaderMode {
  SHADER_NEBULA,
  SHADER_LIFE,
  // SHADER_SWIRL,
  SHADER_CAUSTIC,
  // SHADER_ACID_VISION,
  SHADER_ACID_TUNNEL,
  SHADER_BIOHAZARD,
  SHADER_AVALON,
  // SHADER_TEST,
  NUM_SHADERS
};

ShaderMode currentShader = SHADER_AVALON;

void cycleShader(bool reverse) {
  Serial.println(reverse);
  int progress = reverse ? -1 : 1;
  currentShader = (ShaderMode)((currentShader + NUM_SHADERS + progress) % NUM_SHADERS);
}

void randomizeShader() {
  ShaderMode previous = currentShader;
  do {
    currentShader = (ShaderMode)(random(NUM_SHADERS));
  } while (currentShader == previous);  // avoid picking the same shader
}

float phaseShift1 = 0.0f, phaseShift2 = 0.0f;
float targetPhaseShift1 = 0.0f, targetPhaseShift2 = 0.0f;
float freq1 = 10.0f, freq2 = 15.0f, freq3 = 8.0f;
float targetFreq1 = 10.0f, targetFreq2 = 15.0f, targetFreq3 = 8.0f;
float hueSpinSpeed = 0.1f;
float targetHueSpinSpeed = 0.1f;
float rotationSpeedf = 0.1f;
float targetRotationSpeed = 0.1f;
int lastPerturb = 0;

float patternScale = 1.0f;
float targetPatternScale = 1.0f;

float phaseBaseScale;
float phaseFreqScale;
float phaseTimeScale;

#define MAX_SHADER_STEPS 4
float shaderSeeds[MAX_SHADER_STEPS];

#define CA_W 64
#define CA_H 64

uint8_t caGrid[CA_W * CA_H];        // Current state
uint8_t caNext[CA_W * CA_H];        // Next generation

int caFrameCounter = 0;
const int CA_UPDATE_INTERVAL = 1;  // update every x frames

#define SINE_TABLE_SIZE 1024
float sineTable[SINE_TABLE_SIZE];

void initSwirlPhaseVars() {
  phaseBaseScale  = 0.2f + 0.2f * (random(1000) / 1000.0f);  // 0.2 – 0.4
  phaseFreqScale  = 0.5f + 0.5f * (random(1000) / 1000.0f);  // 0.5 – 1.0
  phaseTimeScale  = 0.02f + 0.06f * (random(1000) / 1000.0f); // 0.02 – 0.08
}

float getPhaseOffset(int i, float time) {
  return phaseBaseScale * sinf(i * phaseFreqScale + time * phaseTimeScale);
}

void initSineTable() {
  for (int i = 0; i < SINE_TABLE_SIZE; i++) {
    sineTable[i] = sinf((float)i * 2.0f * M_PI / SINE_TABLE_SIZE);
  }
}

float acidRandomPhase[4];
float acidRandomScale[4];

void initAcidShader() {
  for (int i = 0; i < 4; i++) {
    acidRandomPhase[i] = random(0, 628) / 100.0f;  // range 0–6.28
    acidRandomScale[i] = 1.0f + random(-15, 15) / 100.0f;  // range ~0.85–1.15
  }
}

void initShaderSeeds() {
  for (int i = 0; i < MAX_SHADER_STEPS; i++) {
    shaderSeeds[i] = random(0, 6283) * 0.001f;  // random 0 to ~2π
  }
}

float fastSin(float x) {
  // Wrap x into [0, 2π]
  x = fmodf(x, 2.0f * M_PI);
  if (x < 0) x += 2.0f * M_PI;

  // Map to table index
  float indexF = x * (SINE_TABLE_SIZE / (2.0f * M_PI));
  int index = (int)indexF % SINE_TABLE_SIZE;

  // Optional: interpolate
  int nextIndex = (index + 1) % SINE_TABLE_SIZE;
  float frac = indexF - index;
  return (1.0f - frac) * sineTable[index] + frac * sineTable[nextIndex];
}

void hueToRGB(float hue, float& r, float& g, float& b) {
  // Hue in [0, 1]
  float angle = hue * 6.28318f; // 2π
  r = 0.5f + 0.5f * fastSin(angle);
  g = 0.5f + 0.5f * fastSin(angle + 2.094f);  // + 120°
  b = 0.5f + 0.5f * fastSin(angle + 4.188f);  // + 240°
}

float hash(float x, float y) {
  return fract(sinf(x * 127.1f + y * 311.7f) * 43758.5453f);
}

void seedCA() {
  for (int y = 0; y < CA_H; y++) {
    for (int x = 0; x < CA_W; x++) {
      caGrid[y * CA_W + x] = (random(100) < 12) ? 1 : 0;  // ~12% chance alive
    }
  }
}

void perturbCA(int count = 10) {
  for (int i = 0; i < count; i++) {
    int x = random(CA_W);
    int y = random(CA_H);
    caGrid[y * CA_W + x] = 1;  // spawn cell
  }
}

void updateCA() {
  if (millis() - lastPerturb > 100) {
    perturbCA(20);
    lastPerturb = millis();
  }
  for (int y = 0; y < CA_H; y++) {
    for (int x = 0; x < CA_W; x++) {
      int count = 0;
      for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
          if (dx == 0 && dy == 0) continue;
          int nx = (x + dx + CA_W) % CA_W;
          int ny = (y + dy + CA_H) % CA_H;
          count += caGrid[ny * CA_W + nx];
        }
      }
      int idx = y * CA_W + x;
      caNext[idx] = (caGrid[idx] && (count == 2 || count == 3)) || (!caGrid[idx] && (count == 3 || count == 6));
    }
  }
  memcpy(caGrid, caNext, sizeof(caGrid));
}

void computeShaderPixelGridPulse(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  Vec2 uv = {
    x / iResolution.y * 10.0f,
    y / iResolution.y * 10.0f
  };

  Vec2 cell = { floorf(fmodf(uv.x, 2.0f)), floorf(fmodf(uv.y, 2.0f)) };
  Vec2 direction = { cell.x * 2.0f - 1.0f, cell.y * 2.0f - 1.0f };

  Vec2 offset = {
    direction.y * fmaxf(fmodf(iTime + 1.0f, 2.0f) - 1.0f, 0.0f),
    direction.x * fmaxf(fmodf(iTime, 2.0f) - 1.0f, 0.0f)
  };

  Vec2 shifted = {
    fmodf(uv.x + offset.x, 1.0f),
    fmodf(uv.y + offset.y, 1.0f)
  };

  float dx = shifted.x - 0.5f;
  float dy = shifted.y - 0.5f;
  float dist = sqrtf(dx * dx + dy * dy) * 2.0f + 0.1f;
  dist = powf(dist, 30.0f);

  // Map dist to palette index
  float t = fminf(dist * 400.0f, 255.0f);  // Adjust 400 as needed
  int idx = (int)t & 0xFF;
  uint16_t color = palette[idx];

  r = ((color >> 11) & 0x1F) / 31.0f;
  g = ((color >> 5) & 0x3F) / 63.0f;
  b = (color & 0x1F) / 31.0f;
}




void computeShaderPixelLife(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  iTime = fmodf(iTime, 1000.0f);
  Vec2 uv = {
    (x + 0.5f - iResolution.x * 0.5f) / iResolution.y,
    (y + 0.5f - iResolution.y * 0.5f) / iResolution.y
  };

  float gridX = (uv.x + 0.5f) * CA_W;
  float gridY = (uv.y + 0.5f) * CA_H;

  float min1 = 99999.0f, min2 = 99999.0f, min3 = 99999.0f;

  for (int j = -6; j <= 6; j++) {
    for (int i = -6; i <= 6; i++) {
      int cx = (int)gridX + i;
      int cy = (int)gridY + j;
      if (cx < 0 || cx >= CA_W || cy < 0 || cy >= CA_H) continue;
      if (caGrid[cy * CA_W + cx]) {
        float fx = cx + 0.5f;
        float fy = cy + 0.5f;
        float dx = fx - gridX;
        float dy = fy - gridY;
        float d2 = dx * dx + dy * dy;

        if (d2 < min1) {
          min3 = min2;
          min2 = min1;
          min1 = d2;
        } else if (d2 < min2) {
          min3 = min2;
          min2 = d2;
        } else if (d2 < min3) {
          min3 = d2;
        }
      }
    }
  }

  float d1 = sqrtf(min1);
  float d2 = sqrtf(min2);
  float d3 = sqrtf(min3);

  // Core = brightness, edge = Voronoi boundary
  float border12 = d2 - d1;
  float border23 = d3 - d2;

  float edge = smoothstep(0.0f, 0.05f, border12) * smoothstep(0.0f, 0.05f, border23);
  float core = expf(-d1 * 1.8f);

  // Optional: add turbulence
  float angle = atan2f(uv.y, uv.x);         // angular component
  float radius = sqrtf(uv.x * uv.x + uv.y * uv.y);
  float turb = 0.3f * fastSin(20.0f * radius + 6.0f * angle + iTime * 1.2f);
  float hue = fmodf(d1 * 0.15f + turb + iTime * 0.1f, 1.0f);
  if (hue < 0.0f) hue += 1.0f;

  float baseR, baseG, baseB;
  hueToRGB(hue, baseR, baseG, baseB);

  r = core * edge * baseR;
  g = core * edge * baseG;
  b = core * edge * baseB;
}

void computeShaderPixelNebula(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  iTime = fmodf(iTime, 1000.0f);
  Vec2 uv = {
    (x + 0.5f - iResolution.x / 2.0f) / iResolution.y,
    (y + 0.5f - iResolution.y / 2.0f) / iResolution.y
  };
  
  uv = rotateUV(uv, rotationAngle);
  uv.x += 0.05f * fastSin(iTime + uv.y * 5.0f + phaseShift2);
  uv.y += 0.05f * fastSin(iTime * 0.8f + uv.x * 5.0f + phaseShift1);

  float s1 = fastSin(10.0f * (uv.x * uv.y) + iTime);
  float s2 = fastSin(15.0f * (uv.x + uv.y) - iTime * 0.8f);
  float s3 = fastSin(8.0f  * (uv.x - uv.y) + iTime * 1.5f);

  // Blend and normalize to [0,1]
  float color = (s1 + s2 + s3) / 3.0f;
  color = 0.5f + 0.5f * color;

  float angle = atan2f(uv.y, uv.x);             // range: [-π, π]
  float radius = sqrtf(uv.x * uv.x + uv.y * uv.y);
  // float hue = fmodf((angle / (2.0f * M_PI)) + 0.5f + iTime * 0.1f, 1.0f);
  float hue = fmodf((angle / (2.0f * M_PI)) + radius + iTime * hueSpinSpeed + phaseShift1, 1.0f);
  float baseR, baseG, baseB;
  hueToRGB(hue, baseR, baseG, baseB);

  r = color * baseR;
  g = color * baseG;
  b = color * baseB;
}

float causticPattern(Vec3 p) {
  float d = 1.0f;

  // Rotation matrix
  float3x3 r = {
    {-0.6f, -0.3f, 0.6f},
    { 0.9f, -0.6f, 0.3f},
    { 0.3f,  0.6f, 0.6f}
  };

  for (int i = 0; i < 3; i++) {
    // Matrix multiply: p = r * p
    Vec3 temp = {
      r[0][0]*p.x + r[0][1]*p.y + r[0][2]*p.z,
      r[1][0]*p.x + r[1][1]*p.y + r[1][2]*p.z,
      r[2][0]*p.x + r[2][1]*p.y + r[2][2]*p.z
    };
    p = temp;

    // Fractal pattern via fractional offset to center
    float fx = p.x - floorf(p.x) - 0.5f;
    float fy = p.y - floorf(p.y) - 0.5f;
    float fz = p.z - floorf(p.z) - 0.5f;

    float dist = sqrtf(fx * fx + fy * fy + fz * fz);
    d = fminf(d, dist);
  }
  return d;
}

void computeShaderPixelCaustic(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  Vec2 uv = {
    (float)x / iResolution.x,
    (float)y / iResolution.y
  };

  // Dynamic scale oscillates over time
  float scale = 8.0f + 4.0f * sinf(iTime * 0.15f);
  uv.x *= scale;
  uv.y *= scale;

  // Animated 3D point for caustic evaluation
  Vec3 p = {
    uv.x + iTime * 0.5f,
    uv.y + iTime * 0.5f,
    iTime * 0.5f
  };

  float c = causticPattern(p);
  float intensity = c * c;

  // Convert intensity to palette index (scaled for contrast)
  float indexFloat = paletteFloat + intensity * 256.0f;
  int idx = ((int)indexFloat) & 0xFF;
  uint16_t color = palette[idx];

  // Convert RGB565 to float RGB
  uint8_t r5 = (color >> 11) & 0x1F;
  uint8_t g6 = (color >> 5) & 0x3F;
  uint8_t b5 = color & 0x1F;

  r = (r5 / 31.0f) * intensity;
  g = (g6 / 63.0f) * intensity;
  b = (b5 / 31.0f) * intensity;
}

// void computeShaderPixelAcidVision(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
//   Vec2 uv = {
//     (2.0f * x - iResolution.x) / iResolution.y,
//     (2.0f * y - iResolution.y) / iResolution.y
//   };

//   Vec2 uv0 = uv;
//   float len0 = sqrtf(uv0.x * uv0.x + uv0.y * uv0.y);

//   float accR = 0.0f, accG = 0.0f, accB = 0.0f;

//   for (int i = 0; i < 3; i++) {
//     // Repeat grid with local offsets
//     uv.x = fractf(uv.x * 1.5f) - 0.5f;
//     uv.y = fractf(uv.y * 1.5f) - 0.5f;

//     float len = sqrtf(uv.x * uv.x + uv.y * uv.y);
//     float d = len * expf(-len0);

//     d = sinf(d * 4.0f + iTime * 0.2f);
//     d = fractf(1.0f / fmaxf(d, 0.001f));
//     d = sinf(d * 2.0f + iTime * 0.2f);
//     d = fabsf(d);
//     float base = 0.002f / fmaxf(d, 0.001f);
//     d = sqrtf(sqrtf(base));  // ~x^0.25 (close to 0.2)

//     // float radial = expf(-len0 * 2.5f);
//     // d = fmaxf(d, 0.3f * radial);

//     // Enforce minimum signal to ensure visibility
//     if (d < 0.1f) d = 0.1f;

//     // Use palette lookup instead of manual cosine palette
//     float t = len0 + i * 4.0f + iTime * 0.5f;
//     float index = fmodf(paletteFloat + t * 48.0f, 256.0f);  // adjust scale as needed
//     int idx = (int)index & 0xFF;

//     uint16_t color = palette[idx];
//     uint8_t r5 = (color >> 11) & 0x1F;
//     uint8_t g6 = (color >> 5) & 0x3F;
//     uint8_t b5 = color & 0x1F;

//     accR += (r5 / 31.0f) * d;
//     accG += (g6 / 63.0f) * d;
//     accB += (b5 / 31.0f) * d;
//   }

//   // Final color normalization (can adjust clamp value)
//   r = fminf(accR, 1.0f);
//   g = fminf(accG, 1.0f);
//   b = fminf(accB, 1.0f);
// }

void computeShaderPixelAcidPaletteTunnel(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  const float phi = 6.2831853f;  // 2π

  Vec2 uv = {
    (2.0f * x - iResolution.x) / iResolution.y,
    (2.0f * y - iResolution.y) / iResolution.y
  };

  // Apply slow rotation around center
  float rotationAngle = iTime * 0.07f;  // Adjust speed here
  uv = rotateUV(uv, rotationAngle);

  Vec2 uv0 = uv;
  float len0 = sqrtf(uv0.x * uv0.x + uv0.y * uv0.y);
  float timer = iTime * 0.4f;

  float accR = 0.0f, accG = 0.0f, accB = 0.0f;

  int numSteps = 2 + (int)(1.5f + sinf(iTime * 0.05f));  // cycles between 2–4 smoothly

  for (int i = 0; i < numSteps; i++) {
    float iFloat = (float)i;

    // Slowly modulating parameters
    float phaseOffset = 6.2831f * sinf(iFloat * 0.7f + iTime * 0.05f + shaderSeeds[i]);
    float scale = 1.0f + 0.15f * sinf(iFloat * 1.3f + iTime * 0.04f + shaderSeeds[i]);

    uv.x = fractf(uv.x * 1.5f) - 0.5f;
    uv.y = fractf(uv.y * 1.5f) - 0.5f;

    float len = sqrtf(uv.x * uv.x + uv.y * uv.y);
    float d = len * expf(-len0);

    d = sinf(d * 4.0f * scale + iTime * 0.4f + phaseOffset) / 4.0f;
    d = fabsf(d);
    float invD = 0.025f / fmaxf(d, 0.001f);
    d = invD * invD;

    // Palette index using evolving phase
    float t = len0 + iFloat * 0.4f + iTime * 0.5f + 0.3f * sinf(iFloat + iTime * 0.05f);
    float hueIdx = fmodf(paletteFloat + t * 48.0f, 256.0f);
    int idx = (int)hueIdx & 0xFF;

    uint16_t color = palette[idx];
    float rf = ((color >> 11) & 0x1F) / 31.0f;
    float gf = ((color >> 5) & 0x3F) / 63.0f;
    float bf = (color & 0x1F) / 31.0f;

    accR += rf * d;
    accG += gf * d;
    accB += bf * d;
  }


  // Normalize to avoid overbright results
  r = fminf(accR, 1.0f);
  g = fminf(accG, 1.0f);
  b = fminf(accB, 1.0f);
}

// void computeShaderPixelSwirlTunnel(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
//   Vec2 uv = {
//     (2.0f * x - iResolution.x) / fminf(iResolution.x, iResolution.y),
//     (2.0f * y - iResolution.y) / fminf(iResolution.x, iResolution.y)
//   };

//   // Apply rotation around center
//   float rotationAngle = iTime * 0.1f;  // or a random rotationSpeed variable
//   uv = rotateUV(uv, rotationAngle);
//   uv.x += 0.05f * fastSin(iTime + uv.y * 5.0f + phaseShift2);
//   uv.y += 0.05f * fastSin(iTime * 0.8f + uv.x * 5.0f + phaseShift1);

//   // Distort base UV slightly
//   uv.x += 0.02f * sinf(uv.y * 12.0f + iTime * 0.3f);
//   uv.y += 0.02f * cosf(uv.x * 9.0f - iTime * 0.25f);
//   float jitterX = 2.5f + 0.05f * sinf(iTime * 0.9f);
//   float jitterY = 1.5f + 0.05f * cosf(iTime * 0.7f);

//   float swirlScale = 1.0f + 0.2f * sinf(iTime * 0.1f);
//   // Apply swirl with random phases
//   for (int j = 1; j < 6; j++) {
//     float i = (float)j;
//     float p = getPhaseOffset(j, iTime);
//     uv.x += 0.6f / i * cosf(i * jitterX * uv.y + iTime + p);
//     uv.y += 0.6f / i * cosf(i * jitterY * uv.x + iTime - p);
//   }

//   float s = fabsf(sinf(iTime - uv.x - uv.y));
//   float intensity = 0.3f / fmaxf(s, 0.001f);  // Avoid div-by-zero
//   intensity = fminf(intensity, 0.6f);         // Clamp to avoid overbright

//   if (paletteFloat >= 256.0f) paletteFloat = fmodf(paletteFloat, 256.0f);
//   // Use spatial/temporal component to pick hue
//   float hueIdx = fmodf(paletteFloat + (uv.x + uv.y) * 32.0f, 256.0f);
//   int idx = (int)hueIdx & 0xFF;
//   uint16_t color = palette[idx];

//   // Convert RGB565 to float
//   uint8_t r5 = (color >> 11) & 0x1F;
//   uint8_t g6 = (color >> 5) & 0x3F;
//   uint8_t b5 = color & 0x1F;

//   float rf = r5 / 31.0f;
//   float gf = g6 / 63.0f;
//   float bf = b5 / 31.0f;

//   r = rf * intensity;
//   g = gf * intensity;
//   b = bf * intensity;
// }

void computeShaderPixelBiohazard(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  Vec2 uv = {
    (float)x / iResolution.x,
    (float)y / iResolution.y
  };

  Vec2 center = { 0.5f, 0.5f };
  uv.x -= center.x;
  uv.y -= center.y;

  // Apply time-based rotation
  float angle = iTime * rotationSpeedf;
  float ca = cosf(angle);
  float sa = sinf(angle);
  float rx = uv.x * ca - uv.y * sa;
  float ry = uv.x * sa + uv.y * ca;
  uv.x = rx + center.x;
  uv.y = ry + center.y;

  Vec2 pos = {
    uv.x + 0.0f,
    uv.y + 0.0f
  };

  float color = 0.0f;
  color += sinf(pos.x * cosf(iTime / freq1 + phaseShift1) * 80.0f) + cosf(pos.y * cosf(iTime / freq1) * 10.0f);
  color += sinf(pos.y * sinf(iTime / freq2 + phaseShift2) * 40.0f) + cosf(pos.x * sinf(iTime / freq2) * 40.0f);
  color += sinf(pos.x * sinf(iTime / freq3) * 10.0f) + sinf(pos.y * sinf(iTime / freq3) * 80.0f);
  color *= sinf(iTime / 10.0f) * 0.5f;

  float indexFloat = paletteFloat + color * 32.0f;
  int idx = ((int)indexFloat) & 0xFF;
  uint16_t color565 = palette[idx];

  uint8_t r5 = (color565 >> 11) & 0x1F;
  uint8_t g6 = (color565 >> 5) & 0x3F;
  uint8_t b5 = color565 & 0x1F;

  r = r5 / 31.0f;
  g = g6 / 63.0f;
  b = b5 / 31.0f;
}

void computeShaderPixelAvalon(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  Vec2 uv = {
    (float)x / iResolution.x,
    (float)y / iResolution.x  // maintain 1:1 aspect ratio across screen
  };

  // Center UV coordinates around (0.5, 0.5)
  uv.x -= 0.5f;
  uv.y -= 0.5f;

  // Apply time-based rotation around center
  float angle = iTime * rotationSpeedf;
  float ca = cosf(angle);
  float sa = sinf(angle);
  float rx = uv.x * ca - uv.y * sa;
  float ry = uv.x * sa + uv.y * ca;
  uv.x = rx + 0.5f;
  uv.y = ry + 0.5f;

  uv.x *= patternScale;
  uv.y *= patternScale;

  // Normalized sine
  auto normsin = [](float x) {
    return (sinf(x) + 1.0f) * 0.5f;
  };

  // Interpolate between two values
  auto interpolate = [](float x, float min_x, float max_x) {
    return x * max_x + (1.0f - x) * min_x;
  };

  // Use smoothly updated frequency and phase values
  float val = normsin(
    freq1 * uv.x +
    interpolate(normsin(freq2 * uv.y + phaseShift1), 3.0f, 6.0f) +
    freq1 * uv.y +
    interpolate(normsin(freq3 * uv.x + phaseShift2), 2.0f, 6.0f) +
    iTime * 2.0f
  );

  // Palette lookup
  float indexFloat = paletteFloat + val * 64.0f;
  int idx = ((int)indexFloat) & 0xFF;
  uint16_t color565 = palette[idx];

  uint8_t r5 = (color565 >> 11) & 0x1F;
  uint8_t g6 = (color565 >> 5) & 0x3F;
  uint8_t b5 = color565 & 0x1F;

  r = r5 / 31.0f;
  g = g6 / 63.0f;
  b = b5 / 31.0f;
}

void computeShaderPixelBeam(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  Vec2 uv = {
    (float)x / iResolution.x,
    (float)y / iResolution.y
  };

  // Centered y-position
  uv.y -= 0.9f;

  // Wavy vertical distortion
  uv.y += sinf(uv.x * 107979.0f + iTime) * 0.4f;

  // Intensity based on vertical distance
  float vertColor = 1.2f - (fminf(fabsf(uv.y), 1.01f) * 100.0f);
  vertColor = fmaxf(0.0f, vertColor); // Clamp to avoid negative

  // Final color: greenish beam
  float indexFloat = paletteFloat + vertColor * 96.0f;
  int idx = ((int)indexFloat) & 0xFF;
  uint16_t color565 = palette[idx];

  r = ((color565 >> 11) & 0x1F) / 31.0f;
  g = ((color565 >> 5) & 0x3F) / 63.0f;
  b = (color565 & 0x1F) / 31.0f;
}



void shaderEffectGenerator() {
  if (++caFrameCounter >= CA_UPDATE_INTERVAL) {
    caFrameCounter = 0;
    updateCA();  // step the automaton
  }

  patternScale += (targetPatternScale - patternScale) * 0.01f;

  static uint32_t lastChange = 0;
  static uint32_t changeIntervalMs = 6000;  // initial interval
  if (millis() - lastChange > 6000) {
    lastChange = millis();
    changeIntervalMs = 6000 + random(6001);  // 6000–12000 ms

    targetFreq1 = 8.0f + random(40) * 0.1f;
    targetFreq2 = 8.0f + random(40) * 0.1f;
    targetFreq3 = 8.0f + random(40) * 0.1f;

    targetPhaseShift1 = random(1000) * 0.001f;
    targetPhaseShift2 = random(1000) * 0.001f;

    targetHueSpinSpeed = 0.05f + random(20) * 0.005f;

    targetRotationSpeed = 0.05f + random(5) * 0.01f;

    targetPatternScale = 0.7f + random(600) * 0.01f;  // Range: 0.7 to 6.7
  }

  // Smooth transitions (once per frame)
  float lerpFactor = 0.01f;  // smoothing factor

  freq1 += (targetFreq1 - freq1) * lerpFactor;
  freq2 += (targetFreq2 - freq2) * lerpFactor;
  freq3 += (targetFreq3 - freq3) * lerpFactor;

  phaseShift1 += (targetPhaseShift1 - phaseShift1) * lerpFactor;
  phaseShift2 += (targetPhaseShift2 - phaseShift2) * lerpFactor;

  rotationSpeedf += (targetRotationSpeed - rotationSpeedf) * lerpFactor;

  static uint32_t iTimeBase = 0;
  static uint32_t timeResetAt = millis();

  if (millis() - timeResetAt > 150000) {  // 5 minutes
    timeResetAt = millis();
    iTimeBase = millis();
  }

  float rawTime = (millis() - iTimeBase) * 0.001f;
  float iTime = fmodf(rawTime, 1000.0f);  // Wrap time to avoid sinf/cosf overflow
  rotationAngle = fmodf(iTime * rotationSpeedf, 6.28318f);  // Wrap rotation to [0, 2π]

  Vec2 iResolution = { (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT };

  for (int y = 0; y <= LOW_H / 2; y++) {
    for (int x = 0; x <= LOW_W / 2; x++) {
      float r, g, b;
      switch (currentShader) {
        case SHADER_NEBULA:
          computeShaderPixelNebula(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
          break;
        case SHADER_LIFE:
          computeShaderPixelLife(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
          break;
        // case SHADER_SWIRL:
        //   computeShaderPixelSwirlTunnel(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
        //   break;
        case SHADER_CAUSTIC:
          computeShaderPixelCaustic(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
          break;
        // case SHADER_ACID_VISION:
        //   computeShaderPixelAcidVision(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
        //   break;
        case SHADER_ACID_TUNNEL:
          computeShaderPixelAcidPaletteTunnel(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
          break;
        case SHADER_BIOHAZARD:
          computeShaderPixelBiohazard(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
          break;
        case SHADER_AVALON:
          computeShaderPixelAvalon(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
          break;
        // case SHADER_TEST:
        //   computeShaderPixelBeam(x * DOWNSCALE_FACTOR, y * DOWNSCALE_FACTOR, iTime, iResolution, r, g, b);
        //   break;
      }
      
      uint16_t color = tft.color565((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));

      lowResBuffer[y * LOW_W + x] = color;  // Only top-left quadrant filled
    }
    if ((y & 7) == 0) vTaskDelay(1);
  }

  updatePaletteShift(1.0f, false, true);
}

void shaderRender() {
  const int radiusLimit = 121;
  const int radiusSq = radiusLimit * radiusLimit;

  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    int dy = y - CENTER_Y;
    int srcY = abs(dy) / DOWNSCALE_FACTOR;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
      int dx = x - CENTER_X;

      if (dx*dx + dy*dy <= radiusSq) {
        int srcX = abs(dx) / DOWNSCALE_FACTOR;
        if (srcX >= LOW_W / 2) srcX = LOW_W / 2 - 1;
        if (srcY >= LOW_H / 2) srcY = LOW_H / 2 - 1;
        fractalFramebuffer[y * SCREEN_WIDTH + x] = lowResBuffer[srcY * LOW_W + srcX];
      } else {
        fractalFramebuffer[y * SCREEN_WIDTH + x] = 0;
      }
    }
    if ((y & 31) == 0) vTaskDelay(1);
  }

  if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, fractalFramebuffer);
    xSemaphoreGive(displayMutex);
  }
}

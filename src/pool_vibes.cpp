#include "shared_config.h"
#include "shader.h"
#include <math.h>

struct Vec3 { float x, y, z; };
typedef float float3x3[3][3];
struct Vec2 { float x, y; };

inline float smoothstep(float edge0, float edge1, float x) {
  float t = fminf(fmaxf((x - edge0) / (edge1 - edge0), 0.0f), 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

#define HALF_W (SCREEN_WIDTH / 2)
#define HALF_H (SCREEN_HEIGHT / 2)

#define HALF_BUFFER_SIZE (HALF_W * HALF_H)
uint16_t halfResBuffer[HALF_BUFFER_SIZE];  // use this instead of renderFB

float poolCausticPattern(Vec3 p) {
  float d = 1.0f;
  float3x3 r = {
    {-0.6f, -0.3f, 0.6f},
    { 0.9f, -0.6f, 0.3f},
    { 0.3f,  0.6f, 0.6f}
  };

  for (int i = 0; i < 3; i++) {
    Vec3 temp = {
      r[0][0]*p.x + r[0][1]*p.y + r[0][2]*p.z,
      r[1][0]*p.x + r[1][1]*p.y + r[1][2]*p.z,
      r[2][0]*p.x + r[2][1]*p.y + r[2][2]*p.z
    };
    p = temp;

    float fx = p.x - floorf(p.x) - 0.5f;
    float fy = p.y - floorf(p.y) - 0.5f;
    float fz = p.z - floorf(p.z) - 0.5f;

    float dist = sqrtf(fx * fx + fy * fy + fz * fz);
    d = fminf(d, dist);
  }
  return d;
}

void computePoolShaderPixelCaustic(int x, int y, float iTime, Vec2 iResolution, float& r, float& g, float& b) {
  Vec2 uv = {
    (float)x / iResolution.x,
    (float)y / iResolution.y
  };

  const float fixedScale = 2.5f;
  uv.x *= fixedScale;
  uv.y *= fixedScale;

  // Add lighter-weight ripple distortion
  float rippleX = sinf(uv.y * 4.0f + iTime * 0.7f);
  float rippleY = cosf(uv.x * 4.0f - iTime * 0.6f);
  uv.x += 0.02f * rippleX;
  uv.y += 0.02f * rippleY;

  Vec3 p = {
    uv.x + iTime * 0.4f,
    uv.y + iTime * 0.4f,
    iTime * 0.4f
  };

  float c = poolCausticPattern(p);
  //  float intensity = c * c;
   float intensity = c * c * (1.5f - 0.5f * c);

  // Teal base and warm highlights
  float baseR = 0.0f;
  float baseG = 0.55f;
  float baseB = 0.5f;

  float waveR = 1.0f;
  float waveG = 1.0f;
  float waveB = 0.88f;

  r = baseR + intensity * (waveR - baseR);
  g = baseG + intensity * (waveG - baseG);
  b = baseB + intensity * (waveB - baseB);
}

static uint32_t iTimeBase = 0;
static uint32_t timeResetAt = millis();

void poolShaderEffectGenerator() {
  if (millis() - timeResetAt > 200000) {  // 5 minutes
    timeResetAt = millis();
    iTimeBase = millis();
  }

  float rawTime = (millis() - iTimeBase) * 0.001f;
  float iTime = fmodf(rawTime, 1000.0f);
  Vec2 iResolution = { (float)HALF_W, (float)HALF_H };

  for (int y = 0; y < HALF_H; y++) {
    for (int x = 0; x < HALF_W; x++) {
      float r, g, b;
      computePoolShaderPixelCaustic(x, y, iTime, iResolution, r, g, b);
      halfResBuffer[y * HALF_W + x] = tft.color565((uint8_t)(r * 255), (uint8_t)(g * 255), (uint8_t)(b * 255));
    }
    if ((y & 7) == 0) vTaskDelay(1);
  }
}

void poolShaderRender() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    int sy = y >> 1;
    uint16_t* src = &halfResBuffer[sy * HALF_W];
    uint16_t* dst = &fractalFramebuffer[y * SCREEN_WIDTH];
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      dst[x] = src[x >> 1];
    }
    if ((y & 31) == 0) vTaskDelay(1);
  }

  if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, fractalFramebuffer);
    xSemaphoreGive(displayMutex);
  }
}


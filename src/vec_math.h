#ifndef VEC_MATH_H
#define VEC_MATH_H

#include <math.h>

// Vector type definitions
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// --- Vec2 operations ---
inline Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
inline Vec2 operator*(Vec2 a, float s) { return { a.x * s, a.y * s }; }
inline Vec2 operator*(float s, Vec2 a) { return a * s; }
inline Vec2 operator/(Vec2 a, float s) { return { a.x / s, a.y / s }; }
inline Vec2 operator+=(Vec2 &a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
inline Vec2 operator*=(Vec2 &a, float s) { a.x *= s; a.y *= s; return a; }

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }
inline float distance(Vec2 a, Vec2 b) { return length(a - b); }

inline Vec2 sin(Vec2 v) { return { sinf(v.x), sinf(v.y) }; }
inline Vec2 floor(Vec2 v) { return { floorf(v.x), floorf(v.y) }; }
inline Vec2 fract(Vec2 v) { return { v.x - floorf(v.x), v.y - floorf(v.y) }; }
inline Vec2 min(Vec2 a, Vec2 b) { return { fminf(a.x, b.x), fminf(a.y, b.y) }; }

inline Vec2 rotate(Vec2 v, float angle) {
  float c = cosf(angle), s = sinf(angle);
  return { v.x * c - v.y * s, v.x * s + v.y * c };
}

// --- Vec3 operations ---
inline Vec3 operator+(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 operator*(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline Vec3 operator*(float s, Vec3 a) { return a * s; }
inline Vec3 operator/(Vec3 a, float s) { return { a.x / s, a.y / s, a.z / s }; }
inline Vec3 operator/(Vec3 a, Vec3 b) { return { a.x / b.x, a.y / b.y, a.z / b.z }; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 v) { return sqrtf(dot(v, v)); }

inline Vec3 sin(Vec3 v) { return { sinf(v.x), sinf(v.y), sinf(v.z) }; }
inline Vec3 cos(Vec3 v) { return { cosf(v.x), cosf(v.y), cosf(v.z) }; }

inline Vec3 normalize(Vec3 v) {
  float l = sqrtf(dot(v, v));
  return { v.x / l, v.y / l, v.z / l };
}

typedef float float3x3[3][3];

inline float fract(float v) { return v - floorf(v); }

inline float fractf(float x) { return x - floorf(x); }

// --- Vec4 operations ---
inline Vec4 operator+(Vec4 a, float s) { return { a.x + s, a.y + s, a.z + s, a.w + s }; }
inline Vec4 operator+(Vec4 a, Vec4 b) { return { a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w }; }
inline Vec4 operator/(Vec4 a, float s) { return { a.x / s, a.y / s, a.z / s, a.w / s }; }

inline Vec4 sin(Vec4 v) { return { sinf(v.x), sinf(v.y), sinf(v.z), sinf(v.w) }; }
inline Vec4 cos(Vec4 v) { return { cosf(v.x), cosf(v.y), cosf(v.z), cosf(v.w) }; }
inline Vec4 tanh(Vec4 v) { return { tanhf(v.x), tanhf(v.y), tanhf(v.z), tanhf(v.w) }; }

// --- Utility functions ---
inline float minf(float a, float b) { return fminf(a, b); }

inline float smoothstep(float edge0, float edge1, float x) {
  float t = fminf(fmaxf((x - edge0) / (edge1 - edge0), 0.0f), 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

Vec2 rotateUV(Vec2 uv, float angle) {
  float c = cosf(angle);
  float s = sinf(angle);
  return {
    uv.x * c - uv.y * s,
    uv.x * s + uv.y * c
  };
}

#endif // VEC_MATH_H

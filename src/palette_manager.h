// palette_manager.h
#pragma once

#include <stdint.h>

struct ColorProfile {
  float phaseR, phaseG, phaseB;
};

extern int paletteSpacing;
extern int paletteDirection;
extern float paletteFloat;

void initPalette();
void generateShiftedPalette(float offset);
void pickNewFadeProfile(bool isOutrun);
void updatePaletteShift(float zoom, bool isOutrun = false, bool isShader = false);

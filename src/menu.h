// menu.h
#pragma once

#include <Arduino.h>

void renderMenuFrame(uint32_t now);
void drawMenu(uint32_t now);
void renderMenu(void);
void handleMenuTouch(float touchX, float touchY, String gesture, uint32_t now);
void disableTouchpadWakeup(void);
void drawPowerButton(void);
void updatePowerButtonAnimation(void);
void handleSwipe(String direction, uint32_t now);
void drawNavigationArrows(void);
void drawFractalButton(void);
void drawVisButton(void);
void drawHexagonLogo(void);
void drawOutrunButton(void);
void drawShaderButton(uint32_t now);
void drawDeepEndButton(uint32_t now);
void drawFlashlightButton(void);
void drawShuffleButton(void);

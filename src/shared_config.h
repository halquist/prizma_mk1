#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "CST816S.h"

extern CST816S touch;

// for switching between menu and fractal mode
enum RenderMode { MODE_MENU, MODE_FRACTAL, MODE_VISUALIZER, MODE_OUTRUN, MODE_SHADER, MODE_DEEP_END, MODE_FLASHLIGHT };

extern RenderMode currentMode;

// Screen and framebuffer settings
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define CENTER_X (SCREEN_WIDTH / 2)
#define CENTER_Y (SCREEN_HEIGHT / 2)
#define CANVAS_WIDTH SCREEN_WIDTH * 3
#define CANVAS_HEIGHT SCREEN_HEIGHT * 3
#define CANVAS_CENTER_X (CANVAS_WIDTH / 2)
#define CANVAS_CENTER_Y (CANVAS_HEIGHT / 2)
#define PALETTE_SIZE 256
#define MIN_ITER 20
#define MAX_ITER 100

#ifndef POINT_STRUCT_DEFINED
#define POINT_STRUCT_DEFINED

void shuffle(void);

struct Point {
  float x;
  float y;
};

#endif

// viewport coordinates
extern int viewX;
extern int viewY;

// Framebuffers
extern uint8_t* fb1;
extern uint8_t* fb2;
extern uint8_t* renderFB;
extern uint8_t* displayFB;
extern uint16_t* menuFramebuffer;
// Full-screen RGB565 buffer shared by fractal/shader/pool render passes.
// Allocated in PSRAM; written by consumer task, pushed to display in one SPI transaction.
extern uint16_t* fractalFramebuffer;


// DMA and rendering semaphores
extern SemaphoreHandle_t frameReadySemaphore;
// Producer waits on this; consumer signals when display pass is complete.
extern SemaphoreHandle_t renderDoneSemaphore;
extern SemaphoreHandle_t displayMutex;

// Task handle
extern TaskHandle_t producerHandle;
extern TaskHandle_t consumerHandle;

extern volatile bool rendererActive;
extern bool fractalWasPaused;

// Palette data
extern uint16_t palette[PALETTE_SIZE];

extern bool flashlightActive;

extern bool shuffleActive;

// Battery bar
extern bool showMenu;
extern bool showVis;
extern bool menuSpriteReady;
extern int batteryBarMaskWidth;
// extern int showMenuDelay;
// extern int showMenuDelayLength;
extern int32_t batteryPercent;

extern bool powerButtonPressed;
extern bool screenTouched;
extern bool touchCooldownActive;

extern float MAX_ZOOM;
extern float MIN_ZOOM;
extern float ITER_SCALE;

extern float targetCenterX;
extern float targetCenterY;

extern float rotationAngle;

extern uint8_t* clockMask;
extern int clockMaskX;
extern int clockMaskY;
extern int clockMaskH;
extern int clockMaskW;

// Display object
extern TFT_eSPI tft;

// menu sprite object
extern TFT_eSprite menuSprite;
extern TFT_eSprite visSprite;
extern TFT_eSprite outrunSprite;

// Shared RGB line buffers
extern uint16_t rgbLineTop[SCREEN_WIDTH];
extern uint16_t rgbLineBottom[SCREEN_WIDTH];

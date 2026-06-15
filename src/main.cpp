// main.ino
#include <TFT_eSPI.h>
#include <SPI.h>
#include "DEV_Config.h"
#include "LCD_1in28.h"
#include "QMI8658.h"
#include "CST816S.h"
#include <stdlib.h> // malloc() free()
#include "bootloader_random.h"
#include "shared_config.h"
#include "palette_manager.h"
#include "battery_display.h"
#include "fractal_renderer.h"
#include "fractal_region_detection.h"
#include "menu.h"
#include "renderer.h"
#include "fps_monitor.h"
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include "visualizers.h"
#include "outrun.h"
#include "shader.h"
#include "driver/uart.h"

// disables USB CDC logging
#define USB_CDC_DISABLED

TFT_eSPI tft = TFT_eSPI();  // <-- this **defines** the tft object

RenderMode currentMode = MODE_MENU;
int touchDelay = 0;

bool shuffleActive = false;
const unsigned long SHUFFLE_INACTIVITY_TIMEOUT = 15000; // 15 seconds
bool shuffleTriggered = false;
static unsigned long lastShuffleTime = 0;
const unsigned long shuffleInterval = 20000;  // 20 seconds, adjust as needed

// Define global variables exactly once here
uint8_t* fb1;
uint8_t* fb2;
uint8_t* renderFB;
uint8_t* displayFB;
uint16_t* menuFramebuffer;
uint16_t* fractalFramebuffer;

SemaphoreHandle_t frameReadySemaphore;
SemaphoreHandle_t renderDoneSemaphore;
SemaphoreHandle_t displayMutex;

SemaphoreHandle_t workerStartSem;
SemaphoreHandle_t workerDoneSem;
FractalWorkerParams workerParams;

TaskHandle_t producerHandle = NULL;
TaskHandle_t consumerHandle = NULL;

uint16_t rgbLineTop[SCREEN_WIDTH];
uint16_t rgbLineBottom[SCREEN_WIDTH];

unsigned long lastTouchTime = 0;
unsigned long cooldownTouchTime = 0;
const int touchCooldown = 1000;  // in milliseconds
bool touchCooldownActive = false;
bool flashlightActive = false;

volatile bool rendererActive = false;

unsigned long lastDraw = 0;
const int frameInterval = 16; // lower is higher FPS

CST816S touch(6, 7, 13, 5);	// sda, scl, rst, irq

void exitToMenu(unsigned long now);

void handleInactivity(unsigned long now) {
  if (!shuffleTriggered && now - lastTouchTime > SHUFFLE_INACTIVITY_TIMEOUT) {
    shuffleTriggered = true;
    shuffleActive = true;
  }
}

void handleShuffle(unsigned long now) {
  if (shuffleActive && now - lastShuffleTime > shuffleInterval) {
    lastShuffleTime = now;
    shuffle();
  }
}

void shuffle() {
  int options[] = {MODE_FRACTAL, MODE_FRACTAL, MODE_SHADER, MODE_SHADER, MODE_SHADER, MODE_VISUALIZER, MODE_DEEP_END};
  int selected = options[random(0, 7)];
  currentMode = (RenderMode)selected;

  if (selected == MODE_SHADER) randomizeShader();
  else if (selected == MODE_VISUALIZER) randomizeVisualizer();

  rendererActive = (selected == MODE_FRACTAL || selected == MODE_DEEP_END || selected == MODE_SHADER);

  fractalWasPaused = true;
}

void handleTouchInput(unsigned long now) {
  if (!touch.available()) return;

  String gesture = touch.gesture();
  lastTouchTime = now;

  switch (currentMode) {
    case MODE_OUTRUN:
      handleOutrunTouch(touch.data.x, touch.data.y, gesture);
      break;

    case MODE_MENU:
      handleMenuTouch(touch.data.x, touch.data.y, gesture, now);
      break;

    case MODE_VISUALIZER:
      if (touchCooldownActive) break;
      if (gesture == "LONG PRESS") exitToMenu(now);
      else if (gesture == "SWIPE UP")  { touchCooldownActive = true; cycleVisualizer(); }
      else if (gesture == "SWIPE DOWN"){ touchCooldownActive = true; cycleVisualizer(true); }
      break;

    case MODE_FRACTAL:
      if (touchCooldownActive) break;
      if (gesture == "LONG PRESS") {
        exitToMenu(now);
        rendererActive = false;
        fractalWasPaused = true;
      }
      break;

    case MODE_SHADER:
      if (touchCooldownActive) break;
      if (gesture == "LONG PRESS") {
        exitToMenu(now);
        rendererActive = false;
      }
      else if (gesture == "SWIPE UP")   { touchCooldownActive = true; cycleShader(); }
      else if (gesture == "SWIPE DOWN") { touchCooldownActive = true; cycleShader(true); }
      break;

    case MODE_FLASHLIGHT:
    case MODE_DEEP_END:
      if (touchCooldownActive) break;
      if (gesture == "LONG PRESS") {
        exitToMenu(now);
        flashlightActive = false;
        rendererActive = false;
      }
      break;
  }
}

void exitToMenu(unsigned long now) {
  currentMode = MODE_MENU;
  shuffleActive = false;
  shuffleTriggered = false;
  lastShuffleTime = now;
}

void renderActiveMode(unsigned long now) {
  // Throttle to 10 fps during shuffle (menu idle) — display isn't being interacted with
  // so CPU cycles spent rendering are wasted. Normal interactive modes stay at ~60 fps.
  unsigned long interval = (shuffleActive && currentMode == MODE_MENU)
                           ? 100UL
                           : (unsigned long)frameInterval;
  if (millis() - lastDraw < interval) return;

  switch (currentMode) {
    case MODE_MENU:
      renderMenuFrame(now);
      if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        menuSprite.pushSprite(-viewX, -viewY);
        xSemaphoreGive(displayMutex);
      }
      break;

    case MODE_VISUALIZER:
      renderVisualizerFrame();
      if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        visSprite.pushSprite(0, 0);
        xSemaphoreGive(displayMutex);
      }
      break;

    case MODE_OUTRUN:
      outrunVisualizerLoop();
      if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        outrunSprite.pushSprite(0, 0);
        xSemaphoreGive(displayMutex);
      }
      break;

    case MODE_FLASHLIGHT:
      flashlightActive = true;
      renderMenuFrame(now);
      if (xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        menuSprite.pushSprite(-viewX, -viewY);
        xSemaphoreGive(displayMutex);
      }
      break;
  }

  lastDraw = millis();
  // Fractal/shader/deep_end FPS is measured in consumerTask after a full frame is displayed.
  if (currentMode != MODE_FRACTAL && currentMode != MODE_SHADER && currentMode != MODE_DEEP_END) {
    fpsMonitorTick(currentMode);
  }
}

const int unusedPins[] = {
  15, 16, 17, 18, 21, 33
};

void configureUnusedGPIOs() {
  for (int i = 0; i < sizeof(unusedPins)/sizeof(unusedPins[0]); i++) {
    int pin = unusedPins[i];

    // Option 1: Drive LOW
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    // Option 2: Pull-down input (if you're not sure about driving outputs)
    // pinMode(pin, INPUT_PULLDOWN);
  }
}

void disableUART0() {
  uart_driver_delete(UART_NUM_0);  // Remove UART0 driver
  gpio_reset_pin(GPIO_NUM_1);      // TX
  gpio_reset_pin(GPIO_NUM_3);      // RX
  gpio_set_direction(GPIO_NUM_1, GPIO_MODE_DISABLE);
  gpio_set_direction(GPIO_NUM_3, GPIO_MODE_DISABLE);
}

void setup() {
#ifdef FPS_MONITOR
  Serial.begin(115200);
  delay(100);
  Serial.println("Prizma FPS monitor enabled");
  fpsMonitorReset();
#endif

  randomSeed(esp_random());

  // power saving disable functions:
  configureUnusedGPIOs();
#ifndef FPS_MONITOR
  disableUART0();
#endif

  findFractalEdgePoint(0);
  beginPanToTarget(false);
  initPalette();
  touch.begin();

  // Check if wakeup was from touchpad
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
    disableTouchpadWakeup();
  }

  initSineTable();
  seedCA();
  initSwirlPhaseVars();
  initAcidShader();
  initShaderSeeds();
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(CC_DATUM);
  tft.setSwapBytes(true);

  fb1 = (uint8_t*)heap_caps_malloc((SCREEN_HEIGHT / 2) * (SCREEN_WIDTH / 2), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  fb2 = (uint8_t*)heap_caps_malloc((SCREEN_HEIGHT / 2) * (SCREEN_WIDTH / 2), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  menuFramebuffer = (uint16_t*) heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
  fractalFramebuffer = (uint16_t*) heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

  if (!fb1 || !fb2 || !menuFramebuffer || !fractalFramebuffer) {
    Serial.println("Framebuffer allocation failed!");
    while (1) delay(1000);
  }

  renderFB  = fb1;
  displayFB = fb2;

  frameReadySemaphore = xSemaphoreCreateBinary();
  renderDoneSemaphore = xSemaphoreCreateBinary();
  displayMutex = xSemaphoreCreateMutex();
  workerStartSem = xSemaphoreCreateBinary();
  workerDoneSem  = xSemaphoreCreateBinary();
  // Producer may begin the first frame immediately.
  xSemaphoreGive(renderDoneSemaphore);

  menuSprite.createSprite(CANVAS_WIDTH, CANVAS_HEIGHT);
  menuSprite.fillSprite(TFT_BLACK);
  menuSprite.setTextColor(TFT_WHITE);
  menuSprite.setTextDatum(CC_DATUM);

  visSprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  visSprite.fillSprite(TFT_BLACK);

  outrunSprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  outrunSprite.fillSprite(TFT_BLACK);

  // Producer and consumer on Core 1 (Arduino default). Worker on Core 0 so both
  // cores compute Mandelbrot lines in parallel during fractal generation.
  xTaskCreatePinnedToCore(producerTask,      "Producer",      8192, NULL, 2, &producerHandle, 1);
  xTaskCreatePinnedToCore(consumerTask,      "Consumer",      4096, NULL, 1, &consumerHandle, 1);
  xTaskCreatePinnedToCore(fractalWorkerTask, "FractalWorker", 4096, NULL, 2, NULL,            0);
  xTaskCreate(batteryMonitorTask, "BatteryMonitor", 2048, NULL, 1, NULL);
}

void loop() {
  unsigned long now = millis();

  if (now - cooldownTouchTime > touchCooldown) {
    batteryPercent = battery_level_percent();
    touchCooldownActive = false;
    cooldownTouchTime = now;
  }

  handleTouchInput(now);
  renderActiveMode(now);
  if (currentMode == MODE_MENU) {
    handleInactivity(now);
  }
  handleShuffle(now);
}

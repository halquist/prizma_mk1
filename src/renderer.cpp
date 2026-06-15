#include "renderer.h"
#include "shared_config.h"
#include "fractal_renderer.h"
#include "menu.h"
#include "shader.h"
#include "pool_vibes.h"
#include "fps_monitor.h"

void swapFramebuffers() {
  uint8_t* temp = renderFB;
  renderFB = displayFB;
  displayFB = temp;
}

void producerTask(void* pvParameters) {
  for (;;) {
    if (rendererActive) {
      // Wait until the consumer has finished displaying the previous frame.
      xSemaphoreTake(renderDoneSemaphore, portMAX_DELAY);
      if (!rendererActive) {
        xSemaphoreGive(renderDoneSemaphore);
        continue;
      }
      switch (currentMode) {
        case MODE_FRACTAL:
          mandelbrotGenerator();
          break;
        case MODE_SHADER:
          shaderEffectGenerator();
          break;
        case MODE_DEEP_END:
          poolShaderEffectGenerator();
          break;
      }
      swapFramebuffers();
      xSemaphoreGive(frameReadySemaphore);
    } else {
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}


void consumerTask(void* pvParameters) {
  for (;;) {
    if (rendererActive) {
      if (xSemaphoreTake(frameReadySemaphore, portMAX_DELAY)) {
        switch (currentMode) {
          case MODE_FRACTAL:
            fractalRender();
            break;
          case MODE_SHADER:
            shaderRender();
            break;
          case MODE_DEEP_END:
            poolShaderRender();
            break;
        }
        fpsMonitorTick(currentMode);
        xSemaphoreGive(renderDoneSemaphore);
      }
    } else {
      // Unblock producer if it was waiting when rendering was stopped.
      xSemaphoreGive(renderDoneSemaphore);
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}
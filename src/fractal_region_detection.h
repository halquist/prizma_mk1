// fractal_renderer.h
#pragma once
#include <Arduino.h>

void findFractalEdgePoint(int retryDepth = 0);

// Set to true to trigger a new background region search.
// The RegionSearch task clears the flag and updates targetCenterX/Y when done.
extern volatile bool regionSearchPending;

// FreeRTOS task that runs region searches in the background at priority 0.
void regionSearchTask(void* pvParameters);

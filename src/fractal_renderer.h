// fractal_renderer.h
#pragma once
#include <stdint.h>

void mandelbrotGenerator(void);
void fractalWorkerTask(void* pvParameters);
void fractalRender(void);
int mandelbrotPoint(float real, float imag, int iterCap);
void mandelbrotLine(uint8_t* line, int y, float cx, float cy,
                    int iterCap, int radiusLimit,
                    float cosA, float sinA,
                    float dreal, float dimag,
                    float anchorX, float anchorY,
                    float invHalfW, float invHalfH, float zoomFactor);
void beginPanToTarget(bool transitionRotationAnchor);

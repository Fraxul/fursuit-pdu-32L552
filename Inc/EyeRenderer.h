#pragma once
#include "WS2812Driver.h"
#include "eye_layout.h"
#include "Shell.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

using glm::vec2;
using glm::vec3;

class EyeRenderer : public WS2812Pattern {
public:
  EyeRenderer();
  ~EyeRenderer();

  virtual uint32_t initFrame();
  virtual glm::u8vec3 shadePixel(uint8_t channel, uint16_t pixelIdx);

  vec3 bgColorHSV = vec3(0.055f, 1.0f, 0.095f);
  vec3 irisColorHSV = vec3(0.055f, 0.675f, 0.25f);
  vec3 pupilColorHSV = vec3(0.055f, 0.675f, 0.005f);
  vec2 pupilSize = vec2(5.0f, 20.0f); // Ellipse
  float pupilBlend = 0.5f;

  float ledPitch = 3.0f; // millimeters

  // rough display center coordinates. units are millimeters
  vec2 pupilCenter[2] = {boundsCenter, boundsCenter};
  vec2 pupilMaxDisplacement = vec2(25, 10);

  float pupilMinSpeed = 100.0f; // mm/sec, for constant speed movement. sets a lower bound
  float pupilMaxMovementTime = 0.125f; // seconds, for constant time movement

  float movementTimeRangeMin = 4.0f;
  float movementTimeRangeMax = 8.0f;

  float blinkTimeRangeMin = 6.0f;
  float blinkTimeRangeMax = 10.0f;
  float blinkDuration = 0.2f; // seconds, full cycle (including dwell time)
  float blinkDwell = 0.05f; // seconds, dwell time at full blink

  // computed state
  vec2 targetPupilCenter[2] = {boundsCenter, boundsCenter};
  float currentMovementSpeed[2] = {0, 0};
  vec2 movementDirection[2];
  float distanceToGo[2] = {0, 0};

  float nextMovementTimer = movementTimeRangeMin;
  float nextBlinkTimer = blinkTimeRangeMin;
  float blinkActiveTimer = (blinkDuration - blinkDwell) * 0.5f;

  vec3 modelMin, modelMax; // axis-aligned bounding box, copied from FrameInfo for use in shader

  // delta-T tracking
  uint32_t lastTickMajor = 0; // full 32-bit value -- milliseconds.
};

#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "main.h"
#include "WS2812Driver.h"
#include "EyeRenderer.h"
#include "eye_layout.h"
#include "Shell.h"

#include "FreeRTOS.h"
#include "task.h"
#include <glm/common.hpp>
#include <glm/gtx/color_space.hpp>

#include "arm_math.h"
#include "arm_neon.h"


static const char* ushNodePath = "/display";
static struct ush_node_object ushNode;
EyeRenderer* s_inst = nullptr;

static size_t vec3_getData(const vec3& v, uint8_t** data) {
  *data = ushFileScratchBuf();
  return snprintf((char*)*data, ushFileScratchLen, "%.3f %.3f %.3f\n",
    v.x, v.y, v.z);
}
static bool vec3_setData(struct ush_object* self, vec3& v, uint8_t* data, size_t size) {
  char* end = nullptr;
  char* p = (char*) data;
  data[size] = '\0'; // ensure data is null-terminated
  float f[3];
  for (int i = 0; i < 3; ++i) {
    f[i] = strtof(p, &end);
    if (p == end) {
      ush_print_status(self, USH_STATUS_ERROR_COMMAND_SYNTAX_ERROR);
      return false; // parse error
    }
    p = end;
  }

  // parse OK
  for (int i = 0; i < 3; ++i) {
    v[i] = f[i];
  }
  return true;
}

static const struct ush_file_descriptor ush_files[] = {
  { .name = "bgColor", .description = "background color (HSV, 0-1)", .help = nullptr, .exec = nullptr,
    .get_data = [](struct ush_object* self, struct ush_file_descriptor const* file, uint8_t** data) -> size_t {
      return vec3_getData(s_inst->bgColorHSV, data);
    },
    .set_data = [](struct ush_object* self, struct ush_file_descriptor const* file, uint8_t* data, size_t size) {
      vec3_setData(self, s_inst->bgColorHSV, data, size);
    }},
  {.name = "irisColor", .description = "pupil color (HSV, 0-255)", .help = nullptr, .exec = nullptr,
    .get_data = [](struct ush_object* self, struct ush_file_descriptor const* file, uint8_t** data) -> size_t {
      return vec3_getData(s_inst->irisColorHSV, data);
    },
    .set_data = [](struct ush_object* self, struct ush_file_descriptor const* file, uint8_t* data, size_t size) {
      vec3_setData(self, s_inst->irisColorHSV, data, size);
    }},
};

EyeRenderer::EyeRenderer() {
  // Critical section to protect modification of ush internals which belong to a different task
  taskENTER_CRITICAL();
  s_inst = this;
  ush_node_mount(ushInstance(), ushNodePath, &ushNode, ush_files, sizeof(ush_files) / sizeof(ush_files[0]));
  taskEXIT_CRITICAL();

}

EyeRenderer::~EyeRenderer() {
  // Critical section to protect modification of ush internals which belong to a different task
  taskENTER_CRITICAL();
  ush_node_unmount(ushInstance(), ushNodePath);
  s_inst = nullptr;
  taskEXIT_CRITICAL();
}


#ifdef HAL_RNG_MODULE_ENABLED // STM32L4 series, or anything with hardware RNG
extern RNG_HandleTypeDef hrng;
uint32_t randomU32() {
  uint32_t res;
  HAL_RNG_GenerateRandomNumber(&hrng, &res);
  return res;
}
#else // STM32F1 series, or anything else without hardware RNG
uint16_t lfsr_state = 0xACE1u; // must be nonzero.
uint16_t lfsr_next() {
  // 7,9,13 triplet from http://www.retroprogramming.com/2017/07/xorshift-pseudorandom-numbers-in-z80.html
  lfsr_state ^= lfsr_state >> 7;
  lfsr_state ^= lfsr_state << 9;
  lfsr_state ^= lfsr_state >> 13;
  return lfsr_state;
}
uint32_t randomU32() {
  return (static_cast<uint32_t>(lfsr_next()) << 16) | lfsr_next();
}
#endif

static inline float randomFloat() { // 0...1
  // Using the fixed-point to floating-point conversion instruction to convert the full range of the 32-bit random int to a 0...1 float
  float res;
  asm volatile(
    "vmov %0, %1 \n"
    "vcvt.f32.u32 %0, %0, #32"
  : "=t"(res) : "r"(randomU32()) : );
  return res;
}

static inline float q15_to_float(q15_t x) {
  float res;
  asm volatile(
    "vmov %0, %1 \n"
    "vcvt.f32.s16 %0, %0, #15"
  : "=t"(res) : "r"(x) : );
  return res;
}

inline float randomFloat(float minVal, float maxVal) { return glm::mix(minVal, maxVal, randomFloat()); }
inline vec2 randomVec2() { return vec2(randomFloat(), randomFloat()); }
static inline float saturate(float f) { return glm::clamp(f, 0.0f, 1.0f); }

static inline float distanceSquared(vec2 a, vec2 b) {
  float x = b.x - a.x;
  float y = b.y - a.y;
  return ((x*x) + (y*y));
}

static inline float lengthSquared(vec2 a) {
  return dot(a, a);
}


// __attribute__((optimize("-Os")))
uint32_t EyeRenderer::initFrame() {
  uint32_t tickMajor = HAL_GetTick();

  uint32_t deltaTick;
  if (tickMajor < lastTickMajor) {
    deltaTick = tickMajor + (std::numeric_limits<uint32_t>::max() - lastTickMajor);
  } else {
    deltaTick = tickMajor - lastTickMajor;
  }
  lastTickMajor = tickMajor;

  float timeDelta = static_cast<float>(deltaTick) * 0.001f;

  uint32_t frameDelayTime = 1000; // milliseconds

  nextMovementTimer -= timeDelta;
  if (nextMovementTimer < 0) {
    uint32_t angle_0to1_q15 = randomU32() & 0x7fff;

    float sinAngle = q15_to_float(arm_sin_q15(angle_0to1_q15));
    float cosAngle = q15_to_float(arm_cos_q15(angle_0to1_q15));

    vec2 rawTargetPupilCenter = boundsCenter + vec2(cosAngle, sinAngle) * (randomFloat() * pupilMaxDisplacement);

    for (uint8_t eyeIdx = 0; eyeIdx < 2; ++eyeIdx) {
      targetPupilCenter[eyeIdx] = rawTargetPupilCenter;

      // Adjust to dampen "looking backwards" per-eye
      float deltaX = targetPupilCenter[eyeIdx].x - boundsCenter.x;
      if ((eyeIdx == 0 && deltaX > 0) ||
          (eyeIdx == 1 && deltaX < 0)) {
        targetPupilCenter[eyeIdx].x -= (deltaX * 0.5f);
      }

      movementDirection[eyeIdx] = glm::normalize(targetPupilCenter[eyeIdx] - pupilCenter[eyeIdx]);
      distanceToGo[eyeIdx] = glm::length(targetPupilCenter[eyeIdx] - pupilCenter[eyeIdx]);

      currentMovementSpeed[eyeIdx] = distanceToGo[eyeIdx] / pupilMaxMovementTime; // compute next movement speed using time constant
      currentMovementSpeed[eyeIdx] = glm::max(currentMovementSpeed[eyeIdx], pupilMinSpeed); // apply minimum speed for shorter movements
    }
    nextMovementTimer = randomFloat(movementTimeRangeMin, movementTimeRangeMax);
    //printf("angle=%fdeg targetPupilCenter=(%f, %f) timer=%f\n", glm::degrees(angle), targetPupilCenter[0], targetPupilCenter[1], nextMovementTimer);

    lastTickMajor = HAL_GetTick(); // fudge timer to avoid stutter next frame on large delta-T
    frameDelayTime = 0; // movement started, no delay
  } else {
    frameDelayTime = glm::min<uint32_t>(frameDelayTime, glm::floor(nextMovementTimer * 1000.0f));
  }

  if (blinkActiveTimer > blinkDuration) { // not blinking
    nextBlinkTimer -= timeDelta;
    if (nextBlinkTimer < 0) {
      nextBlinkTimer = randomFloat(blinkTimeRangeMin, blinkTimeRangeMax);
      blinkActiveTimer = 0; // start blink
      frameDelayTime = 0;
    } else {
      frameDelayTime = glm::min<uint32_t>(frameDelayTime, glm::floor(nextBlinkTimer * 1000.0f));
    }
  } else { // in blink
    blinkActiveTimer += timeDelta;
    frameDelayTime = 0;
  }

  for (uint8_t eyeIdx = 0; eyeIdx < 2; ++eyeIdx) {
    if(distanceToGo[eyeIdx] > 0.1f) {
      float movementDistance = glm::min(distanceToGo[eyeIdx], (currentMovementSpeed[eyeIdx] * timeDelta));
      pupilCenter[eyeIdx] += movementDirection[eyeIdx] * movementDistance;
      distanceToGo[eyeIdx] -= movementDistance;
      frameDelayTime = 0;
    }
  }

  return frameDelayTime;
}


//__attribute__((optimize("-Os")))
glm::u8vec3 EyeRenderer::shadePixel(uint8_t channel, uint16_t pixelIdx)  {
  // Look up position for this channel/pixel from the panel manufacturing data
  vec2 p = channelPoints[channelPointOffsets[channel] + pixelIdx];
  uint8_t eyeIdx = channelEyeIndices[channel];

  // Distance from center in XY-plane
  vec2 d = pupilCenter[eyeIdx] - p;
  float dist2 = glm::dot(d, d);
  float centerMix = glm::smoothstep((18.0f * 18.0f), (32.0f * 32.0f), dist2);

  vec3 hsv = bgColorHSV;
  if (centerMix < 1.0f) {
    hsv = mix(irisColorHSV, bgColorHSV, centerMix);

    // point-in-ellipse test. Point is inside ellipse if result is <= 1.0f
    float pointInPupil = ((d.x * d.x) / (pupilSize.x * pupilSize.x)) + ((d.y * d.y) / (pupilSize.y * pupilSize.y));
    float pupilMix = glm::smoothstep(1.0f - pupilBlend, 1.0f + pupilBlend, pointInPupil);
    hsv = mix(pupilColorHSV, hsv, pupilMix);
  }
  vec3 rgb = glm::rgbColor(vec3(hsv.x * 360.0f, hsv.y, hsv.z));


  if (blinkActiveTimer < blinkDuration) {
    float phaseLen = ((blinkDuration - blinkDwell) * 0.5f);
    float progress;
    if (blinkActiveTimer < phaseLen) {
      // blinking
      progress = 1.0f - glm::smoothstep(0.0f, phaseLen, blinkActiveTimer);
    } else if (blinkActiveTimer < (phaseLen + blinkDwell)) {
      // dwelling
      progress = 0.0f;
    } else {
      // retracting
      progress = glm::smoothstep(phaseLen + blinkDwell, blinkDuration, blinkActiveTimer);
    }
    float vMin = (p.y            - boundsMin.y) / ((boundsMax.y - boundsMin.y) + ledPitch);
    float vMax = (p.y + ledPitch - boundsMin.y) / ((boundsMax.y - boundsMin.y) + ledPitch);
    rgb *= glm::smoothstep(vMin, vMax, progress);
  }

  glm::u8vec3 res;
  res.r = round(saturate(rgb.r) * 255.0f);
  res.g = round(saturate(rgb.g) * 255.0f);
  res.b = round(saturate(rgb.b) * 255.0f);
  return res;
}



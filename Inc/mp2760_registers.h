#pragma once
#include <stdint.h>

// Automatically generated from MP2760GVT-XXXX.txt on Sun May  7 03:53:49 2023

struct MP2760_Register {
  uint8_t id;
  uint16_t value;
};

static constexpr uint8_t mp2760_defaultConfig_length = 16;
extern const MP2760_Register mp2760_defaultConfig[];

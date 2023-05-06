#pragma once
#include <stdint.h>

// Automatically generated from MAX17320_Config.INI on Sat May  6 17:04:48 2023

struct MAX17320_Register {
  uint16_t id;
  uint16_t value;
};

static constexpr uint8_t max17320_defaultConfig_length = 107;
extern const MAX17320_Register max17320_defaultConfig[];

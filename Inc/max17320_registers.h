#pragma once
#include <stdint.h>

// Automatically generated from MAX17320_Config.INI on Sat May  6 04:39:14 2023

struct MAX17320_Register {
  uint16_t id;
  uint16_t value;
};

static constexpr uint8_t max17320_defaultConfig_length = 112;
extern const MAX17320_Register max17320_defaultConfig[];

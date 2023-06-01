#pragma once
#include <stdint.h>

// Automatically generated from MAX17320_Config.INI on Thu Jun  1 14:33:30 2023

struct MAX17320_Register {
  uint16_t id;
  uint16_t value;
};

static constexpr uint8_t max17320_defaultConfig_length = 103;
static constexpr uint16_t max17320_nRSense = 500;

// Convert value from register 0x1c to microamps using the sense resistor value
// sense resistor is 10 microOhms per LSB
// microamps = value *  (1.5625 / (nRSense * 0.00001))
// for 5 mOhms: value * (1.5625 / 0.005 == 312.5)
// sample register 0x1c value for 1 A current with 5 mOhm sense resistor: -3368

static constexpr float max17320_current_conversion_factor_milliamps = 0.3125;
extern const MAX17320_Register max17320_defaultConfig[];

#pragma once
#include <stdint.h>

bool PM_SMBUS_WriteReg16(uint8_t deviceAddr, uint8_t regAddr, uint16_t regValue);
bool PM_SMBUS_ReadReg16(uint8_t deviceAddr, uint8_t regAddr, uint16_t& outValue);
bool PM_SMBUS_RMWReg16(uint8_t deviceAddr, uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet);

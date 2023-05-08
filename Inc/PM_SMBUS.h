#pragma once
#include <stdint.h>
#include "i2c.h"

extern "C" {
  bool PM_SMBUS_WriteReg16(SMBUS_HandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t regValue);
  bool PM_SMBUS_ReadReg16(SMBUS_HandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t& outValue);
  bool PM_SMBUS_RMWReg16(SMBUS_HandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet);

  void MX_SMBUS_Error_Check(SMBUS_HandleTypeDef* pStackContext_);

}

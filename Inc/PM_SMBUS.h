#pragma once
#include <stdint.h>
#include <stm32_SMBUS_stack.h>

extern "C" {

  extern SMBUS_StackHandleTypeDef ctx_smbus1;
  extern SMBUS_StackHandleTypeDef ctx_smbus2;

  void PM_Init_SMBUS_Context(SMBUS_StackHandleTypeDef* ctx, SMBUS_HandleTypeDef* hsmbus);
  bool PM_SMBUS_WriteReg16(SMBUS_StackHandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t regValue);
  bool PM_SMBUS_ReadReg16(SMBUS_StackHandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t& outValue);
  bool PM_SMBUS_RMWReg16(SMBUS_StackHandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet);

  void MX_SMBUS_Error_Check(SMBUS_StackHandleTypeDef* pStackContext_);

}

#pragma once
#include "stm32_config_stack.h"
#include "stm32_PMBUS_stack.h"
#include "main.h"

extern SMBUS_StackHandleTypeDef context;
extern SMBUS_StackHandleTypeDef *pcontext;

#ifdef __cplusplus
extern "C" {
#endif

void MX_STACK_SMBUS_Init(void);
void MX_SMBUS_Error_Check(SMBUS_StackHandleTypeDef* pStackContext_);

#ifdef __cplusplus
}
#endif

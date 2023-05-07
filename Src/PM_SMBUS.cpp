#include "PM_SMBUS.h"

#include <FreeRTOS.h>
#include <task.h>
#include "stm32_SMBUS_stack.h"
#include "stm32_PMBUS_stack.h"
#include "i2c.h"
#include "log.h"
#include <stdlib.h>

SMBUS_StackHandleTypeDef ctx_smbus1;
SMBUS_StackHandleTypeDef ctx_smbus2;

void PM_Init_SMBUS_Context(SMBUS_StackHandleTypeDef* ctx, SMBUS_HandleTypeDef* hsmbus) {
  ctx->CMD_table = (st_command_t*)&PMBUS_COMMANDS_TAB[0];
  ctx->CMD_tableSize = PMBUS_COMMANDS_TAB_SIZE;
  ctx->Device = hsmbus;
  ctx->SRByte = 0x55U;
  ctx->CurrentCommand = NULL;

  /* In SMBUS 10-bit addressing is reserved for future use */
  assert_param(hsmbus->Init.AddressingMode == SMBUS_ADDRESSINGMODE_7BIT);
  ctx->OwnAddress = hsmbus->Init.OwnAddress1;

  /* Address resolved state */
  ctx->StateMachine = SMBUS_SMS_ARP_AR;

  /* checking the HAL host setting */
  assert_param(hsmbus->Init.PeripheralMode == SMBUS_PERIPHERAL_MODE_SMBUS_HOST);

  /* checking the HAL is in accord */
  assert_param(hsmbus->Init.PacketErrorCheckMode == SMBUS_PEC_DISABLE);


  if (STACK_SMBUS_Init(ctx) != HAL_OK) {
    logprintf("PM_Init_SMBUS_Context(%p, %p) failed!\n", ctx, hsmbus);
    Error_Handler();
  }
}

bool PM_SMBUS_WriteReg16(SMBUS_StackHandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t regValue) {
  MX_SMBUS_Error_Check(pCtx);

  uint8_t* buf = STACK_SMBUS_GetBuffer(pCtx);
  st_command_t command;
  command.cmnd_code = regAddr;
  command.cmnd_query = WRITE;
  command.cmnd_master_Tx_size = 3;
  command.cmnd_master_Rx_size = 0;
  *(reinterpret_cast<uint16_t*>(buf)) = regValue;

  HAL_StatusTypeDef st = STACK_SMBUS_HostCommand(pCtx, &command, deviceAddr, WRITE);
  if (st != HAL_OK) {
    logprintf("MP2760_WriteReg16(%x, %x, %x): Bad HAL response %u from STACK_SMBUS_HostCommand\n", deviceAddr, regAddr, regValue, st);
    return false;
  }

  for (int i = 0; i < 100; ++i) {
    if (STACK_SMBUS_IsReady(pCtx) == SMBUS_SMS_READY)
      break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (STACK_SMBUS_IsReady(pCtx) != SMBUS_SMS_READY) {
    logprintf("PM_SMBUS_WriteReg16(%x, %x, %x): SMBUS stack did not become ready after finishing busy. state = 0x%lx\n", deviceAddr, regAddr, regValue, pCtx->StateMachine);
    return false;
  }

  return true;
}


bool PM_SMBUS_ReadReg16(SMBUS_StackHandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t& outValue) {
  MX_SMBUS_Error_Check(pCtx);

  uint8_t* buf = STACK_SMBUS_GetBuffer(pCtx);
  st_command_t command;
  command.cmnd_code = regAddr;
  command.cmnd_query = READ;
  command.cmnd_master_Tx_size = 1;
  command.cmnd_master_Rx_size = 2;

  HAL_StatusTypeDef st = STACK_SMBUS_HostCommand(pCtx, &command, deviceAddr, READ);
  if (st != HAL_OK) {
    logprintf("PM_SMBUS_ReadReg16(%x, %x): Bad HAL response %u from STACK_SMBUS_HostCommand\n", deviceAddr, regAddr, st);
    return false;
  }

  for (int i = 0; i < 100; ++i) {
    if (STACK_SMBUS_IsReady(pCtx) == SMBUS_SMS_READY)
      break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (STACK_SMBUS_IsReady(pCtx) != SMBUS_SMS_READY) {
    logprintf("PM_SMBUS_ReadReg16(%x, %x): SMBUS stack did not become ready after finishing busy. state = 0x%lx\n", deviceAddr, regAddr, pCtx->StateMachine);
    return false;
  }

  outValue = *reinterpret_cast<uint16_t*>(buf);
  return true;
}

bool PM_SMBUS_RMWReg16(SMBUS_StackHandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet) {
  uint16_t value = 0;
  if (!PM_SMBUS_ReadReg16(pCtx, deviceAddr, regAddr, value))
    return false;
  value &= bitMask;
  value |= bitsToSet;
  return PM_SMBUS_WriteReg16(pCtx, deviceAddr, regAddr, value);
}


/**
  * @brief  Stub of an error treatment function - set to ignore most errors.
            Defined "weak" for suggested replacement.
  * @param  pStackContext : Pointer to a SMBUS_StackHandleTypeDef structure that contains
  *                the configuration information for the specified SMBUS.
  * @retval None
  */
void MX_SMBUS_Error_Check(SMBUS_StackHandleTypeDef* pStackContext) {
  if ((STACK_SMBUS_IsBlockingError(pStackContext)) || (STACK_SMBUS_IsCmdError(pStackContext))) {
    /* No action, error symptoms are ignored */
    pStackContext->StateMachine &= ~(SMBUS_ERROR_CRITICAL | SMBUS_COM_ERROR);
  } else if ((pStackContext->StateMachine & SMBUS_SMS_ERR_PECERR) ==
           SMBUS_SMS_ERR_PECERR) /* PEC error, we won't wait for any more action */
  {
    pStackContext->StateMachine |= SMBUS_SMS_READY;
    pStackContext->CurrentCommand = NULL;
    pStackContext->StateMachine &= ~(SMBUS_SMS_ACTIVE_MASK | SMBUS_SMS_ERR_PECERR);
  }
}

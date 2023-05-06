#include <FreeRTOS.h>
#include <task.h>
#include "smbus.h"
#include "stm32_SMBUS_stack.h"
#include "log.h"
#include <stdlib.h>

bool PM_SMBUS_WriteReg16(uint8_t deviceAddr, uint8_t regAddr, uint16_t regValue) {
  MX_SMBUS_Error_Check(pcontext);

  uint8_t* buf = STACK_SMBUS_GetBuffer(pcontext);
  st_command_t command;
  command.cmnd_code = regAddr;
  command.cmnd_query = WRITE;
  command.cmnd_master_Tx_size = 3;
  command.cmnd_master_Rx_size = 0;
  *(reinterpret_cast<uint16_t*>(buf)) = regValue;

  HAL_StatusTypeDef st = STACK_SMBUS_HostCommand(pcontext, &command, deviceAddr, WRITE);
  if (st != HAL_OK) {
    logprintf("MP2760_WriteReg16(%x, %x, %x): Bad HAL response %u from STACK_SMBUS_HostCommand\n", deviceAddr, regAddr, regValue, st);
    return false;
  }

  for (int i = 0; i < 100; ++i) {
    if (STACK_SMBUS_IsReady(pcontext) == SMBUS_SMS_READY)
      break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (STACK_SMBUS_IsReady(pcontext) != SMBUS_SMS_READY) {
    logprintf("PM_SMBUS_WriteReg16(%x, %x, %x): SMBUS stack did not become ready after finishing busy. state = 0x%lx\n", deviceAddr, regAddr, regValue, pcontext->StateMachine);
    return false;
  }

  return true;
}


bool PM_SMBUS_ReadReg16(uint8_t deviceAddr, uint8_t regAddr, uint16_t& outValue) {
  MX_SMBUS_Error_Check(pcontext);

  uint8_t* buf = STACK_SMBUS_GetBuffer(pcontext);
  st_command_t command;
  command.cmnd_code = regAddr;
  command.cmnd_query = READ;
  command.cmnd_master_Tx_size = 1;
  command.cmnd_master_Rx_size = 2;

  HAL_StatusTypeDef st = STACK_SMBUS_HostCommand(pcontext, &command, deviceAddr, READ);
  if (st != HAL_OK) {
    logprintf("PM_SMBUS_ReadReg16(%x, %x): Bad HAL response %u from STACK_SMBUS_HostCommand\n", deviceAddr, regAddr, st);
    return false;
  }

  for (int i = 0; i < 100; ++i) {
    if (STACK_SMBUS_IsReady(pcontext) == SMBUS_SMS_READY)
      break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (STACK_SMBUS_IsReady(pcontext) != SMBUS_SMS_READY) {
    logprintf("PM_SMBUS_ReadReg16(%x, %x): SMBUS stack did not become ready after finishing busy. state = 0x%lx\n", deviceAddr, regAddr, pcontext->StateMachine);
    return false;
  }

  outValue = *reinterpret_cast<uint16_t*>(buf);
  return true;
}

bool PM_SMBUS_RMWReg16(uint8_t deviceAddr, uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet) {
  uint16_t value = 0;
  if (!PM_SMBUS_ReadReg16(deviceAddr, regAddr, value))
    return false;
  value &= bitMask;
  value |= bitsToSet;
  return PM_SMBUS_WriteReg16(deviceAddr, regAddr, value);
}

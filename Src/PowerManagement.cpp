#include "PowerManagement.h"
#include "FreeRTOS.h"

#include "task.h"
#include "cmsis_os.h"
#include "mp2760_registers.h"
#include "smbus.h"
#include "stm32_SMBUS_stack.h"
#include "log.h"
#include <stdlib.h>


extern TaskHandle_t PowerManagementHandle;
#define MP2760_ADDR 0x10

void PM_DisconnectPower() {
  inputPowerState.isReady = 0;

  // Reset to basic USB defaults.
  inputPowerState.maxCurrent_mA = 500;

  inputPowerState.minVoltage_mV = 5000;
  inputPowerState.maxVoltage_mV = 5000;

  inputPowerState.maxPower_mW = 2500;
  PM_NotifyInputPowerStateUpdated();
}

void PM_NotifyInputPowerStateUpdated() {
  xTaskNotify(PowerManagementHandle, 0, eSetValueWithOverwrite);
}

bool MP2760_WriteReg(uint8_t regAddr, uint16_t regValue) {
  MX_SMBUS_Error_Check(pcontext);

  uint8_t* buf = STACK_SMBUS_GetBuffer(pcontext);
  st_command_t command;
  command.cmnd_code = regAddr;
  command.cmnd_query = WRITE;
  command.cmnd_master_Tx_size = 3;
  command.cmnd_master_Rx_size = 0;
  *(reinterpret_cast<uint16_t*>(buf)) = regValue;

  HAL_StatusTypeDef st = STACK_SMBUS_HostCommand(pcontext, &command, MP2760_ADDR, WRITE);
  if (st != HAL_OK) {
    logprintf("MP2760_WriteReg: Bad HAL response %u from STACK_SMBUS_HostCommand\n", st);
    return false;
  }

  for (int i = 0; i < 100; ++i) {
    if (STACK_SMBUS_IsReady(pcontext) == SMBUS_SMS_READY)
      break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (STACK_SMBUS_IsReady(pcontext) != SMBUS_SMS_READY) {
    logprintf("MP2760_WriteReg: SMBUS stack did not become ready after finishing busy. state = 0x%lx\n", pcontext->StateMachine);
    return false;
  }

  return true;
}

bool MP2760_ReadReg(uint8_t regAddr, uint16_t& outValue) {
  MX_SMBUS_Error_Check(pcontext);

  uint8_t* buf = STACK_SMBUS_GetBuffer(pcontext);
  st_command_t command;
  command.cmnd_code = regAddr;
  command.cmnd_query = READ;
  command.cmnd_master_Tx_size = 1;
  command.cmnd_master_Rx_size = 2;

  HAL_StatusTypeDef st = STACK_SMBUS_HostCommand(pcontext, &command, MP2760_ADDR, READ);
  if (st != HAL_OK) {
    logprintf("Bad HAL response %u from STACK_SMBUS_HostCommand\n", st);
    return false;
  }


  for (int i = 0; i < 100; ++i) {
    if (STACK_SMBUS_IsReady(pcontext) == SMBUS_SMS_READY)
      break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  if (STACK_SMBUS_IsReady(pcontext) != SMBUS_SMS_READY) {
    logprintf("MP2760_WriteReg: SMBUS stack did not become ready after finishing busy. state = 0x%lx\n", pcontext->StateMachine);
    return false;
  }

  outValue = *reinterpret_cast<uint16_t*>(buf);
  return true;
}

bool MP2760_RMWReg(uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet) {
  uint16_t value = 0;
  if (!MP2760_ReadReg(regAddr, value))
    return false;
  value &= bitMask;
  value |= bitsToSet;
  return MP2760_WriteReg(regAddr, value);
}

bool MP2760_SetChargeEnabled(bool enabled) {
  // REG12 (Configuration Register 4), bit 0: CHG_EN
  return MP2760_RMWReg(0x12, ~(0x0001), enabled ? 0x0001 : 0x0000);
}

bool MP2760_SetInputCurrentLimit(uint32_t inputCurrent_mA) {
  if (inputCurrent_mA > 5000)
    inputCurrent_mA = 5000;
  // REG08 (Input Current Limit Settings). 50 mA / LSB, max 5000 mA (0x64)
  return MP2760_WriteReg(0x08, (inputCurrent_mA / 50));
}

bool MP2760_InitDefaults() {
  // Init registers on MP2760 charger to our defaults.
  for (uint8_t regIdx = 0; regIdx < mp2760_defaultConfig_length; ++regIdx) {
    if (!MP2760_WriteReg(mp2760_defaultConfig[regIdx].id, mp2760_defaultConfig[regIdx].value))
      return false;
  }

  // Readback registers
  // Init registers on MP2760 charger to our defaults.
  for (uint8_t regIdx = 0; regIdx < mp2760_defaultConfig_length; ++regIdx) {
    uint16_t readValue;
    if (!MP2760_ReadReg(mp2760_defaultConfig[regIdx].id, readValue))
      return false;

    if (readValue != mp2760_defaultConfig[regIdx].value) {
      logprintf("MP2760_InitDefaults: Register 0x%02x mismatch after write: expected 0x%02x, got 0x%02x\n",
        mp2760_defaultConfig[regIdx].id, mp2760_defaultConfig[regIdx].value, readValue);
    }
  }
  return true;
}

void Task_PowerManagement(void*) {
  logprintf("Task_PowerManagement: Start\n");
  if (!MP2760_InitDefaults()) {

    logprintf("Task_PowerManagement: Could not initialize MP2760 charger!");

    vTaskSuspend(nullptr); // Stop this task for debugging.
  }

  logprintf("Task_PowerManagement: InitDefaults() done\n");

  inputPowerState.isReady = 0;
  inputPowerState.minVoltage_mV = 5000;
  inputPowerState.maxVoltage_mV = 5000;
  inputPowerState.maxCurrent_mA = 500;
  inputPowerState.maxPower_mW = 2500;


  while (true) {
    uint32_t notificationValue = 0;
    xTaskNotifyWait(0, 0, &notificationValue, portMAX_DELAY);

    logprintf("Task_PowerManagement: awoke. New settings: isReady=%u V = %lu - %lu mV, max %lu mA, max %lu mW\n",
      inputPowerState.isReady, inputPowerState.minVoltage_mV, inputPowerState.maxVoltage_mV,
      inputPowerState.maxCurrent_mA, inputPowerState.maxPower_mW);


    if (inputPowerState.isReady == 0) {
      MP2760_SetChargeEnabled(false);
      MP2760_SetInputCurrentLimit(500); // Return to the basic USB power limit
      continue;
    }

    // Step up the current gradually. We also have a long delay initially between setting the input current
    // limit and enabling the charger, since the current limit does not seem to take effect immediately.
    // (trying to prevent our chargers from immediately tripping on surge overcurrent)
    uint32_t currentLimitStep = inputPowerState.maxCurrent_mA / 5;
    MP2760_SetInputCurrentLimit(currentLimitStep);
    vTaskDelay(pdMS_TO_TICKS(1000));
    MP2760_SetChargeEnabled(true);

    for (int i = 2; i <= 5; ++i) {
      if (inputPowerState.isReady == 0) {
        MP2760_SetChargeEnabled(false);
        break;
      }

      MP2760_SetInputCurrentLimit(currentLimitStep * i);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }


  }


}


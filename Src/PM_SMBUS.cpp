#include "PM_SMBUS.h"

#include <FreeRTOS.h>
#include <task.h>
#include "i2c.h"
#include "log.h"
#include <stdlib.h>

#include <FreeRTOS.h>
#include "task.h"
#include "i2c.h"
#include <limits.h>

#define TRANSACTION_TIMEOUT_ms 100
#define SMBUS_NOTIFICATION_ERROR 0b001
#define SMBUS_NOTIFICATION_TX_COMPLETE 0b010
#define SMBUS_NOTIFICATION_RX_COMPLETE 0b100
#define SMBUS_NOTIFICATION_MASK 0b111



static volatile TaskHandle_t smbus1Task = nullptr, smbus3Task = nullptr;

volatile TaskHandle_t* taskHandleForSMBUSCtx(SMBUS_HandleTypeDef* hsmbus) {
  if (hsmbus == &hsmbus1) {
    return &smbus1Task;
  } else if (hsmbus == &hsmbus3) {
    return &smbus3Task;
  }

  Error_Handler();
  return nullptr;
}

// HAL callback interfaces for interrupt mode

void WakeTaskForSMBUSInterrupt(SMBUS_HandleTypeDef* hsmbus, uint32_t notificationValue) {
  TaskHandle_t targetTask = *taskHandleForSMBUSCtx(hsmbus);

  if (targetTask == nullptr) {
    logprintf("WakeTaskForSMBUSInterrupt(): no task waiting for %p value %lu\n", hsmbus, notificationValue);
    return;
  }
  xTaskAbortDelay(targetTask);
/*
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  uint32_t previousNotificationValue;
  xTaskGenericNotifyFromISR(targetTask, notificationValue, eSetBits, &previousNotificationValue, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
*/
}

extern "C" void HAL_SMBUS_MasterTxCpltCallback(SMBUS_HandleTypeDef * hsmbus) {
  WakeTaskForSMBUSInterrupt(hsmbus, SMBUS_NOTIFICATION_TX_COMPLETE);
}

extern "C" void HAL_SMBUS_MasterRxCpltCallback(SMBUS_HandleTypeDef * hsmbus) {
  WakeTaskForSMBUSInterrupt(hsmbus, SMBUS_NOTIFICATION_RX_COMPLETE);
}

extern "C" void HAL_SMBUS_ErrorCallback(SMBUS_HandleTypeDef * hsmbus) {
  WakeTaskForSMBUSInterrupt(hsmbus, SMBUS_NOTIFICATION_ERROR);
}

bool PM_SMBUS_WaitForReady(SMBUS_HandleTypeDef* pCtx) {
  int i = 1000;
  while (i--) {
    if (HAL_SMBUS_GetState(pCtx) & HAL_SMBUS_STATE_BUSY)
      vTaskDelay(2);
  }
  if (!i)
    return false;

  return (HAL_SMBUS_GetState(pCtx) == HAL_SMBUS_STATE_READY);
}

bool PM_SMBUS_AttemptRecovery(SMBUS_HandleTypeDef* pCtx) {
  vTaskDelay(100);

  pCtx->State = HAL_SMBUS_STATE_READY;
  pCtx->Lock = HAL_UNLOCKED;

  HAL_SMBUS_Master_Abort_IT(pCtx, /* dummy address=*/ 2);

  if (PM_SMBUS_WaitForReady(pCtx)) {
    logprintf("PM_SMBUS_AttemptRecovery(): OK\n");
  } else {
    logprintf("PM_SMBUS_AttemptRecovery(): Failed (state = %x)\n", pCtx->State);
  }

  pCtx->State = HAL_SMBUS_STATE_READY;
  pCtx->Lock = HAL_UNLOCKED;

  return false; // Used for return chaining.
}

bool PM_SMBUS_WriteReg16(SMBUS_HandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t regValue) {
  if (!PM_SMBUS_WaitForReady(pCtx)) {
    logprintf("PM_SMBUS_WriteReg16(%p, %x, %x, %x): Device not ready -- state = %x (before start)\n", pCtx->Instance, deviceAddr, regAddr, regValue, HAL_SMBUS_GetState(pCtx));
    return PM_SMBUS_AttemptRecovery(pCtx);
  }

  *taskHandleForSMBUSCtx(pCtx) = xTaskGetCurrentTaskHandle();

  uint8_t data[] = {regAddr, (uint8_t) (regValue & 0xffU), (uint8_t) ((regValue & 0xff00U) >> 8)};
  HAL_StatusTypeDef st = HAL_SMBUS_Master_Transmit_IT(pCtx, deviceAddr, data, sizeof(data), SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
  if (st != HAL_OK) {
    logprintf("PM_SMBUS_WriteReg16(%p, %x, %x, %x): Bad HAL response %u from HAL_SMBUS_Master_Transmit_IT\n", pCtx->Instance, deviceAddr, regAddr, regValue, st);
    return PM_SMBUS_AttemptRecovery(pCtx);
  }

  if (!PM_SMBUS_WaitForReady(pCtx)) {
    logprintf("PM_SMBUS_WriteReg16(%p, %x, %x, %x): Timed out waiting for device to become ready after write (state = %x)\n",
      pCtx->Instance, deviceAddr, regAddr, regValue, HAL_SMBUS_GetState(pCtx));
    return PM_SMBUS_AttemptRecovery(pCtx);
  }

  return true;
}

bool PM_SMBUS_ReadReg16(SMBUS_HandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t& outValue) {

  if (!PM_SMBUS_WaitForReady(pCtx)) {
    logprintf("PM_SMBUS_ReadReg16(%p, %x, %x): Device not ready -- state = %x (before start)\n", pCtx->Instance, deviceAddr, regAddr, HAL_SMBUS_GetState(pCtx));
    return PM_SMBUS_AttemptRecovery(pCtx);
  }

  *taskHandleForSMBUSCtx(pCtx) = xTaskGetCurrentTaskHandle();

  HAL_StatusTypeDef st;

  uint8_t txData[] = { regAddr };
  st = HAL_SMBUS_Master_Transmit_IT(pCtx, deviceAddr, txData, sizeof(txData), SMBUS_FIRST_FRAME);
  if (st != HAL_OK) {
    logprintf("PM_SMBUS_ReadReg16(%p, %x, %x): Bad HAL response %x from HAL_SMBUS_Master_Transmit_IT\n",
      pCtx->Instance, deviceAddr, regAddr, st);
    return PM_SMBUS_AttemptRecovery(pCtx);
  }
#if 1
  if (!PM_SMBUS_WaitForReady(pCtx)) {
    logprintf("PM_SMBUS_ReadReg16(%p, %x, %x): Timed out with state %x waiting for device to become ready after write\n",
      pCtx->Instance, deviceAddr, regAddr, HAL_SMBUS_GetState(pCtx));
    return PM_SMBUS_AttemptRecovery(pCtx);
  }
#else
  while (HAL_SMBUS_GetState(pCtx) != HAL_SMBUS_STATE_READY) {}
#endif

  st = HAL_SMBUS_Master_Receive_IT(pCtx, deviceAddr | 1, (uint8_t*) &outValue, sizeof(outValue), SMBUS_LAST_FRAME_NO_PEC);
  if (st != HAL_OK) {
    logprintf("PM_SMBUS_ReadReg16(%p, %x, %x): Bad HAL response %x from HAL_SMBUS_Master_Receive_IT\n", pCtx->Instance, deviceAddr, regAddr, st);
    return PM_SMBUS_AttemptRecovery(pCtx);
  }

  if (!PM_SMBUS_WaitForReady(pCtx)) {
    logprintf("PM_SMBUS_ReadReg16(%p, %x, %x): Timed out with state %x waiting for device to become ready after read\n",
      pCtx->Instance, deviceAddr, regAddr, HAL_SMBUS_GetState(pCtx));
    return PM_SMBUS_AttemptRecovery(pCtx);
  }

  return true;
}

bool PM_SMBUS_RMWReg16(SMBUS_HandleTypeDef* pCtx, uint8_t deviceAddr, uint8_t regAddr, uint16_t bitMask, uint16_t bitsToSet) {
  uint16_t value = 0;
  if (!PM_SMBUS_ReadReg16(pCtx, deviceAddr, regAddr, value))
    return false;
  value &= bitMask;
  value |= bitsToSet;
  return PM_SMBUS_WriteReg16(pCtx, deviceAddr, regAddr, value);
}

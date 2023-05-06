#include "PowerManagement.h"
#include "PM_SMBUS.h"
#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>
#include "mp2760_registers.h"
#include "max17320_registers.h"
#include "smbus.h"
#include "stm32_SMBUS_stack.h"
#include "log.h"
#include <stdlib.h>

extern TaskHandle_t PowerManagementHandle;
#define MP2760_ADDR 0x10

#define MAX17320_BANK0 0x6C // addr 0x000 - 0x0ff via I2C access
#define MAX17320_BANK1 0x16 // addr 0x100 - 0x17f via SMBus access (SBS protocol); addr 0x180 - 0x1ff via I2C access (NV memory bank)

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

bool MP2760_SetPowerInputEnabled(bool enabled) {
  // Turns charging and the DC/DC converter on or off.
  // REG12 (Configuration Register 4)
  // bit 6: DC/DC_EN -- Enable (1) or disable (0) the DC/DC converter.
  // bit 0: CHG_EN -- Enable (1) or disable (0) battery charging.
  const uint16_t bits = 0b0100'0001;
  return PM_SMBUS_RMWReg16(MP2760_ADDR, 0x12, ~(bits), enabled ? bits : 0);
}

bool MP2760_SetInputCurrentLimit(uint32_t inputCurrent_mA) {
  if (inputCurrent_mA > 5000)
    inputCurrent_mA = 5000;
  // REG08 (Input Current Limit Settings). 50 mA / LSB, max 5000 mA (0x64)
  return PM_SMBUS_WriteReg16(MP2760_ADDR, 0x08, (inputCurrent_mA / 50));
}

bool MP2760_InitDefaults() {
  // Init registers on MP2760 charger to our defaults.
  for (uint8_t regIdx = 0; regIdx < mp2760_defaultConfig_length; ++regIdx) {
    if (!PM_SMBUS_WriteReg16(MP2760_ADDR, mp2760_defaultConfig[regIdx].id, mp2760_defaultConfig[regIdx].value))
      return false;
  }

  // Readback registers for verification
  for (uint8_t regIdx = 0; regIdx < mp2760_defaultConfig_length; ++regIdx) {
    uint16_t readValue;
    if (!PM_SMBUS_ReadReg16(MP2760_ADDR, mp2760_defaultConfig[regIdx].id, readValue))
      return false;

    if (readValue != mp2760_defaultConfig[regIdx].value) {
      logprintf("MP2760_InitDefaults: Register 0x%02x mismatch after write: expected 0x%02x, got 0x%02x\n",
        mp2760_defaultConfig[regIdx].id, mp2760_defaultConfig[regIdx].value, readValue);
    }
  }
  return true;
}

bool MAX17320_SetWriteProtect(bool locked) {
  // Write 0x00F9 (to lock write protection) or 0x0000 (to unlock write protection) to the CommStat register (0x61) two times in a row.
  uint16_t wv = locked ? 0x00f9U : 0x0000U;
  return PM_SMBUS_WriteReg16(MAX17320_BANK0, 0x61, wv) && PM_SMBUS_WriteReg16(MAX17320_BANK0, 0x61, wv);
}

bool MAX17320_InitDefaults() {
  // Initialize registers on the MAX17320 battery fuel gauge to our defaults.
  // These can be saved to its nonvolatile storage, but the IC has a very limited number
  // of write cycles -- it can only be updated 7 times.

  bool configRegionDirty = false;

  for (uint8_t regIdx = 0; regIdx < max17320_defaultConfig_length; ++regIdx) {
    const MAX17320_Register& reg = max17320_defaultConfig[regIdx];

    uint16_t value = 0;
    if (!PM_SMBUS_ReadReg16(MAX17320_BANK1, reg.id & 0xff, value)) {
      logprintf("%s: Read failure at %03x\n", __FUNCTION__, reg.id);
      return false;
    }

    if (value == reg.value)
      continue; // match OK

    if (reg.id >= 0x1a0 && reg.id <= 0x1af) {
      // This is in the learning region.
      // The fuel gauge writes to these registers internally as it characterizes the pack;
      // we should avoid clobbering them.
    } else {
      logprintf("%s: NV reg %03x is %04x, should be %04x\n", __FUNCTION__, reg.id, value, reg.value);
      configRegionDirty = true;
    }
  }

  if (!configRegionDirty)
    return true; // No update required.

  // Unlock registers for write
  if (!MAX17320_SetWriteProtect(false)) {
    logprintf("%s: failed to unlock write protection\n", __FUNCTION__);
    return false;
  }

  for (uint8_t regIdx = 0; regIdx < max17320_defaultConfig_length; ++regIdx) {
    const MAX17320_Register& reg = max17320_defaultConfig[regIdx];

    if (reg.id >= 0x1a0 && reg.id <= 0x1af && reg.value == 0) {
      // Only write learning region registers that are not zeroed out in the default config.
      continue;
    }
    if (!PM_SMBUS_WriteReg16(MAX17320_BANK1, reg.id & 0xff, reg.value)) {
      logprintf("%s: Write failure at %03x\n", __FUNCTION__, reg.id);
    }
  }


  // Perform fuel-gauge reset:
  // 1: Unlock write-protect (done earlier, since we had to update NV regs)
  // 2: Write 0x8000 to Config2 register 0x0AB to reset IC fuel gauge operation.
  if (!PM_SMBUS_WriteReg16(MAX17320_BANK0, 0xab, 0x8000)) {
    logprintf("%s: Failed to issue reset command\n", __FUNCTION__);
  }

  // 3: Wait for POR_CMD bit(bit 15) of the Config2 register to be cleared to indicate that the POR sequence is complete.

  for (int i = 0; i < 100; ++i) {
    uint16_t value = 0;
    if (PM_SMBUS_ReadReg16(MAX17320_BANK0, 0xab, value) && ((value & 0x8000) == 0))
      break;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // 4. Lock write-protect
  return MAX17320_SetWriteProtect(true);
}

void Task_PowerManagement(void*) {
  logprintf("Task_PowerManagement: Start\n");
  if (!MP2760_InitDefaults()) {

    logprintf("Task_PowerManagement: Could not initialize MP2760 charger!\n");

    vTaskSuspend(nullptr); // Stop this task for debugging.
  }

  // Set up for the basic USB power limit.
  MP2760_SetPowerInputEnabled(false);
  MP2760_SetInputCurrentLimit(500);
  inputPowerState.isReady = 0;
  inputPowerState.minVoltage_mV = 5000;
  inputPowerState.maxVoltage_mV = 5000;
  inputPowerState.maxCurrent_mA = 500;
  inputPowerState.maxPower_mW = 2500;

  if (!MAX17320_InitDefaults()) {
    logprintf("Task_PowerManagement: Could not initialize MAX17320 fuel gauge!\n");
    vTaskSuspend(nullptr);
  }

  logprintf("Task_PowerManagement: InitDefaults() done\n");

  while (true) {
    uint32_t notificationValue = 0;
    xTaskNotifyWait(0, 0, &notificationValue, portMAX_DELAY);

    logprintf("Task_PowerManagement: awoke. New settings: isReady=%u V = %lu - %lu mV, max %lu mA, max %lu mW\n",
      inputPowerState.isReady, inputPowerState.minVoltage_mV, inputPowerState.maxVoltage_mV,
      inputPowerState.maxCurrent_mA, inputPowerState.maxPower_mW);

    if (inputPowerState.isReady) {
      // Give VBus some time to settle, then check isReady again.
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (inputPowerState.isReady == 0) {
      MP2760_InitDefaults();

      MP2760_SetPowerInputEnabled(false);
      MP2760_SetInputCurrentLimit(500); // Return to the basic USB power limit
      continue;
    }

    // Step up the current gradually. We also have a long delay initially between setting the input current
    // limit and enabling the charger, since the current limit does not seem to take effect immediately.
    // (trying to prevent our chargers from immediately tripping on surge overcurrent)
    uint32_t currentLimitStep = inputPowerState.maxCurrent_mA / 5;
    MP2760_SetInputCurrentLimit(currentLimitStep);
    vTaskDelay(pdMS_TO_TICKS(1000));
    MP2760_SetPowerInputEnabled(true);

    for (int i = 2; i <= 5; ++i) {
      if (inputPowerState.isReady == 0) {
        MP2760_SetPowerInputEnabled(false);
        MP2760_SetInputCurrentLimit(500);
        break;
      }

      MP2760_SetInputCurrentLimit(currentLimitStep * i);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

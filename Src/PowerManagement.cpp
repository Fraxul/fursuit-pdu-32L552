#include "PowerManagement.h"
#include "PM_SMBUS.h"
#include <FreeRTOS.h>
#include <task.h>
#include <cmsis_os.h>
#include "mp2760_registers.h"
#include "max17320_registers.h"
#include "adc.h"
#include "i2c.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern TaskHandle_t PowerManagementHandle;

extern "C" {
  struct InputPowerState_t inputPowerState;
  struct SystemPowerState_t systemPowerState;
  FanState_t fan[kFanCount];
}

// #define SMBUS_DEBUG_LOG_SUCCESSFUL_WRITES

#define MP2760_ADDR 0x10

#define MAX17320_BANK0 0x6C // addr 0x000 - 0x0ff via I2C access
#define MAX17320_BANK1 0x16 // addr 0x100 - 0x17f via SMBus access (SBS protocol); addr 0x180 - 0x1ff via I2C access (NV memory bank)

#define EMC2302_ADDR (0x2e << 1) // EMC2302-1 is 0x2e, EMC2302-2 is 0x2f

#define MAX17320_pCTX &hsmbus2
#define MP2760_pCTX &hsmbus2
#define EMC2302_pCTX &hsmbus2

void PM_DisconnectPower() {
  inputPowerState.isReady = 0;

  // Reset to basic USB defaults.
  inputPowerState.maxCurrent_mA = 500;

  inputPowerState.minVoltage_mV = 5000;
  inputPowerState.maxVoltage_mV = 5000;

  inputPowerState.maxPower_mW = 2500;
  PM_NotifyInputPowerStateUpdated();
}

static volatile bool inputPowerStateUpdated = false;

void PM_NotifyInputPowerStateUpdated() {
  // We'll just pick it up on the next loop.
  inputPowerStateUpdated = true;
}

void PM_RequestPowerOff() {
  systemPowerState.ws2812PowerEnabled = 0;
  systemPowerState.poweroffRequested = 1;
}

bool MP2760_SetPowerInputEnabled(bool enabled) {
  // Turns charging and the DC/DC converter on or off.
  // REG12 (Configuration Register 4)
  // bit 6: DC/DC_EN -- Enable (1) or disable (0) the DC/DC converter.
  // bit 0: CHG_EN -- Enable (1) or disable (0) battery charging.
  // const uint16_t bits = 0b0100'0001; // DC/DC_EN and CHG_EN
  const uint16_t bits = 0b0000'0001; // only CHG_EN
  return PM_SMBUS_RMWReg16(MP2760_pCTX, MP2760_ADDR, 0x12, ~(bits), enabled ? bits : 0);
}

bool MP2760_SetInputCurrentLimit(uint32_t inputCurrent_mA) {
  if (inputCurrent_mA > 5000)
    inputCurrent_mA = 5000;
  // REG08 (Input Current Limit Settings). 50 mA / LSB, max 5000 mA (0x64)
  return PM_SMBUS_WriteReg16(MP2760_pCTX, MP2760_ADDR, 0x08, (inputCurrent_mA / 50));
}

bool MP2760_InitDefaults() {
  // Init registers on MP2760 charger to our defaults.
  for (uint8_t regIdx = 0; regIdx < mp2760_defaultConfig_length; ++regIdx) {
    if (!PM_SMBUS_WriteReg16(MP2760_pCTX, MP2760_ADDR, mp2760_defaultConfig[regIdx].id, mp2760_defaultConfig[regIdx].value))
      return false;
  }

  // Readback registers for verification
  bool readbackOK = true;
  for (uint8_t regIdx = 0; regIdx < mp2760_defaultConfig_length; ++regIdx) {
    uint16_t readValue;
    if (!PM_SMBUS_ReadReg16(MP2760_pCTX, MP2760_ADDR, mp2760_defaultConfig[regIdx].id, readValue))
      return false;

    uint16_t compareValue = mp2760_defaultConfig[regIdx].value;
    if (mp2760_defaultConfig[regIdx].id == 0xe) {
      // REG0E bit 8 is ADC status which will be forced to 1 if DC/DC converter is enabled. Bits 9-15 are reserved.
      // Don't treat a mismatch on those bits as a comparison failure.
      readValue = readValue & 0xff;
      compareValue = compareValue & 0xff;
    }

    if (readValue != compareValue) {
      logprintf("MP2760_InitDefaults: Register 0x%02x mismatch after write: expected 0x%02x, got 0x%02x\n",
        mp2760_defaultConfig[regIdx].id, compareValue, readValue);
      readbackOK = false;
#ifdef SMBUS_DEBUG_LOG_SUCCESSFUL_WRITES
    } else {
      logprintf("MP2760_InitDefaults: Register 0x%02x write OK: 0x%02x\n",
        mp2760_defaultConfig[regIdx].id, readValue);
#endif
    }
  }
  return readbackOK;
}

bool MAX17320_SetWriteProtect(bool locked) {
  // Write 0x00F9 (to lock write protection) or 0x0000 (to unlock write protection) to the CommStat register (0x61) two times in a row.
  uint16_t wv = locked ? 0x00f9U : 0x0000U;
  return PM_SMBUS_WriteReg16(MAX17320_pCTX, MAX17320_BANK0, 0x61, wv) && PM_SMBUS_WriteReg16(MAX17320_pCTX, MAX17320_BANK0, 0x61, wv);
}

bool MAX17320_InitDefaults() {
  // Initialize registers on the MAX17320 battery fuel gauge to our defaults.
  // These can be saved to its nonvolatile storage, but the IC has a very limited number
  // of write cycles -- it can only be updated 7 times.

  uint16_t idVal = 0;
  if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x21, idVal)) {
    logprintf("%s: IDCode read failure\n", __FUNCTION__);
    return false;
  }

  if (idVal != 0x4209 && idVal != 0x420a) {
    logprintf("%s: IDCode mismatch: expected 0x4209 or 0x420a, got %04x\n", __FUNCTION__, idVal);
    return false;
  }

  bool configRegionDirty = false;

  for (uint8_t regIdx = 0; regIdx < max17320_defaultConfig_length; ++regIdx) {
    const MAX17320_Register& reg = max17320_defaultConfig[regIdx];

    uint16_t value = 0;
    if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK1, reg.id & 0xff, value)) {
      logprintf("%s: Read failure at %03x\n", __FUNCTION__, reg.id);
      return false;
    }

    if (value == reg.value) {
#ifdef SMBUS_DEBUG_LOG_SUCCESSFUL_WRITES
      logprintf("%s: NV reg OK: %03x == %04x\n", __FUNCTION__, reg.id, value);
#endif
      continue; // match OK
    }

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
    if (!PM_SMBUS_WriteReg16(MAX17320_pCTX, MAX17320_BANK1, reg.id & 0xff, reg.value)) {
      logprintf("%s: Write failure at %03x\n", __FUNCTION__, reg.id);
      return false;
    }
  }


  // Perform fuel-gauge reset:
  // 1: Unlock write-protect (done earlier, since we had to update NV regs)
  // 2: Write 0x8000 to Config2 register 0x0AB to reset IC fuel gauge operation.
  if (!PM_SMBUS_WriteReg16(MAX17320_pCTX, MAX17320_BANK0, 0xab, 0x8000)) {
    logprintf("%s: Failed to issue reset command\n", __FUNCTION__);
    return false;
  }

  // 3: Wait for POR_CMD bit(bit 15) of the Config2 register to be cleared to indicate that the POR sequence is complete.

  bool por_finished = false;
  for (int i = 0; i < 100; ++i) {
    uint16_t value = 0;
    if (PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0xab, value) && ((value & 0x8000) == 0)) {
      por_finished = true;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (!por_finished) {
    logprintf("%s: Failed to issue reset command\n", __FUNCTION__);
    return false;
  }

  // 4. Lock write-protect
  if (!MAX17320_SetWriteProtect(true)) {
    logprintf("%s: Failed to set write protect flags\n", __FUNCTION__);
    return false;
  }

  // Compare the registers again
  bool compareOK = true;
  for (uint8_t regIdx = 0; regIdx < max17320_defaultConfig_length; ++regIdx) {
    const MAX17320_Register& reg = max17320_defaultConfig[regIdx];

    uint16_t value = 0;
    if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK1, reg.id & 0xff, value)) {
      logprintf("%s: Read failure at %03x\n", __FUNCTION__, reg.id);
      return false;
    }

    if (value == reg.value) {
      continue;
    }

    if (reg.id >= 0x1a0 && reg.id <= 0x1af) {
      // This is in the learning region.
      // The fuel gauge writes to these registers internally as it characterizes the pack;
      // we should avoid clobbering them.
    } else {
      logprintf("%s: NV reg %03x is %04x, should be %04x -- write failed\n", __FUNCTION__, reg.id, value, reg.value);
      compareOK = false;
    }
  }
  return compareOK;
}

bool MP2760_UpdateSystemPowerState() {
  union {
    uint16_t value;
    int16_t value_signed;
  };

  // REG23, ADC Input Voltage
  if (!PM_SMBUS_ReadReg16(MP2760_pCTX, MP2760_ADDR, 0x23, value))
    return false;
  systemPowerState.chargerPowerInput_mV = (value & 0x3ff) * 20; // 20 mV/LSB

  // REG24, ADC Input Current
  if (!PM_SMBUS_ReadReg16(MP2760_pCTX, MP2760_ADDR, 0x24, value))
    return false;
  systemPowerState.chargerPowerInput_mA = ((value & 0x3ffU) * 25U) >> 2; // 6.25 mA/LSB

  // REG26, ADC System Voltage
  if (!PM_SMBUS_ReadReg16(MP2760_pCTX, MP2760_ADDR, 0x26, value))
    return false;
  systemPowerState.systemVoltage = (value & 0x3ff) * 20; // 20 mV/LSB

  // REG27, ADC Charge Current
  if (!PM_SMBUS_ReadReg16(MP2760_pCTX, MP2760_ADDR, 0x27, value))
    return false;
  systemPowerState.chargeCurrent_mA = ((value & 0x3ffU) * 25U) >> 1; // 12.5 mA/LSB.

  // REG2A, ADC Junction Temperature
  if (!PM_SMBUS_ReadReg16(MP2760_pCTX, MP2760_ADDR, 0x2a, value))
    return false;
  value &= 0x3ffU;
  // Junction Temperature read occasionally returns 0 -- just ignore those samples.
  if (value != 0) {
    // TJ = 314 - 0.5703 x REG2Ah
    // scaled by 2048 for fixed-point math:
    // (314*2048) - ((0.5703*2048) * REG2Ah)) / 2048
    systemPowerState.chargerTJ_degC = (643072U - (value  * 1168U)) / 2048;
  }

  return true;
}

bool MAX17320_UpdateSystemPowerState() {
  union {
    uint16_t value;
    int16_t value_signed;
  };

  if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x06 /*RepSOC*/, value))
    return false;
  systemPowerState.stateOfCharge_pct = (value >> 8); // 1/256% LSB

  if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x05 /*RepCap*/, systemPowerState.remainingCapacity_mAH))
    return false;

  if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0xdb /*PCKP*/, value))
    return false;

  // convert from 0.3125mV / LSB to millivolts
  systemPowerState.batteryVoltage_mV = (static_cast<uint32_t>(value) * 3125U) / 10000UL;

  if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x1c /*Current*/, value))
    return false;

  // Convert to mA using a precomputed conversion factor based on nRSense
  systemPowerState.batteryCurrent_mA = static_cast<int16_t>(static_cast<float>(value_signed) * max17320_current_conversion_factor_milliamps);

  if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x1b /*Temp*/, value))
    return false;

  // Temperature is signed, 1/256 degC per LSB.
  // Don't care about the fractional part.
  systemPowerState.batteryTemperature_degC = value_signed / 256;

  systemPowerState.timeToEmpty_seconds = 0xffff;
  systemPowerState.timeToFull_seconds = 0xffff;

  // TTF / TTE registers are 5.625sec/LSB
  if (systemPowerState.batteryCurrent_mA > 50) {
    if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x20 /*TTF*/, value))
      return false;
    uint32_t ttf = (static_cast<uint32_t>(value) * 5625U) / 1000UL;
    systemPowerState.timeToFull_seconds = (ttf >= 0xffffUL ? 0xffffU : ttf);
  }

  if (systemPowerState.batteryCurrent_mA < -50) {
    if (!PM_SMBUS_ReadReg16(MAX17320_pCTX, MAX17320_BANK0, 0x11 /*TTE*/, value))
      return false;
    uint32_t tte = (static_cast<uint32_t>(value) * 5625U) / 1000UL;
    systemPowerState.timeToEmpty_seconds = (tte >= 0xffffUL ? 0xffffU : tte);
  }

  return true;
}

bool EMC2302_Init() {
  bzero(fan, sizeof(FanState_t) * kFanCount);

  // Wait for EMC2302 to start responding
  uint16_t tmp = 0;
  const uint16_t emc2302_id = 0x5d36U;
  if (!PM_SMBUS_ReadReg16(EMC2302_pCTX, EMC2302_ADDR, 0xfd, tmp))
    return false;

  if (tmp != emc2302_id) {
    logprintf("EMC2302_Init: Bad IDcode returned: expected %x, got %x\n", emc2302_id, tmp);
    return false;
  }


  PM_SMBUS_WriteReg8(EMC2302_pCTX, EMC2302_ADDR, 0x20, 0b10000000); // disable ALERT, enable SMBus timeout, powerup watchdog only, no tach clock out, use internal oscillator

  uint8_t fanConfig1 = 0b00001011; // RPM algo disabled, RANGE = 1x, EDGES = 2 (default), update time = 400ms (default)
  uint8_t fanConfig2 = 0b01101000; // Ramp rate control enabled, glitch filter enabled (default), basic derivative (default), error range = 0 RPM (default)
  uint8_t fanMinDrive = 0x33; // 20% minimum PWM drive (0.2 * 255)

  for (int fanIdx = 0; fanIdx < 2; ++fanIdx) {
    fan[fanIdx].pwmDrive = 0x33; // 20% default

    // Fan base addresses are {0x30, 0x40}
    PM_SMBUS_WriteReg8(EMC2302_pCTX, EMC2302_ADDR, (fanIdx * 0x10) + 0x32, fanConfig1); // Configuration 1
    PM_SMBUS_WriteReg8(EMC2302_pCTX, EMC2302_ADDR, (fanIdx * 0x10) + 0x33, fanConfig2); // Configuration 2
    PM_SMBUS_WriteReg8(EMC2302_pCTX, EMC2302_ADDR, (fanIdx * 0x10) + 0x38, fanMinDrive); // Min Drive

    PM_SMBUS_WriteReg8(EMC2302_pCTX, EMC2302_ADDR, (fanIdx * 0x10) + 0x30, fan[fanIdx].pwmDrive); // PWM drive percentage
  }

  return true;
}

void EMC2302_Update() {
  uint8_t status[4];
  PM_SMBUS_ReadMem(EMC2302_pCTX, EMC2302_ADDR, /*addr=*/ 0x25, status, 3);
  fan[0].stalled = status[0] & 1;
  fan[1].stalled = status[0] & 2;
  fan[0].spinupFailure = status[1] & 1;
  fan[1].spinupFailure = status[1] & 2;
  fan[0].driveFailure = status[2] & 1;
  fan[1].driveFailure = status[2] & 2;

  // Tachometer reading. Magic constant division to get RPM is described in the EMC2302 datasheet.
  for (int fanIdx = 0; fanIdx < 2; ++fanIdx) {
    uint16_t tach;
    PM_SMBUS_ReadReg16(EMC2302_pCTX, EMC2302_ADDR, (fanIdx * 0x10) + 0x3e, tach);

    tach = tach >> 3; // Right-shift by 3 because the data is MSB-aligned and there are only 13 valid bits.

    if (tach >= 0x1ffe)
      fan[fanIdx].tachometer = 0; // Counter has maxed out, which probably means that the fan isn't spinning.
    else
      fan[fanIdx].tachometer = 3932160UL / tach;
  }
}

void PM_Shell_DumpPowerStats() {
  printf("SoC: %u%% (%u mAh)\n", systemPowerState.stateOfCharge_pct, systemPowerState.remainingCapacity_mAH);
  printf("%5u mV    %d mA    %d degC\n", systemPowerState.batteryVoltage_mV, systemPowerState.batteryCurrent_mA, systemPowerState.batteryTemperature_degC);
  if (systemPowerState.timeToEmpty_seconds != 0xffff) {
    uint16_t min = systemPowerState.timeToEmpty_seconds / 60;
    uint16_t sec = systemPowerState.timeToEmpty_seconds - (min * 60);
    printf("TTE: %u min %02u sec\n", min, sec);
  }
  if (inputPowerState.isReady) {
    printf("PD: %u - %u mV, max %u mA, max %lu mW\n",
      inputPowerState.minVoltage_mV, inputPowerState.maxVoltage_mV,
      inputPowerState.maxCurrent_mA, inputPowerState.maxPower_mW);
    printf("Charger: %u mV, %u mA in; %u mA out\n",
      systemPowerState.chargerPowerInput_mV,
      systemPowerState.chargerPowerInput_mA,
      systemPowerState.chargeCurrent_mA);
    if (systemPowerState.timeToFull_seconds != 0xffff) {
      uint16_t min = systemPowerState.timeToFull_seconds / 60;
      uint16_t sec = systemPowerState.timeToFull_seconds - (min * 60);
      printf("TTF: %u min %02u sec\n", min, sec);
    }
  }
}


void Task_PowerManagement(void*) {
  logprintf("Task_PowerManagement: Start\n");
  memset(&systemPowerState, 0, sizeof(systemPowerState));


  {
    bool max17320_ok = false;
    bool mp2760_ok = false;
    while (true) {
      if (!max17320_ok) {
        max17320_ok = MAX17320_InitDefaults();

        if (max17320_ok)
          logprintf("Task_PowerManagement: MAX17320 fuel gauge init OK\n");
        else
          logprintf("Task_PowerManagement: Could not initialize MAX17320 fuel gauge!\n");
      }

      if (!mp2760_ok) {
        mp2760_ok = MP2760_InitDefaults();

        if (mp2760_ok)
          logprintf("Task_PowerManagement: MP2760 charger init OK\n");
        else
          logprintf("Task_PowerManagement: Could not initialize MP2760 charger!\n");
      }

      if (max17320_ok && mp2760_ok)
        break;
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  // Set up for the basic USB power limit.
  MP2760_SetPowerInputEnabled(false);
  MP2760_SetInputCurrentLimit(500);
  inputPowerState.isReady = 0;
  inputPowerState.minVoltage_mV = 5000;
  inputPowerState.maxVoltage_mV = 5000;
  inputPowerState.maxCurrent_mA = 500;
  inputPowerState.maxPower_mW = 2500;

  // EMC2302 init does not block PM task running
  bool emc2302_ok = EMC2302_Init();

  logprintf("Task_PowerManagement: InitDefaults() done\n");

  bool inputPowerWasPreviouslyOn = false;

  while (true) {
    // logprintf("PM: 5V_Sense=%u mv Disp5V_Sense=%u mv VBUS=%u mv\n", ADC_read_5V_Sense(), ADC_read_Disp5V_Sense(), ADC_read_TC_VBUS_Sense());

    // Always update system power state every tick.
    MAX17320_UpdateSystemPowerState();
    MP2760_UpdateSystemPowerState();

    if (emc2302_ok) {
      EMC2302_Update();
    } else {
      emc2302_ok = EMC2302_Init();
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (systemPowerState.poweroffRequested) {
      logprintf("Task_PowerManagement: Power off requested.\n");

      bool ok = true;
      do {
        {
          // Configure MP2760 for power off: Disable charging and the DC/DC converter.
          // REG12 (Configuration Register 4)
          // bit 6: DC/DC_EN -- Enable (1) or disable (0) the DC/DC converter.
          // bit 0: CHG_EN -- Enable (1) or disable (0) battery charging.
          const uint16_t bits = 0b0100'0001;
          ok &= PM_SMBUS_RMWReg16(MP2760_pCTX, MP2760_ADDR, 0x12, ~(bits), 0);
        }

        // Request MAX17320 to enter SHIP mode.
        {
          // Config register (0x00B)
          // Bit 10: Pushbutton wake enable
          // Bit  7: Force-enter SHIP mode (after nDelayCfg.UVPTimer expires)
          const uint16_t configBits = 0b0000'0100'1000'0000;
          ok &= PM_SMBUS_RMWReg16(MAX17320_pCTX, MAX17320_BANK0, 0x0b, /*mask=*/ 0xffff, /*toSet=*/ configBits);
        }
      } while (!ok);

      // Shut down the SMBUS link connected to the MAX17320 -- otherwise, it'll assume that the
      // system is still up and prevent entering ship mode.
      HAL_SMBUS_MspDeInit(MAX17320_pCTX);

      LL_GPIO_InitTypeDef pinCfg = {0};
      pinCfg.Pin = LL_GPIO_PIN_13 | LL_GPIO_PIN_14; // PB13/PB14 - I2C2
      pinCfg.Mode = LL_GPIO_MODE_OUTPUT;
      pinCfg.Speed = LL_GPIO_SPEED_FREQ_LOW;
      pinCfg.Pull = LL_GPIO_PULL_NO;
      pinCfg.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
      LL_GPIO_Init(GPIOB, &pinCfg);
      LL_GPIO_ResetOutputPin(GPIOB, pinCfg.Pin);

      logprintf("Task_PowerManagement: SMBUS shutdown, waiting for MAX17320 to enter SHIP mode.\n");

      vTaskSuspend(nullptr); // won't return after this -- power should be turned off.
    }

    if (inputPowerStateUpdated) {
      // Notification received -- update the charger.
      inputPowerStateUpdated = false;

      logprintf("Task_PowerManagement: awoke. New settings: isReady=%u V = %lu - %lu mV, max %lu mA, max %lu mW\n",
        inputPowerState.isReady, inputPowerState.minVoltage_mV, inputPowerState.maxVoltage_mV,
        inputPowerState.maxCurrent_mA, inputPowerState.maxPower_mW);

      if (inputPowerState.isReady) {
        // Give VBus some time to settle, then check isReady again.
        vTaskDelay(pdMS_TO_TICKS(1000));
      }

      if (inputPowerState.isReady == 0) {
        inputPowerWasPreviouslyOn = false;
        MP2760_InitDefaults();

        MP2760_SetPowerInputEnabled(false);
        MP2760_SetInputCurrentLimit(500); // Return to the basic USB power limit
        continue;
      }

      if (!inputPowerWasPreviouslyOn) {
        // Safety check: Always reload the MP2760 config registers when transitioning
        // from no input power to charging mode.
        // (This is mostly useful on the bench when running the STM32 from an
        // external supply -- an undervolt will power-off the MP2760 and reset it to
        // defaults, but not reset the STM32.)
        MP2760_InitDefaults();
        inputPowerWasPreviouslyOn = true;
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
        MAX17320_UpdateSystemPowerState();
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }
  }
}

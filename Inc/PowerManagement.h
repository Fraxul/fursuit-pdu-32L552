#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct InputPowerState_t {
  uint32_t minVoltage_mV;
  uint32_t maxVoltage_mV;
  uint32_t maxCurrent_mA;
  uint32_t maxPower_mW;

  uint8_t isReady;
};

struct SystemPowerState_t {
  // Battery info
  uint8_t stateOfCharge_pct; // percentage
  uint16_t remainingCapacity_mAH; // milliamp-hours
  uint16_t batteryVoltage_mV; // millivolts
  int16_t batteryCurrent_mA; // milliamps. signed value representing charge (positive) and discharge (negative).
  int16_t batteryTemperature_degC; // 1 deg C resolution

  uint16_t timeToEmpty_seconds; // valid only when discharging
  uint16_t timeToFull_seconds; // valid only when charging
};


extern struct InputPowerState_t inputPowerState;
extern struct SystemPowerState_t systemPowerState;

void PM_NotifyInputPowerStateUpdated();
void PM_DisconnectPower(); // Shortcut for setting isReady = 0 and notifying state update

void PM_Shell_DumpPowerStats();

void Task_PowerManagement(void*);

#ifdef __cplusplus
}
#endif

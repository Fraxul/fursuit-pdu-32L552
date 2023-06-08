#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct InputPowerState_t {
  uint16_t minVoltage_mV;
  uint16_t maxVoltage_mV;
  uint32_t maxPower_mW;
  uint16_t maxCurrent_mA;
  uint8_t isReady;
};

struct SystemPowerState_t {

  // System info (from MP2760 charger)
  uint16_t systemVoltage_mV;


  // Battery info
  uint8_t stateOfCharge_pct; // percentage
  uint16_t remainingCapacity_mAH; // milliamp-hours
  uint16_t batteryVoltage_mV; // millivolts
  int16_t batteryCurrent_mA; // milliamps. signed value representing charge (positive) and discharge (negative).
  int32_t batteryPower_mW; // milliwatts. signed value representing charge (positive) and discharge (negative).
  int16_t batteryTemperature_degC; // 1 deg C resolution

  uint16_t timeToEmpty_seconds; // valid only when discharging
  uint16_t timeToFull_seconds; // valid only when charging

  // Charger info
  uint16_t chargerPowerInput_mV;
  uint16_t chargerPowerInput_mA;
  uint16_t chargeCurrent_mA;
  uint16_t chargerTJ_degC;

  // State machine
  uint8_t ws2812PowerEnabled;
  uint8_t poweroffRequested;

  // Error reporting
  uint8_t hasErrorMsg;
  char errorMsg[20];


  uint16_t mp2760_reg16h;
  uint16_t mp2760_reg17h;
};

struct FanState_t {
  uint16_t tachometer; // RPM; 0 if not spinning.
  uint8_t pwmDrive; // drive strength. 0 is off, 255 is full speed.
  // status flags
  uint8_t spinupFailure : 1;
  uint8_t stalled : 1;
  uint8_t driveFailure : 1;
};

#define kFanCount 2
extern struct FanState_t fan[kFanCount];

extern struct InputPowerState_t inputPowerState;
extern struct SystemPowerState_t systemPowerState;

void PM_NotifyInputPowerStateUpdated();
void PM_DisconnectPower(); // Shortcut for setting isReady = 0 and notifying state update

void PM_RequestPowerOff();

void PM_Shell_DumpPowerStats();

void Task_PowerManagement(void*);

#ifdef __cplusplus
}
#endif

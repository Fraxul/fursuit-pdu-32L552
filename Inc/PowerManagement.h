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


struct InputPowerState_t inputPowerState;
void PM_NotifyInputPowerStateUpdated();
void PM_DisconnectPower(); // Shortcut for setting isReady = 0 and notifying state update

void Task_PowerManagement(void*);

#ifdef __cplusplus
}
#endif

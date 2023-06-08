#include "PowerManagement.h"
#include "PM_SMBUS.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "i2c.h"
#include "ssd1306.h"
#include "log.h"
#include "Buttons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern "C" {
  void Task_Display(void*);
  TaskHandle_t DisplayHandle;
}

#define BUS &hi2c1
static const uint8_t lineLength[] = {
  SSD1306_WIDTH / (SSD1306_CHAR_WIDTH * 1),
  SSD1306_WIDTH / (SSD1306_CHAR_WIDTH * 2),
  SSD1306_WIDTH / (SSD1306_CHAR_WIDTH * 3),
  SSD1306_WIDTH / (SSD1306_CHAR_WIDTH * 4)};

void ssd1306_lineprintf(uint8_t ypos, uint8_t fsize, const char* fmt, ...) {
  fsize = fsize & 3; // clamp to 0...3 range;
  va_list v;
  va_start(v, fmt);
  char buf[(lineLength[0]) + 1];
  int res = vsnprintf(buf, lineLength[fsize] + 1, fmt, v);
  va_end(v);

  // Pad the rest of the line with spaces
  while (res < lineLength[fsize])
    buf[res++] = ' ';
  buf[lineLength[fsize]] = '\0';

  ssd1306_SetCursor(0, ypos);
  ssd1306_WriteString(buf, fsize);
}

enum UISelectionState : int {
  kNoSelection,
  kToggleWS2812Power,
  kToggleJetsonPower,
  kPowerOff,
  kUISelectionStateMax
};

UISelectionState uiSelectionState = kNoSelection;

SemaphoreHandle_t uiSelectionUpdatedSemaphore;

void UISelect_ShortPress() {
  int sel = uiSelectionState + 1;
  if (sel >= kUISelectionStateMax)
    sel = kNoSelection;
  uiSelectionState = (UISelectionState) sel;

  xSemaphoreGive(uiSelectionUpdatedSemaphore);
}

void UISelect_LongPress() {
  switch (uiSelectionState) {
    case kNoSelection:
    case kUISelectionStateMax:
      break;

    case kToggleWS2812Power:
      systemPowerState.ws2812PowerEnabled = !systemPowerState.ws2812PowerEnabled;
      break;

    case kToggleJetsonPower:
      PM_SetJetsonPowerState(!systemPowerState.jetsonPowerEnabled);
      break;

    case kPowerOff:
      PM_RequestPowerOff();
      break;
  }

  // clear selection after action
  uiSelectionState = kNoSelection;
}



void Task_Display(void* unused) {
  uiSelectionUpdatedSemaphore = xSemaphoreCreateBinary();

  ssd1306_Init(BUS);

  Button_SetShortPressHandler(kPowerButton, &UISelect_ShortPress);
  Button_SetLongPressHandler(kPowerButton, &UISelect_LongPress);

  while (true) {
    ssd1306_SendConfig();

    unsigned int w = abs(systemPowerState.batteryPower_mW) / 1000;
    unsigned int dw = (abs(systemPowerState.batteryPower_mW) - (w * 1000)) / 100;
    ssd1306_lineprintf(0, 1, "%2u%s %u.%uW", systemPowerState.stateOfCharge_pct, systemPowerState.stateOfCharge_pct >= 100 ? "" : "%", w, dw);

    ssd1306_lineprintf(2, 0, "%uMV %dMA", systemPowerState.batteryVoltage_mV, systemPowerState.batteryCurrent_mA);

    ssd1306_lineprintf(3, 0, "BAT: %dC  CHG: %dC", systemPowerState.batteryTemperature_degC, systemPowerState.chargerTJ_degC);

    if (inputPowerState.isReady) {
      ssd1306_lineprintf(4, 0, "IN: %uV %u/%uMA", (systemPowerState.chargerPowerInput_mV + 500) / 1000, systemPowerState.chargerPowerInput_mA, inputPowerState.maxCurrent_mA);
    } else {
      ssd1306_lineprintf(4, 0, ""); // clear input state line
    }

    if (systemPowerState.timeToEmpty_seconds != 0 && systemPowerState.timeToEmpty_seconds != 0xffff) {
      int minutes = systemPowerState.timeToEmpty_seconds / 60;
      int seconds = systemPowerState.timeToEmpty_seconds - (minutes * 60);
      ssd1306_lineprintf(5, 0, "RUNTIME: %uM %02uS", minutes, seconds);

    } else if (systemPowerState.timeToFull_seconds != 0 && systemPowerState.timeToFull_seconds != 0xffff) {
      int minutes = systemPowerState.timeToFull_seconds / 60;
      int seconds = systemPowerState.timeToFull_seconds - (minutes * 60);
      ssd1306_lineprintf(5, 0, "TO FULL: %uM %02uS", minutes, seconds);
    } else {
      ssd1306_lineprintf(5, 0, ""); // clear the runtime-estimate line, since we have no data.
    }

    if (systemPowerState.hasErrorMsg) {
      ssd1306_lineprintf(6, 0, "%s", systemPowerState.errorMsg);
    } else {
      ssd1306_lineprintf(6, 0, ""); // no data here yet
    }

    switch (uiSelectionState) {
      case kNoSelection:
      case kUISelectionStateMax:
        ssd1306_lineprintf(7, 0, "VSYS: %uMV", systemPowerState.systemVoltage_mV);
        break;

      case kToggleWS2812Power:
        ssd1306_lineprintf(7, 0, ":: WS2812 %s?", systemPowerState.ws2812PowerEnabled ? "OFF" : "ON");
        break;

      case kToggleJetsonPower:
        ssd1306_lineprintf(7, 0, ":: JETSON %s?", systemPowerState.jetsonPowerEnabled ? "OFF" : "ON");
        break;

      case kPowerOff:
        ssd1306_lineprintf(7, 0, ":: SYS POWER OFF?");
        break;
    }

    // Redraw every second, or if we get a notification (due to button press)
    xSemaphoreTake(uiSelectionUpdatedSemaphore, pdMS_TO_TICKS(1000));

    if (systemPowerState.poweroffRequested) {
      ssd1306_lineprintf(0, 0, "");
      ssd1306_lineprintf(1, 0, "");
      ssd1306_lineprintf(2, 0, " POWER OFF REQUESTED ");
      ssd1306_lineprintf(3, 0, "");
      ssd1306_lineprintf(4, 0, "");
      ssd1306_lineprintf(5, 0, "      WAITING...     ");
      ssd1306_lineprintf(6, 0, "");
      ssd1306_lineprintf(7, 0, "");
      vTaskSuspend(nullptr);
    }
  }
}

#include "PowerManagement.h"
#include "PM_SMBUS.h"
#include <FreeRTOS.h>
#include <task.h>
#include "i2c.h"
#include "ssd1306.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern "C" {
  void Task_Display(void*);
}

#define BUS &hi2c1
static const uint8_t lineLength = (SSD1306_WIDTH / SSD1306_CHAR_WIDTH);

void ssd1306_lineprintf(uint8_t ypos, const char* fmt, ...) {
  va_list v;
  va_start(v, fmt);
  char buf[lineLength + 1];
  int res = vsnprintf(buf, lineLength + 1, fmt, v);
  va_end(v);

  // Pad the rest of the line with spaces
  while (res < lineLength)
    buf[res++] = ' ';
  buf[lineLength] = '\0';

  ssd1306_SetCursor(0, ypos);
  ssd1306_WriteString(buf, /*fsize=*/ 0);
}


void Task_Display(void* unused) {

  ssd1306_Init(BUS);

  while (true) {
    ssd1306_lineprintf(0, "%3u%% %uMV %dMA", systemPowerState.stateOfCharge_pct, systemPowerState.batteryVoltage_mV, systemPowerState.batteryCurrent_mA);

    if (inputPowerState.isReady) {
      ssd1306_lineprintf(1, "IN: %uV %u/%uMA", (systemPowerState.chargerPowerInput_mV + 500) / 1000, systemPowerState.chargerPowerInput_mA, inputPowerState.maxCurrent_mA);
    } else {
      ssd1306_lineprintf(1, ""); // clear input state line
    }

    if (systemPowerState.timeToEmpty_seconds != 0 && systemPowerState.timeToEmpty_seconds != 0xffff) {
      int minutes = systemPowerState.timeToEmpty_seconds / 60;
      int seconds = systemPowerState.timeToEmpty_seconds - (minutes * 60);
      ssd1306_lineprintf(3, "RUNTIME: %uM %02uS", minutes, seconds);

    } else if (systemPowerState.timeToFull_seconds != 0 && systemPowerState.timeToFull_seconds != 0xffff) {
        int minutes = systemPowerState.timeToFull_seconds / 60;
        int seconds = systemPowerState.timeToFull_seconds - (minutes * 60);
        ssd1306_lineprintf(3, "TO FULL: %uM %02uS", minutes, seconds);
    } else {
      ssd1306_lineprintf(3, ""); // clear the runtime-estimate line, since we have no data.
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

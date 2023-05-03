#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "usart.h"

#define LOG_BUFFER_SIZE 8192
char sLogBuffer[LOG_BUFFER_SIZE];
uint16_t sLogWritePtr = 0;

extern "C" {

  void logprintf(const char* fmt, ...) {
    va_list v;
    va_start(v, fmt);

    portENTER_CRITICAL();
    char* start = sLogBuffer + sLogWritePtr;
    int remaining = LOG_BUFFER_SIZE - sLogWritePtr;
    int res = vsnprintf(start, remaining, fmt, v);
    va_end(v);
    if (res > remaining) {
      // hit the end of the buffer. the end of the message is cut off.
      sLogWritePtr = 0;
    } else {
      sLogWritePtr += res;
    }

    uint32_t dmaLength = (res > remaining) ? remaining : res;
    HAL_UART_Transmit(&hlpuart1, (const uint8_t*) start, dmaLength, 0xffffffff);
    portEXIT_CRITICAL();
  }

  void log_append(const char* data, size_t length) {
    if (!length)
      return;

    // Ensure that the UART is ready before we enter the critical section
    while (hlpuart1.gState != HAL_UART_STATE_READY) {
      __WFI();
    }

    portENTER_CRITICAL();

    if ((sLogWritePtr + length) < LOG_BUFFER_SIZE) {
      memcpy(sLogBuffer + sLogWritePtr, data, length);
      HAL_UART_Transmit_DMA(&hlpuart1, (const uint8_t*) (sLogBuffer + sLogWritePtr), length);
      sLogWritePtr += length;

      portEXIT_CRITICAL();
    } else {

      uint16_t write1Len = LOG_BUFFER_SIZE - sLogWritePtr;
      memcpy(sLogBuffer + sLogWritePtr, data, write1Len);
      HAL_UART_Transmit_DMA(&hlpuart1, (const uint8_t*)(sLogBuffer + sLogWritePtr), write1Len);
      length -= write1Len;
      memcpy(sLogBuffer, data + write1Len, length);
      sLogWritePtr = length;

      // Exit criticial section before transmitting the second half, since we may need to spin-wait
      // for the first half to finish (and we can't effectively do that with interrupts off)
      portEXIT_CRITICAL();

      while (hlpuart1.gState != HAL_UART_STATE_READY) {
        __WFI();
      }

      HAL_UART_Transmit_DMA(&hlpuart1, (const uint8_t*) sLogBuffer, length);
    }
  }

  void log_read(const char** outChunk1, size_t* outChunk1Length, const char** outChunk2, size_t* outChunk2Length) {
    portENTER_CRITICAL();
    char* p = sLogBuffer + sLogWritePtr;
    size_t l = LOG_BUFFER_SIZE - sLogWritePtr;

    while (*p == '\0' && l) --l;

    *outChunk1 = p;
    *outChunk1Length = l;

    if (sLogBuffer[0] == '\0') {
      *outChunk2 = nullptr;
      *outChunk2Length = 0;
    } else {
      *outChunk2 = sLogBuffer;
      *outChunk2Length = sLogWritePtr;
    }
    portEXIT_CRITICAL();
  }

}
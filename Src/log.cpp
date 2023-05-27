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

static volatile bool sLogTx_DMABusy = false;
static volatile uint16_t sLogTx_NextReadPtr = 0;


#define LOG_DMA_PERIPH DMA1
#define DMA_TARGET LOG_DMA_PERIPH, LL_DMA_CHANNEL_8
#define CLEAR_TC_FLAG() LL_DMA_ClearFlag_TC8(LOG_DMA_PERIPH)

extern "C" {

  // Must be called from within a critical section (or the DMA IRQ handler)
  void LogDMA_StartTx() {
    if (sLogTx_DMABusy)
      return; // DMA is busy, can't do anything.

    if (sLogTx_NextReadPtr == sLogWritePtr)
      return; // nothing to transmit.

    LL_DMA_SetMemoryAddress(DMA_TARGET, (uint32_t) (sLogBuffer + sLogTx_NextReadPtr));
    LL_DMA_SetPeriphAddress(DMA_TARGET, LL_USART_DMA_GetRegAddr(USART1, LL_USART_DMA_REG_DATA_TRANSMIT));

    uint32_t dmaLength;
    if (sLogWritePtr < sLogTx_NextReadPtr) {
      // Write pointer has wrapped around. This DMA will transmit from our current position
      // to the end of the buffer, then we'll wrap around and pick up the rest of it on the
      // next transfer.
      dmaLength = LOG_BUFFER_SIZE - sLogTx_NextReadPtr;
      sLogTx_NextReadPtr = 0;
    } else {
      // See if we were left pointing at the end of the buffer.
      if (sLogTx_NextReadPtr >= LOG_BUFFER_SIZE)
        sLogTx_NextReadPtr = 0;

      dmaLength = sLogWritePtr - sLogTx_NextReadPtr;
      sLogTx_NextReadPtr += dmaLength;
    }

    if (dmaLength == 0)
      return; // nothing to do?

    LL_DMA_SetDataLength(DMA_TARGET, dmaLength);

    LL_USART_EnableDMAReq_TX(USART1);
    LL_USART_ClearFlag_TC(USART1);
    LL_DMA_EnableChannel(DMA_TARGET);
    LL_USART_EnableIT_TC(USART1);

    sLogTx_DMABusy = true;
  }

  void LogDMA_UART_IRQHandler() {
    if (!LL_USART_IsActiveFlag_TC(USART1)) {
      return; // not us.
    }

    LL_USART_DisableIT_TC(USART1);

    // DMA finished, disable the channel.
    LL_DMA_DisableChannel(DMA_TARGET);
    CLEAR_TC_FLAG();
    sLogTx_DMABusy = false;

    // See if we got anything else to transmit in the interim.
    LogDMA_StartTx();
  }

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

    if (!sLogTx_DMABusy) {
      LogDMA_StartTx();
    }

    portEXIT_CRITICAL();
  }
#if 0
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
  #endif

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
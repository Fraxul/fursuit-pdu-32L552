#pragma once
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

void LogDMA_UART_IRQHandler();

void logprintf(const char* fmt, ...);
void log_append(const char*, size_t);
void log_read(const char** outChunk1, size_t* outChunk1Length, const char** outChunk2, size_t* outChunk2Length);

#ifdef __cplusplus
}
#endif

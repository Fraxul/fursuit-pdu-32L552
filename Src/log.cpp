#include "log.h"
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_SIZE 8192
char sLogBuffer[LOG_BUFFER_SIZE];
uint16_t sLogWritePtr = 0;

extern "C" {
  void log_append(const char* data, size_t length) {
    if (!length)
      return;
    if ((sLogWritePtr + length) < LOG_BUFFER_SIZE) {
      memcpy(sLogBuffer + sLogWritePtr, data, length);
      sLogWritePtr += length;
      return;
    }

    uint16_t write1Len = LOG_BUFFER_SIZE - sLogWritePtr;
    memcpy(sLogBuffer + sLogWritePtr, data, write1Len);
    length -= write1Len;
    memcpy(sLogBuffer, data + write1Len, length);
    sLogWritePtr = length;
  }

  void log_read(const char** outChunk1, size_t* outChunk1Length, const char** outChunk2, size_t* outChunk2Length) {
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
  }

}
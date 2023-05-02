#pragma once
#include <microshell.h>

ush_object* ushInstance();
static constexpr uint32_t ushFileScratchLen = 128;
uint8_t* ushFileScratchBuf();

// ush_file_descriptor[] helpers

#define File_RO(FileName, FormatStr, ...) { .name = #FileName, .description = nullptr, .help = nullptr, .exec = nullptr, \
 .get_data = [](struct ush_object* self, struct ush_file_descriptor const* file, uint8_t** data) -> size_t { \
    *data = ushFileScratchBuf(); \
    return snprintf((char*) *data, ushFileScratchLen, FormatStr, ##__VA_ARGS__ ); \
  }, .set_data = nullptr }

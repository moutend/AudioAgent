#pragma once

#include <cstdint>

typedef struct {
  int16_t Type;
  int16_t SoundIndex;
  double WaitDuration;
  int32_t SSMLLen;
  wchar_t *SSMLPtr;
} Command;

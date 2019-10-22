#pragma once

#include <cstdint>

#define export __declspec(dllexport)

typedef struct {
  int16_t Type;
  int16_t SoundIndex;
  double WaitDuration;
  int32_t SSMLLen;
  wchar_t *SSMLPtr;
} Command;

extern "C" {
export void __stdcall Start(int32_t *code, const wchar_t *logPath,
                            int32_t logLevel);
export void __stdcall Quit(int32_t *code);
export void __stdcall FadeIn(int32_t *code);
export void __stdcall FadeOut(int32_t *code);
export void __stdcall Feed(int32_t *code, Command **commandsPtr,
                           int32_t commandsLength);
}

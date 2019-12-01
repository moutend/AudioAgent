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
export void __stdcall GetVoiceCount(int32_t *code, int32_t *numberOfVoices);
export void __stdcall GetVoiceDisplayName(int32_t *code, int32_t index,
                                          wchar_t *displayName);
export void __stdcall GetVoiceDisplayNameLength(int32_t *code, int32_t index,
                                                int32_t *displayNameLength);
export void __stdcall GetVoiceId(int32_t *code, int32_t index, wchar_t *id);
export void __stdcall GetVoiceIdLength(int32_t *code, int32_t index,
                                       int32_t *idLength);
export void __stdcall GetVoiceLanguage(int32_t *code, int32_t index,
                                       wchar_t *language);
export void __stdcall GetVoiceLanguageLength(int32_t *code, int32_t index,
                                             int32_t *languageLength);
}

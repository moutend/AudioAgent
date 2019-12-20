#pragma once

#include <cstdint>

#include "types.h"

#define export __declspec(dllexport)

extern "C" {
export void __stdcall Setup(int32_t *code, const wchar_t *fullLogPath,
                            int32_t logLevel);
export void __stdcall Teardown(int32_t *code);

export void __stdcall FadeIn(int32_t *code);
export void __stdcall FadeOut(int32_t *code);

export void __stdcall Push(int32_t *code, Command **commandsPtr,
                           int32_t commandsLength, bool isForcePush);

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

export void __stdcall GetDefaultVoice(int32_t *code, int32_t *index);
export void __stdcall SetDefaultVoice(int32_t *code, int32_t index);

export void __stdcall GetSpeakingRate(int32_t *code, int32_t index,
                                      double *rate);
export void __stdcall SetSpeakingRate(int32_t *code, int32_t index,
                                      double rate);

export void __stdcall GetAudioPitch(int32_t *code, int32_t index,
                                    double *audioPitch);
export void __stdcall SetAudioPitch(int32_t *code, int32_t index,
                                    double audioPitch);

export void __stdcall GetAudioVolume(int32_t *code, int32_t index,
                                     double *audioVolume);
export void __stdcall SetAudioVolume(int32_t *code, int32_t index,
                                     double audioVolume);
}

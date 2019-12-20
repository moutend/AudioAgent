#pragma once

#include <engine.h>
#include <windows.h>

#include "types.h"

struct LogLoopContext {
  HANDLE FlushEvent = nullptr;
  wchar_t *FullLogPath = nullptr;
};

struct VoiceProperty {
  wchar_t *Id = nullptr;
  wchar_t *DisplayName = nullptr;
  wchar_t *Language = nullptr;
  double SpeakingRate = 1.0;
  double AudioVolume = 1.0;
  double AudioPitch = 1.0;
};

struct VoiceInfoContext {
  unsigned int Count = 0;
  unsigned int DefaultVoiceIndex = 0;
  VoiceProperty **VoiceProperties = nullptr;
};

struct VoiceLoopContext {
  HANDLE FeedEvent = nullptr;
  HANDLE NextEvent = nullptr;
  HANDLE QuitEvent = nullptr;
  bool IsSSML = false;
  wchar_t *BufferPtr = nullptr;
  int32_t BufferLen = 0;
  PCMAudio::RingEngine *VoiceEngine = nullptr;
  unsigned int VoiceCount = 0;
  VoiceInfoContext *VoiceInfoCtx = nullptr;
};

struct SoundLoopContext {
  HANDLE FeedEvent = nullptr;
  HANDLE NextEvent = nullptr;
  HANDLE QuitEvent = nullptr;
  int16_t SoundIndex = 0;
  PCMAudio::LauncherEngine *SoundEngine = nullptr;
};

struct CommandLoopContext {
  HANDLE PushEvent = nullptr;
  HANDLE ForcePushEvent = nullptr;
  HANDLE QuitEvent = nullptr;
  VoiceLoopContext *VoiceLoopCtx = nullptr;
  SoundLoopContext *SoundLoopCtx = nullptr;
  Command **Commands = nullptr;
  int32_t ReadIndex = 0;
  int32_t WriteIndex = 0;
  int32_t MaxCommands = 256;
};

struct AudioLoopContext {
  HANDLE NextEvent = nullptr;
  HANDLE QuitEvent = nullptr;
  PCMAudio::Engine *Engine = nullptr;
};

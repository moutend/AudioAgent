#pragma once

#include <engine.h>
#include <windows.h>

#include "api.h"

struct LogLoopContext {
  HANDLE FlushEvent = nullptr;
  const wchar_t *FullLogPath = nullptr;
};

struct VoiceLoopContext {
  HANDLE FeedEvent = nullptr;
  HANDLE NextEvent = nullptr;
  HANDLE QuitEvent = nullptr;
  wchar_t *BufferPtr = nullptr;
  int32_t BufferLen = 0;
  PCMAudio::RingEngine *VoiceEngine = nullptr;
};

struct SoundLoopContext {
  HANDLE FeedEvent = nullptr;
  HANDLE NextEvent = nullptr;
  HANDLE QuitEvent = nullptr;
  int16_t SoundIndex = 0;
  PCMAudio::LauncherEngine *SoundEngine = nullptr;
};

struct CommandLoopContext {
  HANDLE CommandEvent = nullptr;
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

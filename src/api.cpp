#include <cpplogger.h>
#include <cstring>
#include <engine.h>
#include <mutex>
#include <sstream>
#include <windows.h>

#include "api.h"
#include "audioloop.h"
#include "commandloop.h"
#include "context.h"
#include "logloop.h"
#include "soundloop.h"
#include "util.h"
#include "voiceloop.h"

extern Logger::Logger *Log;

bool isActive{false};
std::mutex apiMutex;

LogLoopContext *logLoopCtx{nullptr};
CommandLoopContext *commandLoopCtx{nullptr};
VoiceLoopContext *voiceLoopCtx{nullptr};
SoundLoopContext *soundLoopCtx{nullptr};
AudioLoopContext *voiceRenderCtx{nullptr};
AudioLoopContext *soundRenderCtx{nullptr};

HANDLE logLoopThread{nullptr};
HANDLE commandLoopThread{nullptr};
HANDLE voiceLoopThread{nullptr};
HANDLE soundLoopThread{nullptr};
HANDLE voiceRenderThread{nullptr};
HANDLE soundRenderThread{nullptr};

HANDLE nextVoiceEvent{nullptr};
HANDLE nextSoundEvent{nullptr};

PCMAudio::RingEngine *voiceEngine{nullptr};
PCMAudio::LauncherEngine *soundEngine{nullptr};

void __stdcall Start(int32_t *code, const wchar_t *logPath, int32_t logLevel) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (isActive) {
    Log->Warn(L"Already initialized", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return;
  }

  Log = new Logger::Logger();

  Log->Info(L"Initialize voice and sound threads", GetCurrentThreadId(),
            __LINE__, __WFILE__);

  logLoopCtx = new LogLoopContext();

  logLoopCtx->FullLogPath = logPath;

  logLoopThread = CreateThread(nullptr, 0, logLoop,
                               static_cast<void *>(logLoopCtx), 0, nullptr);

  if (logLoopThread == nullptr) {
    Log->Fail(L"Failed to create log loop thread", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -5;
    return;
  }

  voiceEngine = new PCMAudio::RingEngine();

  nextVoiceEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (nextVoiceEvent == nullptr) {
    Log->Fail(L"Failed to create next event for voice threads",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -2;
    return;
  }

  voiceLoopCtx = new VoiceLoopContext();
  voiceLoopCtx->NextEvent = nextVoiceEvent;
  voiceLoopCtx->VoiceEngine = voiceEngine;

  voiceLoopCtx->FeedEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceLoopCtx->FeedEvent == nullptr) {
    Log->Fail(L"Failed to create feed event for voice loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -3;
    return;
  }

  voiceLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create quit event for voice loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -4;
    return;
  }

  Log->Info(L"Create voice loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  voiceLoopThread = CreateThread(nullptr, 0, voiceLoop,
                                 static_cast<void *>(voiceLoopCtx), 0, nullptr);

  if (voiceLoopThread == nullptr) {
    Log->Fail(L"Failed to create voice loop thread", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -5;
    return;
  }

  voiceRenderCtx = new AudioLoopContext();
  voiceRenderCtx->NextEvent = nextVoiceEvent;
  voiceRenderCtx->Engine = voiceEngine;

  voiceRenderCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceRenderCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create quit event for voice render thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -6;
    return;
  }

  Log->Info(L"Create voice render thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  voiceRenderThread = CreateThread(
      nullptr, 0, audioLoop, static_cast<void *>(voiceRenderCtx), 0, nullptr);

  if (voiceRenderThread == nullptr) {
    Log->Fail(L"Failed to create voice render thread", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -7;
    return;
  }

  soundEngine = new PCMAudio::LauncherEngine();
  soundEngine->Register(0, "input.wav");

  nextSoundEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (nextSoundEvent == nullptr) {
    Log->Fail(L"Failed to create next event for sound", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -8;
    return;
  }

  soundLoopCtx = new SoundLoopContext();
  soundLoopCtx->NextEvent = nextSoundEvent;
  soundLoopCtx->SoundEngine = soundEngine;

  soundLoopCtx->FeedEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (soundLoopCtx->FeedEvent == nullptr) {
    Log->Fail(L"Failed to create feed event for sound loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -9;
    return;
  }

  soundLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (soundLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create quit event for sound loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -10;
    return;
  }

  Log->Info(L"Create sound loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  soundLoopThread = CreateThread(nullptr, 0, soundLoop,
                                 static_cast<void *>(soundLoopCtx), 0, nullptr);

  if (soundLoopThread == nullptr) {
    Log->Fail(L"Failed to create sound loop thread", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -11;
    return;
  }

  soundRenderCtx = new AudioLoopContext();
  soundRenderCtx->NextEvent = nextSoundEvent;
  soundRenderCtx->Engine = soundEngine;

  soundRenderCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (soundRenderCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create quit event for sound render thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -12;
    return;
  }

  Log->Info(L"Create sound render thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  soundRenderThread = CreateThread(
      nullptr, 0, audioLoop, static_cast<void *>(soundRenderCtx), 0, nullptr);

  if (soundRenderThread == nullptr) {
    Log->Fail(L"Failed to create sound render thread", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -13;
    return;
  }

  commandLoopCtx = new CommandLoopContext();

  commandLoopCtx->CommandEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (commandLoopCtx->CommandEvent == nullptr) {
    Log->Fail(L"Failed to create command event for command loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -14;
    return;
  }

  commandLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (commandLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create quit event for command loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -15;
    return;
  }

  commandLoopCtx->VoiceLoopCtx = voiceLoopCtx;
  commandLoopCtx->SoundLoopCtx = soundLoopCtx;
  commandLoopCtx->Commands =
      new Command *[commandLoopCtx->MaxCommands] { nullptr };

  for (int32_t i = 0; i < commandLoopCtx->MaxCommands; i++) {
    commandLoopCtx->Commands[i] = new Command;
    commandLoopCtx->Commands[i]->Type = 0;
    commandLoopCtx->Commands[i]->SoundIndex = 0;
    commandLoopCtx->Commands[i]->WaitDuration = 0.0;
    commandLoopCtx->Commands[i]->SSMLPtr = nullptr;
    commandLoopCtx->Commands[i]->SSMLLen = 0;
  }

  Log->Info(L"Create command loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  commandLoopThread = CreateThread(
      nullptr, 0, commandLoop, static_cast<void *>(commandLoopCtx), 0, nullptr);

  if (commandLoopThread == nullptr) {
    Log->Fail(L"Failed to create command loop thread", GetCurrentThreadId(),
              __LINE__, __WFILE__);
    *code = -16;
    return;
  }

  isActive = true;
}

void __stdcall Quit(int32_t *code) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (!isActive) {
    *code = -1;
    return;
  }

  Log->Info(L"Send event (commandLoopCtx->QuitEvent)", GetCurrentThreadId(),
            __LINE__, __WFILE__);

  if (!SetEvent(commandLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send quit event to command loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -2;
    return;
  }

  WaitForSingleObject(commandLoopThread, INFINITE);
  CloseHandle(commandLoopThread);
  commandLoopThread = nullptr;

  Log->Info(L"Send event (voiceLoopCtx->QuitEvent)", GetCurrentThreadId(),
            __LINE__, __WFILE__);

  if (!SetEvent(voiceLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send quit event to voice loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -3;
    return;
  }

  WaitForSingleObject(voiceLoopThread, INFINITE);
  CloseHandle(voiceLoopThread);
  voiceLoopThread = nullptr;

  Log->Info(L"Send event (voiceRenderCtx->QuitEvent)", GetCurrentThreadId(),
            __LINE__, __WFILE__);

  if (!SetEvent(voiceRenderCtx->QuitEvent)) {
    Log->Fail(L"Failed to send quit event to voice render thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -4;
    return;
  }

  WaitForSingleObject(voiceRenderThread, INFINITE);
  CloseHandle(voiceRenderThread);
  voiceRenderThread = nullptr;

  Log->Info(L"Send event (soundLoopCtx->QuitEvent)", GetCurrentThreadId(),
            __LINE__, __WFILE__);

  if (!SetEvent(soundLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send quit event to sound loop thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -5;
    return;
  }

  WaitForSingleObject(soundLoopThread, INFINITE);
  CloseHandle(soundLoopThread);
  soundLoopThread = nullptr;

  Log->Info(L"Set event (soundRenderCtx->QuitEvent)", GetCurrentThreadId(),
            __LINE__, __WFILE__);

  if (!SetEvent(soundRenderCtx->QuitEvent)) {
    Log->Fail(L"Failed to send quit event to sound render thread",
              GetCurrentThreadId(), __LINE__, __WFILE__);
    *code = -6;
    return;
  }

  WaitForSingleObject(soundRenderThread, INFINITE);
  CloseHandle(soundRenderThread);
  soundRenderThread = nullptr;

  delete voiceEngine;
  voiceEngine = nullptr;

  delete soundEngine;
  soundEngine = nullptr;

  CloseHandle(nextVoiceEvent);
  nextVoiceEvent = nullptr;

  CloseHandle(nextSoundEvent);
  nextSoundEvent = nullptr;

  for (int32_t i = 0; i < commandLoopCtx->MaxCommands; i++) {
    delete[] commandLoopCtx->Commands[i]->SSMLPtr;
    commandLoopCtx->Commands[i]->SSMLPtr = nullptr;

    delete commandLoopCtx->Commands[i];
    commandLoopCtx->Commands[i] = nullptr;
  }

  delete commandLoopCtx;
  commandLoopCtx = nullptr;

  delete voiceLoopCtx;
  voiceLoopCtx = nullptr;

  delete soundLoopCtx;
  soundLoopCtx = nullptr;

  Log->Info(L"All threads are destroied", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  Log = nullptr;

  isActive = false;
}

void FadeIn(int32_t *code) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (!isActive) {
    *code = -1;
    return;
  }

  voiceEngine->FadeIn();
  soundEngine->FadeIn();
}

void FadeOut(int32_t *code) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (!isActive) {
    *code = -1;
    return;
  }

  voiceEngine->FadeOut();
  soundEngine->FadeOut();
}

void Feed(int32_t *code, Command **commandsPtr, int32_t commandsLength) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (!isActive) {
    *code = -1;
    return;
  }
  if (commandsPtr == nullptr || commandsLength <= 0) {
    *code = -2;
    return;
  }

  int32_t base = commandLoopCtx->WriteIndex;

  for (int32_t i = 0; i < commandsLength; i++) {
    int32_t offset = (base + i) % commandLoopCtx->MaxCommands;

    commandLoopCtx->Commands[offset]->Type = commandsPtr[i]->Type;

    switch (commandsPtr[i]->Type) {
    case 1:
      commandLoopCtx->Commands[offset]->SoundIndex = commandsPtr[i]->SoundIndex;
      break;
    case 2:
      commandLoopCtx->Commands[offset]->WaitDuration =
          commandsPtr[i]->WaitDuration;
      break;
    case 3:
      delete[] commandLoopCtx->Commands[offset]->SSMLPtr;
      commandLoopCtx->Commands[offset]->SSMLPtr = nullptr;
      commandLoopCtx->Commands[offset]->SSMLPtr =
          new wchar_t[commandsPtr[i]->SSMLLen];
      std::wmemcpy(commandLoopCtx->Commands[offset]->SSMLPtr,
                   commandsPtr[i]->SSMLPtr, commandsPtr[i]->SSMLLen);
      commandLoopCtx->Commands[offset]->SSMLLen = commandsPtr[i]->SSMLLen;

      break;
    }

    commandLoopCtx->WriteIndex =
        (commandLoopCtx->WriteIndex + 1) % commandLoopCtx->MaxCommands;
  }

  commandLoopCtx->ReadIndex = base;

  if (!SetEvent(commandLoopCtx->CommandEvent)) {
    Log->Fail(L"Failed to set command event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -3;
    return;
  }
}

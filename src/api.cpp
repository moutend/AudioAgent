#include <cpplogger.h>
#include <cstring>
#include <engine.h>
#include <mutex>
#include <sstream>
#include <windows.h>

#include <strsafe.h>

#include "api.h"
#include "audioloop.h"
#include "commandloop.h"
#include "context.h"
#include "logloop.h"
#include "soundloop.h"
#include "util.h"
#include "voiceinfo.h"
#include "voiceloop.h"

extern Logger::Logger *Log;

bool isActive{false};
std::mutex apiMutex;

LogLoopContext *logLoopCtx{nullptr};
CommandLoopContext *commandLoopCtx{nullptr};
VoiceInfoContext *voiceInfoCtx{nullptr};
VoiceLoopContext *voiceLoopCtx{nullptr};
SoundLoopContext *soundLoopCtx{nullptr};
AudioLoopContext *voiceRenderCtx{nullptr};
AudioLoopContext *soundRenderCtx{nullptr};

HANDLE logLoopThread{nullptr};
HANDLE commandLoopThread{nullptr};
HANDLE voiceInfoThread{nullptr};
HANDLE voiceLoopThread{nullptr};
HANDLE soundLoopThread{nullptr};
HANDLE voiceRenderThread{nullptr};
HANDLE soundRenderThread{nullptr};

HANDLE nextVoiceEvent{nullptr};
HANDLE nextSoundEvent{nullptr};

PCMAudio::RingEngine *voiceEngine{nullptr};
PCMAudio::LauncherEngine *soundEngine{nullptr};

void __stdcall Setup(int32_t *code, const wchar_t *fullLogPath,
                     int32_t logLevel) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
  if (fullLogPath == nullptr) {
    *code = -1;
    return;
  }
  if (isActive) {
    Log->Warn(L"Already initialized", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  Log = new Logger::Logger();

  Log->Info(L"Setup audio node", GetCurrentThreadId(), __LINE__, __WFILE__);

  logLoopCtx = new LogLoopContext();

  size_t fullLogPathLen = std::wcslen(fullLogPath) + 1;
  logLoopCtx->FullLogPath = new wchar_t[fullLogPathLen];
  std::wmemcpy(logLoopCtx->FullLogPath, fullLogPath, fullLogPathLen);

  Log->Info(L"Create log loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  logLoopThread = CreateThread(nullptr, 0, logLoop,
                               static_cast<void *>(logLoopCtx), 0, nullptr);

  if (logLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  voiceInfoCtx = new VoiceInfoContext();

  Log->Info(L"Create voice info loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  voiceInfoThread = CreateThread(nullptr, 0, voiceInfo,
                                 static_cast<void *>(voiceInfoCtx), 0, nullptr);

  if (voiceInfoThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(voiceInfoThread, INFINITE);
  CloseHandle(voiceInfoThread);
  voiceInfoThread = nullptr;

  Log->Info(L"Delete voice info thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  voiceEngine = new PCMAudio::RingEngine();

  nextVoiceEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (nextVoiceEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  voiceLoopCtx = new VoiceLoopContext();
  voiceLoopCtx->NextEvent = nextVoiceEvent;
  voiceLoopCtx->VoiceEngine = voiceEngine;
  voiceLoopCtx->VoiceInfoCtx = voiceInfoCtx;

  voiceLoopCtx->FeedEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceLoopCtx->FeedEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  voiceLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create voice loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  voiceLoopThread = CreateThread(nullptr, 0, voiceLoop,
                                 static_cast<void *>(voiceLoopCtx), 0, nullptr);

  if (voiceLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  voiceRenderCtx = new AudioLoopContext();
  voiceRenderCtx->NextEvent = nextVoiceEvent;
  voiceRenderCtx->Engine = voiceEngine;

  voiceRenderCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceRenderCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create voice render thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  voiceRenderThread = CreateThread(
      nullptr, 0, audioLoop, static_cast<void *>(voiceRenderCtx), 0, nullptr);

  if (voiceRenderThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  soundEngine = new PCMAudio::LauncherEngine();

  for (int i = 0; i < 128; i++) {
    wchar_t *filename = new wchar_t[256]{};
    HRESULT hr = StringCbPrintfW(filename, 255, L"waves\\%03d.wav", i + 1);

    if (FAILED(hr)) {
      Log->Fail(L"Failed to build filename", GetCurrentThreadId(), __LINE__,
                __WFILE__);
      *code = -1;
      return;
    }

    soundEngine->Register(i, filename);

    delete[] filename;
    filename = nullptr;
  }

  nextSoundEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (nextSoundEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  soundLoopCtx = new SoundLoopContext();
  soundLoopCtx->NextEvent = nextSoundEvent;
  soundLoopCtx->SoundEngine = soundEngine;

  soundLoopCtx->FeedEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (soundLoopCtx->FeedEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  soundLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (soundLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create sound loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  soundLoopThread = CreateThread(nullptr, 0, soundLoop,
                                 static_cast<void *>(soundLoopCtx), 0, nullptr);

  if (soundLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  soundRenderCtx = new AudioLoopContext();
  soundRenderCtx->NextEvent = nextSoundEvent;
  soundRenderCtx->Engine = soundEngine;

  soundRenderCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (soundRenderCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create sound render thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  soundRenderThread = CreateThread(
      nullptr, 0, audioLoop, static_cast<void *>(soundRenderCtx), 0, nullptr);

  if (soundRenderThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  commandLoopCtx = new CommandLoopContext();

  commandLoopCtx->CommandEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (commandLoopCtx->CommandEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  commandLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (commandLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
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
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    *code = -1;
    return;
  }

  isActive = true;
  *code = 0;
}

void __stdcall Teardown(int32_t *code) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
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

void __stdcall FadeIn(int32_t *code) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
  if (!isActive) {
    *code = -1;
    return;
  }
  if (Log != nullptr) {
    Log->Info(L"Called FadeIn", GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  voiceEngine->FadeIn();
  soundEngine->FadeIn();
}

void __stdcall FadeOut(int32_t *code) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
  if (!isActive) {
    *code = -1;
    return;
  }
  if (Log != nullptr) {
    Log->Info(L"Called FadeOut", GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  voiceEngine->FadeOut();
  soundEngine->FadeOut();
}

void __stdcall Feed(int32_t *code, Command **commandsPtr,
                    int32_t commandsLength) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
  if (!isActive) {
    *code = -1;
    return;
  }
  if (commandsPtr == nullptr || commandsLength <= 0) {
    *code = -2;
    return;
  }
  if (Log != nullptr) {
    Log->Info(L"Called Feed", GetCurrentThreadId(), __LINE__, __WFILE__);
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

void __stdcall GetVoiceCount(int32_t *code, int32_t *numberOfVoices) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (numberOfVoices == nullptr) {
    *code = -1;
    return;
  }
  if (voiceInfoCtx == nullptr) {
    *code = -2;
    return;
  }
  if (Log != nullptr) {
    Log->Info(L"Called GetVoiceCount", GetCurrentThreadId(), __LINE__,
              __WFILE__);
  }

  *numberOfVoices = voiceInfoCtx->Count;
}

void __stdcall GetVoiceDisplayName(int32_t *code, int32_t index,
                                   wchar_t *displayName) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetVoiceDisplayName (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  size_t displayNameLength =
      wcslen(voiceInfoCtx->VoiceProperties[index]->DisplayName);
  std::wmemcpy(displayName, voiceInfoCtx->VoiceProperties[index]->DisplayName,
               displayNameLength);
}

void __stdcall GetVoiceDisplayNameLength(int32_t *code, int32_t index,
                                         int32_t *displayNameLength) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr = StringCbPrintfW(
      s, 256, L"Called GetVoiceDisplayNameLength (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  *displayNameLength = static_cast<int32_t>(
      wcslen(voiceInfoCtx->VoiceProperties[index]->DisplayName));
}

void __stdcall GetVoiceId(int32_t *code, int32_t index, wchar_t *id) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr = StringCbPrintfW(s, 256, L"Called GetVoiceId (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  size_t idLength =
      static_cast<int32_t>(wcslen(voiceInfoCtx->VoiceProperties[index]->Id));
  std::wmemcpy(id, voiceInfoCtx->VoiceProperties[index]->Id, idLength);
}

void __stdcall GetVoiceIdLength(int32_t *code, int32_t index,
                                int32_t *idLength) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetVoiceIdLength (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  *idLength =
      static_cast<int32_t>(wcslen(voiceInfoCtx->VoiceProperties[index]->Id));
}

void __stdcall GetVoiceLanguage(int32_t *code, int32_t index,
                                wchar_t *language) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetVoiceLanguage (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  size_t languageLength =
      wcslen(voiceInfoCtx->VoiceProperties[index]->Language);
  std::wmemcpy(language, voiceInfoCtx->VoiceProperties[index]->Language,
               languageLength);
}

void __stdcall GetVoiceLanguageLength(int32_t *code, int32_t index,
                                      int32_t *languageLength) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr = StringCbPrintfW(
      s, 256, L"Called GetVoiceLanguageLength (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  *languageLength = static_cast<int32_t>(
      wcslen(voiceInfoCtx->VoiceProperties[index]->Language));
}

void __stdcall GetDefaultVoice(int32_t *code, int32_t *index) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index == nullptr) {
    *code = -2;
    return;
  }
  if (Log != nullptr) {
    Log->Info(L"Called GetDefaultVoice", GetCurrentThreadId(), __LINE__,
              __WFILE__);
  }

  *index = voiceInfoCtx->DefaultVoiceIndex;
}

void __stdcall SetDefaultVoice(int32_t *code, int32_t index) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (Log != nullptr) {
    Log->Info(L"Called GetDefaultVoice", GetCurrentThreadId(), __LINE__,
              __WFILE__);
  }

  voiceInfoCtx->DefaultVoiceIndex = index;
}

void __stdcall GetSpeakingRate(int32_t *code, int32_t index, double *rate) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetSpeakingRate (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  *rate = voiceInfoCtx->VoiceProperties[index]->SpeakingRate;
}

void __stdcall SetSpeakingRate(int32_t *code, int32_t index, double rate) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr = StringCbPrintfW(
      s, 256, L"Called SetSpeakingRate (index=%d, rate=%.2f)", index, rate);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  if (rate < 0.5) {
    rate = 0.5;
  }
  if (rate > 6.0) {
    rate = 6.0;
  }
  voiceInfoCtx->VoiceProperties[index]->SpeakingRate = rate;
}

void __stdcall GetAudioPitch(int32_t *code, int32_t index, double *audioPitch) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetAudioPitch (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  *audioPitch = voiceInfoCtx->VoiceProperties[index]->AudioPitch;
}

void __stdcall SetAudioPitch(int32_t *code, int32_t index, double audioPitch) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr = StringCbPrintfW(
      s, 256, L"Called SetAudioPitch (index=%d, audioPitch=%.2f)", index,
      audioPitch);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  if (audioPitch < 0.0) {
    audioPitch = 0.0;
  }
  if (audioPitch > 2.0) {
    audioPitch = 2.0;
  }

  voiceInfoCtx->VoiceProperties[index]->AudioPitch = audioPitch;
}

void __stdcall GetAudioVolume(int32_t *code, int32_t index,
                              double *audioVolume) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetAudioVolume (index=%d)", index);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  *audioVolume = voiceInfoCtx->VoiceProperties[index]->AudioVolume;
}

void __stdcall SetAudioVolume(int32_t *code, int32_t index,
                              double audioVolume) {
  if (code == nullptr) {
    return;
  }

  *code = 0;

  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -3;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr = StringCbPrintfW(
      s, 256, L"Called SetAudioVolume (index=%d, audioVolume=%.2f)", index,
      audioVolume);

  if (FAILED(hr)) {
    *code = -4;
    return;
  }
  if (Log != nullptr) {
    Log->Info(s, GetCurrentThreadId(), __LINE__, __WFILE__);
  }

  delete[] s;
  s = nullptr;

  if (audioVolume < 0.0) {
    audioVolume = 0.0;
  }
  if (audioVolume > 1.0) {
    audioVolume = 1.0;
  }

  voiceInfoCtx->VoiceProperties[index]->AudioVolume = audioVolume;
}

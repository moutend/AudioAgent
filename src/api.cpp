#include <cppaudio/engine.h>
#include <cpplogger/cpplogger.h>
#include <cstring>
#include <fstream>
#include <mutex>
#include <windows.h>

#include <strsafe.h>

#include "api.h"
#include "audioloop.h"
#include "commandloop.h"
#include "context.h"
#include "logloop.h"
#include "sfxloop.h"
#include "util.h"
#include "voiceinfo.h"
#include "voiceloop.h"

extern Logger::Logger *Log;

int16_t maxWaves = 128;
bool isActive{false};
std::mutex apiMutex;

LogLoopContext *logLoopCtx{nullptr};
CommandLoopContext *commandLoopCtx{nullptr};
VoiceInfoContext *voiceInfoCtx{nullptr};
VoiceLoopContext *voiceLoopCtx{nullptr};
SFXLoopContext *sfxLoopCtx{nullptr};
AudioLoopContext *voiceRenderCtx{nullptr};
AudioLoopContext *sfxRenderCtx{nullptr};

HANDLE logLoopThread{nullptr};
HANDLE commandLoopThread{nullptr};
HANDLE voiceInfoThread{nullptr};
HANDLE voiceLoopThread{nullptr};
HANDLE voiceRenderThread{nullptr};
HANDLE sfxLoopThread{nullptr};
HANDLE sfxRenderThread{nullptr};

HANDLE nextVoiceEvent{nullptr};
HANDLE nextSoundEvent{nullptr};

PCMAudio::RingEngine *voiceEngine{nullptr};
PCMAudio::LauncherEngine *sfxEngine{nullptr};

void __stdcall Setup(int32_t *code, int32_t logLevel) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
  if (isActive) {
    Log->Warn(L"Already initialized", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  isActive = true;

  Log = new Logger::Logger(L"AudioNode", L"v0.1.0-develop", 4096);

  Log->Info(L"Setup AudioNode", GetCurrentThreadId(), __LONGFILE__);

  logLoopCtx = new LogLoopContext();

  logLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (logLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create log loop thread", GetCurrentThreadId(), __LONGFILE__);

  logLoopThread = CreateThread(nullptr, 0, logLoop,
                               static_cast<void *>(logLoopCtx), 0, nullptr);

  if (logLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  voiceInfoCtx = new VoiceInfoContext();

  Log->Info(L"Create voice info loop thread", GetCurrentThreadId(),
            __LONGFILE__);

  voiceInfoThread = CreateThread(nullptr, 0, voiceInfo,
                                 static_cast<void *>(voiceInfoCtx), 0, nullptr);

  if (voiceInfoThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(voiceInfoThread, INFINITE);
  SafeCloseHandle(&voiceInfoThread);

  Log->Info(L"Delete voice info thread", GetCurrentThreadId(), __LONGFILE__);

  voiceEngine = new PCMAudio::RingEngine();

  nextVoiceEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (nextVoiceEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
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
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  voiceLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create voice loop thread", GetCurrentThreadId(), __LONGFILE__);

  voiceLoopThread = CreateThread(nullptr, 0, voiceLoop,
                                 static_cast<void *>(voiceLoopCtx), 0, nullptr);

  if (voiceLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  voiceRenderCtx = new AudioLoopContext();
  voiceRenderCtx->NextEvent = nextVoiceEvent;
  voiceRenderCtx->Engine = voiceEngine;

  voiceRenderCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (voiceRenderCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create voice render thread", GetCurrentThreadId(), __LONGFILE__);

  voiceRenderThread = CreateThread(
      nullptr, 0, audioLoop, static_cast<void *>(voiceRenderCtx), 0, nullptr);

  if (voiceRenderThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  sfxEngine = new PCMAudio::LauncherEngine(maxWaves);

  for (int16_t i = 0; i < maxWaves; i++) {
    wchar_t *filePath = new wchar_t[256]{};
    HRESULT hr = StringCbPrintfW(filePath, 255, L"waves\\%03d.wav", i + 1);

    if (FAILED(hr)) {
      Log->Fail(L"Failed to build file path", GetCurrentThreadId(),
                __LONGFILE__);
      continue;
    }

    std::ifstream file(filePath, std::ios::binary | std::ios::in);

    if (!sfxEngine->Register(i, file)) {
      Log->Fail(L"Failed to register", GetCurrentThreadId(), __LONGFILE__);
      continue;
    }

    Log->Info(filePath, GetCurrentThreadId(), __LONGFILE__);

    delete[] filePath;
    filePath = nullptr;

    file.close();
  }

  nextSoundEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (nextSoundEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  sfxLoopCtx = new SFXLoopContext();
  sfxLoopCtx->NextEvent = nextSoundEvent;
  sfxLoopCtx->SFXEngine = sfxEngine;

  sfxLoopCtx->FeedEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (sfxLoopCtx->FeedEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  sfxLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (sfxLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create sfx loop thread", GetCurrentThreadId(), __LONGFILE__);

  sfxLoopThread = CreateThread(nullptr, 0, sfxLoop,
                               static_cast<void *>(sfxLoopCtx), 0, nullptr);

  if (sfxLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  sfxRenderCtx = new AudioLoopContext();
  sfxRenderCtx->NextEvent = nextSoundEvent;
  sfxRenderCtx->Engine = sfxEngine;

  sfxRenderCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (sfxRenderCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Create SFX render thread", GetCurrentThreadId(), __LONGFILE__);

  sfxRenderThread = CreateThread(nullptr, 0, audioLoop,
                                 static_cast<void *>(sfxRenderCtx), 0, nullptr);

  if (sfxRenderThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  commandLoopCtx = new CommandLoopContext();

  commandLoopCtx->PushEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (commandLoopCtx->PushEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  commandLoopCtx->QuitEvent =
      CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

  if (commandLoopCtx->QuitEvent == nullptr) {
    Log->Fail(L"Failed to create event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  commandLoopCtx->VoiceLoopCtx = voiceLoopCtx;
  commandLoopCtx->SFXLoopCtx = sfxLoopCtx;
  commandLoopCtx->Commands =
      new Command *[commandLoopCtx->MaxCommands] { nullptr };

  for (int32_t i = 0; i < commandLoopCtx->MaxCommands; i++) {
    commandLoopCtx->Commands[i] = new Command;
    commandLoopCtx->Commands[i]->Type = 0;
    commandLoopCtx->Commands[i]->SFXIndex = 0;
    commandLoopCtx->Commands[i]->WaitDuration = 0.0;
    commandLoopCtx->Commands[i]->Text = nullptr;
  }

  Log->Info(L"Create command loop thread", GetCurrentThreadId(), __LONGFILE__);

  commandLoopThread = CreateThread(
      nullptr, 0, commandLoop, static_cast<void *>(commandLoopCtx), 0, nullptr);

  if (commandLoopThread == nullptr) {
    Log->Fail(L"Failed to create thread", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  Log->Info(L"Complete setup AudioNode", GetCurrentThreadId(), __LONGFILE__);
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

  Log->Info(L"Teardown AudioNode", GetCurrentThreadId(), __LONGFILE__);

  if (commandLoopThread == nullptr) {
    goto END_COMMANDLOOP_CLEANUP;
  }
  if (!SetEvent(commandLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(commandLoopThread, INFINITE);
  SafeCloseHandle(&commandLoopThread);

  for (int32_t i = 0; i < commandLoopCtx->MaxCommands; i++) {
    delete[] commandLoopCtx->Commands[i]->Text;
    commandLoopCtx->Commands[i]->Text = nullptr;

    delete commandLoopCtx->Commands[i];
    commandLoopCtx->Commands[i] = nullptr;
  }

  SafeCloseHandle(&(commandLoopCtx->PushEvent));
  SafeCloseHandle(&(commandLoopCtx->QuitEvent));

  delete commandLoopCtx;
  commandLoopCtx = nullptr;

  Log->Info(L"Delete command loop thread", GetCurrentThreadId(), __LONGFILE__);

END_COMMANDLOOP_CLEANUP:

  if (voiceLoopThread == nullptr) {
    goto END_VOICELOOP_CLEANUP;
  }
  if (!SetEvent(voiceLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(voiceLoopThread, INFINITE);
  SafeCloseHandle(&voiceLoopThread);

  SafeCloseHandle(&(voiceLoopCtx->QuitEvent));
  SafeCloseHandle(&(voiceLoopCtx->NextEvent));
  SafeCloseHandle(&(voiceLoopCtx->FeedEvent));

  for (unsigned int i = 0; i < voiceInfoCtx->Count; i++) {
    delete[] voiceInfoCtx->VoiceProperties[i]->Id;
    voiceInfoCtx->VoiceProperties[i]->Id = nullptr;

    delete[] voiceInfoCtx->VoiceProperties[i]->DisplayName;
    voiceInfoCtx->VoiceProperties[i]->DisplayName = nullptr;

    delete[] voiceInfoCtx->VoiceProperties[i]->Language;
    voiceInfoCtx->VoiceProperties[i]->Language = nullptr;
  }

  delete[] voiceInfoCtx->VoiceProperties;
  voiceInfoCtx->VoiceProperties = nullptr;

  delete voiceInfoCtx;
  voiceInfoCtx = nullptr;

  delete voiceLoopCtx;
  voiceLoopCtx = nullptr;

  Log->Info(L"Delete voice loop thread", GetCurrentThreadId(), __LONGFILE__);

END_VOICELOOP_CLEANUP:

  if (voiceRenderThread == nullptr) {
    goto END_VOICERENDER_CLEANUP;
  }
  if (!SetEvent(voiceRenderCtx->QuitEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(voiceRenderThread, INFINITE);
  SafeCloseHandle(&voiceRenderThread);

  SafeCloseHandle(&(voiceRenderCtx->NextEvent));
  SafeCloseHandle(&(voiceRenderCtx->QuitEvent));

  delete voiceRenderCtx;
  voiceRenderCtx = nullptr;

  delete voiceEngine;
  voiceEngine = nullptr;

  Log->Info(L"Delete voice render thread", GetCurrentThreadId(), __LONGFILE__);

END_VOICERENDER_CLEANUP:

  if (sfxLoopThread == nullptr) {
    goto END_SFXLOOP_CLEANUP;
  }
  if (!SetEvent(sfxLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(sfxLoopThread, INFINITE);
  SafeCloseHandle(&sfxLoopThread);

  SafeCloseHandle(&(sfxLoopCtx->FeedEvent));
  SafeCloseHandle(&(sfxLoopCtx->NextEvent));
  SafeCloseHandle(&(sfxLoopCtx->QuitEvent));

  delete sfxLoopCtx;
  sfxLoopCtx = nullptr;

  Log->Info(L"Delete SFX loop thread", GetCurrentThreadId(), __LONGFILE__);

END_SFXLOOP_CLEANUP:

  if (sfxRenderThread == nullptr) {
    goto END_SFXRENDER_CLEANUP;
  }
  if (!SetEvent(sfxRenderCtx->QuitEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(sfxRenderThread, INFINITE);
  SafeCloseHandle(&sfxRenderThread);

  SafeCloseHandle(&(sfxRenderCtx->NextEvent));
  SafeCloseHandle(&(sfxRenderCtx->QuitEvent));

  delete sfxEngine;
  sfxEngine = nullptr;

  Log->Info(L"Delete SFX render thread", GetCurrentThreadId(), __LONGFILE__);

END_SFXRENDER_CLEANUP:

  SafeCloseHandle(&nextVoiceEvent);
  SafeCloseHandle(&nextSoundEvent);

  Log->Info(L"Complete teardown AudioNode", GetCurrentThreadId(), __LONGFILE__);

  if (logLoopThread == nullptr) {
    goto END_LOGLOOP_CLEANUP;
  }
  if (!SetEvent(logLoopCtx->QuitEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  WaitForSingleObject(logLoopThread, INFINITE);
  SafeCloseHandle(&logLoopThread);

  SafeCloseHandle(&(logLoopCtx->QuitEvent));

  delete logLoopCtx;
  logLoopCtx = nullptr;

END_LOGLOOP_CLEANUP:

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

  Log->Info(L"Called FadeIn()", GetCurrentThreadId(), __LONGFILE__);

  voiceEngine->FadeIn();
  sfxEngine->FadeIn();

  *code = 0;
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

  Log->Info(L"Called FadeOut()", GetCurrentThreadId(), __LONGFILE__);

  voiceEngine->FadeOut();
  sfxEngine->FadeOut();

  *code = 0;
}

void __stdcall Push(int32_t *code, Command **commandsPtr,
                    int32_t commandsLength, int32_t isForcePush) {
  std::lock_guard<std::mutex> lock(apiMutex);

  if (code == nullptr) {
    return;
  }
  if (!isActive) {
    *code = -1;
    return;
  }
  if (commandsPtr == nullptr || commandsLength <= 0) {
    *code = -1;
    return;
  }

  wchar_t *msg = new wchar_t[256]{};

  HRESULT hr = StringCbPrintfW(
      msg, 255, L"Called Push Read=%d,Write=%d,IsForce=%d",
      commandLoopCtx->ReadIndex, commandLoopCtx->WriteIndex, isForcePush);

  if (FAILED(hr)) {
    *code = -1;
    return;
  }

  Log->Info(msg, GetCurrentThreadId(), __LONGFILE__);

  delete[] msg;
  msg = nullptr;

  bool isIdle = commandLoopCtx->IsIdle;
  int32_t base = commandLoopCtx->WriteIndex;
  size_t textLen{};

  for (int32_t i = 0; i < commandsLength; i++) {
    int32_t offset = (base + i) % commandLoopCtx->MaxCommands;

    commandLoopCtx->Commands[offset]->Type = commandsPtr[i]->Type;

    switch (commandsPtr[i]->Type) {
    case 1:
      commandLoopCtx->Commands[offset]->SFXIndex =
          commandsPtr[i]->SFXIndex <= 0 ? 0 : commandsPtr[i]->SFXIndex - 1;
      break;
    case 2:
      commandLoopCtx->Commands[offset]->WaitDuration =
          commandsPtr[i]->WaitDuration;
      break;
    case 3: // Generate voice from plain text
    case 4: // Generate voice from SSML
      delete[] commandLoopCtx->Commands[offset]->Text;
      commandLoopCtx->Commands[offset]->Text = nullptr;

      textLen = std::wcslen(commandsPtr[i]->Text) + 1;
      commandLoopCtx->Commands[offset]->Text = new wchar_t[textLen];
      std::wmemcpy(commandLoopCtx->Commands[offset]->Text, commandsPtr[i]->Text,
                   textLen);

      break;
    default:
      // do nothing
      continue;
    }

    commandLoopCtx->WriteIndex =
        (commandLoopCtx->WriteIndex + 1) % commandLoopCtx->MaxCommands;
  }
  if (isForcePush) {
    commandLoopCtx->ReadIndex = base;
  }
  if (!isForcePush && !isIdle) {
    *code = 0;
    return;
  }
  if (!SetEvent(commandLoopCtx->PushEvent)) {
    Log->Fail(L"Failed to send event", GetCurrentThreadId(), __LONGFILE__);
    *code = -1;
    return;
  }

  commandLoopCtx->IsIdle = false;
  *code = 0;
}

void __stdcall GetVoiceCount(int32_t *code, int32_t *numberOfVoices) {
  if (code == nullptr) {
    return;
  }
  if (numberOfVoices == nullptr) {
    *code = -1;
    return;
  }
  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }

  Log->Info(L"Called GetVoiceCount()", GetCurrentThreadId(), __LONGFILE__);

  *numberOfVoices = voiceInfoCtx->Count;
  *code = 0;
}

void __stdcall GetVoiceDisplayName(int32_t *code, int32_t index,
                                   wchar_t *displayName) {
  if (code == nullptr) {
    return;
  }
  if (voiceInfoCtx == nullptr) {
    *code = -1;
    return;
  }
  if (index < 0 || index > static_cast<int32_t>(voiceInfoCtx->Count)) {
    *code = -2;
    return;
  }
  if (voiceInfoCtx->VoiceProperties == nullptr) {
    *code = -1;
    return;
  }

  wchar_t *s = new wchar_t[256]{};
  HRESULT hr =
      StringCbPrintfW(s, 256, L"Called GetVoiceDisplayName (index=%d)", index);

  if (FAILED(hr)) {
    *code = -1;
    return;
  }

  Log->Info(s, GetCurrentThreadId(), __LONGFILE__);

  delete[] s;
  s = nullptr;

  size_t displayNameLength =
      wcslen(voiceInfoCtx->VoiceProperties[index]->DisplayName);
  std::wmemcpy(displayName, voiceInfoCtx->VoiceProperties[index]->DisplayName,
               displayNameLength);
  *code = 0;
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(L"Called GetDefaultVoice", GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(L"Called GetDefaultVoice", GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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
    Log->Info(s, GetCurrentThreadId(), __LONGFILE__);
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

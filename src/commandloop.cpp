#include <cpplogger.h>

#include "commandloop.h"
#include "context.h"
#include "util.h"

extern Logger::Logger *Log;

DWORD WINAPI commandLoop(LPVOID context) {
  Log->Info(L"Start command loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  CommandLoopContext *ctx = static_cast<CommandLoopContext *>(context);

  if (ctx == nullptr) {
    Log->Fail(L"Failed to obtain ctx", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return E_FAIL;
  }

  Command *cmd{nullptr};
  bool isActive{true};

  while (isActive) {
    HANDLE waitArray[4] = {ctx->QuitEvent, ctx->PushEvent,
                           ctx->VoiceLoopCtx->NextEvent,
                           ctx->SoundLoopCtx->NextEvent};
    DWORD waitResult = WaitForMultipleObjects(4, waitArray, FALSE, INFINITE);

    if (waitResult == WAIT_OBJECT_0 + 0) {
      isActive = false;
      continue;
    }
    if (ctx->ReadIndex == ctx->WriteIndex) {
      continue;
    }
    switch (waitResult) {
    case WAIT_OBJECT_0 + 1: // ctx->PushEvent
      ctx->VoiceLoopCtx->VoiceEngine->FadeOut();
      ctx->SoundLoopCtx->SoundEngine->FadeOut();
      break;
    }

    cmd = ctx->Commands[ctx->ReadIndex];
    ctx->ReadIndex = (ctx->ReadIndex + 1) % ctx->MaxCommands;

    switch (cmd->Type) {
    case 1:
      Log->Info(L"Play sound", GetCurrentThreadId(), __LINE__, __WFILE__);

      ctx->SoundLoopCtx->SoundIndex = cmd->SoundIndex;

      if (!SetEvent(ctx->SoundLoopCtx->FeedEvent)) {
        Log->Fail(L"Failed to set feed event", GetCurrentThreadId(), __LINE__,
                  __WFILE__);
      }

      break;
    case 2:
      Log->Info(L"Play silent", GetCurrentThreadId(), __LINE__, __WFILE__);

      // TODO: implement me!

      break;
    case 3:
      Log->Info(L"Play generated voice (TEXT)", GetCurrentThreadId(), __LINE__,
                __WFILE__);

      ctx->VoiceLoopCtx->IsSSML = false;
      ctx->VoiceLoopCtx->BufferPtr = cmd->SSMLPtr;
      ctx->VoiceLoopCtx->BufferLen = cmd->SSMLLen;

      if (!SetEvent(ctx->VoiceLoopCtx->FeedEvent)) {
        Log->Fail(L"Failed to set feed event", GetCurrentThreadId(), __LINE__,
                  __WFILE__);
      }

      break;
    case 4:
      Log->Info(L"Play generated voice (SSML)", GetCurrentThreadId(), __LINE__,
                __WFILE__);

      ctx->VoiceLoopCtx->IsSSML = true;
      ctx->VoiceLoopCtx->BufferPtr = cmd->SSMLPtr;
      ctx->VoiceLoopCtx->BufferLen = cmd->SSMLLen;

      if (!SetEvent(ctx->VoiceLoopCtx->FeedEvent)) {
        Log->Fail(L"Failed to set event", GetCurrentThreadId(), __LINE__,
                  __WFILE__);
      }

      break;
    }
  }

  Log->Info(L"End command loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  return S_OK;
}

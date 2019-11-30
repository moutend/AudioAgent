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
    HANDLE waitArray[4] = {ctx->QuitEvent, ctx->CommandEvent,
                           ctx->VoiceLoopCtx->NextEvent,
                           ctx->SoundLoopCtx->NextEvent};
    DWORD waitResult = WaitForMultipleObjects(4, waitArray, FALSE, INFINITE);

    if (waitResult == WAIT_OBJECT_0 + 0) {
      Log->Info(L"Received quit event", GetCurrentThreadId(), __LINE__,
                __WFILE__);

      isActive = false;
      continue;
    }
    if (ctx->ReadIndex == ctx->WriteIndex) {
      continue;
    }
    switch (waitResult) {
    case WAIT_OBJECT_0 + 1: // ctx->CommandEvent
      ctx->VoiceLoopCtx->VoiceEngine->FadeOut();
      ctx->SoundLoopCtx->SoundEngine->FadeOut();
    case WAIT_OBJECT_0 + 2: // ctx->VoiceLoopContext->NextEvent
    case WAIT_OBJECT_0 + 3: // ctx->SoundLoopContext->NextEvent
      cmd = ctx->Commands[ctx->ReadIndex];
      ctx->ReadIndex = (ctx->ReadIndex + 1) % ctx->MaxCommands;
      break;
    }
    switch (cmd->Type) {
    case 1:
      ctx->SoundLoopCtx->SoundIndex = cmd->SoundIndex;

      Log->Info(L"Send event (ctx->SoundLoopCtx->FeedEvent)",
                GetCurrentThreadId(), __LINE__, __WFILE__);

      if (!SetEvent(ctx->SoundLoopCtx->FeedEvent)) {
        Log->Fail(L"Failed to set feed event for sound loop",
                  GetCurrentThreadId(), __LINE__, __WFILE__);
      }

      break;
    case 2:
      break;
    case 3:
      ctx->VoiceLoopCtx->BufferPtr = cmd->SSMLPtr;
      ctx->VoiceLoopCtx->BufferLen = cmd->SSMLLen;

      Log->Info(L"Send event (ctx->VoiceLoopCtx->FeedEvent)",
                GetCurrentThreadId(), __LINE__, __WFILE__);

      if (!SetEvent(ctx->VoiceLoopCtx->FeedEvent)) {
        Log->Fail(L"Failed to set feed event for voice loop",
                  GetCurrentThreadId(), __LINE__, __WFILE__);
      }

      break;
    }
  }

  Log->Info(L"End command loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  return S_OK;
}

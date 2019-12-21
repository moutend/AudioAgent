#include <cpplogger.h>

#include "context.h"
#include "sfxloop.h"
#include "util.h"

extern Logger::Logger *Log;

DWORD WINAPI sfxLoop(LPVOID context) {
  Log->Info(L"Start SFX loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  SFXLoopContext *ctx = static_cast<SFXLoopContext *>(context);

  if (ctx == nullptr) {
    Log->Fail(L"Failed to obtain ctx", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return E_FAIL;
  }

  bool isActive{true};

  while (isActive) {
    HANDLE waitArray[2] = {ctx->QuitEvent, ctx->FeedEvent};
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);

    // ctx->QuitEvent
    if (waitResult == WAIT_OBJECT_0 + 0) {
      break;
    }

    bool ok{};

    if (ctx->SFXIndex < 0) {
      ok = ctx->SFXEngine->Sleep(ctx->WaitDuration);
    } else {
      ok = ctx->SFXEngine->Feed(ctx->SFXIndex);
    }
    if (!ok) {
      Log->Warn(L"Failed to feed", GetCurrentThreadId(), __LINE__, __WFILE__);
    }
  }

  Log->Info(L"End SFX loop thread", GetCurrentThreadId(), __LINE__, __WFILE__);

  return S_OK;
}

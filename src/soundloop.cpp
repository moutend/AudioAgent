#include <cpplogger.h>

#include "context.h"
#include "soundloop.h"
#include "util.h"

extern Logger::Logger *Log;

DWORD WINAPI soundLoop(LPVOID context) {
  Log->Info(L"Start sound loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  SoundLoopContext *ctx = static_cast<SoundLoopContext *>(context);

  if (ctx == nullptr) {
    Log->Fail(L"Failed to obtain ctx", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return E_FAIL;
  }

  bool isActive{true};

  while (isActive) {
    HANDLE waitArray[2] = {ctx->FeedEvent, ctx->QuitEvent};
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);

    switch (waitResult) {
    case WAIT_OBJECT_0 + 0: // ctx->FeedEvent
      ctx->SoundEngine->Feed(ctx->SoundIndex);
      break;
    case WAIT_OBJECT_0 + 1: // ctx->QuitEvent
      isActive = false;
      break;
    }
  }

  Log->Info(L"End sound loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  return S_OK;
}

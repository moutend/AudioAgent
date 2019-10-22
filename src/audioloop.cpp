#include <logger/logger.h>
#include <roapi.h>

#include "audiocore.h"
#include "audioloop.h"
#include "context.h"
#include "util.h"

using namespace Windows::Media::Devices;

extern Logger::Logger *Log;

DWORD audioLoop(LPVOID Context) {
  Log->Info(L"Start audio loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  AudioLoopContext *ctx = static_cast<AudioLoopContext *>(Context);

  if (ctx == nullptr) {
    Log->Fail(L"Failed to obtain ctx", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return E_FAIL;
  }

  RoInitialize(RO_INIT_MULTITHREADED);

  bool isActive{true};

  while (isActive) {
    HANDLE refreshEvent =
        CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

    if (refreshEvent == nullptr) {
      Log->Fail(L"Failed to create refreshEvent", GetCurrentThreadId(),
                __LINE__, __WFILE__);
      return E_FAIL;
    }

    HANDLE failEvent =
        CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);

    if (failEvent == nullptr) {
      Log->Fail(L"Failed to create failEvent", GetCurrentThreadId(), __LINE__,
                __WFILE__);
      return E_FAIL;
    }

    Platform::String ^ deviceId = MediaDevice::GetDefaultAudioRenderId(
        Windows::Media::Devices::AudioDeviceRole::Default);

    IActivateAudioInterfaceAsyncOperation *op{nullptr};
    IActivateAudioInterfaceCompletionHandler *obj{nullptr};
    ComPtr<AudioCore> renderer =
        Make<AudioCore>(ctx->Engine, refreshEvent, failEvent, ctx->NextEvent);

    HRESULT hr = renderer->QueryInterface(IID_PPV_ARGS(&obj));

    if (FAILED(hr)) {
      Log->Fail(L"Failed to create instance of "
                "IActivateAudioInterfaceCompletionHandler",
                GetCurrentThreadId(), __LINE__, __WFILE__);
      return hr;
    }

    hr = ActivateAudioInterfaceAsync(deviceId->Data(), __uuidof(IAudioClient3),
                                     nullptr, obj, &op);

    if (FAILED(hr)) {
      Log->Fail(L"Failed to call ActivateAudioInterfaceAsync",
                GetCurrentThreadId(), __LINE__, __WFILE__);
      return hr;
    }

    SafeRelease(&op);

    HANDLE waitArray[3] = {refreshEvent, failEvent, ctx->QuitEvent};
    DWORD waitResult = WaitForMultipleObjects(3, waitArray, FALSE, INFINITE);

    switch (waitResult) {
    case WAIT_OBJECT_0 + 0: // refreshEvent
      Log->Info(L"Refresh audio renderer", GetCurrentThreadId(), __LINE__,
                __WFILE__);
      break;
    case WAIT_OBJECT_0 + 1: // failEvent
      Log->Warn(L"Try initializing audio renderer", GetCurrentThreadId(),
                __LINE__, __WFILE__);
      Sleep(1000);
      break;
    case WAIT_OBJECT_0 + 2: // ctx->QuitEvent
      isActive = false;
      break;
    }

    CloseHandle(refreshEvent);
    refreshEvent = nullptr;

    CloseHandle(failEvent);
    failEvent = nullptr;

    renderer->Shutdown();
  }

  RoUninitialize();

  Log->Info(L"End audio loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  return S_OK;
}

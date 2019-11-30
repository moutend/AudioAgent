#include <cpplogger.h>
#include <cstring>
#include <engine.h>
#include <ppltasks.h>
#include <roapi.h>
#include <robuffer.h>
#include <wrl.h>

#include "context.h"
#include "util.h"
#include "voiceloop.h"

using namespace Microsoft::WRL;
using namespace Windows::Media::SpeechSynthesis;
using namespace concurrency;
using namespace Windows::Storage::Streams;
using namespace Windows::Media;

using Windows::Foundation::Metadata::ApiInformation;

extern Logger::Logger *Log;

DWORD WINAPI voiceLoop(LPVOID context) {
  Log->Info(L"Start Voice loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  VoiceLoopContext *ctx = static_cast<VoiceLoopContext *>(context);

  if (ctx == nullptr) {
    Log->Fail(L"Failed to obtain ctx", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return E_FAIL;
  }

  RoInitialize(RO_INIT_MULTITHREADED);

  bool isActive{true};
  auto synth = ref new SpeechSynthesizer();

  if (ApiInformation::IsApiContractPresent(
          "Windows.Foundation.UniversalApiContract", 6, 0)) {
    auto options = synth->Options;
    options->AppendedSilence = SpeechAppendedSilence::Min;
  }

  ctx->VoiceCount = synth->AllVoices->Size;
  ctx->VoiceList = new wchar_t *[synth->AllVoices->Size];

  for (unsigned int i = 0; i < ctx->VoiceCount; ++i) {
    VoiceInformation ^ info = synth->AllVoices->GetAt(i);
    size_t voiceNameLen = wcslen(info->Id->Data());
    ctx->VoiceList[i] = new wchar_t[voiceNameLen + 1]{};

    std::wmemcpy(ctx->VoiceList[i], info->Id->Data(), voiceNameLen);
  }
  while (isActive) {
    HANDLE waitArray[2] = {ctx->FeedEvent, ctx->QuitEvent};
    DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);

    switch (waitResult) {
    case WAIT_OBJECT_0 + 0: // ctx->FeedEvent
      break;
    case WAIT_OBJECT_0 + 1: // ctx->QuitEvent
      isActive = false;
      continue;
    }

    task<SpeechSynthesisStream ^> speechTask;
    Platform::String ^ ssml = ref new Platform::String(ctx->BufferPtr);

    try {
      speechTask = create_task(synth->SynthesizeSsmlToStreamAsync(ssml));
    } catch (Platform::Exception ^ e) {
      Log->Warn(L"Failed to call SynthesizeSsmlToStreamAsync",
                GetCurrentThreadId(), __LINE__, __WFILE__);
      continue;
    }

    // I don't know why, but the broken SSML makes the task done.

    if (speechTask.is_done()) {
      Log->Warn(
          L"Failed to continue speech synthesis (probably SSML is broken)",
          GetCurrentThreadId(), __LINE__, __WFILE__);
      continue;
    }

    // The voice is generated as WAVE file format, int32_t type is enough to
    // holding the byte array length.
    int32_t waveLength{0};

    speechTask
        .then([&ctx, &waveLength](SpeechSynthesisStream ^ speechStream) {
          waveLength = static_cast<int32_t>(speechStream->Size);
          Buffer ^ buffer = ref new Buffer(waveLength);

          auto result = create_task(speechStream->ReadAsync(
              buffer, waveLength,
              Windows::Storage::Streams::InputStreamOptions::None));

          return result;
        })
        .then([&ctx, &waveLength](IBuffer ^ buffer) {
          if (waveLength > 0) {
            char *wave = getBytes(buffer);
            ctx->VoiceEngine->Feed(wave, waveLength);
          }
        })
        .then([](task<void> previous) {
          try {
            previous.get();
          } catch (Platform::Exception ^ e) {
            Log->Warn(L"Failed to complete speech synthesis",
                      GetCurrentThreadId(), __LINE__, __WFILE__);
          }
        })
        .wait();
  }
  for (unsigned int i = 0; i < ctx->VoiceCount; i++) {
    delete[] ctx->VoiceList[i];
  }

  delete[] ctx->VoiceList;
  RoUninitialize();

  Log->Info(L"End Voice loop thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  return S_OK;
}

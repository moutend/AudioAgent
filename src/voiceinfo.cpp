#include <cpplogger.h>
#include <cstring>
#include <ppltasks.h>
#include <roapi.h>
#include <robuffer.h>
#include <wrl.h>

#include "context.h"
#include "util.h"
#include "voiceinfo.h"

using namespace Microsoft::WRL;
using namespace Windows::Media::SpeechSynthesis;
using namespace concurrency;
using namespace Windows::Storage::Streams;
using namespace Windows::Media;

using Windows::Foundation::Metadata::ApiInformation;

extern Logger::Logger *Log;

DWORD WINAPI voiceInfo(LPVOID context) {
  Log->Info(L"Start Voice info thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  VoiceInfoContext *ctx = static_cast<VoiceInfoContext *>(context);

  if (ctx == nullptr) {
    Log->Fail(L"Failed to obtain ctx", GetCurrentThreadId(), __LINE__,
              __WFILE__);
    return E_FAIL;
  }

  RoInitialize(RO_INIT_MULTITHREADED);

  auto synth = ref new SpeechSynthesizer();

  ctx->VoiceCount = synth->AllVoices->Size;
  ctx->VoiceProperties = new VoiceProperty *[synth->AllVoices->Size];

  for (unsigned int i = 0; i < ctx->VoiceCount; ++i) {
    VoiceInformation ^ info = synth->AllVoices->GetAt(i);

    ctx->VoiceProperties[i] = new VoiceProperty();

    size_t idLength = wcslen(info->Id->Data());
    ctx->VoiceProperties[i]->Id = new wchar_t[idLength + 1]{};
    std::wmemcpy(ctx->VoiceProperties[i]->Id, info->Id->Data(), idLength);

    size_t displayNameLength = wcslen(info->DisplayName->Data());
    ctx->VoiceProperties[i]->DisplayName = new wchar_t[displayNameLength + 1]{};
    std::wmemcpy(ctx->VoiceProperties[i]->DisplayName,
                 info->DisplayName->Data(), displayNameLength);

    size_t languageLength = wcslen(info->Language->Data());
    ctx->VoiceProperties[i]->Language = new wchar_t[languageLength + 1]{};
    std::wmemcpy(ctx->VoiceProperties[i]->Language, info->Language->Data(),
                 languageLength);
  }

  RoUninitialize();

  Log->Info(L"End Voice info thread", GetCurrentThreadId(), __LINE__,
            __WFILE__);

  return S_OK;
}

#include "logloop.h"

#include <logger/logger.h>

#include "context.h"

extern Logger::Logger *Log;

DWORD logLoop(LPVOID Context) {
  LogLoopContext *ctx = static_cast<LogLoopContext *>(Context);

  if (ctx == nullptr) {
    return E_FAIL;
  }

  bool isActive{true};
  HANDLE hLogFile{nullptr};

  while (isActive) {
    Sleep(5000);

    if (Log->IsEmpty()) {
      continue;
    }

    hLogFile =
        CreateFileW(ctx->FullLogPath, FILE_APPEND_DATA, FILE_SHARE_READ,
                    nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hLogFile == nullptr) {
      break;
    }

    int dataLen = Log->Size();
    wchar_t *data = new wchar_t[dataLen];

    Log->Copy(data, dataLen);

    DWORD bytesWritten{};
    int bufferLen = WideCharToMultiByte(CP_UTF8, 0, data, dataLen, nullptr, 0,
                                        nullptr, nullptr);

    if (bufferLen <= 0) {
      break;
    }

    char *buffer = new char[bufferLen]{};

    if (WideCharToMultiByte(CP_UTF8, 0, data, dataLen, buffer, bufferLen,
                            nullptr, nullptr) <= 0) {
      break;
    }
    if (!WriteFile(hLogFile, buffer, bufferLen, &bytesWritten, nullptr)) {
      break;
    }

    CloseHandle(hLogFile);
    hLogFile = nullptr;

    Log->Clear();
  }

  return S_OK;
}

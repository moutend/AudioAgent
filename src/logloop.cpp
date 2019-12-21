#include <cpplogger.h>
#include <sstream>

#include "context.h"
#include "logloop.h"

extern Logger::Logger *Log;

DWORD WINAPI logLoop(LPVOID context) {
  LogLoopContext *ctx = static_cast<LogLoopContext *>(context);

  if (ctx == nullptr) {
    return E_FAIL;
  }

  std::wstringstream wss;
  wss << L"Log output path:";
  wss << ctx->FullLogPath;
  Log->Info(wss.str(), GetCurrentThreadId(), __LINE__, __WFILE__);

  bool isActive{true};
  HANDLE hLogFile{nullptr};

  while (isActive) {
    HANDLE waitArray[1] = {ctx->QuitEvent};
    DWORD waitResult = WaitForMultipleObjects(1, waitArray, FALSE, INFINITE);

    if (waitResult == WAIT_OBJECT_0 + 0) {
      isActive = false;
      continue;
    }

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

    SafeCloseHandle(&hLogFile);
    Log->Clear();
  }

  return S_OK;
}

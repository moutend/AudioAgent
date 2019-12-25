#include <cpplogger/cpplogger.h>
#include <cpprest/http_client.h>
#include <sstream>

#include "context.h"
#include "logloop.h"
#include "util.h"

using namespace web;
using namespace web::http;
using namespace web::http::client;

extern Logger::Logger *Log;

DWORD WINAPI logLoop(LPVOID context) {
  LogLoopContext *ctx = static_cast<LogLoopContext *>(context);

  if (ctx == nullptr) {
    return E_FAIL;
  }

  std::wstringstream wss;
  wss << L"Log output path:";
  wss << ctx->FullLogPath;
  Log->Info(wss.str(), GetCurrentThreadId(), __LONGFILE__);

  bool isActive{true};
  HANDLE hLogFile{nullptr};

  while (isActive) {
    HANDLE waitArray[1] = {ctx->QuitEvent};
    DWORD waitResult = WaitForMultipleObjects(1, waitArray, FALSE, 0);

    if (waitResult == WAIT_OBJECT_0 + 0) {
      isActive = false;
      continue;
    }

    Sleep(5000);

    if (Log->IsEmpty()) {
      continue;
    }

    Log->Lock();

    // json::value message = Log->ToJSON();
    json::value message;
    message[L"foo"] = json::value(1234);
    try {
      pplx::create_task([&message] {
        http_client client(L"http://localhost:7901/v1/log");
        return client.request(methods::POST, L"", message.serialize(),
                              L"application/json");
      }).then([](http_response response) {});
    } catch (...) {
      // todo
    }
    Log->Clear();
    Log->Unlock();
  }

  return S_OK;
}

#include <chrono>
#include <cpplogger/cpplogger.h>
#include <cpprest/http_client.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

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

  bool isActive{true};
  std::chrono::milliseconds timeout(3000);

  http_client_config config;
  config.set_timeout(timeout);

  try {
    http_client client(L"http://localhost:7901/v1/log", config);
  } catch (std::exception &e) {
    std::ofstream output("error.txt", std::ofstream::binary);
    output << e.what();
    output.close();
  }

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

    json::value message = Log->ToJSON();

    try {
      /*
        client
            .request(methods::POST, L"", message.serialize(),
        L"application/json") .wait();
            */
    } catch (...) {
      Log->Warn(L"Failed to send log messages", GetCurrentThreadId(),
                __LONGFILE__);

      continue;
    }

    Log->Clear();
  }

  return S_OK;
}

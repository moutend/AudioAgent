#include <chrono>
#include <cstring>
#include <mutex>
#include <sstream>

#include "logger.h"

std::mutex mtx;

namespace Logger {
Logger::Logger() {}

Logger::~Logger() {}

void Logger::Info(const std::wstring &message, int64_t threadId,
                  int32_t lineNumber, const std::wstring &fileName) {
  Write(INFO, message, threadId, lineNumber, fileName);
}

void Logger::Warn(const std::wstring &message, int64_t threadId,
                  int32_t lineNumber, const std::wstring &fileName) {
  Write(WARN, message, threadId, lineNumber, fileName);
}

void Logger::Fail(const std::wstring &message, int64_t threadId,
                  int32_t lineNumber, const std::wstring &fileName) {
  Write(FAIL, message, threadId, lineNumber, fileName);
}

void Logger::Write(Level level, const std::wstring &message, int64_t threadId,
                   int32_t lineNumber, const std::wstring &fileName) {
  std::lock_guard<std::mutex> guard(mtx);

  int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();

  int64_t sec = now / 1000000000;
  int64_t nsec = now % 1000000000;

  mBuffer << L"level:";

  switch (level) {
  case INFO:
    mBuffer << L"INFO\t";
    break;
  case WARN:
    mBuffer << L"WARN\t";
    break;
  case FAIL:
    mBuffer << L"FAIL\t";
    break;
  }

  mBuffer << L"message:" << message << L"\t";
  mBuffer << L"timestamp:" << sec << L"." << nsec << L"\t";
  mBuffer << L"thread:" << threadId << L"\t";
  mBuffer << L"file:" << fileName << L":" << lineNumber << L"\n";
}

void Logger::Copy(wchar_t *destination, size_t numberOfChars) {
  std::mutex mtx;

  std::wmemcpy(destination, mBuffer.str().c_str(), numberOfChars);
}

int Logger::Size() {
  std::lock_guard<std::mutex> guard(mtx);

  return static_cast<int>(mBuffer.str().size());
}

void Logger::Clear() {
  std::lock_guard<std::mutex> guard(mtx);

  std::wstringstream().swap(mBuffer);
}

bool Logger::IsEmpty() {
  std::lock_guard<std::mutex> guard(mtx);

  return mBuffer.str().empty();
}
} // namespace Logger

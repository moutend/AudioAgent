#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define __WFILE__ WIDE1(__FILE__)

namespace Logger {
enum Level { INFO = 1, WARN = 2, FAIL = 3 };

class Logger {
public:
  Logger();
  ~Logger();

  void Info(const std::wstring &message, int64_t threadId, int32_t lineNumber,
            const std::wstring &fileName);
  void Warn(const std::wstring &message, int64_t threadId, int32_t lineNumber,
            const std::wstring &fileName);
  void Fail(const std::wstring &message, int64_t threadId, int32_t lineNumber,
            const std::wstring &fileName);
  void Write(Level level, const std::wstring &message, int64_t threadId,
             int32_t lineNumber, const std::wstring &fileName);
  void Copy(wchar_t *destination, size_t numberOfChars);
  int Size();
  void Clear();
  bool IsEmpty();

private:
  std::wstringstream mBuffer;
};
} // namespace Logger

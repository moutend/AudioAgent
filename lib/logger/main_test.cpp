#include "logger.h"
#include <codecvt>
#include <fstream>
#include <iostream>
#include <locale>
#include <string>

int main() {
  Logger::Logger *log = new Logger::Logger();

  std::cout << log->IsEmpty() << std::endl;

  log->Fail(L"Foobar", 1234, __LINE__, __WFILE__);

  std::cout << log->IsEmpty() << std::endl;

  log->Clear();

  std::cout << log->IsEmpty() << std::endl;
}

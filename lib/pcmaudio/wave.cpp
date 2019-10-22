#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "wave.h"

namespace PCMAudio {
Wave::Wave(const char *filePath) {
  std::ifstream input(filePath, std::ios::binary | std::ios::in);

  if (!input.is_open()) {
    // throw
    return;
  }

  initialize(input);
}

Wave::Wave(char *buffer, size_t bufferLength) {
  std::istringstream input(std::string(buffer, bufferLength));

  initialize(input);
}

Wave::~Wave() { delete[] mData; }

int16_t Wave::FormatTag() { return mFormatTag; }
int16_t Wave::Channels() { return mChannels; }
int32_t Wave::SamplesPerSec() { return mSamplesPerSec; }
int32_t Wave::AvgBytesPerSec() { return mAvgBytesPerSec; }
int16_t Wave::BlockAlign() { return mBlockAlign; }
int16_t Wave::BitsPerSample() { return mBitsPerSample; }
std::streamsize Wave::DataLength() { return mDataLength; }
char *Wave::Data() { return mData; }

int32_t Wave::initialize(std::istream &input) {
  char chunkNameStr[5]{}; // null terminated C-style string.
  char dwordStr[4]{};     // buffer for DWORD type
  char wordStr[2]{};      // buffer for WORD type
  int32_t chunkSize;
  std::streampos fileSize;

  input.read(chunkNameStr, std::streamsize(sizeof(char) * 4));

  if (std::strcmp(chunkNameStr, "RIFF") != 0) {
    return 2;
  }

  input.read(dwordStr, std::streamsize(sizeof(char) * 4));
  std::memcpy(&chunkSize, dwordStr, sizeof(char) * 4);

  if (chunkSize > 0) {
    // Adding size of 'RIFF' + chunk size
    fileSize = chunkSize + 8;
  } else {
    return 5;
  }

  input.read(chunkNameStr, std::streamsize(sizeof(char) * 4));

  if (std::strcmp(chunkNameStr, "WAVE") != 0) {
    return -4;
  }
  while (true) {
    if (input.tellg() == fileSize) {
      break;
    }
    if (input.fail()) {
      return 6;
    }

    input.read(chunkNameStr, std::streamsize(sizeof(char) * 4));
    input.read(dwordStr, std::streamsize(sizeof(char) * 4));
    memcpy(&chunkSize, dwordStr, sizeof(char) * 4);

    if (std::strcmp(chunkNameStr, "fmt ") == 0) {
      input.read(wordStr, std::streamsize(sizeof(char) * 2));
      std::memcpy(&mFormatTag, wordStr, sizeof(char) * 2);

      input.read(wordStr, std::streamsize(sizeof(char) * 2));
      std::memcpy(&mChannels, wordStr, sizeof(char) * 2);

      input.read(dwordStr, std::streamsize(sizeof(char) * 4));
      std::memcpy(&mSamplesPerSec, dwordStr, sizeof(char) * 4);

      input.read(dwordStr, std::streamsize(sizeof(char) * 4));
      std::memcpy(&mAvgBytesPerSec, dwordStr, sizeof(char) * 4);

      input.read(wordStr, std::streamsize(sizeof(char) * 2));
      std::memcpy(&mBlockAlign, wordStr, sizeof(char) * 2);

      input.read(wordStr, std::streamsize(sizeof(char) * 2));
      std::memcpy(&mBitsPerSample, wordStr, sizeof(char) * 2);

      // Skip remaining info.
      std::streamoff diff = chunkSize - 16;
      input.seekg(diff, std::ios_base::cur);
    } else if (std::strcmp(chunkNameStr, "data") == 0) {
      mData = new char[chunkSize];
      mDataLength = chunkSize;

      input.read(mData, std::streamsize(chunkSize));
    } else {
      // Skip reading this chunk
      std::streamoff position = chunkSize;
      input.seekg(position, std::ios_base::cur);
    }
  }

  return 0;
}
} // namespace PCMAudio

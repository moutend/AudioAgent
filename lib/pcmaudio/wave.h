#pragma once

#include <cstdint>
#include <iostream>

namespace PCMAudio {
class Wave {
public:
  Wave(const char *filePath);
  Wave(char *buffer, size_t bufferLength);
  ~Wave();

  int16_t FormatTag();
  int16_t Channels();
  int32_t SamplesPerSec();
  int32_t AvgBytesPerSec();
  int16_t BlockAlign();
  int16_t BitsPerSample();
  std::streamsize DataLength();
  char *Data();

private:
  int32_t initialize(std::istream &input);

  int16_t mFormatTag;
  int16_t mChannels;
  int32_t mSamplesPerSec;
  int32_t mAvgBytesPerSec;
  int16_t mBlockAlign;
  int16_t mBitsPerSample;

  std::streamsize mDataLength;
  char *mData = nullptr;
};
} // namespace PCMAudio

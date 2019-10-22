#pragma once

#include <cstdint>

#include "reader.h"
#include "wave.h"

namespace PCMAudio {
class Engine {
public:
  virtual void Reset() = 0;
  virtual void SetTargetSamplesPerSec(int32_t samples) = 0;
  virtual void FadeIn() = 0;
  virtual void FadeOut() = 0;
  virtual bool IsCompleted() = 0;
  virtual void Next() = 0;
  virtual double Read() = 0;
};

class LauncherEngine : public Engine {
public:
  LauncherEngine();
  ~LauncherEngine();

  void Reset();
  void SetTargetSamplesPerSec(int32_t samples);
  void FadeIn();
  void FadeOut();
  bool IsCompleted();
  void Next();
  double Read();

  void Feed(int16_t index);
  void Register(int16_t index, const char *filePath);

private:
  Wave **mWaves = nullptr;
  Reader **mReaders = nullptr;

  bool mCompleted = false;
  int16_t mIndex = 0;
  int16_t mMaxWaves = 256;
  int16_t mMaxReaders = 16;
  int16_t mCurrentChannel = 0;
  int32_t mTargetSamplesPerSec = 44100;
};

class RingEngine : public Engine {
public:
  RingEngine();
  ~RingEngine();

  void Reset();
  void SetTargetSamplesPerSec(int32_t samples);
  void FadeIn();
  void FadeOut();
  bool IsCompleted();
  void Next();
  double Read();

  void Feed(char *buffer, int32_t bufferLength);

private:
  Wave **mWaves = nullptr;
  Reader **mReaders = nullptr;

  bool mCompleted = false;
  int16_t mMaxReaders = 16;

  int32_t mTargetSamplesPerSec = 44100;
  int16_t mCurrentChannel = 0;
  int16_t mIndex = 0;
};
} // namespace PCMAudio

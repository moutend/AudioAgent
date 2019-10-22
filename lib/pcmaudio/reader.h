#pragma once

#include <cstdint>

#include "wave.h"

namespace PCMAudio {
class Reader {
public:
  virtual void SetTargetSamplesPerSec(int32_t samples) = 0;
  virtual void FadeIn() = 0;
  virtual void FadeOut() = 0;
  virtual bool IsCompleted() = 0;
  virtual void Next() = 0;
  virtual double Read() = 0;
};

class WaveReader : public Reader {
public:
  WaveReader(Wave *&wave, int32_t delayCount);

  void SetTargetSamplesPerSec(int32_t samples);
  void FadeIn();
  void FadeOut();
  bool IsCompleted();
  void Next();
  double Read();

  double ReadDouble(double index);
  int32_t ReadInt32t(int32_t index);

private:
  Wave *mWave = nullptr;

  bool mPause = false;
  bool mCompleted = false;
  int32_t mDelayCount = 0;
  int16_t mBytesPerSec = 0;
  int16_t mBytesPerSample = 0;
  int16_t mChannelCount = 0;
  int16_t mSkipCount = 0;
  int16_t mTargetChannels = 2;
  double mTargetSamplesPerSec = 44100.0;
  double mDiff = 1.0;
  double mDiffCount = 0.0;
  double mSamples = 0.0;
  double mFadeGain = 1.0;
  double mFadeFactor = 0.0;
};

class SilentReader : public Reader {
public:
  SilentReader(double samplesPerSec, double duration /* ms*/);

  void SetTargetSamplesPerSec(int32_t samples);
  void FadeIn();
  void FadeOut();
  bool IsCompleted();
  void Next();
  double Read();

private:
  bool mCompleted = false;
  int16_t mChannels = 2;
  double mSamplesPerSec = 44100.0;
  double mDuration = 0.0;
  double mSamples = 0.0;
  double mSampleCount = 0.0;
};
} // namespace PCMAudio

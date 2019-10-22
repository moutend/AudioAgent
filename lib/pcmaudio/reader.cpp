#include <cmath>
#include <cstring>
#include <iostream>

#include "reader.h"
namespace PCMAudio {
WaveReader::WaveReader(Wave *&wave, int32_t delayCount) {
  mWave = wave;
  mDelayCount = delayCount;

  mBytesPerSample = mWave->BitsPerSample() / 8;
  mBytesPerSec = mBytesPerSample * mWave->Channels();
  mSamples = static_cast<double>(wave->DataLength()) / mBytesPerSec;
  mTargetSamplesPerSec = wave->SamplesPerSec();
}

void WaveReader::SetTargetSamplesPerSec(int32_t samples) {
  mTargetSamplesPerSec = static_cast<double>(mWave->SamplesPerSec());
  mDiff = mTargetSamplesPerSec / samples;
}

void WaveReader::FadeOut() {
  // For now, use 4 ms (3.90625 ms) as a fade in / out duration.
  mFadeFactor = -1024.0 / mTargetSamplesPerSec;
}

void WaveReader::FadeIn() {
  // For now, use 4 ms (3.90625 ms) as a fade in / out duration.
  mFadeFactor = 1024.0 / mTargetSamplesPerSec;
  mPause = false;
}

bool WaveReader::IsCompleted() { return mCompleted; }

void WaveReader::Next() {
  if (mDelayCount > 0) {
    mDelayCount -= 1;
    return;
  }
  if (mPause || mCompleted) {
    return;
  }

  mSkipCount += 1;

  if (mSkipCount <= mTargetChannels - mWave->Channels()) {
    return;
  }

  mSkipCount = 0;
  mFadeGain += mFadeFactor;
  mChannelCount = (mChannelCount + 1) % mWave->Channels();

  if (mChannelCount == 0) {
    mDiffCount += mDiff;
  }
  if (mFadeGain < 0.0) {
    mFadeFactor = 0.0;
    mFadeGain = 0.0;
    mPause = true;
  }
  if (mFadeGain > 1.0) {
    mFadeFactor = 0.0;
    mFadeGain = 1.0;
  }
  if (mDiffCount > mSamples - 1) {
    mCompleted = true;
  }
}

double WaveReader::Read() {
  if (mPause || mDelayCount > 0 || mCompleted) {
    return 0.0;
  }

  double diff = mDiffCount - floor(mDiffCount);
  double index = mChannelCount + floor(mDiffCount) * mWave->Channels() + diff;

  return ReadDouble(index);
}

double WaveReader::ReadDouble(double index) {
  int32_t i1 = static_cast<int32_t>(floor(index));
  int32_t i2 = i1 + mWave->Channels();

  double ratio = index - i1;
  double v1 = ReadInt32t(i1 * mBytesPerSample);
  double v2 = ReadInt32t(i2 * mBytesPerSample);

  return mFadeGain * (v1 + (v2 - v1) * ratio);
}

/*
 * ReadInt32T returns a sample as 32 bit integer.
 *
 * 16 bit value 0x1234 will converted to 0x12340000.
 * 24 bit value 0x123456 will converted to 0x12345600.
 * 32 bit value 0x12345678 stays the same.
 */
int32_t WaveReader::ReadInt32t(int32_t index) {
  int32_t s32{0};

  std::memcpy(&s32, mWave->Data() + index, mBytesPerSample);

  return s32 << (8 * (4 - mBytesPerSample));
}

SilentReader::SilentReader(double samplesPerSec, double duration /* ms */)
    : mSamplesPerSec(samplesPerSec), mDuration(duration) {}

void SilentReader::SetTargetSamplesPerSec(int32_t samples) {
  mSamplesPerSec = samples;
  mSamples = mChannels * mSamplesPerSec * mDuration / 1000.0;
}

void SilentReader::FadeIn() {}

void SilentReader::FadeOut() {}

bool SilentReader::IsCompleted() { return mCompleted; }

void SilentReader::Next() {
  if (mDuration < 0.0) {
    return;
  }

  mSampleCount += 1.0;

  if (mSampleCount > mSamples - 1) {
    mCompleted = true;
  }
}

double SilentReader::Read() { return 0.0; }
} // namespace PCMAudio

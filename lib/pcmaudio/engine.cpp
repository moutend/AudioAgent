#include <mutex>

#include "engine.h"

std::mutex launcherEngineMutex;
std::mutex ringEngineMutex;

namespace PCMAudio {
LauncherEngine::LauncherEngine() {
  mWaves = new Wave *[mMaxWaves] {};
  mReaders = new Reader *[mMaxReaders] {};
}

LauncherEngine::~LauncherEngine() {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    delete mWaves[i];
    mWaves[i] = nullptr;
    mReaders[i] = nullptr;
  }

  delete[] mWaves;
  mWaves = nullptr;

  delete[] mReaders;
  mReaders = nullptr;
}

void LauncherEngine::Reset() {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  mCompleted = false;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    mReaders[i] = nullptr;
  }
}

void LauncherEngine::SetTargetSamplesPerSec(int32_t samples) {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  mTargetSamplesPerSec = samples;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->SetTargetSamplesPerSec(mTargetSamplesPerSec);
    }
  }
}

void LauncherEngine::FadeIn() {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->FadeIn();
    }
  }
}

void LauncherEngine::FadeOut() {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->FadeOut();
    }
  }
}

bool LauncherEngine::IsCompleted() { return mCompleted; }

void LauncherEngine::Next() {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  mCurrentChannel = (mCurrentChannel + 1) % 2;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->Next();
      mCompleted = mCompleted | mReaders[i]->IsCompleted();
    }
  }
}

double LauncherEngine::Read() {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  double result = 0.0;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      result += mReaders[i]->Read();
    }
  }

  return result;
}

void LauncherEngine::Feed(int16_t index) {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->FadeOut();
    }
  }

  mReaders[mIndex] = new WaveReader(mWaves[index], mCurrentChannel);
  mReaders[mIndex]->SetTargetSamplesPerSec(mTargetSamplesPerSec);

  mIndex = (mIndex + 1) % mMaxReaders;
  mCompleted = false;
}

void LauncherEngine::Register(int16_t index, const char *filePath) {
  std::lock_guard<std::mutex> guard(launcherEngineMutex);

  if (index < 0 || index > mMaxReaders || filePath == nullptr) {
    return;
  }

  delete mWaves[index];
  mWaves[index] = nullptr;
  mWaves[index] = new Wave(filePath);
}

RingEngine::RingEngine() {
  mWaves = new Wave *[mMaxReaders] {};
  mReaders = new Reader *[mMaxReaders] {};
}

RingEngine::~RingEngine() {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    delete mWaves[i];
    mWaves[i] = nullptr;
  }

  delete[] mWaves;
  mWaves = nullptr;

  delete[] mReaders;
  mReaders = nullptr;
}

void RingEngine::Reset() {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  mCompleted = false;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    mReaders[i] = nullptr;
  }
}

void RingEngine::SetTargetSamplesPerSec(int32_t samples) {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  mTargetSamplesPerSec = samples;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->SetTargetSamplesPerSec(mTargetSamplesPerSec);
    }
  }
}

void RingEngine::FadeIn() {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  int16_t i = mIndex - 1 > 0 ? mIndex - 1 : mMaxReaders - 1;

  if (mReaders[i] != nullptr) {
    mReaders[i]->FadeIn();
  }
}

void RingEngine::FadeOut() {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->FadeOut();
    }
  }
}

void RingEngine::Feed(char *buffer, int32_t bufferLength) {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->FadeOut();
    }
  }

  delete mWaves[mIndex];
  mWaves[mIndex] = nullptr;
  mWaves[mIndex] = new Wave(buffer, bufferLength);

  mReaders[mIndex] = new WaveReader(mWaves[mIndex], mCurrentChannel);
  mReaders[mIndex]->SetTargetSamplesPerSec(mTargetSamplesPerSec);

  mIndex = (mIndex + 1) % mMaxReaders;
  mCompleted = false;
}

bool RingEngine::IsCompleted() { return mCompleted; }

void RingEngine::Next() {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  mCurrentChannel = (mCurrentChannel + 1) % 2;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      mReaders[i]->Next();
      mCompleted = mCompleted | mReaders[i]->IsCompleted();
    }
  }
}

double RingEngine::Read() {
  std::lock_guard<std::mutex> guard(ringEngineMutex);

  double result = 0.0;

  for (int16_t i = 0; i < mMaxReaders; i++) {
    if (mReaders[i] != nullptr) {
      result += mReaders[i]->Read();
    }
  }

  return result;
}
} // namespace PCMAudio

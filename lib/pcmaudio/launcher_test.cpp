#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>

#include "engine.h"
#include "reader.h"
#include "wave.h"

namespace Random {
std::mt19937
    mersenne(static_cast<std::mt19937::result_type>(std::time(nullptr)));
} // namespace Random

int generateRandom() {
  std::uniform_int_distribution<> die(0, 50);
  return die(Random::mersenne);
}

int main() {
  PCMAudio::LauncherEngine *le = new PCMAudio::LauncherEngine();
  le->Register(0, "voice.wav");
  le->SetTargetSamplesPerSec(44100);
  PCMAudio::Engine *engine = le;

  char buffer[4]{};
  std::ofstream out("output.raw", std::ios::binary | std::ios::out);

  for (int32_t i = 0; i < 384000; i++) {
    if (i == 0) {
      le->Feed(0);
    }
    if (engine->IsCompleted()) {
      le->Feed(0);
    }
    int32_t s32 = static_cast<int32_t>(engine->Read());

    for (int32_t j = 0; j < 4; j++) {
      buffer[4 - 1 - j] = s32 >> (8 * (4 - 1 - j)) & 0xFF;
    }
    out.write(reinterpret_cast<const char *>(buffer), 4);
    engine->Next();
  }

  out.close();

  return 0;
}

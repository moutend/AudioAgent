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
  std::ifstream input("voice.wav", std::ios::binary);
  input.seekg(0, input.end);
  std::streampos fileSize = input.tellg();
  input.seekg(0, std::ios_base::beg);

  char *data = new char[fileSize];
  input.read(data, fileSize);
  input.close();

  PCMAudio::RingEngine *re = new PCMAudio::RingEngine();
  re->SetTargetSamplesPerSec(44100);
  PCMAudio::Engine *engine = re;

  char buffer[4]{};
  std::ofstream output("output.raw", std::ios::binary | std::ios::out);

  for (int i = 0; i < 384000; i++) {
    if (i == 0) {
      re->Feed(data, fileSize);
    }
    if (engine->IsCompleted()) {
      re->Feed(data, fileSize);
    }

    int32_t s32 = static_cast<int32_t>(engine->Read());

    for (int32_t j = 0; j < 4; j++) {
      buffer[4 - 1 - j] = s32 >> (8 * (4 - 1 - j)) & 0xFF;
    }

    output.write(reinterpret_cast<const char *>(buffer), 4);
    engine->Next();
  }

  output.close();

  return 0;
}

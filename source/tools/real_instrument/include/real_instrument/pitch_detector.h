#pragma once
#include "types.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace real_instrument {
class PitchDetector {
public:
    explicit PitchDetector(float noiseFloor = 0.005f);
    PitchFrame Process(const float* mono, size_t count, uint32_t sampleRate, double timeSeconds);
private:
    float noiseFloor_;
    std::vector<double> difference_;
};
}

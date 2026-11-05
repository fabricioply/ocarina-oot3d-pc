#pragma once
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __FILE__ << ':' << __LINE__ << " failed: " #x "\n"; std::exit(1); } } while(false)
inline std::vector<float> tone(double hz, bool harmonics=false, size_t n=4096) {
    std::vector<float> samples(n);
    for(size_t i=0;i<n;++i) {
        double p = 6.283185307179586 * hz * static_cast<double>(i) / 48000.0;
        samples[i] = static_cast<float>(0.2*std::sin(p) + (harmonics ? 0.12*std::sin(2*p)+0.07*std::sin(3*p) : 0));
    }
    return samples;
}

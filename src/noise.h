/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include <cstdint>
#include <vector>

struct noise_t {
    uint32_t width;
    uint32_t height;
    std::vector<int8_t> map;
};
extern noise_t noise;

void noise_generate(uint64_t seed, uint32_t width, uint32_t height);
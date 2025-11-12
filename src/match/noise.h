/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include "defines.h"

struct Noise {
    uint32_t width;
    uint32_t height;
    int8_t* map;
};

Noise noise_generate(uint64_t seed, uint32_t width, uint32_t height);
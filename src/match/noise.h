/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include "defines.h"

const uint8_t NOISE_VALUE_WATER = 0;
const uint8_t NOISE_VALUE_LOWGROUND = 1;
const uint8_t NOISE_VALUE_HIGHGROUND = 2;

struct Noise {
    uint32_t width;
    uint32_t height;
    uint8_t* map;
};

Noise noise_generate(uint64_t seed, uint32_t width, uint32_t height);
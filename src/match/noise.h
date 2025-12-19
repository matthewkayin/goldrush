/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include "defines.h"
#include "core/match_setting.h"

const uint8_t NOISE_VALUE_WATER = 0;
const uint8_t NOISE_VALUE_TREE = 1;
const uint8_t NOISE_VALUE_LOWGROUND = 2;
const uint8_t NOISE_VALUE_HIGHGROUND = 3;

struct Noise {
    int width;
    int height;
    uint8_t* map;
};

Noise noise_generate(MapType map_type, uint64_t seed, int width, int height);
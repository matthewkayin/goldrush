/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include "defines.h"
#include "core/match_setting.h"
#include <cstdio>

const uint8_t NOISE_VALUE_WATER = 0;
const uint8_t NOISE_VALUE_TREE = 1;
const uint8_t NOISE_VALUE_LOWGROUND = 2;
const uint8_t NOISE_VALUE_HIGHGROUND = 3;

struct Noise {
    int width;
    int height;
    uint8_t* map;
    uint8_t* forest;
};

Noise* noise_generate(MapType map_type, uint64_t seed, uint64_t forest_seed, int width, int height);
void noise_free(Noise* noise);

size_t noise_serialized_size(const Noise* noise);
size_t noise_serialize(const Noise* noise, uint8_t* buffer);
Noise* noise_deserialize(uint8_t* buffer);
void noise_fwrite(const Noise* noise, FILE* file);
Noise* noise_fread(FILE* file);
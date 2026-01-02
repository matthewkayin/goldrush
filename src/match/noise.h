/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include "defines.h"
#include "core/match_setting.h"
#include <cstdio>

const uint8_t NOISE_VALUE_WATER = 0;
const uint8_t NOISE_VALUE_LOWGROUND = 1;
const uint8_t NOISE_VALUE_HIGHGROUND = 2;
// Used by editor
const uint8_t NOISE_VALUE_RAMP = 3;

struct Noise {
    int width;
    int height;
    uint8_t* map;
    uint8_t* forest;
};

struct NoiseGenParams {
    int width;
    int height;
    uint64_t map_seed;
    uint64_t forest_seed;
    uint32_t water_threshold;
    uint32_t lowground_threshold;
    uint32_t forest_threshold;
    bool map_inverted;
};

NoiseGenParams noise_create_noise_gen_params(MapType map_type, MapSize map_size, uint64_t map_seed, uint64_t forest_seed);
Noise* noise_init(int width, int height);
Noise* noise_generate(const NoiseGenParams& params);
void noise_free(Noise* noise);

size_t noise_serialized_size(const Noise* noise);
size_t noise_serialize(const Noise* noise, uint8_t* buffer);
Noise* noise_deserialize(uint8_t* buffer);
void noise_fwrite(const Noise* noise, FILE* file);
Noise* noise_fread(FILE* file);
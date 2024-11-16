/* Adapted from K.jpg's OpenSimplex2
 * https://github.com/KdotJPG/OpenSimplex2
*/

#pragma once

#include <cstdint>

void noise_generate(uint64_t seed, uint32_t width, uint32_t height);
void noise_init(uint32_t width, uint32_t height, int8_t* map);
int8_t* noise_get_map();
uint32_t noise_get_width();
uint32_t noise_get_height();
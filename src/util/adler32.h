#pragma once

#include <cstdint>
#include <cstddef>

uint32_t adler32_scaler(uint8_t* data, size_t length);
uint32_t adler32_simd(uint8_t* data, size_t length);
void adler32_test(uint8_t* data, size_t length);
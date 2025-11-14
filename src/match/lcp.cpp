#include "lcg.h"

#include <cstdint>

uint32_t lcg_rand(uint32_t* previous) {
    uint32_t result = (((*previous * 1103515245U) + 12345U) >> 16) & 0x7fff;
    *previous = ((*previous * 1103515245U) + 12345U) & 0x7fffffff;
    return result;
}
#include "lcg.h"

#include <cstdint>

int32_t lcg_rand(int32_t* previous) {
    int result = (((*previous * 1103515245U) + 12345U) >> 16) & 0x7fff;
    *previous = ((*previous * 1103515245U) + 12345U) & 0x7fffffff;
    return result;
}
#include "lcg.h"

#include "core/logger.h"
#include <cstdint>

int32_t lcg_rand(int32_t* previous) {
    int result = (((*previous * 1103515245U) + 12345U) >> 16) & 0x7fff;
    log_trace("LCG previous %i result %i", *previous, result);
    *previous = ((*previous * 1103515245U) + 12345U) & 0x7fffffff;
    return result;
}
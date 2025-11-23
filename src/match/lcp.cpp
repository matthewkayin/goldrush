#include "lcg.h"

#include <cstdint>

int32_t lcg_rand(int32_t* previous) {
    *previous = ((*previous * 1103515245) + 12345) & 0x7fffffff;
    return *previous & 0x7fff;
}
#include "lcg.h"

#include "core/asserts.h"
#include <cstdint>

static int previous = 0;

void lcg_srand(int seed) {
    GOLD_ASSERT(sizeof(int) == sizeof(int32_t));
    previous = seed;
}

int lcg_rand() {
    int result = (((previous * 1103515245U) + 12345U) >> 16) & 0x7fff;
    previous = ((previous * 1103515245U) + 12345U) & 0x7fffffff;
    return result;
}
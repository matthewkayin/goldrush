#include "lcg.h"

#include "asserts.h"
#include <cstdint>

static int previous = 0;

void lcg_srand(int seed) {
    GOLD_ASSERT(sizeof(int) == sizeof(int32_t));
    previous = seed;
}

// short_rand uses only the upper 16 bits which creates a more "random" result for most of the game's use cases
// but the plain lcg_rand is still a good option for rand() % 2 because it created a perfect 50/50 ratio even at low sample sizes

int lcg_rand() {
    previous = ((previous * 1103515245U) + 12345U) & 0x7fffffff;
    return previous;
}

int lcg_short_rand() {
    int result = (((previous * 1103515245U) + 12345U) >> 16) & 0x7fff;
    previous = ((previous * 1103515245U) + 12345U) & 0x7fffffff;
    return result;
}
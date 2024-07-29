#include "lcg.h"

static const uint64_t multiplier = 1103515245;
static const uint64_t increment = 12345;
static const uint64_t modulus = UINT64_MAX;
static uint64_t previous = 0;

void lcg_srand(uint64_t seed) {
    previous = seed;
}

int lcg_rand() {
    previous = ((multiplier * previous) + increment) % modulus;
    return (int)(previous >> 32);
}
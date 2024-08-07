#include "lcg.h"

#include "asserts.h"
#include "logger.h"

static int previous = 0;

void lcg_srand(int seed) {
    GOLD_ASSERT(sizeof(int) == sizeof(int32_t));
    previous = seed;
}

int lcg_rand() {
    previous = ((previous * 1103515245U) + 12345U) & 0x7fffffff;
    return previous;
}
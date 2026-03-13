#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include <cstdio>

#define TEST_ASSERT(expr)                                                                            \
    {                                                                                                \
        if (expr) {                                                                                  \
        } else {                                                                                     \
            printf("Test assertion failure: %s, in file %s, line %d\n", #expr, __FILE__, __LINE__);  \
            return false;                                                                            \
        }                                                                                            \
    }                                                                                                \

void test_run_all();

#else

#define test_run_all()

#endif
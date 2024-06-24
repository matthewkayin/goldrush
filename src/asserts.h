#pragma once

#ifdef GOLD_DEBUG
    #if _MSC_VER
        #include <intrin.h>
        #define debug_break() __debugbreak()
    #else
        #define debug_break() __builtin_trap()
    #endif

    void report_assertion_failure(const char* expression, const char* message, const char* file, int line);

    #define GOLD_ASSERT(expr) {                                     \
        if (expr) { } else {                                         \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            debug_break();                                           \
        }                                                            \
    }                                                                

    #define GOLD_ASSERT_MESSAGE(expr, message) {                         \
        if (expr) { } else {                                              \
            report_assertion_failure(#expr, message, __FILE__, __LINE__); \
            debug_break();                                                \
        }                                                                 \
    }                                                                

#else 
    #define GOLD_ASSERT(expr)
    #define GOLD_ASSERT_MESSAGE(expr, message)
#endif 
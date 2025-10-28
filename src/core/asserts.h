#pragma once

#include "defines.h"

#if defined(__has_builtin) && !defined(__ibmx1__)
#   if __has_builtin(__builtin_debugtrap)
#       define gold_debug_break() __builtin_debugtrap()
#   elif __has_builtin(__debugbreak)
#       define gold_debug_break() __debugbreak()
#   endif
#endif

#if !defined(gold_debug_break)
#   if defined(__clang__) || defined(__gcc__)
#       define gold_debug_break() __builtin_trap()
#   elif defined(_MSC_VER)
#       include <intrin.h>
#       define gold_debug_break() __debugbreak()
#   else
#       define gold_debug_break() asm { int 3 }
#   endif
#endif

#ifdef GOLD_DEBUG
    void report_assertion_failure(const char* expression, const char* message, const char* file, int32_t line);

#   define GOLD_ASSERT(expr)                                             \
        {                                                                \
            if (expr) {                                                  \
            } else {                                                     \
                report_assertion_failure(#expr, "", __FILE__, __LINE__); \
                gold_debug_break();                                      \
            }                                                            \
        }

#   define GOLD_ASSERT_MESSAGE(expr, message)                                    \
        {                                                                        \
            if (expr) {                                                          \
            } else {                                                             \
                report_assertion_failure(#expr, message, __FILE__, __LINE__);    \
                gold_debug_break();                                              \
            }                                                                    \
        }
#else
#   define GOLD_ASSERT(expr)
#   define GOLD_ASSERT_MESSAGE(expr, message)
#endif
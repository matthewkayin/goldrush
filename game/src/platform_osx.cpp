#include "platform.h"

#ifdef PLATFORM_OSX

#include <cstdio>
#include <mach/mach_time.h>

static mach_timebase_info_data_t clock_timebase;
static uint64_t clock_start_time;

void platform_clock_init() {
    mach_timebase_info(&clock_timebase);
    clock_start_time = mach_absolute_time();
}

double platform_get_absolute_time() {
    uint64_t mach_absolute = mach_absolute_time();
    uint64_t nanos = (double)((mach_absolute - clock_start_time) * (uint64_t)clock_timebase.numer) / (double)clock_timebase.denom;
    return nanos / 1.0e9;
}

void platform_console_write(const char* message, int log_level) {
    static const char* TEXT_COLORS[4] = { 
        "31", // ERROR
        "33", // WARN
        "32", // INFO
        "0"  // TRACE
    };
    if (log_level == 0) {
        fprintf(stderr, "\x1B[%sm%s", TEXT_COLORS[log_level], message);
    } else {
        printf("\x1B[%sm%s", TEXT_COLORS[log_level], message);
    }
}

void platform_setup_unhandled_exception_filter() {

}

#endif
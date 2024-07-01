#include "platform.h"

#ifdef PLATFORM_OSX

#include <cstdio>
#include <mach/mach_time.h>

double platform_get_absolute_time() {
    mach_timebase_info_data_t clock_timebase;
    mach_timebase_info(&clock_timebase);

    uint64_t mach_absolute = mach_absolute_time();
    uint64_t nanos = (double)(mach_absolute * (uint64_t)clock_timebase.numer) / (double)clock_timebase.denom;
    return nanos / 1.0e9;
}

void platform_console_write(const char* message) {
    printf("\x1B[0m%s", message);
}

void platform_console_write_error(const char* message) {
    fprintf(stderr, "\x1B[31m%s", message);
}

#endif
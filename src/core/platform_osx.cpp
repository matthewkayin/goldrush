#include "platform.h"

#ifdef PLATFORM_OSX

#include <cstdio>
#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>

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
    #ifdef GOLD_STD_OUT
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
    #endif
}

const char* platform_get_resource_path(char* buffer, const char* subpath) {
    CFBundleRef main_bundle = CFBundleGetMainBundle();
    CFStringEncoding url_encoding = CFStringGetSystemEncoding();

    CFStringRef subpath_str = CFStringCreateWithCString(NULL, subpath, url_encoding);
    CFURLRef url = CFBundleCopyResourceURL(main_bundle, subpath_str, NULL, NULL);
    CFStringRef url_str = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);

    strcpy(buffer, CFStringGetCStringPtr(url_str, url_encoding));
    const char* path = CFStringGetCStringPtr(url_str, url_encoding);

    CFRelease(subpath_str);
    CFRelease(url_str);

    return path;
}

#endif
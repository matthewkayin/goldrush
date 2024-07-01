#include "platform.h"

#ifdef PLATFORM_OSX

void platform_console_write(const char* message) {
    printf("\x1B[0m%s", message);
}

void platform_console_write_error(const char* message) {
    fprintf(stderr, "\x1B[31m%s", message);
}

#endif
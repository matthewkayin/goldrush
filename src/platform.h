#pragma once

#include "defines.h"
#include <cstdint>

void platform_clock_init();
double platform_get_absolute_time();
void platform_console_write(const char* message, int log_level);
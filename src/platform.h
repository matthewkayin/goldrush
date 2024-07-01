#pragma once

#include "defines.h"

double platform_get_absolute_time();
void platform_console_write(const char* message);
void platform_console_write_error(const char* message);
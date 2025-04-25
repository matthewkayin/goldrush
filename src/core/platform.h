#pragma once

#include "defines.h"
#include "defines.h"
#include <cstdio>

void platform_clock_init();
double platform_get_absolute_time();
void platform_console_write(const char* message, int log_level);
const char* platform_get_resource_path(char* buffer, const char* subpath);
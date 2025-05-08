#pragma once

#include "defines.h"
#include "defines.h"
#include <cstdio>

void platform_console_write(const char* message, int log_level);

void platform_get_resource_path(char* buffer, const char* subpath);
void platform_get_datafile_path(char* buffer, const char* filename);
void platform_get_replay_path(char* buffer, const char* filename);

void platform_search_replays_folder(const char* search_query);
const char* platform_get_replay_file_name(size_t index);
size_t platform_get_replay_file_count();
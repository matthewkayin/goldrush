#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define PLATFORM_WIN32
#elif __APPLE__
    #define PLATFORM_OSX
#endif

#include <vector>
#include <string>

const char* platform_exe_path();
std::vector<std::string> platform_get_report_filenames();
bool platform_open_report_window();
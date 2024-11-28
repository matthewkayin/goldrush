#pragma once

#include <cstdint>

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.3.1"
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define MAX_PLAYERS 4
#define MAX_USERNAME_LENGTH 14
#define BASE_PORT 6530

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define PLATFORM_WIN32 1
    #define WIN32_LEAN_AND_MEAN
    #define SDL_MAIN_HANDLED
    #ifndef _WIN64
        #error "64-bit is required on Windows!"
    #endif
#elif __APPLE__
    #define PLATFORM_OSX 1
#endif

#define GOLD_DEBUG
#define GOLD_LOG_LEVEL 3
#define GOLD_ASSERTS_ENABLED
#define GOLD_RESOURCE_PATH "../res/"
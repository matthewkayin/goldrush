#pragma once

#include <cstdint>

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.1"
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define MAX_PLAYERS 6
#define TILE_SIZE 16
#define GOLD_RAND_SEED 1722091950
#define DEBUG_FAST_BUILD 1
#define DEBUG_FAST_TRAIN 1
#define DEBUG_SHOW_UNIT_PATHS 1
#define DEBUG_SHOW_UNIT_STATE 1

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define PLATFORM_WIN32 1
    #define SDL_MAIN_HANDLED
    #ifndef _WIN64
        #error "64-bit is required on Windows!"
    #endif
#elif __APPLE__
    #define PLATFORM_OSX 1
#endif
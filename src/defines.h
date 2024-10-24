#pragma once

#include <cstdint>

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.2.6"
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define MAX_PLAYERS 8
#define TILE_SIZE 16
#define UI_HEIGHT 88
// #define GOLD_RAND_SEED 1728833364
#ifdef GOLD_DEBUG
    #define GOLD_DEBUG_FAST_BUILD
    #define GOLD_DEBUG_FAST_TRAIN
    // #define GOLD_DEBUG_UNIT_PATHS
    // #define GOLD_DEBUG_UNIT_STATE
    // #define GOLD_DEBUG_MOUSE
    // #define GOLD_DEBUG_FOG_DISABLED
#endif

#ifdef GOLD_DEBUG
    #define RESOURCE_BASE_PATH "../res/"
#else
    #define RESOURCE_BASE_PATH "./res/"
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define PLATFORM_WIN32 1
    #define SDL_MAIN_HANDLED
    #ifndef _WIN64
        #error "64-bit is required on Windows!"
    #endif
#elif __APPLE__
    #define PLATFORM_OSX 1
#endif
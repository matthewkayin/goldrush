#pragma once

#include <cstdint>

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

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.5"

#define GOLD_DEBUG
#define GOLD_LOG_LEVEL 3
#define GOLD_RAND_SEED 1743160839

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define RESOURCE_PATH "../res/"
#define TILE_SIZE 16
#define MAX_PLAYERS 4
#define MAX_USERNAME_LENGTH 32
#define SELECTION_LIMIT 20

typedef uint16_t EntityId;
const EntityId ID_MAX = 4096;
const EntityId ID_NULL = ID_MAX + 1;
const uint32_t INDEX_INVALID = 65535;
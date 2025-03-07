#pragma once

#include <cstdint>

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.5"
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define MAX_PLAYERS 4
#define PLAYER_NONE UINT8_MAX
#define MAX_USERNAME_LENGTH 32
#define BASE_PORT 6530
#define SCANNER_PORT 6529
#define TILE_SIZE 16
#define INPUT_BUFFER_SIZE 1024
#define SELECTION_LIMIT 20

typedef uint16_t entity_id;
const entity_id ID_MAX = 4096;
const entity_id ID_NULL = ID_MAX + 1;
const uint32_t INDEX_INVALID = 65535;

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
#define GOLD_DEBUG_FAST_TRAIN
#define GOLD_DEBUG_FAST_BUILD
// #define GOLD_DEBUG_FOG_DISABLED
// #define GOLD_NEAR_SPAWN
// #define GOLD_DEBUG_UNIT_PATHS
// #define GOLD_RAND_SEED 1740999513
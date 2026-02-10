#pragma once

#include <cstdint>

#if defined(__clang__) || defined(__GNUC__)
    #define STATIC_ASSERT _Static_assert
#else
    #define STATIC_ASSERT static_assert
#endif

STATIC_ASSERT(sizeof(int) == sizeof(int32_t));

#if defined(_WIN32)
    #define PLATFORM_WIN32 1
    #define GOLD_PLATFORM_STR "WINDOWS"
    #define WIN32_LEAN_AND_MEAN
    #define SDL_MAIN_HANDLED
    #ifndef _WIN64
        #rror "64-bit is required on Windows!"
    #endif
#elif __APPLE__
    #define PLATFORM_MACOS 1
    #define GOLD_PLATFORM_STR "MACOS"
#elif defined(__linux__) || defined(__gnu_linux__)
    #define PLATFORM_LINUX 1
    #define GOLD_PLATFORM_STR "LINUX"
#endif

#define APP_NAME "Gold Rush"
#ifdef RELEASE_VERSION
    #define APP_VERSION RELEASE_VERSION
    #define GOLD_BUILD_TYPE_STR "Release"
#else
    #define APP_VERSION "dev"
    #define GOLD_DEBUG
    #define GOLD_BUILD_TYPE_STR "Debug"
#endif

#ifdef GOLD_DEBUG
    // #define GOLD_RAND_SEED 1768947521
    // #define GOLD_TEST_SEED 934781452
#endif

// #define GOLD_STEAM
#ifdef GOLD_STEAM
    // #define GOLD_STEAM_APP_ID 3774270U
    #define GOLD_STEAM_APP_ID 3831190U
#endif

#ifdef PLATFORM_WIN32
    #define GOLD_PATH_SEPARATOR '\\'
#else
    #define GOLD_PATH_SEPARATOR '/'
#endif

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define WINDOWED_WIDTH 1280
#define WINDOWED_HEIGHT 720
#define UPDATES_PER_SECOND 60
#define MAX_PLAYERS 4
#define MAP_SIZE_MAX 160
#define PLAYER_NONE MAX_PLAYERS
#define TILE_SIZE 16
#define MAX_USERNAME_LENGTH 32
#define SELECTION_LIMIT 20U
#define HOTKEY_GROUP_SIZE 6
#define MATCH_SHELL_UI_HEIGHT 88

typedef uint16_t EntityId;
const EntityId ID_MAX = UINT16_MAX;
const EntityId ID_NULL = ID_MAX;
const uint32_t INDEX_INVALID = UINT16_MAX;
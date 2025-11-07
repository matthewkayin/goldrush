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
    #define PLATFORM_MACOS 1
#elif defined(__linux__) || defined(__gnu_linux__)
    #define PLATFORM_LINUX 1
#endif

#define APP_NAME "Gold Rush"
#ifdef RELEASE_VERSION
    #define APP_VERSION RELEASE_VERSION
#else
    #define APP_VERSION "dev"
#endif

#define GOLD_STEAM
#ifdef GOLD_STEAM
    // #define GOLD_STEAM_APP_ID 3774270U
    #define GOLD_STEAM_APP_ID 3831190U
#endif

#ifndef GOLD_RELEASE
    #define GOLD_DEBUG
#endif
#define GOLD_LOG_LEVEL 3
#ifdef GOLD_DEBUG
    #define GOLD_RAND_SEED 1762429187
#endif

#define TRACY_ENABLE
#define TRACY_NO_CALLSTACK

#define UPDATES_PER_SECOND 60
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define TILE_SIZE 32
#define MAX_PLAYERS 4
#define MAX_USERNAME_LENGTH 32
#define PLAYER_NONE MAX_PLAYERS
#define SELECTION_LIMIT 20U
#define HOTKEY_GROUP_SIZE 6

#define PATHFIND_ITERATION_MAX 1999

typedef uint16_t EntityId;
const EntityId ID_MAX = 4096;
const EntityId ID_NULL = ID_MAX + 1;
const uint32_t INDEX_INVALID = 65535;
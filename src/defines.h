#pragma once

#include <cstdint>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define PLATFORM_WIN32 1
    #define WIN32_LEAN_AND_MEAN
    #define SDL_MAIN_HANDLED
    #ifndef _WIN64
        #rror "64-bit is required on Windows!"
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
    #define GOLD_DEBUG
#endif

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define UPDATES_PER_SECOND 60
#define MAX_PLAYERS 4
#define PLAYER_NONE MAX_PLAYERS
#define TILE_SIZE 32
#define MAX_USERNAME_LENGTH 32
#define SELECTION_LIMIT 20U
#define HOTKEY_GROUP_SIZE 6
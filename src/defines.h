#pragma once

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.1"
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 360
#define MAX_PLAYERS 8
#define MAX_UNITS 200
#define MAX_BUILDINGS 64
#define TILE_SIZE 16

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    #define PLATFORM_WIN32 1
    #define SDL_MAIN_HANDLED
    #ifndef _WIN64
        #error "64-bit is required on Windows!"
    #endif
#elif __APPLE__
    #define PLATFORM_OSX 1
#endif
#pragma once

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

#define TILE_SIZE 16

#define GOLD_DEBUG
#define GOLD_LOG_LEVEL 3
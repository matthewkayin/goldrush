#include "defines.h"
#include "core/platform.h"
#include "core/logger.h"
#include "math/gmath.h"
#include "render/render.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cstring>

#define APP_NAME "GOLD RUSH"
#define APP_VERSION "0.5"

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 && !defined GOLD_DEBUG
#include <windows.h>

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR lp_cmd_line, int n_cmd_show) {
    return gold_main(0, NULL);
}
#else
int main(int argc, char** argv) {
    return gold_main(argc, argv);
}
#endif

int gold_main(int argc, char** argv) {
    char logfile_path[128];
    sprintf(logfile_path, "./logs.log");

    // Parse system arguments
    for (int argn = 1; argn < argc; argn++) {
        if (strcmp(argv[argn], "--logfile") == 0 && argn + 1 < argc) {
            argn++;
            strcpy(logfile_path, argv[argn]);
        }
    }

    if(!logger_init(logfile_path)) {
        return -1;
    }
    platform_setup_unhandled_exception_filter();
    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    #if defined(PLATFORM_WIN32)
        log_info("Detected platform WIN32.");
    #elif defined(PLATFORM_OSX)
        log_info("Detected platform OSX.");
    #endif
    platform_clock_init();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        log_error("SDL failed to initialize: %s", SDL_GetError());
        logger_quit();
        return -1;
    }

    // Init SDL image
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log_error("SDL_image failed to initialize: %s", IMG_GetError());
        return false;
    }

    // Init TTF
    if (TTF_Init() == -1) {
        log_error("SDL_ttf failed to initialize: %s", TTF_GetError());
        logger_quit();
        return -1;
    }

    uint32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
    ivec2 window_size = ivec2(1280, 720);
    SDL_Window* window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_size.x, window_size.y, window_flags);

    if (!render_init(window)) {
        logger_quit();
        return -1;
    }

    bool is_running = true;
    bool render_debug_info = true;
    const double UPDATE_TIME = 1.0 / 60;
    double last_time = platform_get_absolute_time();
    double last_second = last_time;
    double update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    while (is_running) {
        // TIMEKEEP
        double current_time = platform_get_absolute_time();
        update_accumulator += current_time - last_time;
        last_time = current_time;

        if (current_time - last_second >= 1.0) {
            fps = frames;
            frames = 0;
            ups = updates;
            updates = 0;
            last_second += 1.0;
        }
        frames++;

        // INPUT
        if (update_accumulator >= UPDATE_TIME) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                // Handle quit
                if (event.type == SDL_QUIT) {
                    is_running = false;
                    break;
                }

                // Capture mouse
                if (SDL_GetWindowGrab(window) == SDL_FALSE) {
                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        SDL_SetWindowGrab(window, SDL_TRUE);
                        continue;
                    }
                    // If the mouse is not captured, don't handle any other input
                    break;
                }
                // Release mouse
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
                    SDL_SetWindowGrab(window, SDL_FALSE);
                    break;
                }

                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3) {
                    render_debug_info = !render_debug_info;
                    continue;
                }
            }
        }

        // UPDATE
        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;
        }

        // RENDER
        render_prepare_frame();
        render_present_frame();
    }

    render_quit();

    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    log_info("Application quit gracefully.");

    return 0;
}
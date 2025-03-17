#include "defines.h"
#include "core/platform.h"
#include "core/logger.h"
#include "core/cursor.h"
#include "core/animation.h"
#include "core/input.h"
#include "math/gmath.h"
#include "render/render.h"
#include "menu/menu.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cstring>

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
    if (!cursor_init()) {
        logger_quit();
        return -1;
    }
    animation_init();
    input_init();

    bool is_running = true;
    bool render_debug_info = false;
    const double UPDATE_TIME = 1.0 / 60;
    double last_time = platform_get_absolute_time();
    double last_second = last_time;
    double update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    menu_state_t menu_state = menu_init();

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

            #ifdef GOLD_DEBUG
                if (ups != 60) {
                    log_warn("Update count is off! ups: %u", ups);
                }
            #endif
        }
        frames++;

        // INPUT
        if (update_accumulator >= UPDATE_TIME) {
            input_poll_events(window);
            if (input_user_requests_exit()) {
                is_running = false;
                break;
            }

            if (input_is_action_just_pressed(INPUT_F3)) {
                render_debug_info = !render_debug_info;
            }
        }

        // UPDATE
        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;

            menu_update(menu_state);
        }

        // RENDER
        render_prepare_frame();

        menu_render(menu_state);
        if (render_debug_info) {
            char fps_text[32];
            sprintf(fps_text, "FPS: %u", fps);
            render_text(FONT_HACK_OFFBLACK, fps_text, ivec2(0, 0));
        }

        render_present_frame();
    }

    render_quit();

    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    log_info("Application quit gracefully.");

    return 0;
}
#include "defines.h"
#include "core/platform.h"
#include "core/logger.h"
#include "core/cursor.h"
#include "core/animation.h"
#include "core/input.h"
#include "core/network.h"
#include "core/sound.h"
#include "core/options.h"
#include "math/gmath.h"
#include "render/render.h"
#include "menu/menu.h"
#include "match/ui.h"
#include "match/noise.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <cstring>
#include <ctime>
#include <cstdlib>

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

enum GameMode {
    GAME_MODE_MENU,
    GAME_MODE_MATCH
};

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
    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    #if defined(PLATFORM_WIN32)
        log_info("Detected platform WIN32.");
    #elif defined(PLATFORM_OSX)
        log_info("Detected platform OSX.");
    #endif
    platform_clock_init();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        log_error("SDL failed to initialize: %s", SDL_GetError());
        logger_quit();
        return -1;
    }

    // Init TTF
    if (!TTF_Init()) {
        log_error("SDL_ttf failed to initialize: %s", SDL_GetError());
        logger_quit();
        return -1;
    }

    uint32_t window_flags = SDL_WINDOW_OPENGL;
    ivec2 window_size = ivec2(1280, 720);
    SDL_Window* window = SDL_CreateWindow(APP_NAME, window_size.x, window_size.y, window_flags);

    if (!render_init(window)) {
        logger_quit();
        return -1;
    }
    if (!cursor_init()) {
        logger_quit();
        return -1;
    }
    if (!sound_init()) {
        logger_quit();
        return -1;
    }
    if (!network_init()) {
        logger_quit();
        return -1;
    }
    animation_init();
    input_init(window);
    options_load();
    srand(time(NULL));

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

    GameMode game_mode = GAME_MODE_MENU;
    MenuState menu_state = menu_init();
    MatchUiState match_ui_state;

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
            input_poll_events();
            if (input_user_requests_exit()) {
                is_running = false;
                break;
            }

            if (input_is_action_just_pressed(INPUT_ACTION_F3)) {
                render_debug_info = !render_debug_info;
            }

            network_service();
            NetworkEvent event;
            while (network_poll_events(&event)) {
                switch (game_mode) {
                    case GAME_MODE_MENU: {
                        if (event.type == NETWORK_EVENT_MATCH_LOAD) {
                            network_scanner_destroy();
                            match_ui_state = match_ui_init(event.match_load.lcg_seed, event.match_load.noise);
                            free(event.match_load.noise.map);
                            game_mode = GAME_MODE_MATCH;
                        } else {
                            menu_handle_network_event(menu_state, event);
                        }
                        break;
                    }
                    case GAME_MODE_MATCH: {
                        match_ui_handle_network_event(match_ui_state, event);
                        break;
                    }
                }
            }
        }

        // UPDATE
        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;

            switch (game_mode) {
                case GAME_MODE_MENU: {
                    menu_update(menu_state);
                    if (menu_state.mode == MENU_MODE_EXIT) {
                        is_running = false;
                        break;
                    } else if (menu_state.mode == MENU_MODE_LOAD_MATCH) {
                        network_scanner_destroy();

                        // This is when the host is beginning a match load
                        // Set LCG seed
                        #ifdef GOLD_RAND_SEED
                            int32_t lcg_seed = GOLD_RAND_SEED;
                        #else
                            int32_t lcg_seed = (int32_t)time(NULL);
                        #endif

                        // Generate noise for map generation
                        uint64_t noise_seed = (uint64_t)lcg_seed;
                        uint32_t map_width = 128;
                        uint32_t map_height = map_width;
                        Noise noise = noise_generate(noise_seed, map_width, map_height);

                        network_begin_loading_match(lcg_seed, noise);

                        match_ui_state = match_ui_init(lcg_seed, noise);
                        free(noise.map);
                        game_mode = GAME_MODE_MATCH;
                    }
                    break;
                }
                case GAME_MODE_MATCH: {
                    match_ui_update(match_ui_state);
                    if (match_ui_state.mode == MATCH_UI_MODE_LEAVE_MATCH) {
                        sound_end_fire_loop();
                        menu_state = menu_init();
                        game_mode = GAME_MODE_MENU;
                    }
                    break;
                }
            }
        }

        sound_update();

        // RENDER
        render_prepare_frame();

        switch (game_mode) {
            case GAME_MODE_MENU: {
                menu_render(menu_state);
                break;
            }
            case GAME_MODE_MATCH: {
                match_ui_render(match_ui_state);
                break;
            }
        }

        if (render_debug_info) {
            char fps_text[32];
            sprintf(fps_text, "FPS: %u", fps);
            render_text(FONT_HACK_WHITE, fps_text, ivec2(0, 0));
            render_sprite_batch();
        }

        render_present_frame();
    }

    network_quit();
    sound_quit();
    render_quit();

    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    options_save();

    log_info("Application quit gracefully.");

    return 0;
}
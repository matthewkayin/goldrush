#include "defines.h"
#include "logger.h"
#include "platform.h"
#include "engine.h"
#include "menu.h"
#include "ui.h"
#include "network.h"
#include "lcg.h"
#include <ctime>
#include <cstdio>

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 && !defined GOLD_DEBUG
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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

    FILE* logfile = logger_init(logfile_path);
    if (logfile == NULL) {
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
    if (!network_init()) {
        logger_quit();
        return -1;
    }
    if (!engine_init()) {
        logger_quit();
        return -1;
    }

    const double UPDATE_TIME = 1.0 / 60;
    double last_time = platform_get_absolute_time();
    double last_second = last_time;
    double update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    bool game_is_running = true;
    GameMode game_mode = GAME_MODE_MENU;
    menu_state_t menu_state = menu_init();
    ui_state_t ui_state;

    while (game_is_running) {
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
                    game_is_running = false;
                    break;
                }
                // Capture mouse
                if (SDL_GetWindowGrab(engine.window) == SDL_FALSE) {
                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        SDL_SetWindowGrab(engine.window, SDL_TRUE);
                        continue;
                    }
                    // If the mouse is not captured, don't handle any other input
                    break;
                }
                // Release mouse
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
                    SDL_SetWindowGrab(engine.window, SDL_FALSE);
                    break;
                }
                // Update mouse position
                if (event.type == SDL_MOUSEMOTION) {
                    engine.mouse_position = xy(event.motion.x, event.motion.y);
                    continue;
                }
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3) {
                    engine.render_debug_info = !engine.render_debug_info;
                    continue;
                }
                // If input not handled, pass event to current game mode
                switch (game_mode) {
                    case GAME_MODE_MENU: 
                        menu_handle_input(menu_state, event);
                        break;
                    case GAME_MODE_MATCH:
                        ui_handle_input(ui_state, event);
                        if (ui_state.mode == UI_MODE_LEAVE_MATCH) {
                            menu_state = menu_init();
                            game_mode = GAME_MODE_MENU;
                        }
                        break;
                }
            } // End while PollEvent
        } // End if should handle input

        network_service();
        network_event_t network_event;
        while (network_poll_events(&network_event)) {
            switch (game_mode) {
                case GAME_MODE_MENU: {
                    if (network_event.type == NETWORK_EVENT_MATCH_LOAD) {
                        network_scanner_destroy();
                        ui_state = ui_init(network_event.match_load.lcg_seed, network_event.match_load.noise);
                        free(network_event.match_load.noise.map);
                        game_mode = GAME_MODE_MATCH;
                    } else {
                        menu_handle_network_event(menu_state, network_event);
                    }
                    break;
                }
                case GAME_MODE_MATCH: {
                    ui_handle_network_event(ui_state, network_event);
                    break;
                }
            }
        }

        // UPDATE
        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;

            switch (game_mode) {
                case GAME_MODE_MENU: 
                    menu_update(menu_state);
                    if (menu_state.mode == MENU_MODE_EXIT) {
                        game_is_running = false;
                    } else if (menu_state.mode == MENU_MODE_LOAD_MATCH) {
                        network_scanner_destroy();

                        // This is when the host is beginning a match load
                        // Set LCG seed
                        #ifdef GOLD_RAND_SEED
                            int32_t lcg_seed = GOLD_RAND_SEED;
                        #else
                            int32_t lcg_seed = (int32_t)time(NULL);
                        #endif
                        lcg_srand(lcg_seed);
                        log_trace("Host: set random seed to %i", lcg_seed);

                        // Generate noise for map generation
                        uint64_t noise_seed = (uint64_t)lcg_seed;
                        uint32_t map_width = MAP_SIZE_DATA.at((MapSize)network_get_match_setting(MATCH_SETTING_MAP_SIZE)).tile_size;
                        uint32_t map_height = map_width;
                        noise_t noise = noise_generate(noise_seed, map_width, map_height);

                        network_begin_loading_match(lcg_seed, noise);

                        ui_state = ui_init(lcg_seed, noise);
                        free(noise.map);
                        game_mode = GAME_MODE_MATCH;
                    }
                    break;
                case GAME_MODE_MATCH:
                    ui_update(ui_state);
                    break;
            }
        } // End while update

        // RENDER
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderClear(engine.renderer);

        switch (game_mode) {
            case GAME_MODE_MENU: 
                menu_render(menu_state);
                break;
            case GAME_MODE_MATCH:
                ui_render(ui_state);
                break;
        }

        if (engine.render_debug_info) {
            char text_buffer[32];
            sprintf(text_buffer, "FPS: %u | UPS: %u", fps, ups);
            render_text(FONT_HACK_WHITE, text_buffer, xy(0, 0));

            switch (network_get_status()) {
                case NETWORK_STATUS_OFFLINE:
                    sprintf(text_buffer, "Offline");
                    break;
                case NETWORK_STATUS_SERVER:
                    sprintf(text_buffer, "Server");
                    break;
                case NETWORK_STATUS_CONNECTING:
                    sprintf(text_buffer, "Connecting");
                    break;
                case NETWORK_STATUS_CONNECTED:
                    sprintf(text_buffer, "Connected");
                    break;
                case NETWORK_STATUS_DISCONNECTING:
                    sprintf(text_buffer, "Disconnecting");
                    break;
            }
            render_text(FONT_HACK_WHITE, text_buffer, xy(0, 24));
        }

        SDL_RenderPresent(engine.renderer);
    }

    network_quit();
    engine_quit();
    logger_quit();

    log_info("%s quit gracefully.", APP_NAME);
    
    return 0;
}
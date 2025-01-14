#include "defines.h"
#include "logger.h"
#include "platform.h"
#include "engine.h"
#include "menu.h"
#include "match.h"
#include "network.h"
#include <ctime>
#include <cstdio>

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 && !defined GOLD_DEBUG
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
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    sprintf(logfile_path, "./logs/%d-%02d-%02dT%02d%02d%02d.log", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);

    // Parse system arguments
    for (int argn = 1; argn < argc; argn++) {
        if (strcmp(argv[argn], "--logfile") == 0 && argn + 1 < argc) {
            argn++;
            strcpy(logfile_path, argv[argn]);
        }
    }

    logger_init(logfile_path);
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
    bool render_fps = false;

    bool game_is_running = true;
    GameMode game_mode = GAME_MODE_MENU;
    menu_state_t menu_state = menu_init();
    match_state_t match_state;

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
                    render_fps = !render_fps;
                    continue;
                }
                // If input not handled, pass event to current game mode
                switch (game_mode) {
                    case GAME_MODE_MENU: 
                        menu_handle_input(menu_state, event);
                        break;
                    case GAME_MODE_MATCH:
                        match_handle_input(match_state, event);
                        if (match_state.ui_mode == UI_MODE_LEAVE_MATCH) {
                            menu_state = menu_init();
                            game_mode = GAME_MODE_MENU;
                        }
                        break;
                }
            } // End while PollEvent
        } // End if should handle input

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
                        match_state = match_init();
                        game_mode = GAME_MODE_MATCH;
                    }
                    break;
                case GAME_MODE_MATCH:
                    match_update(match_state);
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
                match_render(match_state);
                break;
        }

        if (render_fps) {
            char text_buffer[16];
            sprintf(text_buffer, "FPS: %u", fps);
            render_text(FONT_HACK_WHITE, text_buffer, xy(0, 0));
            sprintf(text_buffer, "UPS: %u", ups);
            render_text(FONT_HACK_WHITE, text_buffer, xy(0, 12));
        }

        SDL_RenderPresent(engine.renderer);
    }

    network_quit();
    engine_quit();
    logger_quit();

    log_info("%s quit gracefully.", APP_NAME);
    
    return 0;
}
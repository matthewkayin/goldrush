#include "defines.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/input.h"
#include "core/cursor.h"
#include "core/sound.h"
#include "core/options.h"
#include "network/network.h"
#include "render/render.h"
#include "menu/menu.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <ctime>

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 and not defined GOLD_DEBUG
#include <windows.h>
#define MAX_ARGS 16

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR lp_cmd_line, int n_cmd_show) {
    char arg_buffer[MAX_ARGS][256];
    char* argv[MAX_ARGS];
    int argv_index = 0;
    int argc = 0;

    char* command_line = GetCommandLineA();
    int command_line_index = 0;
    bool is_in_quotes = false;
    while (command_line[command_line_index] != '\0') {
        if (command_line[command_line_index] == '"') {
            is_in_quotes = !is_in_quotes;
        } else {
            arg_buffer[argc][argv_index] = command_line[command_line_index];
            argv_index++;
        }
        command_line_index++;

        if ((command_line[command_line_index] == ' ' && !is_in_quotes) || command_line[command_line_index] == '\0') {
            command_line_index++;
            arg_buffer[argc][argv_index] = '\0';
            argv[argc] = arg_buffer[argc];
            argc++;
            argv_index = 0;
        }
    }

    return gold_main(argc, argv);
}
#else
int main(int argc, char** argv) {
    return gold_main(argc, argv);
}
#endif

static const uint64_t UPDATE_DURATION = SDL_NS_PER_SECOND / UPDATES_PER_SECOND;

enum GameMode {
    GAME_MODE_NONE,
    GAME_MODE_MENU,
    GAME_MODE_MATCH
};

struct GameState {
    GameMode mode = GAME_MODE_NONE;
    MenuState* menu_state = nullptr;
};

void game_set_mode(GameState& state, GameMode next_mode) {
    if (next_mode == GAME_MODE_MENU) {
        state.menu_state = menu_init();
    }

    if (state.mode == GAME_MODE_MENU) {
        delete state.menu_state;
        state.menu_state = nullptr;
    }

    state.mode = next_mode;
}

int gold_main(int argc, char** argv) {
    #ifdef GOLD_STEAM
        if (SteamAPI_RestartAppIfNecessary(GOLD_STEAM_APP_ID)) {
            return 1;
        }
    #endif

    std::string logfile_path = filesystem_get_timestamp_str() + ".log";

    // Parse system arguments
    for (int argn = 1; argn < argc; argn++) {
        #ifdef GOLD_DEBUG
            if (strcmp(argv[argn], "--logfile") == 0 && argn + 1 < argc) {
                argn++;
                logfile_path = argv[argn];
            }
        #endif
    }

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return 1;
    }
    if (!TTF_Init()) {
        return 1;
    }

    filesystem_create_required_folders();

    if (!logger_init(logfile_path.c_str())) {
        return 1;
    }

    // Log initialization messages
    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    #if defined(PLATFORM_WIN32)
        log_info("Detected platform WIN32.");
    #elif defined(PLATFORM_MACOS);
        log_info("Detected platform MACOS.");
    #elif defined(PLATFORM_LINUX)
        log_info("Detected platform LINUX.");
    #endif
    #ifdef GOLD_DEBUG
        log_info("Debug build.");
    #elif
        log_info("Release build.");
    #endif

    // Create window
    SDL_Window* window = SDL_CreateWindow(APP_NAME, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    // Init subsystems
    if (!render_init(window)) {
        logger_quit();
        return 1;
    }
    if (!cursor_init()) {
        logger_quit();
        return 1;
    }
    if (!sound_init()) {
        logger_quit();
        return 1;
    }
    if (!network_init()) {
        logger_quit();
        return 1;
    }
    input_init(window);
    options_load();
    srand((uint32_t)time(0));

    uint64_t last_time = SDL_GetTicksNS();
    uint64_t last_second = last_time;
    uint64_t update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;

    GameState state;
    game_set_mode(state, GAME_MODE_MENU);

    while (!input_user_requests_exit()) {
        // Timekeep
        uint64_t current_time = SDL_GetTicksNS();
        update_accumulator += current_time - last_time;
        last_time = current_time;

        if (current_time - last_second >= SDL_NS_PER_SECOND) {
            fps = frames;
            frames = 0;
            last_second += SDL_NS_PER_SECOND;
        }
        frames++;

        // Update
        while (update_accumulator >= UPDATE_DURATION) {
            update_accumulator -= UPDATE_DURATION;

            // Input
            input_poll_events();

            // Network
            network_service();
            NetworkEvent event;
            while (network_poll_events(&event)) {
                switch (state.mode) {
                    case GAME_MODE_NONE: 
                        break;
                    case GAME_MODE_MENU: {
                        menu_handle_network_event(state.menu_state, event);
                        break;
                    }
                    case GAME_MODE_MATCH: {
                        break;
                    }
                }
            }

            // Update
            switch (state.mode) {
                case GAME_MODE_NONE:
                    break;
                case GAME_MODE_MENU: {
                    menu_update(state.menu_state);
                    break;
                }
                case GAME_MODE_MATCH: {
                    break;
                }
            }
        }

        sound_update();

        // Render
        render_prepare_frame();

        switch (state.mode) {
            case GAME_MODE_NONE:
                break;
            case GAME_MODE_MENU: {
                menu_render(state.menu_state);
                break;
            }
            case GAME_MODE_MATCH: {
                break;
            }
        }

        render_present_frame();
    }

    // Disconnect the network
    if (network_get_status() == NETWORK_STATUS_CONNECTED || network_get_status() == NETWORK_STATUS_HOST) {
        network_disconnect();
    }

    // Delete the current game sub-state
    game_set_mode(state, GAME_MODE_NONE);

    options_save();

    // Quit subsystems
    network_quit();
    sound_quit();
    cursor_quit();
    render_quit();
    logger_quit();

#ifdef GOLD_STEAM
    SteamAPI_Shutdown();
#endif

    TTF_Quit();
    SDL_Quit();

    log_info("%s quit gracefully.", APP_NAME);

    return 0;
}
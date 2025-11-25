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
#include "shell/shell.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <string>
#include <ctime>

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

int gold_main(int argc, char** argv);

#ifdef PLATFORM_WIN32
    #include <windows.h>
    #define FILENAME_MAX_LENGTH MAX_PATH
#endif

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
    GAME_MODE_MATCH,
    GAME_MODE_REPLAY
};

struct GameState {
    GameMode mode = GAME_MODE_NONE;
    MenuState* menu_state = nullptr;
    MatchShellState* match_shell_state = nullptr;
};

#ifdef GOLD_STEAM
    struct GameSetModeMenuParams {
        uint64_t steam_invite_id;
    };
#endif

struct GameSetModeMatchParams {
    int lcg_seed;
    Noise noise;
};

struct GameSetModeReplayParams {
    char filename[FILENAME_MAX_LENGTH + 1];
};

struct GameSetModeParams {
    GameMode mode;
    union {
        #ifdef GOLD_STEAM
            GameSetModeMenuParams menu;
        #endif
        GameSetModeMatchParams match;
        GameSetModeReplayParams replay;
    };
};

void game_set_mode(GameState& state, GameSetModeParams params) {
    if (params.mode == GAME_MODE_MENU) {
        state.menu_state = menu_init();

        #ifdef GOLD_STEAM
            if (params.menu.steam_invite_id != 0) {
                network_set_backend(NETWORK_BACKEND_STEAM);
                CSteamID steam_id;
                steam_id.SetFromUint64(params.menu.steam_invite_id);
                network_steam_accept_invite(steam_id);
            }
        #endif
    }
    if (params.mode == GAME_MODE_MATCH) {
        state.match_shell_state = match_shell_init(params.match.lcg_seed, params.match.noise);
        free(params.match.noise.map);
    }
    if (params.mode == GAME_MODE_REPLAY) {
        state.match_shell_state = replay_shell_init(params.replay.filename);
        if (state.match_shell_state == nullptr) {
            menu_show_status(state.menu_state, "Error opening replay file.");
            return;
        }
    }

    if (state.mode == GAME_MODE_MENU) {
        delete state.menu_state;
        state.menu_state = nullptr;
    }
    if (state.mode == GAME_MODE_MATCH || state.mode == GAME_MODE_REPLAY) {
        sound_end_fire_loop();
        delete state.match_shell_state;
        state.match_shell_state = nullptr;
    }

    state.mode = params.mode;
}

bool game_is_running(const GameState& state) {
    if (input_user_requests_exit()) {
        return false;
    }
    if (state.mode == GAME_MODE_MENU && state.menu_state->mode == MENU_MODE_EXIT) {
        return false;
    }
    if ((state.mode == GAME_MODE_MATCH || state.mode == GAME_MODE_REPLAY) && state.match_shell_state->mode == MATCH_SHELL_MODE_EXIT_PROGRAM) {
        return false;
    }

    return true;
}

int gold_main(int argc, char** argv) {
    #ifdef GOLD_STEAM
        if (SteamAPI_RestartAppIfNecessary(GOLD_STEAM_APP_ID)) {
            return 1;
        }

        uint64_t steam_invite_id = 0;
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
        #ifdef GOLD_STEAM
            if (strcmp(argv[argn], "+connect_lobby") == 0 && argn + 1 < argc) {
                argn++;
                steam_invite_id = std::stoull(argv[argn]);
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

    #ifdef GOLD_STEAM
        if (!SteamAPI_Init()) {
            log_error("Error initializing Steam API.");
            return 1;
        }
        log_info("Initialized Steam API.");
        if (steam_invite_id != 0) {
            log_debug("Got connect_lobby from sys args. steam_invite_id: %u", steam_invite_id);
        }
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

    // Set game mode to menu
    {
        GameSetModeParams params;
        params.mode = GAME_MODE_MENU;
        #ifdef GOLD_STEAM
            params.menu.steam_invite_id = steam_invite_id;
        #endif
        game_set_mode(state, params);
    }

    while (game_is_running(state)) {
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
                        match_shell_handle_network_event(state.match_shell_state, event);
                        break;
                    }
                    case GAME_MODE_REPLAY:
                        break;
                }
            }

            // Update
            switch (state.mode) {
                case GAME_MODE_NONE:
                    break;
                case GAME_MODE_MENU: {
                    menu_update(state.menu_state);

                    // Host begin load match
                    if (state.menu_state->mode == MENU_MODE_LOAD_MATCH) {
                        // Set LCG Seed
                        #ifdef GOLD_RAND_SEED
                            int lcg_seed = GOLD_RAND_SEED;
                        #else
                            int lcg_seed = (int)time(NULL);
                        #endif

                        // Generate noise
                        uint64_t noise_seed = (uint64_t)lcg_seed;
                        int map_width = match_setting_get_map_size((MatchSettingMapSizeValue)network_get_match_setting(MATCH_SETTING_MAP_SIZE));
                        int map_height = map_width;
                        Noise noise = noise_generate(noise_seed, map_width, map_height);

                        network_begin_loading_match(lcg_seed, noise);

                        game_set_mode(state, (GameSetModeParams) {
                            .mode = GAME_MODE_MATCH,
                            .match = (GameSetModeMatchParams) {
                                .lcg_seed = lcg_seed,
                                .noise = noise
                            }
                        });
                    } else if (state.menu_state->mode == MENU_MODE_LOAD_REPLAY) {
                        GameSetModeParams params;
                        params.mode = GAME_MODE_REPLAY;
                        strncpy(params.replay.filename, menu_get_selected_replay_filename(state.menu_state), FILENAME_MAX_LENGTH);
                        game_set_mode(state, params);
                    }

                    break;
                }
                case GAME_MODE_MATCH:
                case GAME_MODE_REPLAY: {
                    match_shell_update(state.match_shell_state);

                    if (state.match_shell_state->mode == MATCH_SHELL_MODE_LEAVE_MATCH) {
                        GameSetModeParams params;
                        params.mode = GAME_MODE_MENU;
                        #ifdef GOLD_STEAM
                            params.menu.steam_invite_id = 0;
                        #endif
                        game_set_mode(state, params);
                    }

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
            case GAME_MODE_MATCH:
            case GAME_MODE_REPLAY: {
                match_shell_render(state.match_shell_state);
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
    game_set_mode(state, (GameSetModeParams) {
        .mode = GAME_MODE_NONE
    });

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
#include "defines.h"
#include "util.h"
#include "core/filesystem.h"
#include "core/logger.h"
#include "core/cursor.h"
#include "core/animation.h"
#include "core/input.h"
#include "network/network.h"
#include "core/sound.h"
#include "core/options.h"
#include "math/gmath.h"
#include "render/render.h"
#include "menu/menu.h"
#include "match/ui.h"
#include "match/noise.h"
#include "match/replay.h"
#include "network/network.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <steam/steam_api.h>

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

enum GameMode {
    GAME_MODE_MENU,
    GAME_MODE_MATCH,
    GAME_MODE_REPLAY,
    GAME_MODE_LOADING
};

struct LoadMenuParams {
    uint64_t steam_invite_id;
};

struct LoadMatchParams {
    int32_t lcg_seed;
    Noise noise;
};

struct LoadReplayParams {
    char filename[256];
};

struct LoadParams {
    GameMode mode;
    union {
        LoadMenuParams menu;
        LoadMatchParams match;
        LoadReplayParams replay;
    };
};

struct GameState {
    GameMode mode;
    MatchUiState match;
    MenuState menu;
    LoadParams load_params;
    SDL_Thread* loading_thread;
    Animation loading_animation;
};
static GameState state;

static int game_load_next_mode(void* ptr) {
    if (state.load_params.mode == GAME_MODE_MENU) {
        state.menu = menu_init();
        if (state.load_params.menu.steam_invite_id != 0) {
            network_set_backend(NETWORK_BACKEND_STEAM);
            network_steam_accept_invite(state.load_params.menu.steam_invite_id);
        }
    } else if (state.load_params.mode == GAME_MODE_MATCH) {
        state.match = match_ui_init(state.load_params.match.lcg_seed, state.load_params.match.noise);
        free(state.load_params.match.noise.map);
    } else if (state.load_params.mode == GAME_MODE_REPLAY) {
        state.match = match_ui_init_from_replay(state.load_params.replay.filename);
    }

    return 0;
}

void game_set_mode(LoadParams params) {
    input_stop_text_input();
    state.load_params = params;
    state.mode = GAME_MODE_LOADING;
    state.loading_animation = animation_create(ANIMATION_UNIT_BUILD);
    state.loading_thread = SDL_CreateThread(game_load_next_mode, "loading_thread", (void*)NULL);
    if (!state.loading_thread) {
        log_error("Error creating loading thread: %s", SDL_GetError());
    }
}

int gold_main(int argc, char** argv) {
    if (SteamAPI_RestartAppIfNecessary(GOLD_STEAM_APP_ID)) {
        return 1;
    }

    char logfile_path[128];
    char* logfile_ptr = logfile_path;
    logfile_ptr += sprintf(logfile_ptr, "logs/");
    logfile_ptr += sprintf_timestamp(logfile_ptr);
    logfile_ptr += sprintf(logfile_ptr, ".log");
    uint64_t steam_invite_id = 0;

    // Parse system arguments
    for (int argn = 1; argn < argc; argn++) {
        #ifdef GOLD_DEBUG
        if (strcmp(argv[argn], "--logfile") == 0 && argn + 1 < argc) {
            argn++;
            strcpy(logfile_path, argv[argn]);
        }
        if (strcmp(argv[argn], "--replay-file") == 0 && argn + 1 < argc) {
            argn++;
            replay_debug_set_file_name(argv[argn]);
        }
        #endif
        if (strcmp(argv[argn], "+connect_lobby") == 0 && argn + 1 < argc) {
            argn++;
            steam_invite_id = std::stoull(argv[argn]);
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return -1;
    }

    // Init TTF
    if (!TTF_Init()) {
        return -1;
    }

    filesystem_create_data_folder("logs");
    filesystem_create_data_folder("replays");

    if(!logger_init(logfile_path)) {
        return -1;
    }

    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    #if defined(PLATFORM_WIN32)
        log_info("Detected platform WIN32.");
    #elif defined(PLATFORM_OSX)
        log_info("Detected platform OSX.");
    #endif

    if (!SteamAPI_Init()) {
        log_error("Error initializing Steam API.");
        return -1;
    }
    log_info("Initialized Steam API");
    if (steam_invite_id != 0) {
        log_trace("Got connect_lobby from sys args, steam_invite_id:%u", steam_invite_id);
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
    match_setting_init();
    input_init(window);
    options_load();
    srand(time(NULL));

    log_info("base path %s", SDL_GetBasePath());

    bool is_running = true;
    bool render_debug_info = false;
    #ifdef GOLD_DEBUG_TURBO
        render_debug_info = true;
    #endif
    const uint64_t UPDATE_DURATION = SDL_NS_PER_SECOND / UPDATES_PER_SECOND;
    uint64_t last_time = SDL_GetTicksNS();
    uint64_t last_second = last_time;
    uint64_t update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;

    #ifdef GOLD_DEBUG_TURBO
    uint64_t playback_speed = 1;
    #endif

    game_set_mode((LoadParams) {
        .mode = GAME_MODE_MENU,
        .menu = (LoadMenuParams) {
            .steam_invite_id = steam_invite_id
        }
    });

    while (is_running) {
        // TIMEKEEP
        uint64_t current_time = SDL_GetTicksNS();
        #ifdef GOLD_DEBUG_TURBO
        if (input_is_action_just_pressed(INPUT_ACTION_TURBO)) {
            if (playback_speed == 1) {
                playback_speed = 4;
            } else if (playback_speed == 4) {
                playback_speed = 8;
            } else {
                playback_speed = 1;
            }
        } 

        update_accumulator += (current_time - last_time) * playback_speed;
        #else
        update_accumulator += current_time - last_time;
        #endif
        last_time = current_time;

        if (current_time - last_second >= SDL_NS_PER_SECOND) {
            fps = frames;
            if (fps < 60) {
                log_warn("FPS DIP %u.", fps);
            }
            frames = 0;
            last_second += SDL_NS_PER_SECOND;
        }
        frames++;

        // INPUT
        if (update_accumulator >= UPDATE_DURATION) {
            input_poll_events();
            if (input_user_requests_exit()) {
                if (network_get_status() == NETWORK_STATUS_CONNECTED || network_get_status() == NETWORK_STATUS_HOST) {
                    network_disconnect();
                }
                is_running = false;
                break;
            }

            if (input_is_action_just_pressed(INPUT_ACTION_F3)) {
                render_debug_info = !render_debug_info;
            }

            network_service();
            NetworkEvent event;
            while (state.mode != GAME_MODE_LOADING && network_poll_events(&event)) {
                switch (state.mode) {
                    case GAME_MODE_MENU: {
                        if (event.type == NETWORK_EVENT_MATCH_LOAD) {
                            game_set_mode((LoadParams) {
                                .mode = GAME_MODE_MATCH,
                                .match = (LoadMatchParams) {
                                    .lcg_seed = event.match_load.lcg_seed,
                                    .noise = event.match_load.noise
                                }
                            });
                        } else {
                            menu_handle_network_event(state.menu, event);
                        }
                        break;
                    }
                    case GAME_MODE_MATCH: 
                    case GAME_MODE_REPLAY: {
                        match_ui_handle_network_event(state.match, event);
                        break;
                    }
                    case GAME_MODE_LOADING: {
                        break;
                    }
                }
            }
        }

        // UPDATE
        while (update_accumulator >= UPDATE_DURATION) {
            update_accumulator -= UPDATE_DURATION;

            switch (state.mode) {
                case GAME_MODE_MENU: {
                    menu_update(state.menu);
                    if (state.menu.mode == MENU_MODE_EXIT) {
                        is_running = false;
                        break;
                    } else if (state.menu.mode == MENU_MODE_LOAD_MATCH) {
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

                        game_set_mode((LoadParams) {
                            .mode = GAME_MODE_MATCH,
                            .match = (LoadMatchParams) {
                                .lcg_seed = lcg_seed,
                                .noise = noise
                            }
                        });
                    } else if (state.menu.mode == MENU_MODE_LOAD_REPLAY) {
                        LoadParams params;
                        params.mode = GAME_MODE_REPLAY;
                        strcpy(params.replay.filename, state.menu.replay_filenames[state.menu.lobbylist_item_selected].c_str());
                        game_set_mode(params);
                    }
                    break;
                }
                case GAME_MODE_MATCH:
                case GAME_MODE_REPLAY: {
                    match_ui_update(state.match);
                    if (state.match.mode == MATCH_UI_MODE_LEAVE_MATCH) {
                        sound_end_fire_loop();
                        game_set_mode((LoadParams) {
                            .mode = GAME_MODE_MENU,
                            .menu = (LoadMenuParams) {
                                .steam_invite_id = 0
                            }
                        });
                    } else if (state.match.mode == MATCH_UI_MODE_EXIT_PROGRAM) {
                        sound_end_fire_loop();
                        is_running = false;
                    }
                    break;
                }
                case GAME_MODE_LOADING: {
                    animation_update(state.loading_animation);
                    if (SDL_GetThreadState(state.loading_thread) == SDL_THREAD_COMPLETE) {
                        // clean up the thread
                        SDL_WaitThread(state.loading_thread, NULL);
                        state.mode = state.load_params.mode;
                    }
                    break;
                }
            }
        }

        sound_update();

        // RENDER
        render_prepare_frame();

        switch (state.mode) {
            case GAME_MODE_MENU: {
                menu_render(state.menu);
                break;
            }
            case GAME_MODE_MATCH: 
            case GAME_MODE_REPLAY: {
                match_ui_render(state.match);
                break;
            }
            case GAME_MODE_LOADING: {
                const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_MINER_BUILDING);
                Rect src_rect = (Rect) {
                    .x = state.loading_animation.frame.x * sprite_info.frame_width,
                    .y = sprite_info.frame_height * 2,
                    .w = sprite_info.frame_width,
                    .h = sprite_info.frame_height
                };
                Rect dst_rect = (Rect) {
                    .x = (SCREEN_WIDTH / 2) - sprite_info.frame_width,
                    .y = (SCREEN_HEIGHT / 2) - sprite_info.frame_height - 4,
                    .w = sprite_info.frame_width * 2,
                    .h = sprite_info.frame_height * 2
                };
                render_sprite(SPRITE_MINER_BUILDING, src_rect, dst_rect, 0);
                ivec2 text_size = render_get_text_size(FONT_HACK_WHITE, "Loading...");
                render_text(FONT_HACK_WHITE, "Loading...", ivec2((SCREEN_WIDTH / 2) - (text_size.x / 2), (SCREEN_HEIGHT / 2) + sprite_info.frame_height + 4));
                render_sprite_batch();
                break;
            }
        }

        if (render_debug_info) {
            char fps_text[32];
            sprintf(fps_text, "FPS: %u", fps);
            render_text(FONT_HACK_WHITE, fps_text, ivec2(0, 0));

            #ifdef GOLD_DEBUG_TURBO
                char turbo_text[32];
                sprintf(turbo_text, "Playback Speed x%llu", playback_speed);
                render_text(FONT_HACK_WHITE, turbo_text, ivec2(0, 10));
            #endif

            render_sprite_batch();
        }

        render_present_frame();
    }

    network_quit();
    sound_quit();
    render_quit();

    SDL_DestroyWindow(window);

    SteamAPI_Shutdown();

    TTF_Quit();
    SDL_Quit();

    options_save();

    log_info("Application quit gracefully.");

    return 0;
}
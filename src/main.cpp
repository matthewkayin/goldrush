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
#include "match/lcg.h"
#include "shell/desync.h"
#include "profile/profile.h"
#include "editor/editor.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_ttf.h>
#include <ctime>
#include <string>

#ifdef GOLD_STEAM
#include <steam/steam_api.h>
#endif

static const uint64_t UPDATE_DURATION = SDL_NS_PER_SECOND / UPDATES_PER_SECOND;

enum GameMode {
    GAME_MODE_MENU,
    GAME_MODE_MATCH,
#ifdef GOLD_DEBUG
    GAME_MODE_EDITOR
#endif
};

enum LaunchMode {
    LAUNCH_MODE_GAME,
#ifdef GOLD_DEBUG
    LAUNCH_MODE_TEST_HOST,
    LAUNCH_MODE_TEST_JOIN,
    LAUNCH_MODE_EDITOR
#endif
};

struct GameState {
    LaunchMode launch_mode;
    GameMode mode;
    MenuState* menu_state = nullptr;
    MatchShellState* match_shell_state = nullptr;

#ifdef GOLD_DEBUG
    Bot test_bot;
#endif
};
static GameState state;

bool gold_get_argv(int argc, char** argv, const char* key, const char** result);
bool game_is_running();
void game_set_mode_match(int lcg_seed, Noise* noise);
void game_set_mode_replay(const char* replay_path);

#ifdef GOLD_DEBUG
void game_test_update();
#endif

int gold_main(int argc, char** argv) {
    // Lua doc
#ifdef GOLD_DEBUG
    if (gold_get_argv(argc, argv, "--lua-doc", NULL)) {
        match_shell_script_generate_doc();
        return 0;
    }
#endif

    // Steam restart if necessary
#ifdef GOLD_STEAM
    if (SteamAPI_RestartAppIfNecessary(GOLD_STEAM_APP_ID)) {
        return 1;
    }
#endif

    // Logger
    std::string logfile_path = "latest.log";
#ifdef GOLD_DEBUG
    std::string desync_foldername = "desync";

    logfile_path = filesystem_get_timestamp_str() + ".log";
    const char* logfile_path_value;
    if (gold_get_argv(argc, argv, "--logfile", &logfile_path_value)) {
        logfile_path = logfile_path_value;
        desync_foldername = "desync_" + logfile_path.substr(0, logfile_path.find('.'));

        std::string replay_filename = logfile_path.substr(0, logfile_path.find('.')) + ".rep";
        replay_set_filename(replay_filename.c_str());
    }
#endif
    filesystem_create_required_folders();
    if (!logger_init(logfile_path.c_str())) {
        return false;
    }

    // Log initialization messages
    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    log_info("Detected platform %s.", GOLD_PLATFORM_STR);
    log_info("%s build.", GOLD_BUILD_TYPE_STR);

    // Desync
#ifdef GOLD_DEBUG
    bool desync_debug = gold_get_argv(argc, argv, "--desync", NULL);
    if (desync_debug) {
        if (!desync_init(desync_foldername.c_str())) {
            logger_quit();
            return 1;
        }
    }
#endif

    // Load options
    options_load();

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        log_error("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }
    if (!TTF_Init()) {
        log_error("Failed to initialize SDL_ttf: %s", SDL_GetError());
        return false;
    }

    // Init Steam API
#ifdef GOLD_STEAM
    if (!SteamAPI_Init()) {
        log_error("Error initializing Steam API.");
        return false;
    }
    log_info("Initialized Steam API. App ID %u User logged on? %i", SteamUtils()->GetAppID(), (int)(SteamUser()->BLoggedOn()));
#endif

    // Create window
    SDL_Window* window;
    {
        int window_width = SCREEN_WIDTH;
        int window_height = SCREEN_HEIGHT;
        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
        if (option_get_value(OPTION_DISPLAY) == RENDER_DISPLAY_FULLSCREEN) {
            SDL_DisplayID* display_id = SDL_GetDisplays(NULL);
            const SDL_DisplayMode* display_mode = SDL_GetDesktopDisplayMode(display_id[0]);
            window_width = display_mode->w;
            window_height = display_mode->h;
            flags |= SDL_WINDOW_FULLSCREEN;
        } else {
            window_width = WINDOWED_WIDTH;
            window_height = WINDOWED_HEIGHT;
        }
        window = SDL_CreateWindow(APP_NAME, window_width, window_height, flags);
    }

    // Init subsystems
    if (!render_init(window)) {
        logger_quit();
        return false;
    }
    if (!cursor_init()) {
        logger_quit();
        return false;
    }
    if (!sound_init()) {
        logger_quit();
        return false;
    }
    if (!network_init()) {
        logger_quit();
        return false;
    }

    input_init(window);
    srand((uint32_t)time(0));

    state.match_shell_state = nullptr;
    state.menu_state = nullptr;

    // Launch mode
#ifdef GOLD_DEBUG
    state.launch_mode = LAUNCH_MODE_GAME;
    const char* launch_mode_str;
    if (gold_get_argv(argc, argv, "--launch", &launch_mode_str)) {
        if (strcmp(launch_mode_str, "test-host") == 0) {
            state.launch_mode = LAUNCH_MODE_TEST_HOST;
        } else if (strcmp(launch_mode_str, "test-join") == 0) {
            state.launch_mode = LAUNCH_MODE_TEST_JOIN;
        } else if (strcmp(launch_mode_str, "editor") == 0) {
            state.launch_mode = LAUNCH_MODE_EDITOR;
        } else {
            log_error("Launch mdoe %s not recognized.", launch_mode_str);
            return 1;
        }
    }

    bool should_render_debug_info = false;
    uint64_t debug_playback_speed = 1;
    if ((state.launch_mode == LAUNCH_MODE_TEST_HOST || state.launch_mode == LAUNCH_MODE_TEST_JOIN) && !desync_debug) {
        debug_playback_speed = 4;
    }

    if (state.launch_mode == LAUNCH_MODE_EDITOR) {
        log_info("Launching as scenario editor.");
        editor_init(window);
        state.mode = GAME_MODE_EDITOR;
    }

    if (state.launch_mode == LAUNCH_MODE_TEST_HOST || state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
        state.menu_state = menu_init();
        state.mode = GAME_MODE_MENU;
    }
#endif

    if (state.launch_mode == LAUNCH_MODE_GAME) {
        state.menu_state = menu_init();
        state.mode = GAME_MODE_MENU;

        // Check for Steam lobby invite
    #ifdef GOLD_STEAM
        uint64_t steam_invite_id = 0;
        const char* steam_invite_id_str;
        if (gold_get_argv(argc, argv, "+connect_lobby", &steam_invite_id_str)) {
            steam_invite_id = std::stoull(steam_invite_id_str);
        }
        if (steam_invite_id != 0) {
            network_set_backend(NETWORK_BACKEND_STEAM);
            CSteamID steam_id;
            steam_id.SetFromUint64(steam_invite_id);
            network_steam_accept_invite(steam_id);
            log_info("Launching with steam_invite_id %u", steam_invite_id);
        }
    #endif
    }

    // Timekeeping
    uint64_t last_time = SDL_GetTicksNS();
    uint64_t last_second = last_time;
    uint64_t update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;

    while (game_is_running()) {
        // Timekeep
        uint64_t current_time = SDL_GetTicksNS();
        #ifdef GOLD_DEBUG
            update_accumulator += (current_time - last_time) * debug_playback_speed;
        #else
            update_accumulator += current_time - last_time;
        #endif
        last_time = current_time;

        if (current_time - last_second >= SDL_NS_PER_SECOND) {
            fps = frames;
            frames = 0;
            last_second += SDL_NS_PER_SECOND;
        }
        frames++;

    #ifdef GOLD_STEAM
        SteamAPI_RunCallbacks();
    #endif

        // Update
        while (update_accumulator >= UPDATE_DURATION) {
            update_accumulator -= UPDATE_DURATION;

            // Input
            input_poll_events();
            #ifdef GOLD_DEBUG
                if (input_is_action_just_pressed(INPUT_ACTION_F3)) {
                    should_render_debug_info = !should_render_debug_info;
                }
                if (input_is_action_just_pressed(INPUT_ACTION_TURBO)) {
                    debug_playback_speed = debug_playback_speed == 1 ? 4 : 1;
                }
            #endif

            // Network
            network_service();
            NetworkEvent event;
            while (network_poll_events(&event)) {
                switch (state.mode) {
                    case GAME_MODE_MENU: {
                        if (event.type == NETWORK_EVENT_MATCH_LOAD) {
                            game_set_mode_match(event.match_load.lcg_seed, event.match_load.noise);
                            break;
                        }

                        menu_handle_network_event(state.menu_state, event);
                        break;
                    }
                    case GAME_MODE_MATCH: {
                        match_shell_handle_network_event(state.match_shell_state, event);
                        break;
                    }
                #ifdef GOLD_DEBUG
                    case GAME_MODE_EDITOR:
                        break;
                #endif
                }

                // Needed to free match load noise
                network_cleanup_event(event);
            }

            // Test mode update
        #ifdef GOLD_DEBUG
            if (state.launch_mode == LAUNCH_MODE_TEST_HOST || state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
                game_test_update();
            }
        #endif

            // Update
            switch (state.mode) {
                case GAME_MODE_MENU: {
                    menu_update(state.menu_state);

                    // Host begin load match
                    if (state.menu_state->mode == MENU_MODE_LOAD_MATCH) {
                        // Set LCG seed
                    #ifdef GOLD_RAND_SEED
                        int lcg_seed = GOLD_RAND_SEED;
                    #else
                        int lcg_seed = (int)time(NULL);
                    #endif

                        // Generate noise
                        MapType map_type = (MapType)network_get_match_setting(MATCH_SETTING_MAP_TYPE);
                        MapSize map_size = (MapSize)network_get_match_setting(MATCH_SETTING_MAP_SIZE);
                        int noise_lcg_seed = lcg_seed;
                        uint64_t map_seed = (uint64_t)noise_lcg_seed;
                        uint64_t forest_seed = (uint64_t)lcg_rand(&noise_lcg_seed);
                        NoiseGenParams params = noise_create_noise_gen_params(map_type, map_size, map_seed, forest_seed);
                        Noise* noise = noise_generate(params);

                        network_begin_loading_match(lcg_seed, noise);
                        game_set_mode_match(lcg_seed, noise);

                        noise_free(noise);
                        break;
                    }

                    // Load replay
                    if (state.menu_state->mode == MENU_MODE_LOAD_REPLAY) {
                        game_set_mode_replay(menu_get_selected_replay_filename(state.menu_state));
                        break;
                    }

                    break;
                }
                case GAME_MODE_MATCH: {
                    match_shell_update(state.match_shell_state);

                    if (state.match_shell_state->mode == MATCH_SHELL_MODE_LEAVE_MATCH) {
                    #ifdef GOLD_DEBUG
                        if (state.launch_mode == LAUNCH_MODE_EDITOR) {
                            match_shell_free(state.match_shell_state);
                            editor_end_playtest();
                            state.mode = GAME_MODE_EDITOR;
                            break;
                        }
                    #endif

                        sound_stop_all();
                        state.menu_state = menu_init();
                        match_shell_free(state.match_shell_state);
                        state.mode = GAME_MODE_MENU;
                    }
                    break;
                }
            #ifdef GOLD_DEBUG
                case GAME_MODE_EDITOR: {
                    editor_update();
                    if (editor_requests_playtest()) {
                        network_set_backend(NETWORK_BACKEND_LAN);
                        network_open_lobby("Test Game", NETWORK_LOBBY_PRIVACY_SINGLEPLAYER);
                    #ifndef GOLD_STEAM
                        network_set_username("Player");
                    #endif
                        state.match_shell_state = match_shell_init_from_scenario(editor_get_scenario(), editor_get_scenario_script_path().c_str());
                        if (state.match_shell_state == nullptr) {
                            network_disconnect();
                            editor_end_playtest();
                            break;
                        }
                        if (state.match_shell_state->mode != MATCH_SHELL_MODE_LEAVE_MATCH) {
                            for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
                                if (!state.match_shell_state->match_state.players[player_id].active) {
                                    continue;
                                }
                                network_add_bot();
                            }
                        }
                        state.mode = GAME_MODE_MATCH;
                    }
                    break;
                }
            #endif
            }
        }

        // Render
        render_prepare_frame();

        switch (state.mode) {
            case GAME_MODE_MENU: {
                menu_render(state.menu_state);
                break;
            }
            case GAME_MODE_MATCH: {
                match_shell_render(state.match_shell_state);
                break;
            }
        #ifdef GOLD_DEBUG
            case GAME_MODE_EDITOR: {
                editor_render();
                break;
            }
        #endif
        }

        #ifdef GOLD_DEBUG
            if (should_render_debug_info) {
                int render_y = 0;
                char debug_text[256];
                sprintf(debug_text, "FPS: %u", fps);
                render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                render_y += 10;

                if (state.mode == GAME_MODE_MATCH) {
                    sprintf(debug_text, "Paused ? %i", (int)state.match_shell_state->is_paused);
                    render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                    render_y += 10;
                } 

                if (state.mode == GAME_MODE_MATCH && !match_shell_is_mouse_in_ui()) {
                    ivec2 cell = (input_get_mouse_position() + state.match_shell_state->camera_offset) / TILE_SIZE;
                    Tile tile = map_get_tile(state.match_shell_state->match_state.map, cell);
                    sprintf(debug_text, "Cell <%i, %i> Elevation %u Tile <%u, %u> Region %i Minimap Pixel %u", cell.x, cell.y, tile.elevation, tile.frame_x, tile.frame_y, map_get_region(state.match_shell_state->match_state.map, cell), match_shell_get_minimap_pixel_for_cell(state.match_shell_state, cell));
                    render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                    render_y += 10;

                    render_draw_rect((Rect) {
                        .x = (cell.x * TILE_SIZE) - state.match_shell_state->camera_offset.x,
                        .y = (cell.y * TILE_SIZE) - state.match_shell_state->camera_offset.y,
                        .w = TILE_SIZE,
                        .h = TILE_SIZE
                    }, RENDER_COLOR_WHITE);

                    Cell map_cell = map_get_cell(state.match_shell_state->match_state.map, CELL_LAYER_GROUND, cell);
                    if (map_cell.type == CELL_UNIT || map_cell.type == CELL_BUILDING || map_cell.type == CELL_MINER || map_cell.type == CELL_GOLDMINE) {
                        const Entity& entity = state.match_shell_state->match_state.entities.get_by_id(map_cell.id);
                        ivec2 target_cell = entity_is_target_invalid(state.match_shell_state->match_state, entity) ? ivec2(-1, -1) : entity_get_target_cell(state.match_shell_state->match_state, entity);
                        sprintf(debug_text, "Entity %u %s mode %u target %u cell <%i, %i> is mining %i goldmine id %u pathfind attempts %u", map_cell.id, entity_get_data(entity.type).name, entity.mode, entity.target.type, target_cell.x, target_cell.y, (int)entity_is_mining(state.match_shell_state->match_state, entity), entity.goldmine_id, entity.pathfind_attempts);
                        render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                        render_y += 10;
                    }
                }
            }
        #endif

        #ifdef TRACY_ENABLE
            char fps_text[128];
            sprintf(fps_text, "Tracy Enabled | FPS: %u", fps);
            render_text(FONT_HACK_WHITE, fps_text, ivec2(0, 0));
        #endif

        render_present_frame();

        FrameMark;
    } // End game loop

    // DEINITIALIZE

    // Disconnect the network
    if (network_get_status() == NETWORK_STATUS_CONNECTED || network_get_status() == NETWORK_STATUS_HOST) {
        network_disconnect();
    }

    // Cleanup the game state
#ifdef GOLD_DEBUG
    if (state.launch_mode == LAUNCH_MODE_EDITOR) {
        editor_quit();
    }
#endif
    if (state.mode == GAME_MODE_MENU) {
        delete state.menu_state;
    }
    if (state.mode == GAME_MODE_MATCH) {
        match_shell_free(state.match_shell_state);
    }

    options_save();

    // Quit subsystems
#ifdef GOLD_DEBUG
    desync_quit();
#endif
    network_quit();
    sound_quit();
    cursor_quit();
    render_quit();
    logger_quit();

#ifdef GOLD_STEAM
    SteamAPI_Shutdown();
#endif

    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    log_info("%s quit gracefully.", APP_NAME);

    return 0;
}

bool gold_get_argv(int argc, char** argv, const char* key, const char** result) {
    for (int argn = 1; argn < argc; argn++) {
        if (strcmp(argv[argn], key) == 0) {
            if (result != NULL && argn + 1 == argc) {
                return false;
            }
            if (result != NULL) {
                *result = argv[argn + 1];
            }
            return true;
        }
    }
    
    return false;
}

// GAME

bool game_is_running() {
    if (input_user_requests_exit()) {
        return false;
    }
    if (state.mode == GAME_MODE_MENU && state.menu_state->mode == MENU_MODE_EXIT) {
        return false;
    }
    if (state.mode == GAME_MODE_MATCH && state.match_shell_state->mode == MATCH_SHELL_MODE_EXIT_PROGRAM) {
        return false;
    }

    return true;
}

void game_set_mode_match(int lcg_seed, Noise* noise) {
    state.match_shell_state = match_shell_init(lcg_seed, noise);
    delete state.menu_state;
    state.mode = GAME_MODE_MATCH;

#ifdef GOLD_DEBUG
    if (state.launch_mode == LAUNCH_MODE_TEST_HOST || state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
    #ifdef GOLD_TEST_SEED
        int test_lcg_seed = GOLD_TEST_SEED
    #else
        int test_lcg_seed = rand();
    #endif
        log_debug("TEST seed set to %u", test_lcg_seed);
        BotConfig bot_config = bot_config_init_from_difficulty(DIFFICULTY_HARD);
        bot_config.opener = BOT_OPENER_TECH_FIRST;
        bot_config.preferred_unit_comp = bot_config_roll_preferred_unit_comp(&test_lcg_seed);
        state.test_bot = bot_init(state.match_shell_state->match_state, network_get_player_id(), bot_config);
    }
#endif
}

void game_set_mode_replay(const char* replay_path) {
    state.match_shell_state = replay_shell_init(replay_path);
    if (state.match_shell_state == nullptr) {
        menu_show_status(state.menu_state, "Error opening replay file.");
        return;
    }
    delete state.menu_state;
    state.mode = GAME_MODE_MATCH;
}

#ifdef GOLD_DEBUG

void game_test_update() {
    if (state.mode == GAME_MODE_MENU) {
        switch (state.menu_state->mode) {
        #ifndef GOLD_STEAM
            case MENU_MODE_USERNAME: {
                state.menu_state->username = state.launch_mode == LAUNCH_MODE_TEST_HOST 
                    ? "Burr" 
                    : "Hamilton";
                network_set_username(state.menu_state->username.c_str());
                menu_set_mode(state.menu_state, MENU_MODE_MAIN);
                break;
            }
        #endif
            case MENU_MODE_MAIN: {
            #ifdef GOLD_STEAM
                network_set_backend(NETWORK_BACKEND_STEAM);
                menu_set_mode(state.menu_state, MENU_MODE_LOBBYLIST);
            #else
                menu_set_mode_local_network_lobbylist(state.menu_state);
            #endif
                break;
            }
            case MENU_MODE_LOBBYLIST: {
                if (state.launch_mode == LAUNCH_MODE_TEST_HOST) {
                    menu_set_mode(state.menu_state, MENU_MODE_CREATE_LOBBY);
                } else if (state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
                    if (network_get_lobby_count() == 0) {
                        network_search_lobbies("");
                    } else {
                        network_join_lobby(network_get_lobby(0).connection_info);
                        menu_set_mode(state.menu_state, MENU_MODE_CONNECTING);
                    }
                }
                break;
            }
            case MENU_MODE_CREATE_LOBBY: {
                network_open_lobby(state.menu_state->lobby_name.c_str(), (NetworkLobbyPrivacy)state.menu_state->lobby_privacy);
                menu_set_mode(state.menu_state, MENU_MODE_CONNECTING);
                break;
            }
            case MENU_MODE_LOBBY: {
                if (state.launch_mode == LAUNCH_MODE_TEST_HOST) {
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_MAP_SIZE) != MAP_SIZE_MEDIUM) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_MAP_SIZE, (uint8_t)MAP_SIZE_MEDIUM);
                        break;
                    }
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_TEAMS) != TEAMS_ENABLED) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_TEAMS, (uint8_t)TEAMS_ENABLED);
                        break;
                    }
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_DIFFICULTY) != DIFFICULTY_HARD) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_DIFFICULTY, (uint8_t)DIFFICULTY_HARD);
                        break;
                    }
                    if (network_get_player_count() == 1) {
                        // wait for player 2
                        break;
                    }
                    if (network_get_player_count() < MAX_PLAYERS) {
                        network_add_bot();
                        break;
                    }
                    if (network_get_player(2).team != 0) {
                        network_set_player_team(2, 0);
                        break;
                    }
                    if (network_get_player(3).team != 1) {
                        network_set_player_team(3, 1);
                        break;
                    }
                    if (network_get_player(1).status == NETWORK_PLAYER_STATUS_READY) {
                        menu_set_mode(state.menu_state, MENU_MODE_LOAD_MATCH);
                    }
                } else if (state.launch_mode == LAUNCH_MODE_TEST_JOIN) {
                    if (network_get_player(network_get_player_id()).team == 0 &&
                            network_get_match_setting((uint8_t)MATCH_SETTING_TEAMS) == TEAMS_ENABLED) {
                        network_set_player_team(network_get_player_id(), 1);
                        break;
                    }
                    if (network_get_player(network_get_player_id()).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                        network_set_player_ready(true);
                    }
                }
                break;
            }
            default:
                break;
        }
    } else if (state.mode == GAME_MODE_MATCH) {
        if (state.match_shell_state->mode == MATCH_SHELL_MODE_NOT_STARTED ||
                state.match_shell_state->mode == MATCH_SHELL_MODE_LEAVE_MATCH ||
                state.match_shell_state->mode == MATCH_SHELL_MODE_EXIT_PROGRAM ||
                state.match_shell_state->mode == MATCH_SHELL_MODE_DESYNC) {
            return;
        }

        if (state.match_shell_state->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY || 
                state.match_shell_state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT) {
            match_shell_leave_match(state.match_shell_state, false);
        } else if (state.match_shell_state->match_timer % TURN_DURATION == 0 && 
                state.match_shell_state->match_state.players[network_get_player_id()].active &&
                state.match_shell_state->input_queue.empty()) {
            uint32_t turn_number = state.match_shell_state->match_timer / TURN_DURATION;
            if (turn_number % TURN_OFFSET == 0) {
                MatchInput input;
                input = bot_get_turn_input(state.match_shell_state->match_state, state.test_bot, state.match_shell_state->match_timer);
                state.match_shell_state->input_queue.push_back(input);
            } 

            // Check for surrender
            if (bot_should_surrender(state.match_shell_state->match_state, state.test_bot, state.match_shell_state->match_timer)) {
                network_send_chat("gg");
                match_shell_leave_match(state.match_shell_state, false);
            }
        }
    }
}

#endif

// APP ENTRY

#if defined PLATFORM_WIN32 and not defined GOLD_DEBUG

#include <windows.h>
#define MAX_ARGS 16

int WINAPI WinMain(HINSTANCE /*h_instance*/, HINSTANCE /*h_prev_instance*/, LPSTR /*lp_cmd_line*/, int /*n_cmd_show*/) {
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
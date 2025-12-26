#include "defines.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/input.h"
#include "core/cursor.h"
#include "core/sound.h"
#include "core/options.h"
#include "game/game.h"
#include "network/network.h"
#include "render/render.h"
#include "menu/menu.h"
#include "shell/desync.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <ctime>
#include <string>
#include <tracy/tracy/Tracy.hpp>

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

int gold_main(int argc, char** argv);

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

static const uint64_t UPDATE_DURATION = SDL_NS_PER_SECOND / UPDATES_PER_SECOND;

int gold_main(int argc, char** argv) {
    #ifdef GOLD_STEAM
        if (SteamAPI_RestartAppIfNecessary(GOLD_STEAM_APP_ID)) {
            return 1;
        }

        uint64_t steam_invite_id = 0;
    #endif

    std::string logfile_path = filesystem_get_timestamp_str() + ".log";
    #ifdef GOLD_DEBUG_DESYNC
        std::string desync_foldername = "desync";
    #endif
    #ifdef GOLD_DEBUG
        TestMode test_mode = TEST_MODE_NONE;
    #endif

    // Parse system arguments
    for (int argn = 1; argn < argc; argn++) {
        #ifdef GOLD_DEBUG
            if (strcmp(argv[argn], "--logfile") == 0 && argn + 1 < argc) {
                argn++;
                logfile_path = argv[argn];
                #ifdef GOLD_DEBUG_DESYNC
                    desync_foldername = "desync_" + logfile_path.substr(0, logfile_path.find('.'));
                #endif
            }
            if (strcmp(argv[argn], "--test") == 0 && argn + 1 < argc) {
                argn++;
                if (strcmp(argv[argn], "host") == 0) {
                    test_mode = TEST_MODE_HOST;
                } else if (strcmp(argv[argn], "join") == 0) {
                    test_mode = TEST_MODE_JOIN;
                }
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
    #elif defined(PLATFORM_MACOS)
        log_info("Detected platform MACOS.");
    #elif defined(PLATFORM_LINUX)
        log_info("Detected platform LINUX.");
    #endif
    #ifdef GOLD_DEBUG
        log_info("Debug build.");
    #else
        log_info("Release build.");
    #endif

    #ifdef GOLD_STEAM
        if (!SteamAPI_Init()) {
            log_error("Error initializing Steam API.");
            return 1;
        }
        log_info("Initialized Steam API. App ID %u User logged on? %i", SteamUtils()->GetAppID(), (int)(SteamUser()->BLoggedOn()));
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
    #ifdef GOLD_DEBUG_DESYNC
        if (!desync_init(desync_foldername.c_str())) {
            logger_quit();
            return 1;
        }
    #endif
    input_init(window);
    options_load();
    srand((uint32_t)time(0));

    uint64_t last_time = SDL_GetTicksNS();
    uint64_t last_second = last_time;
    uint64_t update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    #ifdef GOLD_DEBUG
        bool should_render_debug_info = false;
        uint64_t debug_playback_speed = 1;
        if (test_mode != TEST_MODE_NONE) {
            // debug_playback_speed = 4;
        }
    #endif

    #ifdef GOLD_DEBUG
        GameState state = game_test_init(test_mode);
    #else
        GameState state = game_init();
    #endif

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
                game_handle_network_event(state, event);
            }

            // Update
            game_update(state);
        }

        sound_update();

        // Render
        render_prepare_frame();

        game_render(state);

        #ifdef GOLD_DEBUG
            if (should_render_debug_info) {
                int render_y = 0;
                char debug_text[256];
                sprintf(debug_text, "FPS: %u", fps);
                render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                render_y += 20;

                if (state.mode == GAME_MODE_MATCH) {
                    sprintf(debug_text, "Paused ? %i", (int)state.match_shell_state->is_paused);
                    render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                    render_y += 20;
                } 

                if ((state.mode == GAME_MODE_MATCH || state.mode == GAME_MODE_REPLAY) && !match_shell_is_mouse_in_ui()) {
                    ivec2 cell = (input_get_mouse_position() + state.match_shell_state->camera_offset) / TILE_SIZE;
                    Tile tile = map_get_tile(state.match_shell_state->match_state.map, cell);
                    sprintf(debug_text, "Cell <%i, %i> Elevation %u Tile <%i, %i> Region %i Minimap Pixel %u", cell.x, cell.y, tile.elevation, tile.frame.x, tile.frame.y, map_get_region(state.match_shell_state->match_state.map, cell), match_shell_get_minimap_pixel_for_cell(state.match_shell_state, cell));
                    render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                    render_y += 20;

                    render_draw_rect((Rect) {
                        .x = (cell.x * TILE_SIZE) - state.match_shell_state->camera_offset.x,
                        .y = (cell.y * TILE_SIZE) - state.match_shell_state->camera_offset.y,
                        .w = TILE_SIZE,
                        .h = TILE_SIZE
                    }, RENDER_COLOR_WHITE);

                    Cell map_cell = map_get_cell(state.match_shell_state->match_state.map, CELL_LAYER_GROUND, cell);
                    if (map_cell.type == CELL_UNIT || map_cell.type == CELL_BUILDING || map_cell.type == CELL_MINER || map_cell.type == CELL_GOLDMINE) {
                        const Entity& entity = state.match_shell_state->match_state.entities.get_by_id(map_cell.id);
                        sprintf(debug_text, "Entity %u %s mode %u target %u is mining %i goldmine id %u pathfind attempts %u", map_cell.id, entity_get_data(entity.type).name, entity.mode, entity.target.type, (int)entity_is_mining(state.match_shell_state->match_state, entity), entity.goldmine_id, entity.pathfind_attempts);
                        render_text(FONT_HACK_WHITE, debug_text, ivec2(0, render_y));
                        render_y += 20;
                    }
                }
            }
        #endif

        render_present_frame();

        FrameMark;
    }

    // Disconnect the network
    if (network_get_status() == NETWORK_STATUS_CONNECTED || network_get_status() == NETWORK_STATUS_HOST) {
        network_disconnect();
    }

    // Delete the current game sub-state
    game_quit(state);

    options_save();

    // Quit subsystems
    #ifdef GOLD_DEBUG_DESYNC
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

    TTF_Quit();
    SDL_Quit();

    log_info("%s quit gracefully.", APP_NAME);

    return 0;
}
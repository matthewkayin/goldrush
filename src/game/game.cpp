#include "game.h"

#include "core/input.h"
#include "core/logger.h"
#include <ctime>
#include <tracy/tracy/Tracy.hpp>

GameState game_init() {
    GameState state;
    state.match_shell_state = nullptr;
    state.menu_state = nullptr;
    state.mode = GAME_MODE_NONE;

    return state;
}

void game_quit(GameState& state) {
    game_set_mode(state, (GameSetModeParams) {
        .mode = GAME_MODE_NONE
    });
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

    #ifdef GOLD_DEBUG
        game_test_set_mode(state, state.mode);
    #endif
}

void game_handle_network_event(GameState& state, const NetworkEvent& event) {
    switch (state.mode) {
        case GAME_MODE_NONE: 
            break;
        case GAME_MODE_MENU: {
            if (event.type == NETWORK_EVENT_MATCH_LOAD) {
                game_set_mode(state, (GameSetModeParams) {
                    .mode = GAME_MODE_MATCH,
                    .match = (GameSetModeMatchParams) {
                        .lcg_seed = event.match_load.lcg_seed,
                        .noise = event.match_load.noise
                    }
                });
                break;
            }

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

void game_update(GameState& state) {
    #ifdef GOLD_DEBUG
        game_test_update(state);
    #endif

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

void game_render(const GameState& state) {
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
}

#ifdef GOLD_DEBUG

GameState game_test_init(TestMode test_mode) {
    GameState state = game_init();
    state.test_mode = test_mode;

    return state;
}

void game_test_set_mode(GameState& state, GameMode mode) {
    if (state.test_mode == TEST_MODE_NONE) {
        return;
    }

    if (mode == GAME_MODE_MATCH) {
        int lcg_seed = rand();
        MatchSettingDifficultyValue difficulty = MATCH_SETTING_DIFFICULTY_HARD;
        BotOpener opener = BOT_OPENER_TECH_FIRST;
        BotUnitComp unit_comp = bot_roll_preferred_unit_comp(&lcg_seed);
        state.test_bot = bot_init(network_get_player_id(), difficulty, opener, unit_comp);
    }
}

void game_test_update(GameState& state) {
    ZoneScoped;

    if (state.test_mode == TEST_MODE_NONE) {
        return;
    }

    if (state.mode == GAME_MODE_MENU) {
        switch (state.menu_state->mode) {
            case MENU_MODE_USERNAME: {
                state.menu_state->username = state.test_mode == TEST_MODE_HOST 
                    ? "Burr" 
                    : "Hamilton";
                network_set_username(state.menu_state->username.c_str());
                menu_set_mode(state.menu_state, MENU_MODE_MAIN);
                break;
            }
            case MENU_MODE_MAIN: {
                menu_set_mode_local_network_lobbylist(state.menu_state);
                break;
            }
            case MENU_MODE_LOBBYLIST: {
                if (state.test_mode == TEST_MODE_HOST) {
                    menu_set_mode(state.menu_state, MENU_MODE_CREATE_LOBBY);
                } else if (state.test_mode == TEST_MODE_JOIN) {
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
                if (state.test_mode == TEST_MODE_HOST) {
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_MAP_SIZE) != MATCH_SETTING_MAP_SIZE_MEDIUM) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_MAP_SIZE, (uint8_t)MATCH_SETTING_MAP_SIZE_MEDIUM);
                        break;
                    }
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_TEAMS) != MATCH_SETTING_TEAMS_ENABLED) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_TEAMS, (uint8_t)MATCH_SETTING_TEAMS_ENABLED);
                        break;
                    }
                    if (network_get_match_setting((uint8_t)MATCH_SETTING_DIFFICULTY) != MATCH_SETTING_DIFFICULTY_HARD) {
                        network_set_match_setting((uint8_t)MATCH_SETTING_DIFFICULTY_HARD, (uint8_t)MATCH_SETTING_DIFFICULTY_HARD);
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
                } else if (state.test_mode == TEST_MODE_JOIN) {
                    if (network_get_player(network_get_player_id()).team == 0 &&
                            network_get_match_setting((uint8_t)MATCH_SETTING_TEAMS) == MATCH_SETTING_TEAMS_ENABLED) {
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
            MatchInput input = turn_number % TURN_OFFSET == 0
                    ? bot_get_turn_input(state.match_shell_state->match_state, state.test_bot, state.match_shell_state->match_timer)
                    : (MatchInput) { .type = MATCH_INPUT_NONE };
            state.match_shell_state->input_queue.push_back(input);

            // Check for surrender
            if (bot_should_surrender(state.match_shell_state->match_state, state.test_bot, state.match_shell_state->match_timer)) {
                network_send_chat("gg");
                match_shell_leave_match(state.match_shell_state, false);
            }
        }
    }
}

#endif
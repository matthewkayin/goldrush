#include "menu.h"

#include "logger.h"
#include "asserts.h"
#include <cstring>
#include <unordered_map>

static const std::unordered_map<MenuMode, std::vector<MenuButton>> MODE_BUTTONS = {
    { MENU_MODE_MAIN, { MENU_BUTTON_PLAY, MENU_BUTTON_REPLAYS, MENU_BUTTON_OPTIONS, MENU_BUTTON_EXIT } },
    { MENU_MODE_USERNAME, { MENU_BUTTON_USERNAME_BACK, MENU_BUTTON_USERNAME_OK } },
    { MENU_MODE_MATCHLIST, { MENU_BUTTON_HOST, MENU_BUTTON_JOIN, MENU_BUTTON_MATCHLIST_BACK } },
    { MENU_MODE_LOBBY, { MENU_BUTTON_LOBBY_BACK, MENU_BUTTON_LOBBY_READY, MENU_BUTTON_LOBBY_START } },
    { MENU_MODE_CONNECTING, { MENU_BUTTON_CONNECTING_BACK } }
};

struct menu_button_t {
    const char* text;
    int position_x;
    int position_y;
};

static const SDL_Rect MATCHLIST_RECT = (SDL_Rect) {
    .x = 36, .y = 32, .w = 432, .h = 200
};
static const SDL_Rect PLAYERLIST_RECT = (SDL_Rect) {
    .x = 16, .y = 4, .w = 432, .h = 128
};
static const SDL_Rect LOBBY_CHAT_RECT = (SDL_Rect) {
    .x = 16, .y = 4 + 128 + 4, .w = 432, .h = 158
};
static const SDL_Rect LOBBY_CHAT_TEXTBOX_RECT = (SDL_Rect) {
    .x = 16 + 8, .y = 8 + 128 + 158 + 4, .w = 432 - 16, .h = 24
};
static const SDL_Rect LOBBY_CHAT_TEXTBOX_INPUT_RECT = (SDL_Rect) {
    .x = 16 + 44, .y = 8 + 128 + 158 + 8, .w = 432 - 16 - 38, .h = 16
};
static const SDL_Rect MATCH_SETTINGS_RECT = (SDL_Rect) {
    .x = PLAYERLIST_RECT.x + PLAYERLIST_RECT.w + 4,
    .y = PLAYERLIST_RECT.y,
    .w = 172,
    .h = PLAYERLIST_RECT.h
};
static SDL_Rect TEXT_INPUT_RECT = (SDL_Rect) {
    .x = MATCHLIST_RECT.x + 8 + 9, .y = 136, .w = 150, .h = 18 
};
static SDL_Rect MATCHLIST_SEARCH_RECT = (SDL_Rect) {
    .x = 36 + 8, .y = 4, .w = 300, .h = 24
};
static SDL_Rect MATCHLIST_SEARCH_TEXTINPUT_RECT = (SDL_Rect) {
    .x = 96, .y = 4 + 4, .w = 300 - 96, .h = 16
};
static const xy REFRESH_BUTTON_POSITION = xy(MATCHLIST_SEARCH_RECT.x + MATCHLIST_SEARCH_RECT.w + 4, 4);
static const xy PREV_BUTTON_POSITION = xy(MATCHLIST_SEARCH_RECT.x + MATCHLIST_SEARCH_RECT.w + 32 + 28, 4);
static const xy NEXT_BUTTON_POSITION = xy(MATCHLIST_SEARCH_RECT.x + MATCHLIST_SEARCH_RECT.w + 32 + 28 + 28, 4);

static const int BUTTON_X = MATCHLIST_RECT.x + 8;
static const int BUTTON_Y = 128;
static const int BUTTON_Y_DIST = 21 + 4;
static const std::unordered_map<MenuButton, menu_button_t> MENU_BUTTON_DATA = {
    { MENU_BUTTON_PLAY, (menu_button_t) {
        .text = "PLAY",
        .position_x = BUTTON_X, 
        .position_y = BUTTON_Y
    }},
    { MENU_BUTTON_REPLAYS, (menu_button_t) {
        .text = "REPLAYS",
        .position_x = BUTTON_X, 
        .position_y = BUTTON_Y + BUTTON_Y_DIST
    }},
    { MENU_BUTTON_OPTIONS, (menu_button_t) {
        .text = "OPTIONS",
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + (BUTTON_Y_DIST * 2)
    }},
    { MENU_BUTTON_EXIT, (menu_button_t) {
        .text = "EXIT",
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + (BUTTON_Y_DIST * 3)
    }},
    { MENU_BUTTON_HOST, (menu_button_t) {
        .text = "HOST",
        .position_x = BUTTON_X + 56 + 4,
        .position_y = MATCHLIST_RECT.y + MATCHLIST_RECT.h + 4
    }},
    { MENU_BUTTON_JOIN, (menu_button_t) {
        .text = "JOIN",
        .position_x = BUTTON_X + ((56 + 4) * 2),
        .position_y = MATCHLIST_RECT.y + MATCHLIST_RECT.h + 4
    }},
    { MENU_BUTTON_MATCHLIST_BACK, (menu_button_t) {
        .text = "BACK",
        .position_x = BUTTON_X,
        .position_y = MATCHLIST_RECT.y + MATCHLIST_RECT.h + 4
    }},
    { MENU_BUTTON_USERNAME_BACK, (menu_button_t) {
        .text = "BACK",
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_USERNAME_OK, (menu_button_t) {
        .text = "OK",
        .position_x = BUTTON_X + 56 + 4,
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_LOBBY_BACK, (menu_button_t) {
        .text = "BACK",
        .position_x = LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12,
        .position_y = PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 4
    }},
    { MENU_BUTTON_LOBBY_READY, (menu_button_t) {
        .text = "READY",
        .position_x = LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12 + 56 + 4,
        .position_y = PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 4
    }},
    { MENU_BUTTON_LOBBY_START, (menu_button_t) {
        .text = "START",
        .position_x = LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12 + 56 + 4,
        .position_y = PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 4
    }},
    { MENU_BUTTON_CONNECTING_BACK, (menu_button_t) {
        .text = "BACK",
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + 42
    }},
};

static const uint32_t STATUS_TIMER_DURATION = 60;
static const int PARALLAX_TIMER_DURATION = 2;

static const int COL_HEADER_Y = PLAYERLIST_RECT.y + 10;
static const int COL_NAME_X = PLAYERLIST_RECT.x + 16;
static const int COL_STATUS_X = PLAYERLIST_RECT.x + 216;
static const int COL_TEAM_X = PLAYERLIST_RECT.x + 280;
static const int COL_COLOR_X = PLAYERLIST_RECT.x + 336;
static const int MINI_DROPDOWN_TEXT_Y_OFFSET = -10;

static const int TEXT_INPUT_BLINK_DURATION = 30;
static const size_t MATCHLIST_PAGE_SIZE = 9;
static const int WAGON_X_DEFAULT = 380;
static const int WAGON_X_LOBBY = 480;

static const int CHAT_MAX_MESSAGES = 12;
static const int CHAT_LINE_MAX = 64;
static const int CHAT_MESSAGE_MAX_LENGTH = 60;

struct match_setting_t {
    const char* name;
    uint32_t default_value;
    uint32_t value_count;
};
static const std::unordered_map<MatchSetting, match_setting_t> MATCH_SETTING_DATA = {
    { MATCH_SETTING_MAP_SIZE, (match_setting_t) {
        .name = "Map Size:",
        .default_value = MAP_SIZE_SMALL,
        .value_count = MAP_SIZE_COUNT
    }},
    { MATCH_SETTING_TEAMS, (match_setting_t) {
        .name = "Team Game:",
        .default_value = TEAMS_DISABLED,
        .value_count = TEAMS_COUNT
    }}
};

menu_state_t menu_init() {
    menu_state_t state;

    state.status_text = "";
    state.username = "";
    state.text_input = "";
    state.status_timer = 0;
    state.hover = (menu_hover_t) {
        .type = MENU_HOVER_NONE
    };
    menu_set_mode(state, MENU_MODE_MAIN);

    state.text_input_blink_timer = 0;
    state.dropdown_open = -1;
    state.matchlist_page = 0;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;

    state.options_state.mode = OPTION_MENU_CLOSED;

    return state;
}

void menu_handle_input(menu_state_t& state, SDL_Event event) {
    if (state.options_state.mode != OPTION_MENU_CLOSED) {
        options_menu_handle_input(state.options_state, event);
        return;
    }

    // Mouse pressed
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (state.mode == MENU_MODE_LOBBY && state.dropdown_open != -1) {
            if (state.hover.type == MENU_HOVER_DROPDOWN_ITEM) {
                sound_play(SOUND_UI_SELECT);
                if (state.dropdown_open == MENU_DROPDOWN_COLOR) {
                    network_set_player_color((uint8_t)state.hover.item);
                } else {
                    network_set_match_setting((MatchSetting)state.dropdown_open, state.hover.item);
                }
            }
            state.dropdown_open = -1;
            return;
        }

        // Button pressed
        if (state.hover.type == MENU_HOVER_BUTTON) {
            switch (state.hover.button) {
                case MENU_BUTTON_PLAY: {
                    menu_set_mode(state, MENU_MODE_USERNAME);
                    break;
                }
                case MENU_BUTTON_OPTIONS: {
                    state.options_state = options_menu_open();
                    break;
                }
                case MENU_BUTTON_EXIT: {
                    menu_set_mode(state, MENU_MODE_EXIT);
                    break;
                }
                case MENU_BUTTON_USERNAME_BACK: {
                    menu_set_mode(state, MENU_MODE_MAIN);
                    break;
                }
                case MENU_BUTTON_USERNAME_OK: {
                    if (state.text_input.empty()) {
                        menu_show_status(state, "Invalid username.");
                        break;
                    }

                    SDL_StopTextInput();
                    state.username = state.text_input;
                    menu_set_mode(state, MENU_MODE_MATCHLIST);
                    break;
                }
                case MENU_BUTTON_MATCHLIST_BACK: {
                    menu_set_mode(state, MENU_MODE_MAIN);
                    break;
                }
                case MENU_BUTTON_HOST: {
                    if(!network_server_create(state.username.c_str())) {
                        menu_show_status(state, "Could not create server.");
                        break;
                    }

                    for (int setting = 0; setting < MATCH_SETTING_COUNT; setting++) {
                        network_set_match_setting((MatchSetting)setting, MATCH_SETTING_DATA.at((MatchSetting)setting).default_value);
                    }
                    menu_set_mode(state, MENU_MODE_LOBBY);
                    break;
                }
                case MENU_BUTTON_JOIN: {
                    GOLD_ASSERT(state.item_selected != -1 && state.item_selected < network_get_lobby_count());
                    const lobby_t& lobby = network_get_lobby((state.matchlist_page * MATCHLIST_PAGE_SIZE) + state.item_selected);
                    network_client_create(state.username.c_str(), lobby.ip, lobby.port);
                    menu_set_mode(state, MENU_MODE_CONNECTING);
                    break;
                }
                case MENU_BUTTON_LOBBY_BACK: {
                    network_disconnect();
                    menu_set_mode(state, MENU_MODE_MATCHLIST);
                    break;
                }
                case MENU_BUTTON_CONNECTING_BACK: {
                    network_disconnect();
                    menu_set_mode(state, MENU_MODE_MATCHLIST);
                    break;
                }
                case MENU_BUTTON_LOBBY_READY: {
                    network_toggle_ready();
                    break;
                }
                case MENU_BUTTON_LOBBY_START: {
                    NetworkError error = network_get_error();
                    switch (error) {
                        case NETWORK_ERROR_NOT_READY: {
                            menu_add_chat_message(state, "Cannot start match: Some players are not ready!");
                            break;
                        }
                        case NETWORK_ERROR_SAME_COLOR: {
                            menu_add_chat_message(state, "Cannot start match: Some players have selected the same color!");
                            break;
                        }
                        case NETWORK_ERROR_NONE: {
                            menu_set_mode(state, MENU_MODE_LOAD_MATCH);
                            break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            sound_play(SOUND_UI_SELECT);
            return;
        }

        state.item_selected = state.mode == MENU_MODE_MATCHLIST && state.hover.type == MENU_HOVER_ITEM
                                ?  state.item_selected = state.hover.item
                                : -1;
        state.dropdown_open = state.mode == MENU_MODE_LOBBY && state.hover.type == MENU_HOVER_DROPDOWN ? state.hover.item : -1;
        if ((state.mode == MENU_MODE_LOBBY && state.hover.type == MENU_HOVER_DROPDOWN) || (state.mode == MENU_MODE_MATCHLIST && state.hover.type == MENU_HOVER_ITEM)) {
            sound_play(SOUND_UI_SELECT);
        }

        // Text input pressed
        if (state.hover.type == MENU_HOVER_NONE && state.mode == MENU_MODE_USERNAME &&
                sdl_rect_has_point(TEXT_INPUT_RECT, engine.mouse_position)) {
            SDL_SetTextInputRect(&TEXT_INPUT_RECT);
            SDL_StartTextInput();
            sound_play(SOUND_UI_SELECT);
            return;
        }
        if (state.hover.type == MENU_HOVER_NONE && state.mode == MENU_MODE_LOBBY && sdl_rect_has_point(LOBBY_CHAT_TEXTBOX_INPUT_RECT, engine.mouse_position)) {
            SDL_SetTextInputRect(&LOBBY_CHAT_TEXTBOX_INPUT_RECT);
            SDL_StartTextInput();
            sound_play(SOUND_UI_SELECT);
            return;
        }

        if (state.hover.type == MENU_HOVER_NONE && state.mode == MENU_MODE_MATCHLIST && sdl_rect_has_point(MATCHLIST_SEARCH_TEXTINPUT_RECT, engine.mouse_position)) {
            SDL_SetTextInputRect(&MATCHLIST_SEARCH_TEXTINPUT_RECT);
            SDL_StartTextInput();
            sound_play(SOUND_UI_SELECT);
            return;
        }

        if (state.hover.type == MENU_HOVER_REFRESH) {
            state.matchlist_page = 0;
            network_scanner_search(state.text_input.c_str());
            sound_play(SOUND_UI_SELECT);
            return;
        }

        if (state.hover.type == MENU_HOVER_PREVIOUS) {
            state.matchlist_page = std::max(0, state.matchlist_page - 1);
            sound_play(SOUND_UI_SELECT);
            return;
        }

        if (state.hover.type == MENU_HOVER_NEXT) {
            state.matchlist_page = std::min((int)menu_get_matchlist_page_count() - 1, state.matchlist_page + 1);
            sound_play(SOUND_UI_SELECT);
            return;
        }

        if (state.hover.type == MENU_HOVER_TEAM_PICKER) {
            network_set_team(network_get_player(network_get_player_id()).team == 1 ? 2 : 1);
            sound_play(SOUND_UI_SELECT);
        }

        if (state.hover.type == MENU_HOVER_NONE && SDL_IsTextInputActive()) {
            SDL_StopTextInput();
        }
    } else if (event.type == SDL_TEXTINPUT) {
        state.text_input += std::string(event.text.text);
        if (state.text_input.length() > menu_get_text_input_max(state)) {
            state.text_input = state.text_input.substr(0, menu_get_text_input_max(state));
        }
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
        if (state.text_input.length() > 0) {
            state.text_input.pop_back();
        } 
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        SDL_StopTextInput();
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) {
        if (state.mode == MENU_MODE_MATCHLIST) {
            network_scanner_search(state.text_input.c_str());
            state.matchlist_page = 0;
        } else if (state.mode == MENU_MODE_LOBBY) {
            std::string message = std::string(state.username) + ": " + state.text_input;
            menu_add_chat_message(state, message);
            network_send_lobby_chat_message(message.c_str());
            state.text_input = "";
        }
    }
}

void menu_handle_network_event(menu_state_t& state, const network_event_t& network_event) {
    switch (network_event.type) {
        case NETWORK_EVENT_CONNECTION_FAILED: {
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            state.item_selected = -1;
            menu_show_status(state, "Failed to connect to server.");
            break;
        }
        case NETWORK_EVENT_LOBBY_FULL: {
            network_disconnect();
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "The lobby is full.");
            break;
        }
        case NETWORK_EVENT_INVALID_VERSION: {
            network_disconnect();
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "Client version does not match.");
            break;
        }
        case NETWORK_EVENT_GAME_ALREADY_STARTED: {
            network_disconnect();
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "That game has already started.");
            break;
        }
        case NETWORK_EVENT_JOINED_LOBBY: {
            network_send_lobby_chat_message((state.username + " joined the game.").c_str());
            menu_set_mode(state, MENU_MODE_LOBBY);
            break;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            if (network_event.player_disconnected.player_id == 0) {
                network_disconnect();
                menu_set_mode(state, MENU_MODE_MATCHLIST);
                state.item_selected = -1;
            } else {
                menu_add_chat_message(state, std::string(network_get_player(network_event.player_disconnected.player_id).name) + " disconnected.");
            }
            break;
        }
        case NETWORK_EVENT_LOBBY_CHAT: {
            menu_add_chat_message(state, std::string(network_event.lobby_chat.message));
            break;
        }
        default:
            break;
    }
}

void menu_update(menu_state_t& state) {
    if (state.status_timer > 0) {
        state.status_timer--;
    }

    engine_set_cursor(CURSOR_DEFAULT);

    animation_update(state.wagon_animation);
    state.parallax_timer--;
    state.parallax_x = (state.parallax_x + 1) % (SCREEN_WIDTH * 2);
    if (state.parallax_timer == 0) {
        state.parallax_cloud_x = (state.parallax_cloud_x + 1) % (SCREEN_WIDTH * 2);
        if (state.parallax_x == 0) {
            state.parallax_cactus_offset = (state.parallax_cactus_offset + 1) % 5;
        }
        state.parallax_timer = PARALLAX_TIMER_DURATION;
    }
    int expected_wagon_x = state.mode == MENU_MODE_LOBBY ? WAGON_X_LOBBY : WAGON_X_DEFAULT;
    if (state.wagon_x < expected_wagon_x) {
        state.wagon_x++;
    } else if (state.wagon_x > expected_wagon_x) {
        state.wagon_x--;
    }

    if (SDL_IsTextInputActive()) {
        if (state.text_input_blink_timer != 0) {
            state.text_input_blink_timer--;
        }
        if (state.text_input_blink_timer == 0) {
            state.text_input_blink_timer = TEXT_INPUT_BLINK_DURATION;
            state.text_input_blink = !state.text_input_blink;
        }
    } else {
        state.text_input_blink_timer = 0;
        state.text_input_blink = false;
    }

    state.hover = (menu_hover_t) {
        .type = MENU_HOVER_NONE
    };
    if (state.options_state.mode != OPTION_MENU_CLOSED) {
        options_menu_update(state.options_state);
        return;
    }

    if (SDL_GetWindowMouseGrab(engine.window) == SDL_TRUE && state.dropdown_open == -1) {
        auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
        if (mode_buttons_it != MODE_BUTTONS.end()) {
            for (MenuButton button : mode_buttons_it->second) {
                if (menu_is_button_disabled(state, button)) {
                    continue;
                }

                const menu_button_t& button_data = MENU_BUTTON_DATA.at(button);
                SDL_Rect button_rect = render_get_button_rect(button_data.text, xy(button_data.position_x, button_data.position_y));
                if (sdl_rect_has_point(button_rect, engine.mouse_position)) {
                    state.hover = (menu_hover_t) {
                        .type = MENU_HOVER_BUTTON,
                        .button = button
                    };
                    break;
                }
            }
        }
    }
    if (state.mode == MENU_MODE_MATCHLIST && state.hover.type == MENU_HOVER_NONE && SDL_GetWindowMouseGrab(engine.window) == SDL_TRUE) {
        for (int match_index = 0; match_index < network_get_lobby_count(); match_index++) {
            SDL_Rect item_rect = menu_get_lobby_text_frame_rect(match_index);
            if (sdl_rect_has_point(item_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_ITEM,
                    .item = match_index
                };
                break;
            }
        }

        if (state.hover.type == MENU_HOVER_NONE) {
            SDL_Rect button_rect = (SDL_Rect) { 
                .x = REFRESH_BUTTON_POSITION.x, 
                .y = REFRESH_BUTTON_POSITION.y, 
                .w = engine.sprites[SPRITE_MENU_REFRESH].frame_size.x,
                .h = engine.sprites[SPRITE_MENU_REFRESH].frame_size.y
            };
            if (sdl_rect_has_point(button_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_REFRESH
                };
            }

            button_rect.x = PREV_BUTTON_POSITION.x;
            button_rect.y = PREV_BUTTON_POSITION.y;
            if (state.matchlist_page != 0 && sdl_rect_has_point(button_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_PREVIOUS
                };
            }

            button_rect.x = NEXT_BUTTON_POSITION.x;
            button_rect.y = NEXT_BUTTON_POSITION.y;
            if (state.matchlist_page != menu_get_matchlist_page_count() - 1 && sdl_rect_has_point(button_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_NEXT
                };
            }
        }
    }

    if (state.mode == MENU_MODE_LOBBY && state.hover.type == MENU_HOVER_NONE && SDL_GetWindowMouseGrab(engine.window) == SDL_TRUE) {
        // Get dropdown rect
        int player_index = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (player_id == network_get_player_id()) {
                break;
            }
            if (network_get_player(player_id).status == PLAYER_STATUS_NONE) {
                continue;
            }
            player_index++;
        }
        SDL_Rect color_dropdown_rect = menu_get_dropdown_rect(player_index);
        SDL_Rect team_dropdown_rect = (SDL_Rect) { .x = COL_TEAM_X, .y = color_dropdown_rect.y, .w = engine.sprites[SPRITE_UI_TEAM_PICKER].frame_size.x, .h = engine.sprites[SPRITE_UI_TEAM_PICKER].frame_size.y };

        // Check for dropdown hover
        if (state.dropdown_open == -1) {
            if (sdl_rect_has_point(color_dropdown_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_DROPDOWN,
                    .item = MENU_DROPDOWN_COLOR
                };
            } else if (network_get_match_setting(MATCH_SETTING_TEAMS) == TEAMS_ENABLED && sdl_rect_has_point(team_dropdown_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) { .type = MENU_HOVER_TEAM_PICKER };
            } else if (network_is_server()) {
                for (int setting = 0; setting < MATCH_SETTING_COUNT; setting++) {
                    SDL_Rect setting_dropdown_rect = menu_get_match_setting_rect(setting);
                    if (sdl_rect_has_point(setting_dropdown_rect, engine.mouse_position)) {
                        state.hover = (menu_hover_t) {
                            .type = MENU_HOVER_DROPDOWN,
                            .item = setting
                        };
                        break;
                    }
                }
            }
        // Check for dropdown item hover
        } else {
            SDL_Rect dropdown_rect = state.dropdown_open == MENU_DROPDOWN_COLOR ? color_dropdown_rect : menu_get_match_setting_rect(state.dropdown_open);
            int dropdown_value_count = state.dropdown_open == MENU_DROPDOWN_COLOR ? MAX_PLAYERS : MATCH_SETTING_DATA.at((MatchSetting)state.dropdown_open).value_count;
            for (int index = 0; index < dropdown_value_count; index++) {
                dropdown_rect.y += dropdown_rect.h;
                if (sdl_rect_has_point(dropdown_rect, engine.mouse_position)) {
                    state.hover = (menu_hover_t) {
                        .type = MENU_HOVER_DROPDOWN_ITEM,
                        .item = index
                    };
                    break;
                }
            }
        }
    }
}

void menu_show_status(menu_state_t& state, const char* text) {
    state.status_text = text;
    state.status_timer = STATUS_TIMER_DURATION;
}

void menu_set_mode(menu_state_t& state, MenuMode mode) {
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }

    state.mode = mode;

    if (state.mode == MENU_MODE_LOBBY) {
        state.chat.clear();
    }

    state.dropdown_open = -1;
    if (state.mode == MENU_MODE_MATCHLIST) {
        state.item_selected = -1;
        state.matchlist_page = 0;
        state.text_input = "";

        if (!network_scanner_create()) {
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "Error occurred while scanning for LAN games.");
            return;
        }
        network_scanner_search(state.text_input.c_str());
    }

    if (state.mode == MENU_MODE_USERNAME) {
        state.text_input = state.username;
        SDL_SetTextInputRect(&TEXT_INPUT_RECT);
        SDL_StartTextInput();
    }
}

size_t menu_get_text_input_max(const menu_state_t& state) {
    if (state.mode == MENU_MODE_USERNAME) {
        return MAX_USERNAME_LENGTH;
    } else if (state.mode == MENU_MODE_MATCHLIST) {
        return LOBBY_NAME_MAX;
    } else if (state.mode == MENU_MODE_LOBBY) {
        return CHAT_MESSAGE_MAX_LENGTH;
    } else {
        log_warn("Unhandled case in menu_get_text_input_max()");
        return 0;
    }
}

bool menu_is_button_disabled(const menu_state_t& state, MenuButton button) {
    switch (button) {
        case MENU_BUTTON_LOBBY_START:
            return !network_is_server();
        case MENU_BUTTON_LOBBY_READY:
            return network_is_server();
        case MENU_BUTTON_JOIN:
            return state.item_selected == -1;
        default:
            return false;
    }
}

void menu_render(const menu_state_t& state) {
    SDL_Rect background_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_SetRenderDrawColor(engine.renderer, COLOR_SKY_BLUE.r, COLOR_SKY_BLUE.g, COLOR_SKY_BLUE.b, COLOR_SKY_BLUE.a);
    SDL_RenderFillRect(engine.renderer, &background_rect);

    // Render tiles
    static const int menu_tile_width = (SCREEN_WIDTH / TILE_SIZE) * 2;
    static const int menu_tile_height = 3;
    for (int x = 0; x < menu_tile_width; x++) {
        for (int y = 0; y < menu_tile_height; y++) {
            SDL_Rect src_rect = (SDL_Rect) {
                .x = (y == 0 || y == 2) && (x + y) % 10 == 0 ? 48 : 16,
                .y = 0,
                .w = TILE_SIZE,
                .h = TILE_SIZE
            };
            SDL_Rect dst_rect = (SDL_Rect) {
                .x = (x * TILE_SIZE * 2) - state.parallax_x,
                .y = SCREEN_HEIGHT - ((menu_tile_height - y) * TILE_SIZE * 2),
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            if (dst_rect.x > SCREEN_WIDTH || dst_rect.x + (TILE_SIZE * 2) < 0) {
                continue;
            }
            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILESET_ARIZONA].texture, &src_rect, &dst_rect);

        }
    }

    // Render tile decorations
    static const xy tile_decoration_coords[3] = { xy(680, TILE_SIZE / 4), xy(920, 2 * (TILE_SIZE * 2)), xy(1250, TILE_SIZE / 4) };
    for (int i = 0; i < 3; i++) {
            SDL_Rect src_rect = (SDL_Rect) {
                .x = (((i * 2) + state.parallax_cactus_offset) % 5) * TILE_SIZE,
                .y = 0,
                .w = TILE_SIZE,
                .h = TILE_SIZE
            };
            SDL_Rect dst_rect = (SDL_Rect) {
                .x = tile_decoration_coords[i].x - state.parallax_x,
                .y = SCREEN_HEIGHT - (menu_tile_height * TILE_SIZE * 2) + tile_decoration_coords[i].y,
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            if (dst_rect.x > SCREEN_WIDTH || dst_rect.x + (TILE_SIZE * 2) < 0) {
                continue;
            }
            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILE_DECORATION].texture, &src_rect, &dst_rect);
    }
    // Render wagon animation
    SDL_Rect src_rect = (SDL_Rect) {
        .x = engine.sprites[SPRITE_UNIT_WAGON].frame_size.x * state.wagon_animation.frame.x,
        .y = engine.sprites[SPRITE_UNIT_WAGON].frame_size.y * 2,
        .w = engine.sprites[SPRITE_UNIT_WAGON].frame_size.x,
        .h = engine.sprites[SPRITE_UNIT_WAGON].frame_size.y
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = state.wagon_x,
        .y = SCREEN_HEIGHT - (6 * TILE_SIZE),
        .w = src_rect.w * 2,
        .h = src_rect.h * 2
    };
    uint8_t wagon_recolor_id;
    if (!(network_get_status() == NETWORK_STATUS_CONNECTED || network_get_status() == NETWORK_STATUS_SERVER)) {
        wagon_recolor_id = 0;
    } else if (state.mode == MENU_MODE_LOBBY && state.hover.type == MENU_HOVER_DROPDOWN_ITEM && state.dropdown_open == MENU_DROPDOWN_COLOR) {
        wagon_recolor_id = (uint8_t)state.hover.item;
    } else {
        wagon_recolor_id = network_get_player(network_get_player_id()).recolor_id;
    }
    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_UNIT_WAGON].colored_texture[wagon_recolor_id], &src_rect, &dst_rect);

    // Render clouds
    static const int cloud_count = 6;
    static const xy cloud_coords[cloud_count] = { xy(640, 16), xy(950, 64), xy(1250, 48), xy(-30, 48), xy(320, 32), xy(1600, 32) };
    static const int cloud_frame_x[cloud_count] = { 0, 1, 2, 2, 1, 1};
    for (int i = 0; i < cloud_count; i++) {
        SDL_Rect src_rect = (SDL_Rect) {
            .x = cloud_frame_x[i] * engine.sprites[SPRITE_MENU_CLOUDS].frame_size.x,
            .y = 0,
            .w = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.x,
            .h = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.y,
        };
        SDL_Rect dst_rect = (SDL_Rect) {
            .x = cloud_coords[i].x - state.parallax_cloud_x,
            .y = cloud_coords[i].y,
            .w = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.x * 2,
            .h = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.y * 2,
        };
        if (dst_rect.x > SCREEN_WIDTH || dst_rect.x + dst_rect.w < 0) {
            continue;
        }
        SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_MENU_CLOUDS].texture, &src_rect, &dst_rect);
    }

    if (state.mode != MENU_MODE_LOBBY && state.mode != MENU_MODE_MATCHLIST) {
        render_text(FONT_WESTERN32_OFFBLACK, "GOLD RUSH", xy(24, 24));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8_OFFBLACK, version_string, xy(4, SCREEN_HEIGHT - 14));

    if (state.status_timer > 0) {
        render_text(FONT_WESTERN8_RED, state.status_text.c_str(), xy(48, TEXT_INPUT_RECT.y - 36));
    }

    if (state.mode == MENU_MODE_USERNAME) {
        render_sprite(SPRITE_MENU_USERNAME, xy(0, 0), xy(TEXT_INPUT_RECT.x - (engine.sprites[SPRITE_MENU_USERNAME].frame_size.x - TEXT_INPUT_RECT.w), TEXT_INPUT_RECT.y), RENDER_SPRITE_NO_CULL);
        if (state.text_input_blink) {
            xy cursor_pos = xy(TEXT_INPUT_RECT.x + 4, TEXT_INPUT_RECT.y + 7) + xy(render_get_text_size(FONT_HACK_BLACK, state.text_input.c_str()).x - 4, 0);
            render_text(FONT_HACK_BLACK, "|", cursor_pos, TEXT_ANCHOR_BOTTOM_LEFT);
        }
        render_text(FONT_HACK_BLACK, state.text_input.c_str(), xy(TEXT_INPUT_RECT.x + 4, TEXT_INPUT_RECT.y + 7), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (state.mode == MENU_MODE_CONNECTING) {
        render_text(FONT_WESTERN16_OFFBLACK, "Connecting...", xy(48, TEXT_INPUT_RECT.y));
    }

    if (state.mode == MENU_MODE_MATCHLIST) {
        render_ninepatch(SPRITE_UI_FRAME, MATCHLIST_RECT, 16);

        if (network_get_lobby_count() == 0) {
            xy no_lobbies_text_size = render_get_text_size(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...");
            render_text(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...", xy(MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - (no_lobbies_text_size.x / 2), MATCHLIST_RECT.y + 8));
        }
        for (int lobby_index = 0; lobby_index < std::min(MATCHLIST_PAGE_SIZE, network_get_lobby_count() - (state.matchlist_page * MATCHLIST_PAGE_SIZE)); lobby_index++) {
            menu_render_lobby_text(state, lobby_index);
        }

        render_ninepatch(SPRITE_UI_FRAME_SMALL, MATCHLIST_SEARCH_RECT, 8);
        char lobby_name_text[256];
        sprintf(lobby_name_text, "Search: %s", state.text_input.c_str());
        render_text(FONT_HACK_GOLD, lobby_name_text, xy(MATCHLIST_SEARCH_RECT.x + 4, MATCHLIST_SEARCH_RECT.y - 6));
        if (state.text_input_blink) {
            xy cursor_pos = xy(MATCHLIST_SEARCH_RECT.x + 4, MATCHLIST_SEARCH_RECT.y - 6) + xy(render_get_text_size(FONT_HACK_GOLD, lobby_name_text).x - 4, 0);
            render_text(FONT_HACK_GOLD, "|", cursor_pos);
        }

        // Refresh button
        render_sprite(SPRITE_MENU_REFRESH, xy(state.hover.type == MENU_HOVER_REFRESH ? 1 : 0, 0), REFRESH_BUTTON_POSITION, RENDER_SPRITE_NO_CULL);
        int prev_hframe = 0;
        if (state.matchlist_page == 0) {
            prev_hframe = 2;
        } else if (state.hover.type == MENU_HOVER_PREVIOUS) {
            prev_hframe = 1;
        }
        render_sprite(SPRITE_MENU_NEXT, xy(prev_hframe, 0), PREV_BUTTON_POSITION, RENDER_SPRITE_NO_CULL | RENDER_SPRITE_FLIP_H);
        int next_hframe = 0;
        if (state.matchlist_page == menu_get_matchlist_page_count() - 1) {
            next_hframe = 2;
        } else if (state.hover.type == MENU_HOVER_NEXT) {
            next_hframe = 1;
        }
        render_sprite(SPRITE_MENU_NEXT, xy(next_hframe, 0), NEXT_BUTTON_POSITION, RENDER_SPRITE_NO_CULL);
    }

    if (state.mode == MENU_MODE_LOBBY) {
        // Player list
        render_ninepatch(SPRITE_UI_FRAME, PLAYERLIST_RECT, 16);
        render_ninepatch(SPRITE_UI_FRAME, MATCH_SETTINGS_RECT, 16);
        render_ninepatch(SPRITE_UI_FRAME, LOBBY_CHAT_RECT, 16);
        xy lobby_name_text_size = render_get_text_size(FONT_HACK_GOLD, network_get_lobby_name());
        xy lobby_name_text_pos = xy(PLAYERLIST_RECT.x + (PLAYERLIST_RECT.w / 2) - (lobby_name_text_size.x / 2), PLAYERLIST_RECT.y - 6);
        render_text(FONT_HACK_GOLD, network_get_lobby_name(), lobby_name_text_pos);

        render_text(FONT_HACK_GOLD, "Name", xy(COL_NAME_X, COL_HEADER_Y));
        render_text(FONT_HACK_GOLD, "Status", xy(COL_STATUS_X, COL_HEADER_Y));
        render_text(FONT_HACK_GOLD, "Team", xy(COL_TEAM_X, COL_HEADER_Y));
        render_text(FONT_HACK_GOLD, "Color", xy(COL_COLOR_X, COL_HEADER_Y));

        uint32_t player_index = 0;
        uint32_t current_player_index = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            char player_status_text[16];
            if (player.status == PLAYER_STATUS_HOST) {
                sprintf(player_status_text, "HOST");
            } else if (player.status == PLAYER_STATUS_NOT_READY) {
                sprintf(player_status_text, "NOT READY");
            } else if (player.status == PLAYER_STATUS_READY) {
                sprintf(player_status_text, "READY");
            }

            SDL_Rect dropdown_rect = menu_get_dropdown_rect(player_index);
            int dropdown_vframe = 0;
            if (player_id != network_get_player_id()) {
                dropdown_vframe = 3;
            } else if (state.hover.type == MENU_HOVER_DROPDOWN && state.hover.item == MENU_DROPDOWN_COLOR) {
                dropdown_vframe = 1;
            } else if (state.dropdown_open == MENU_DROPDOWN_COLOR) {
                dropdown_vframe = 2;
            }

            render_text(FONT_HACK_GOLD, player.name, xy(COL_NAME_X, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));
            render_text(FONT_HACK_GOLD, player_status_text, xy(COL_STATUS_X, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));

            char player_team_text[4];
            if (network_get_match_setting(MATCH_SETTING_TEAMS) == TEAMS_ENABLED) {
                sprintf(player_team_text, "%u", player.team);
            } else {
                sprintf(player_team_text, "-");
            }
            bool is_hovered = player_id == network_get_player_id() && state.hover.type == MENU_HOVER_TEAM_PICKER;
            render_sprite(SPRITE_UI_TEAM_PICKER, xy(is_hovered ? 1 : 0, 0), xy(COL_TEAM_X, dropdown_rect.y), RENDER_SPRITE_NO_CULL);
            render_text(is_hovered ? FONT_HACK_WHITE : FONT_HACK_BLACK, player_team_text, xy(COL_TEAM_X + (player.team == 1 ? 4 : 5), dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));

            render_sprite(SPRITE_UI_DROPDOWN_MINI, xy(0, dropdown_vframe), xy(dropdown_rect.x, dropdown_rect.y));
            render_text(dropdown_vframe == 1 ? FONT_HACK_WHITE : FONT_HACK_BLACK, PLAYER_COLORS[player.recolor_id].name, xy(dropdown_rect.x + 5, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));

            if (player_id == network_get_player_id()) {
                current_player_index = player_index;
            }
            player_index++;
        } // End for each player id

        // Settings
        xy settings_text_size = render_get_text_size(FONT_HACK_GOLD, "Settings");
        render_text(FONT_HACK_GOLD, "Settings", xy(MATCH_SETTINGS_RECT.x + (MATCH_SETTINGS_RECT.w / 2) - (settings_text_size.x / 2), MATCH_SETTINGS_RECT.y - 6));
        for (int setting = 0; setting < MATCH_SETTING_COUNT; setting++) {
            const match_setting_t& setting_data = MATCH_SETTING_DATA.at((MatchSetting)setting);
            SDL_Rect dropdown_rect = menu_get_match_setting_rect(setting);
            int dropdown_vframe = 0;
            if (!network_is_server()) {
                dropdown_vframe = 3;
            } else if (state.hover.type == MENU_HOVER_DROPDOWN && state.hover.item == setting) {
                dropdown_vframe = 1;
            } else if (state.dropdown_open == setting) {
                dropdown_vframe = 2;
            }
            render_text(FONT_HACK_GOLD, setting_data.name, xy(MATCH_SETTINGS_RECT.x + 8, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));
            render_sprite(SPRITE_UI_DROPDOWN_MINI, xy(0, dropdown_vframe), xy(dropdown_rect.x, dropdown_rect.y), RENDER_SPRITE_NO_CULL);
            render_text(dropdown_vframe == 1 ? FONT_HACK_WHITE : FONT_HACK_BLACK, menu_match_setting_value_str((MatchSetting)setting, network_get_match_setting((MatchSetting)setting)), xy(dropdown_rect.x + 5, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));
        }

        // Chat
        for (int chat_index = 0; chat_index < state.chat.size(); chat_index++) {
            render_text(FONT_HACK_GOLD, state.chat[chat_index].c_str(), xy(LOBBY_CHAT_RECT.x + 16, LOBBY_CHAT_RECT.y - 5 + (12 * chat_index)));
        }
        render_ninepatch(SPRITE_UI_FRAME_SMALL, LOBBY_CHAT_TEXTBOX_RECT, 8);
        char chat_text[128];
        sprintf(chat_text, "Chat: %s", state.text_input.c_str());
        if (state.text_input_blink) {
            xy cursor_pos = xy(LOBBY_CHAT_TEXTBOX_INPUT_RECT.x + 4, LOBBY_CHAT_TEXTBOX_RECT.y - 6) + xy(render_get_text_size(FONT_HACK_BLACK, state.text_input.c_str()).x - 4, 0);
            render_text(FONT_HACK_GOLD, "|", cursor_pos);
        }
        render_text(FONT_HACK_GOLD, chat_text, xy(LOBBY_CHAT_TEXTBOX_RECT.x + 4, LOBBY_CHAT_TEXTBOX_RECT.y - 6));

        if (state.dropdown_open != -1) {
            SDL_Rect dropdown_rect = state.dropdown_open == MENU_DROPDOWN_COLOR ? menu_get_dropdown_rect(current_player_index) : menu_get_match_setting_rect(state.dropdown_open);
            int dropdown_value_count = state.dropdown_open == MENU_DROPDOWN_COLOR ? MAX_PLAYERS : MATCH_SETTING_DATA.at((MatchSetting)state.dropdown_open).value_count;
            for (int item = 0; item < dropdown_value_count; item++) {
                dropdown_rect.y += dropdown_rect.h;
                bool hovered = state.hover.type == MENU_HOVER_DROPDOWN_ITEM && state.hover.item == item;
                render_sprite(SPRITE_UI_DROPDOWN_MINI, xy(0, hovered ? 4 : 3), xy(dropdown_rect.x, dropdown_rect.y));
                render_text(hovered ? FONT_HACK_WHITE : FONT_HACK_BLACK, state.dropdown_open == MENU_DROPDOWN_COLOR ? PLAYER_COLORS[item].name : menu_match_setting_value_str((MatchSetting)state.dropdown_open, item), xy(dropdown_rect.x + 5, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));
            }
        } 
    } // End if menu mode lobby

    auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
    if (mode_buttons_it != MODE_BUTTONS.end()) {
        for (MenuButton button : mode_buttons_it->second) {
            if (menu_is_button_disabled(state, button)) {
                continue;
            }
            
            const menu_button_t& button_data = MENU_BUTTON_DATA.at(button);
            render_menu_button(button_data.text, xy(button_data.position_x, button_data.position_y), state.hover.type == MENU_HOVER_BUTTON && button == state.hover.button);
        }
    }

    if (state.options_state.mode != OPTION_MENU_CLOSED) {
        options_menu_render(state.options_state);
        return;
    }
}

SDL_Rect menu_get_lobby_text_frame_rect(int lobby_index) {
    char lobby_text[64];
    sprintf(lobby_text, "* %s (4/4) *", network_get_lobby(lobby_index).name);

    xy text_size = render_get_text_size(FONT_HACK_BLACK, lobby_text);
    int frame_width = (text_size.x / 15) + 1;
    if (text_size.x % 15 != 0) {
        frame_width++;
    }

    return (SDL_Rect) {
        .x = MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - ((frame_width * 15) / 2), .y = MATCHLIST_RECT.y + (20 * lobby_index) + 12,
        .w = frame_width * 15, .h = 15
    };
}

void menu_render_lobby_text(const menu_state_t& state, int lobby_index) {
    SDL_Rect rect = menu_get_lobby_text_frame_rect(lobby_index);

    bool text_hovered = state.hover.type == MENU_HOVER_ITEM && state.hover.item == lobby_index;

    xy position = xy(rect.x, rect.y + (text_hovered ? -1 : 0));
    int frame_width = rect.w / 15;
    for (int frame_x = 0; frame_x < frame_width; frame_x++) {
        int x_frame = 1;
        if (frame_x == 0) {
            x_frame = 0;
        } else if (frame_x == frame_width - 1) {
            x_frame = 2;
        }
        render_sprite(SPRITE_UI_TEXT_FRAME, xy(x_frame, 0), position + xy(frame_x * 15, 0), RENDER_SPRITE_NO_CULL);
    }


    char lobby_text[64];
    const lobby_t& lobby = network_get_lobby((state.matchlist_page* MATCHLIST_PAGE_SIZE) + lobby_index);
    sprintf(lobby_text, "%s %s (%u/%u) %s", state.item_selected == lobby_index ? "*" : "", lobby.name, lobby.player_count, MAX_PLAYERS, state.item_selected == lobby_index ? "*" : "");
    xy text_size = render_get_text_size(FONT_HACK_BLACK, lobby_text);

    xy text_position_offset = xy(((frame_width * 15) / 2) - (text_size.x / 2), -10); 
    render_text(text_hovered ? FONT_HACK_WHITE : FONT_HACK_BLACK, lobby_text, position + text_position_offset);
}

SDL_Rect menu_get_dropdown_rect(int index) {
    return (SDL_Rect) {
        .x = COL_COLOR_X,
        .y = COL_HEADER_Y + 32 + ((engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.y + 4) * index),
        .w = engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.x,
        .h = engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.y
    };
}

SDL_Rect menu_get_match_setting_rect(int index) {
    return (SDL_Rect) {
        .x = MATCH_SETTINGS_RECT.x + MATCH_SETTINGS_RECT.w - engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.x - 8,
        .y = COL_HEADER_Y + 10 + ((engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.y + 4) * index),
        .w = engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.x,
        .h = engine.sprites[SPRITE_UI_DROPDOWN_MINI].frame_size.y
    };
}

const char* menu_match_setting_value_str(MatchSetting setting, uint32_t value) {
    switch (setting) {
        case MATCH_SETTING_MAP_SIZE: {
            switch (value) {
                case MAP_SIZE_SMALL:
                    return "Small";
                case MAP_SIZE_MEDIUM:
                    return "Medium";
                case MAP_SIZE_LARGE:
                    return "Large";
                default:
                    return "";
            }
        }
        case MATCH_SETTING_TEAMS: {
            return value == TEAMS_ENABLED ? "Yes" : "No";
        }
        default:
            return "";
    }
}

size_t menu_get_matchlist_page_count() {
    if (network_get_lobby_count() == 0) {
        return 1;
    }
    return (network_get_lobby_count() / MATCHLIST_PAGE_SIZE) + (int)(network_get_lobby_count() % MATCHLIST_PAGE_SIZE != 0);
}

std::vector<std::string> menu_split_chat_message(std::string message) {
    std::vector<std::string> words;
    while (message.length() != 0) {
        size_t space_index = message.find(' ');
        if (space_index == std::string::npos) {
            words.push_back(message);
            message = "";
        } else {
            words.push_back(message.substr(0, space_index));
            message = message.substr(space_index + 1);
        }
    }

    std::vector<std::string> lines;
    std::string line;
    while (!words.empty()) {
        if (!line.empty()) {
            line += " ";
        }

        std::string next = words[0];
        words.erase(words.begin());

        if (next.length() + line.length() > CHAT_LINE_MAX) {
            lines.push_back(line);
            line = "";
        } else if (!line.empty()) {
            line += " ";
        }
        line += next;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }

    return lines;
}

void menu_add_chat_message(menu_state_t& state, std::string message) {
    std::vector<std::string> lines = menu_split_chat_message(message);
    while (!lines.empty()) {
        if (state.chat.size() == CHAT_MAX_MESSAGES) {
            state.chat.erase(state.chat.begin());
        }
        state.chat.push_back(lines[0]);
        lines.erase(lines.begin());
    }
}
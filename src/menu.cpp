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
    .x = 36, .y = 32, .w = 432, .h = 128
};
static const SDL_Rect PLAYERLIST_RECT = (SDL_Rect) {
    .x = 16, .y = 32, .w = 432, .h = 128
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
static SDL_Rect LOBBY_TEXT_INPUT_RECT = (SDL_Rect) {
    .x = PLAYERLIST_RECT.x - (PLAYERLIST_RECT.w / 2) - 75, .y = PLAYERLIST_RECT.y - 4, .w = 150, .h = 18
};
static const xy REFRESH_BUTTON_POSITION = xy(MATCHLIST_RECT.x + MATCHLIST_RECT.w + 8, MATCHLIST_RECT.y + 8);

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
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_JOIN, (menu_button_t) {
        .text = "JOIN",
        .position_x = BUTTON_X + ((56 + 4) * 2),
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_MATCHLIST_BACK, (menu_button_t) {
        .text = "BACK",
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + 42
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
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_LOBBY_READY, (menu_button_t) {
        .text = "READY",
        .position_x = BUTTON_X + 56 + 4,
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_LOBBY_START, (menu_button_t) {
        .text = "START",
        .position_x = BUTTON_X + 56 + 4,
        .position_y = BUTTON_Y + 42
    }},
    { MENU_BUTTON_CONNECTING_BACK, (menu_button_t) {
        .text = "BACK",
        .position_x = BUTTON_X,
        .position_y = BUTTON_Y + 42
    }},
};

static const uint32_t STATUS_TIMER_DURATION = 60;
static const int PARALLAX_TIMER_DURATION = 2;

static const int COL_HEADER_Y = PLAYERLIST_RECT.y + 12;
static const int COL_NAME_X = PLAYERLIST_RECT.x + 16;
static const int COL_STATUS_X = PLAYERLIST_RECT.x + 216;
static const int COL_TEAM_X = PLAYERLIST_RECT.x + 280;
static const int COL_COLOR_X = PLAYERLIST_RECT.x + 336;
static const int MINI_DROPDOWN_TEXT_Y_OFFSET = -10;

struct match_setting_t {
    const char* name;
    uint32_t default_value;
    uint32_t value_count;
};
static const std::unordered_map<MatchSetting, match_setting_t> MATCH_SETTING_DATA = {
    { MATCH_SETTING_MAP_SIZE, (match_setting_t) {
        .name = "Map Size:",
        .default_value = MATCH_SETTING_MAP_SIZE_SMALL,
        .value_count = MATCH_SETTING_MAP_SIZE_COUNT
    }},
    { MATCH_SETTING_TEAMS, (match_setting_t) {
        .name = "Team Game:",
        .default_value = MATCH_SETTING_TEAMS_DISABLED,
        .value_count = MATCH_SETTING_TEAMS_COUNT
    }}
};

menu_state_t menu_init() {
    menu_state_t state;

    state.status_text = "";
    state.username = "";
    state.status_timer = 0;
    state.hover = (menu_hover_t) {
        .type = MENU_HOVER_NONE
    };
    menu_set_mode(state, MENU_MODE_MAIN);

    state.dropdown_open = -1;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
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
                    if (state.username.empty()) {
                        menu_show_status(state, "Invalid username.");
                        break;
                    }

                    SDL_StopTextInput();
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

                    state.lobby_name = std::string(network_get_lobby_name());
                    for (int setting = 0; setting < MATCH_SETTING_COUNT; setting++) {
                        network_set_match_setting((MatchSetting)setting, MATCH_SETTING_DATA.at((MatchSetting)setting).default_value);
                    }
                    menu_set_mode(state, MENU_MODE_LOBBY);
                    break;
                }
                case MENU_BUTTON_JOIN: {
                    GOLD_ASSERT(state.item_selected != -1 && state.item_selected < network_get_lobby_count());
                    const lobby_t& lobby = network_get_lobby(state.item_selected);
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
                    if (!network_are_all_players_ready()) {
                        break;
                    }

                    bool are_all_players_different_colors = true;
                    int color_occurances[MAX_PLAYERS];
                    memset(color_occurances, 0, sizeof(color_occurances));
                    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                        const player_t& player = network_get_player(player_id);
                        if (player.status == PLAYER_STATUS_NONE) {
                            continue;
                        }
                        color_occurances[player.recolor_id]++;
                        if (color_occurances[player.recolor_id] > 1) {
                            are_all_players_different_colors = false;
                            break;
                        }
                    }
                    if (!are_all_players_different_colors) {
                        break;
                    }

                    network_scanner_destroy();
                    network_begin_loading_match();
                    menu_set_mode(state, MENU_MODE_LOAD_MATCH);
                    break;
                }
                default:
                    break;
            }
            sound_play(SOUND_UI_SELECT);
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
        }

        if (state.hover.type == MENU_HOVER_REFRESH) {
            network_scanner_search();
            sound_play(SOUND_UI_SELECT);
        }

        if (state.hover.type == MENU_HOVER_EDIT_BUTTON) {
            state.editing_lobby_name = true;
            SDL_SetTextInputRect(&LOBBY_TEXT_INPUT_RECT);
            SDL_StartTextInput();
            sound_play(SOUND_UI_SELECT);
        }
    } else if (event.type == SDL_TEXTINPUT) {
        if (state.mode == MENU_MODE_USERNAME) {
            state.username += std::string(event.text.text);
            if (state.username.length() > MAX_USERNAME_LENGTH) {
                state.username = state.username.substr(0, MAX_USERNAME_LENGTH);
            }
        } else if (state.editing_lobby_name) {
            state.lobby_name += std::string(event.text.text);
            if (state.lobby_name.length() > LOBBY_NAME_MAX) {
                state.lobby_name = state.lobby_name.substr(0, LOBBY_NAME_MAX);
            }
        }
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
        if (state.mode == MENU_MODE_USERNAME && state.username.length() > 0) {
            state.username.pop_back();
        } else if (state.editing_lobby_name && state.lobby_name.length() > 0) {
            state.lobby_name.pop_back();
        }
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && state.editing_lobby_name) {
        state.lobby_name = std::string(network_get_lobby_name());
        state.editing_lobby_name = false;
        SDL_StopTextInput();
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) {
        if (state.editing_lobby_name && state.lobby_name.length() > 0) {
            network_set_lobby_name(state.lobby_name.c_str());
            state.editing_lobby_name = false;
            SDL_StopTextInput();
        }
    }
}

void menu_update(menu_state_t& state) {
    if (state.status_timer > 0) {
        state.status_timer--;
    }

    network_service();
    network_event_t network_event;
    while (network_poll_events(&network_event)) {
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
                menu_set_mode(state, MENU_MODE_LOBBY);
                break;
            }
            case NETWORK_EVENT_PLAYER_DISCONNECTED: {
                if (network_event.player_disconnected.player_id == 0) {
                    network_disconnect();
                    menu_set_mode(state, MENU_MODE_MATCHLIST);
                    state.item_selected = -1;
                }
                break;
            }
            case NETWORK_EVENT_MATCH_LOAD: {
                network_scanner_destroy();
                menu_set_mode(state, MENU_MODE_LOAD_MATCH);
                break;
            }
            default:
                break;
        }
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

    state.hover = (menu_hover_t) {
        .type = MENU_HOVER_NONE
    };
    if (state.options_state.mode != OPTION_MENU_CLOSED) {
        options_menu_update(state.options_state);
        return;
    }
    if (state.editing_lobby_name) {
        return;
    }

    if (network_is_server() && state.mode == MENU_MODE_LOBBY) {
        SDL_Rect edit_button_rect = menu_get_lobby_edit_button_rect(state.lobby_name.c_str());
        if (sdl_rect_has_point(edit_button_rect, engine.mouse_position)) {
            state.hover = (menu_hover_t) {
                .type = MENU_HOVER_EDIT_BUTTON
            };
        }
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
            SDL_Rect refresh_button_rect = (SDL_Rect) { 
                .x = REFRESH_BUTTON_POSITION.x, 
                .y = REFRESH_BUTTON_POSITION.y, 
                .w = engine.sprites[SPRITE_MENU_REFRESH].frame_size.x,
                .h = engine.sprites[SPRITE_MENU_REFRESH].frame_size.y
            };
            if (sdl_rect_has_point(refresh_button_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_REFRESH
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

        // Check for dropdown hover
        if (state.dropdown_open == -1) {
            if (sdl_rect_has_point(color_dropdown_rect, engine.mouse_position)) {
                state.hover = (menu_hover_t) {
                    .type = MENU_HOVER_DROPDOWN,
                    .item = MENU_DROPDOWN_COLOR
                };
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
    if (state.mode == MENU_MODE_LOBBY && state.editing_lobby_name) {
        state.editing_lobby_name = false;
        SDL_StopTextInput();
    }

    state.mode = mode;

    state.dropdown_open = -1;
    if (state.mode == MENU_MODE_MATCHLIST) {
        state.item_selected = -1;

        if (!network_scanner_create()) {
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "Error occurred while scanning for LAN games.");
            return;
        }
        network_scanner_search();
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
        .x = 380,
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
        render_text(FONT_HACK_BLACK, state.username.c_str(), xy(TEXT_INPUT_RECT.x + 4, TEXT_INPUT_RECT.y + 7), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (state.mode == MENU_MODE_CONNECTING) {
        render_text(FONT_WESTERN16_OFFBLACK, "Connecting...", xy(48, TEXT_INPUT_RECT.y));
    }

    if (state.mode == MENU_MODE_MATCHLIST) {
        render_ninepatch(SPRITE_UI_FRAME, MATCHLIST_RECT, 16);

        for (int lobby_index = 0; lobby_index < network_get_lobby_count(); lobby_index++) {
            menu_render_lobby_text(state, lobby_index);
        }

        // Refresh button
        SDL_Rect refresh_src_rect = (SDL_Rect) { 
            .x = state.hover.type == MENU_HOVER_REFRESH ? engine.sprites[SPRITE_MENU_REFRESH].frame_size.x : 0,
            .y = 0,
            .w = engine.sprites[SPRITE_MENU_REFRESH].frame_size.x,
            .h = engine.sprites[SPRITE_MENU_REFRESH].frame_size.y
        };
        SDL_Rect refresh_dst_rect = (SDL_Rect) {
            .x = REFRESH_BUTTON_POSITION.x,
            .y = REFRESH_BUTTON_POSITION.y,
            .w = refresh_src_rect.w,
            .h = refresh_src_rect.h
        };
        SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_MENU_REFRESH].texture, &refresh_src_rect, &refresh_dst_rect);
    }

    // Player list
    if (state.mode == MENU_MODE_LOBBY) {
        render_ninepatch(SPRITE_UI_FRAME, PLAYERLIST_RECT, 16);
        render_ninepatch(SPRITE_UI_FRAME, MATCH_SETTINGS_RECT, 16);
        xy lobby_name_text_size = render_get_text_size(FONT_HACK_GOLD, network_is_server() ? state.lobby_name.c_str() : network_get_lobby_name());
        render_text(FONT_HACK_GOLD, network_is_server() ? state.lobby_name.c_str() : network_get_lobby_name(), xy(PLAYERLIST_RECT.x + (PLAYERLIST_RECT.w / 2) - (lobby_name_text_size.x / 2), PLAYERLIST_RECT.y - 4));
        if (!state.editing_lobby_name && network_is_server()) {
            SDL_Rect lobby_edit_button_rect = menu_get_lobby_edit_button_rect(state.lobby_name.c_str());
            render_sprite(SPRITE_MENU_EDIT_BUTTON, xy(0, (int)(state.hover.type == MENU_HOVER_EDIT_BUTTON)), xy(lobby_edit_button_rect.x, lobby_edit_button_rect.y + (state.hover.type == MENU_HOVER_EDIT_BUTTON ? -1 : 0)), RENDER_SPRITE_NO_CULL);
        }

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
            render_text(FONT_HACK_GOLD, "-", xy(COL_TEAM_X, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));
            render_sprite(SPRITE_UI_DROPDOWN_MINI, xy(0, dropdown_vframe), xy(dropdown_rect.x, dropdown_rect.y));
            render_text(dropdown_vframe == 1 ? FONT_HACK_WHITE : FONT_HACK_BLACK, PLAYER_COLORS[player.recolor_id].name, xy(dropdown_rect.x + 5, dropdown_rect.y + MINI_DROPDOWN_TEXT_Y_OFFSET));

            if (player_id == network_get_player_id()) {
                current_player_index = player_index;
            }
            player_index++;
        } // End for each player id

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
        .x = MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - ((frame_width * 15) / 2), .y = MATCHLIST_RECT.y + (20 * lobby_index) + 8,
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
    const lobby_t& lobby = network_get_lobby(lobby_index);
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
                case MATCH_SETTING_MAP_SIZE_SMALL:
                    return "Small";
                case MATCH_SETTING_MAP_SIZE_MEDIUM:
                    return "Medium";
                case MATCH_SETTING_MAP_SIZE_LARGE:
                    return "Large";
                default:
                    return "";
            }
        }
        case MATCH_SETTING_TEAMS: {
            return value == MATCH_SETTING_TEAMS_ENABLED ? "Yes" : "No";
        }
        default:
            return "";
    }
}

SDL_Rect menu_get_lobby_edit_button_rect(const char* lobby_name) {
    xy lobby_name_size = render_get_text_size(FONT_HACK_GOLD, lobby_name);
    xy lobby_name_pos = xy(PLAYERLIST_RECT.x + (PLAYERLIST_RECT.w / 2) - (lobby_name_size.x / 2), PLAYERLIST_RECT.y - 4);
    return (SDL_Rect) {
        .x = lobby_name_pos.x + lobby_name_size.x + 8,
        .y = lobby_name_pos.y + 10,
        .w = engine.sprites[SPRITE_MENU_EDIT_BUTTON].frame_size.x,
        .h = engine.sprites[SPRITE_MENU_EDIT_BUTTON].frame_size.y,
    };
}
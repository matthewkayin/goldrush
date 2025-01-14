#include "menu.h"

#include "logger.h"
#include "asserts.h"
#include <cstring>
#include <unordered_map>

static const std::unordered_map<MenuMode, std::vector<MenuButton>> MODE_BUTTONS = {
    { MENU_MODE_MAIN, { MENU_BUTTON_PLAY, MENU_BUTTON_OPTIONS, MENU_BUTTON_EXIT } },
    { MENU_MODE_USERNAME, { MENU_BUTTON_USERNAME_BACK, MENU_BUTTON_USERNAME_OK } },
    { MENU_MODE_MATCHLIST, { MENU_BUTTON_HOST, MENU_BUTTON_JOIN, MENU_BUTTON_MATCHLIST_BACK } },
    { MENU_MODE_LOBBY, { MENU_BUTTON_LOBBY_BACK, MENU_BUTTON_LOBBY_READY, MENU_BUTTON_LOBBY_START } },
    { MENU_MODE_CONNECTING, { MENU_BUTTON_CONNECTING_BACK } }
};

struct menu_button_t {
    const char* text;
    SDL_Rect rect;
};

static SDL_Rect TEXT_INPUT_RECT = (SDL_Rect) {
    .x = 48, .y = 120, .w = 264, .h = 35 
};
static const SDL_Rect PLAYERLIST_RECT = (SDL_Rect) {
    .x = 24, .y = 32, .w = 256, .h = 128
};
static const int PLAYERLIST_ITEM_HEIGHT = 16;
static const xy REFRESH_BUTTON_POSITION = xy(PLAYERLIST_RECT.x + PLAYERLIST_RECT.w + 8, PLAYERLIST_RECT.y);

static const std::unordered_map<MenuButton, menu_button_t> MENU_BUTTON_DATA = {
    { MENU_BUTTON_PLAY, (menu_button_t) {
        .text = "PLAY",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128,
            .w = 84, .h = 30
    }}},
    { MENU_BUTTON_OPTIONS, (menu_button_t) {
        .text = "OPTIONS",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42,
            .w = 120, .h = 30
    }}},
    { MENU_BUTTON_EXIT, (menu_button_t) {
        .text = "EXIT",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42 + 42,
            .w = 72, .h = 30
    }}},
    { MENU_BUTTON_HOST, (menu_button_t) {
        .text = "HOST",
        .rect = (SDL_Rect) {
            .x = PLAYERLIST_RECT.x, .y = 128 + 42,
            .w = 76, .h = 30
    }}},
    { MENU_BUTTON_JOIN, (menu_button_t) {
        .text = "JOIN",
        .rect = (SDL_Rect) {
            .x = PLAYERLIST_RECT.x + 76 + 8, .y = 128 + 42,
            .w = 72, .h = 30
    }}},
    { MENU_BUTTON_MATCHLIST_BACK, (menu_button_t) {
        .text = "BACK",
        .rect = (SDL_Rect) {
            .x = PLAYERLIST_RECT.x, .y = 128 + 42 + 42,
            .w = 78, .h = 30
    }}},
    { MENU_BUTTON_USERNAME_BACK, (menu_button_t) {
        .text = "BACK",
        .rect = (SDL_Rect) {
            .x = 48 + 48 + 8, .y = 128 + 42,
            .w = 78, .h = 30
    }}},
    { MENU_BUTTON_USERNAME_OK, (menu_button_t) {
        .text = "OK",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42,
            .w = 48, .h = 30
    }}},
    { MENU_BUTTON_LOBBY_BACK, (menu_button_t) {
        .text = "BACK",
        .rect = (SDL_Rect) {
            .x = PLAYERLIST_RECT.x, .y = PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 8,
            .w = 78, .h = 30
    }}},
    { MENU_BUTTON_LOBBY_READY, (menu_button_t) {
        .text = "READY",
        .rect = (SDL_Rect) {
            .x = PLAYERLIST_RECT.x + 78 + 8, .y = PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 8,
            .w = 100, .h = 30
    }}},
    { MENU_BUTTON_LOBBY_START, (menu_button_t) {
        .text = "START",
        .rect = (SDL_Rect) {
            .x = PLAYERLIST_RECT.x + 78 + 8, .y = PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 8,
            .w = 92, .h = 30
    }}},
    { MENU_BUTTON_CONNECTING_BACK, (menu_button_t) {
        .text = "BACK",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42,
            .w = 78, .h = 30
    }}},
};

static const uint32_t STATUS_TIMER_DURATION = 60;
static const int PARALLAX_TIMER_DURATION = 2;

menu_state_t menu_init() {
    menu_state_t state;

    state.status_text = "";
    state.username = "";
    state.status_timer = 0;
    state.hover = (menu_hover_t) {
        .type = MENU_HOVER_NONE
    };
    menu_set_mode(state, MENU_MODE_MAIN);

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;

    return state;
}

void menu_handle_input(menu_state_t& state, SDL_Event event) {
    // Mouse pressed
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        // Button pressed
        if (state.hover.type == MENU_HOVER_BUTTON) {
            switch (state.hover.button) {
                case MENU_BUTTON_PLAY: {
                    menu_set_mode(state, MENU_MODE_USERNAME);
                    break;
                }
                case MENU_BUTTON_OPTIONS: {
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
                    network_scanner_destroy();
                    network_begin_loading_match();
                    menu_set_mode(state, MENU_MODE_LOAD_MATCH);
                    break;
                }
                default:
                    break;
            }
        }

        state.item_selected = state.mode == MENU_MODE_MATCHLIST && state.hover.type == MENU_HOVER_ITEM
                                ?  state.item_selected = state.hover.item
                                : -1;

        // Text input pressed
        if (state.hover.type == MENU_HOVER_NONE && state.mode == MENU_MODE_USERNAME &&
            sdl_rect_has_point(TEXT_INPUT_RECT, engine.mouse_position)) {
            SDL_SetTextInputRect(&TEXT_INPUT_RECT);
            SDL_StartTextInput();
        }

        if (state.hover.type == MENU_HOVER_REFRESH) {
            network_scanner_search();
        }
    } else if (event.type == SDL_TEXTINPUT) {
        if (state.mode == MENU_MODE_USERNAME) {
            state.username += std::string(event.text.text);
            if (state.username.length() > MAX_USERNAME_LENGTH) {
                state.username = state.username.substr(0, MAX_USERNAME_LENGTH);
            }
        }
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
        if (state.mode == MENU_MODE_USERNAME && state.username.length() > 0) {
            state.username.pop_back();
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

    state.hover = (menu_hover_t) {
        .type = MENU_HOVER_NONE
    };
    if (SDL_GetWindowMouseGrab(engine.window) == SDL_TRUE) {
        auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
        if (mode_buttons_it != MODE_BUTTONS.end()) {
            for (MenuButton button : mode_buttons_it->second) {
                if (menu_is_button_disabled(state, button)) {
                    continue;
                }

                if (sdl_rect_has_point(MENU_BUTTON_DATA.at(button).rect, engine.mouse_position)) {
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
            SDL_Rect item_rect = (SDL_Rect) { 
                .x = PLAYERLIST_RECT.x, 
                .y = PLAYERLIST_RECT.y + (PLAYERLIST_ITEM_HEIGHT * match_index), 
                .w = PLAYERLIST_RECT.w, 
                .h = PLAYERLIST_ITEM_HEIGHT 
            };
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
}

void menu_show_status(menu_state_t& state, const char* text) {
    state.status_text = text;
    state.status_timer = STATUS_TIMER_DURATION;
}

void menu_set_mode(menu_state_t& state, MenuMode mode) {
    state.mode = mode;

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
    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_UNIT_WAGON].colored_texture[network_get_status() == NETWORK_STATUS_CONNECTED ? network_get_player_id() : 0], &src_rect, &dst_rect);

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
        const char* prompt_text = "USERNAME";
        render_text(FONT_WESTERN8_OFFBLACK, prompt_text, xy(TEXT_INPUT_RECT.x + 1, TEXT_INPUT_RECT.y - 13));
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(engine.renderer, &TEXT_INPUT_RECT);
        render_text(FONT_WESTERN16_OFFBLACK, state.username.c_str(), xy(TEXT_INPUT_RECT.x + 4, TEXT_INPUT_RECT.y + 31), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (state.mode == MENU_MODE_CONNECTING) {
        render_text(FONT_WESTERN16_OFFBLACK, "Connecting...", xy(48, TEXT_INPUT_RECT.y));
    }

    if (state.mode == MENU_MODE_MATCHLIST) {
        SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
        SDL_RenderDrawRect(engine.renderer, &PLAYERLIST_RECT);

        for (int lobby_index = 0; lobby_index < network_get_lobby_count(); lobby_index++) {
            const lobby_t& lobby = network_get_lobby(lobby_index);

            char lobby_text[64];
            sprintf(lobby_text, "%s (%u/%u)", lobby.name, lobby.player_count, MAX_PLAYERS);

            int line_y = 16 * (lobby_index + 1);
            if (lobby_index == state.item_selected) {
                SDL_Rect item_rect = (SDL_Rect) {
                    .x = PLAYERLIST_RECT.x,
                    .y = PLAYERLIST_RECT.y + (PLAYERLIST_ITEM_HEIGHT * lobby_index),
                    .w = PLAYERLIST_RECT.w,
                    .h = PLAYERLIST_ITEM_HEIGHT
                };
                SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(engine.renderer, &item_rect);
                SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
            } 
            render_text(FONT_WESTERN8_OFFBLACK, lobby_text, xy(PLAYERLIST_RECT.x + 4, PLAYERLIST_RECT.y + line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            SDL_RenderDrawLine(engine.renderer, PLAYERLIST_RECT.x, PLAYERLIST_RECT.y + line_y, PLAYERLIST_RECT.x + PLAYERLIST_RECT.w - 1, PLAYERLIST_RECT.y + line_y);
        }

        if (state.hover.type == MENU_HOVER_ITEM) {
            SDL_Rect item_rect = (SDL_Rect) {
                .x = PLAYERLIST_RECT.x,
                .y = PLAYERLIST_RECT.y + (PLAYERLIST_ITEM_HEIGHT * state.hover.item),
                .w = PLAYERLIST_RECT.w,
                .h = PLAYERLIST_ITEM_HEIGHT
            };
            SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(engine.renderer, &item_rect);
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
        SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
        SDL_RenderDrawRect(engine.renderer, &PLAYERLIST_RECT);

        uint32_t player_index = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            char player_name_text[32];
            char* player_name_text_ptr = player_name_text;
            player_name_text_ptr += sprintf(player_name_text_ptr, "%s", player.name);
            if (player.status == PLAYER_STATUS_HOST) {
                player_name_text_ptr += sprintf(player_name_text_ptr, ": HOST");
            } else if (player.status == PLAYER_STATUS_NOT_READY) {
                player_name_text_ptr += sprintf(player_name_text_ptr, ": NOT READY");
            } else if (player.status == PLAYER_STATUS_READY) {
                player_name_text_ptr += sprintf(player_name_text_ptr, ": READY");
            }

            int line_y = PLAYERLIST_ITEM_HEIGHT * (player_index + 1);
            render_text(FONT_WESTERN8_OFFBLACK, player_name_text, xy(PLAYERLIST_RECT.x + 4, PLAYERLIST_RECT.y + line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            SDL_RenderDrawLine(engine.renderer, PLAYERLIST_RECT.x, PLAYERLIST_RECT.y + line_y, PLAYERLIST_RECT.x + PLAYERLIST_RECT.w - 1, PLAYERLIST_RECT.y + line_y);
            player_index++;
        } // End for each player id

        if (state.mode == MENU_MODE_LOBBY && network_is_server()) {
            render_text(FONT_WESTERN8_OFFBLACK, "You are the host.", xy(PLAYERLIST_RECT.x + PLAYERLIST_RECT.w + 2, PLAYERLIST_RECT.y));
        }
    }

    auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
    if (mode_buttons_it != MODE_BUTTONS.end()) {
        for (MenuButton button : mode_buttons_it->second) {
            if (menu_is_button_disabled(state, button)) {
                continue;
            }
            
            SDL_Color button_color = state.hover.type == MENU_HOVER_BUTTON && button == state.hover.button ? COLOR_WHITE : COLOR_OFFBLACK;
            Font button_font = state.hover.type == MENU_HOVER_BUTTON && button == state.hover.button ? FONT_WESTERN16_WHITE : FONT_WESTERN16_OFFBLACK;
            SDL_SetRenderDrawColor(engine.renderer, button_color.r, button_color.g, button_color.b, button_color.a);

            const menu_button_t& button_data = MENU_BUTTON_DATA.at(button);
            SDL_RenderDrawRect(engine.renderer, &button_data.rect);
            render_text(button_font, button_data.text, xy(button_data.rect.x + 4, button_data.rect.y + 4));
        }
    }
}
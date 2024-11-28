#include "menu.h"

#include "network.h"
#include <cstring>
#include <unordered_map>

static const std::unordered_map<MenuMode, std::vector<MenuButton>> MODE_BUTTONS = {
    { MENU_MODE_MAIN, { MENU_BUTTON_PLAY, MENU_BUTTON_OPTIONS, MENU_BUTTON_EXIT } },
    { MENU_MODE_USERNAME, { MENU_BUTTON_USERNAME_BACK, MENU_BUTTON_USERNAME_OK } },
    { MENU_MODE_MATCHLIST, { MENU_BUTTON_HOST, MENU_BUTTON_JOIN, MENU_BUTTON_MATCHLIST_BACK } },
    { MENU_MODE_LOBBY, { MENU_BUTTON_LOBBY_BACK, MENU_BUTTON_LOBBY_READY } },
    { MENU_MODE_LOBBY_HOST, { MENU_BUTTON_LOBBY_BACK, MENU_BUTTON_LOBBY_START } },
    { MENU_MODE_CONNECTING, { MENU_BUTTON_CONNECTING_BACK } }
};

struct menu_button_t {
    const char* text;
    SDL_Rect rect;
};

static const SDL_Rect TEXT_INPUT_RECT = (SDL_Rect) {
    .x = 48, .y = 120, .w = 264, .h = 35 
};
static const SDL_Rect PLAYERLIST_RECT = (SDL_Rect) {
    .x = 24, .y = 32, .w = 256, .h = 128
};

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
            .x = 48, .y = 128 + 42,
            .w = 76, .h = 30
    }}},
    { MENU_BUTTON_JOIN, (menu_button_t) {
        .text = "JOIN",
        .rect = (SDL_Rect) {
            .x = 48 + 76 + 8, .y = 128 + 42,
            .w = 72, .h = 30
    }}},
    { MENU_BUTTON_MATCHLIST_BACK, (menu_button_t) {
        .text = "BACK",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42 + 42,
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

menu_state_t menu_init() {
    menu_state_t state;

    state.mode = MENU_MODE_MAIN;
    state.status_text = "";
    state.username = "";
    state.status_timer = 0;
    state.button_hovered = MENU_BUTTON_NONE;

    state.parallax_x = 0;

    return state;
}

void menu_handle_input(menu_state_t& state, SDL_Event event) {
    // Mouse pressed
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        // Button pressed
        switch (state.button_hovered) {
            case MENU_BUTTON_PLAY: {
                state.mode = MENU_MODE_USERNAME;
                break;
            }
            case MENU_BUTTON_OPTIONS: {
                break;
            }
            case MENU_BUTTON_EXIT: {
                state.mode = MENU_MODE_EXIT;
                break;
            }
            case MENU_BUTTON_USERNAME_BACK: {
                state.mode = MENU_MODE_MAIN;
                break;
            }
            case MENU_BUTTON_USERNAME_OK: {
                if (state.username.empty()) {
                    menu_show_status(state, "Invalid username.");
                    break;
                }

                state.mode = MENU_MODE_MATCHLIST;
                break;
            }
            case MENU_BUTTON_MATCHLIST_BACK: {
                state.mode = MENU_MODE_MAIN;
                break;
            }
            case MENU_BUTTON_HOST: {
                if(!network_server_create(state.username.c_str())) {
                    menu_show_status(state, "Could not create server.");
                    break;
                }

                state.mode = MENU_MODE_LOBBY_HOST;
                break;
            }
            case MENU_BUTTON_JOIN: {
                // Temporary
                network_client_create(state.username.c_str(), "127.0.0.1", BASE_PORT);
                state.mode = MENU_MODE_CONNECTING;
                break;
            }
            case MENU_BUTTON_LOBBY_BACK: {
                network_disconnect();
                state.mode = MENU_MODE_MATCHLIST;
                break;
            }
            case MENU_BUTTON_CONNECTING_BACK: {
                network_disconnect();
                state.mode = MENU_MODE_MATCHLIST;
                break;
            }
            case MENU_BUTTON_LOBBY_READY: {
                network_toggle_ready();
                break;
            }
            default:
                break;
        }

        // Text input pressed
        if (state.button_hovered == MENU_BUTTON_NONE && state.mode == MENU_MODE_USERNAME &&
            sdl_rect_has_point(TEXT_INPUT_RECT, engine.mouse_position)) {
            SDL_SetTextInputRect(&TEXT_INPUT_RECT);
            SDL_StartTextInput();
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
                state.mode = MENU_MODE_MATCHLIST;
                menu_show_status(state, "Failed to connect to server.");
                break;
            }
            case NETWORK_EVENT_LOBBY_FULL: {
                state.mode = MENU_MODE_MAIN;
                menu_show_status(state, "The lobby is full.");
                break;
            }
            case NETWORK_EVENT_INVALID_VERSION: {
                state.mode = MENU_MODE_MAIN;
                menu_show_status(state, "Client version does not match.");
                break;
            }
            case NETWORK_EVENT_JOINED_LOBBY: {
                state.mode = MENU_MODE_LOBBY;
                break;
            }
            case NETWORK_EVENT_PLAYER_DISCONNECTED: {
                if (network_event.player_disconnected.player_id == 0) {
                    network_disconnect();
                    state.mode = MENU_MODE_MATCHLIST;
                }
            }
            default:
                break;
        }
    }

    state.button_hovered = MENU_BUTTON_NONE;
    if (SDL_GetWindowMouseGrab(engine.window) == SDL_TRUE) {
        auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
        if (mode_buttons_it != MODE_BUTTONS.end()) {
            for (MenuButton button : mode_buttons_it->second) {
                if (sdl_rect_has_point(MENU_BUTTON_DATA.at(button).rect, engine.mouse_position)) {
                    state.button_hovered = button;
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

    if (state.mode != MENU_MODE_LOBBY && state.mode != MENU_MODE_LOBBY_HOST && state.mode != MENU_MODE_MATCHLIST) {
        render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_OFFBLACK, xy(24, 24));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_OFFBLACK, xy(4, SCREEN_HEIGHT - 14));

    if (state.status_timer > 0) {
        render_text(FONT_WESTERN8, state.status_text.c_str(), COLOR_RED, xy(48, TEXT_INPUT_RECT.y - 36));
    }

    if (state.mode == MENU_MODE_USERNAME) {
        const char* prompt_text = "USERNAME";
        render_text(FONT_WESTERN8, prompt_text, COLOR_OFFBLACK, xy(TEXT_INPUT_RECT.x + 1, TEXT_INPUT_RECT.y - 13));
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(engine.renderer, &TEXT_INPUT_RECT);
        render_text(FONT_WESTERN16, state.username.c_str(), COLOR_BLACK, xy(TEXT_INPUT_RECT.x + 4, TEXT_INPUT_RECT.y + 31), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (state.mode == MENU_MODE_CONNECTING) {
        render_text(FONT_WESTERN16, "Connecting...", COLOR_OFFBLACK, xy(48, TEXT_INPUT_RECT.y));
    }

    if (state.mode == MENU_MODE_LOBBY || state.mode == MENU_MODE_LOBBY_HOST) {
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

            int line_y = 16 * (player_index + 1);
            render_text(FONT_WESTERN8, player_name_text, COLOR_OFFBLACK, xy(PLAYERLIST_RECT.x + 4, PLAYERLIST_RECT.y + line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            SDL_RenderDrawLine(engine.renderer, PLAYERLIST_RECT.x, PLAYERLIST_RECT.y + line_y, PLAYERLIST_RECT.x + PLAYERLIST_RECT.w - 1, PLAYERLIST_RECT.y + line_y);
            player_index++;
        } // End for each player id

        if (state.mode == MENU_MODE_LOBBY_HOST) {
            render_text(FONT_WESTERN8, "You are the host.", COLOR_OFFBLACK, xy(PLAYERLIST_RECT.x + PLAYERLIST_RECT.w + 2, PLAYERLIST_RECT.y));
        }
    }

    auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
    if (mode_buttons_it != MODE_BUTTONS.end()) {
        for (MenuButton button : mode_buttons_it->second) {
            SDL_Color button_color = button == state.button_hovered ? COLOR_WHITE : COLOR_OFFBLACK;
            SDL_SetRenderDrawColor(engine.renderer, button_color.r, button_color.g, button_color.b, button_color.a);

            const menu_button_t& button_data = MENU_BUTTON_DATA.at(button);
            SDL_RenderDrawRect(engine.renderer, &button_data.rect);
            render_text(FONT_WESTERN16, button_data.text, button_color, xy(button_data.rect.x + 4, button_data.rect.y + 4));
        }
    }
}
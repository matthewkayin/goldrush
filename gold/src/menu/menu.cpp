#include "menu.h"

#include "defines.h"
#include "core/cursor.h"
#include "core/logger.h"
#include "core/input.h"
#include "core/asserts.h"
#include "render/render.h"
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <algorithm>

#define MENU_ITEM_NOT_FOUND -1

static const int WAGON_X_DEFAULT = 380;
static const int WAGON_X_LOBBY = 480;
static const int PARALLAX_TIMER_DURATION = 2;

static const int MENU_TILE_WIDTH = (SCREEN_WIDTH / TILE_SIZE) * 2;
static const int MENU_TILE_HEIGHT = 2;
static const ivec2 MENU_DECORATION_COORDS[3] = { 
    ivec2(680, -8), 
    ivec2(920, 30), 
    ivec2(1250, -8) 
};

static const int CLOUD_COUNT = 6;
static const ivec2 CLOUD_COORDS[CLOUD_COUNT] = { 
    ivec2(640, 16), ivec2(950, 64), 
    ivec2(1250, 48), ivec2(-30, 48), 
    ivec2(320, 32), ivec2(1600, 32) 
};
static const int CLOUD_FRAME_X[CLOUD_COUNT] = { 0, 1, 2, 2, 1, 1};

static const int TEXT_INPUT_BLINK_DURATION = 30;
static const int BUTTON_X = 44;
static const int BUTTON_Y = 128;
static const int BUTTON_Y_DIST = 25;

static const Rect MATCHLIST_RECT = (Rect) {
    .x = 36, .y = 32, .w = 432, .h = 200
};
static const Rect PLAYERLIST_RECT = (Rect) {
    .x = 16, .y = 4, .w = 432, .h = 128
};
static const Rect LOBBY_CHAT_RECT = (Rect) {
    .x = 16, .y = 136, .w = 432, .h = 158
};
static const Rect MATCH_SETTINGS_RECT = (Rect) {
    .x = PLAYERLIST_RECT.x + PLAYERLIST_RECT.w + 4,
    .y = PLAYERLIST_RECT.y,
    .w = 172,
    .h = PLAYERLIST_RECT.h
};

static const uint32_t STATUS_DURATION = 60 * 2;
static const uint32_t MATCHLIST_PAGE_SIZE = 9;

static const int PLAYERLIST_COL_HEADER_Y = PLAYERLIST_RECT.y + 22;
static const int PLAYERLIST_COL_NAME_X = PLAYERLIST_RECT.x + 16;
static const int PLAYERLIST_COL_STATUS_X = PLAYERLIST_RECT.x + 216;
static const int PLAYERLIST_COL_TEAM_X = PLAYERLIST_RECT.x + 280;
static const int PLAYERLIST_COL_COLOR_X = PLAYERLIST_RECT.x + 336;
static const int PLAYERLIST_ROW_HEIGHT = 20;
static const int PLAYERLIST_TEXT_Y_OFFSET = 2;

static const ivec2 TITLE_TEXT_POSITION = ivec2(24, 24);

static const uint32_t CONNECTION_TIMEOUT = 60 * 15;

enum MenuDropdownState {
    MENU_DROPDOWN_CLOSED,
    MENU_DROPDOWN_HOVERED,
    MENU_DROPDOWN_OPEN,
    MENU_DROPDOWN_DISABLED
};

static const std::unordered_map<MenuDropdownName, std::vector<const char*>> MENU_DROPDOWN_VALUE_STRINGS = {
    { MENU_DROPDOWN_PLAYER_COLOR, {
        "Blue", "Red", "Green", "Purple"
    }}
};

void menu_set_mode(MenuState& state, MenuMode mode);
void menu_refresh_items(MenuState& state);
MenuItem menu_create_text(FontName font, const char* text, ivec2 position);
MenuItem menu_create_button(MenuButtonName name, const char* text, ivec2 position);
MenuItem menu_create_matchlist_lobby(size_t lobby_index, int index);
MenuItem menu_create_sprite_button(MenuButtonName name, SpriteName sprite, ivec2 position, bool disabled, bool flip_h);
MenuItem menu_create_team_picker(char value, bool disabled, ivec2 position);
const char* menu_get_player_status_string(NetworkPlayerStatus status);
MenuItem menu_create_dropdown(MenuDropdownName name, int value, bool disabled, ivec2 position);
MenuItem menu_create_dropdown_item(MenuDropdownName name, int value, ivec2 dropdown_position);

size_t menu_get_matchlist_page_count(const MenuState& state);
void menu_handle_button_press(MenuState& state, MenuButtonName button);
void menu_show_status(MenuState& state, const char* status);
void menu_refresh_lobby_search(MenuState& state);
void menu_add_chat_message(MenuState& state, const char* message);

void menu_render_decoration(const MenuState& state, int index);
void menu_render_button(const MenuItem& button, bool hovered);
void menu_render_textbox(const MenuItem& textbox, bool show_cursor);
void menu_render_matchlist_lobby(const MenuItem& item, bool hovered, bool selected);
void menu_render_sprite_button(const MenuItem& item, bool hovered);
void menu_render_team_picker(const MenuItem& item, bool hovered);
void menu_render_dropdown(const MenuItem& item, MenuDropdownState state);
void menu_render_dropdown_item(const MenuItem& item, bool hovered);

MenuState menu_init() {
    MenuState state;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;
    state.text_input_cursor_blink_timer = TEXT_INPUT_BLINK_DURATION;
    state.text_input_show_cursor = false;
    state.connection_timeout = 0;

    state.mode = MENU_MODE_MAIN;
    menu_set_mode(state, state.mode);

    return state;
}

void menu_set_mode(MenuState& state, MenuMode mode) {
    state.status_timer = 0;
    state.item_selected = -1;

    if (input_is_text_input_active()) {
        input_stop_text_input();
    }

    state.mode = mode;
    if (mode == MENU_MODE_USERNAME) {
        input_start_text_input(&state.username, MAX_USERNAME_LENGTH);
    } else if (mode == MENU_MODE_MATCHLIST) {
        if (!network_scanner_create()) {
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "Error occurred while searching for LAN games.");
            return;
        }
        menu_refresh_lobby_search(state);
    } else if (mode == MENU_MODE_CONNECTING) {
        state.connection_timeout = CONNECTION_TIMEOUT;
    }

    menu_refresh_items(state);
}

void menu_refresh_items(MenuState& state) {
    state.menu_items.clear();
    switch (state.mode) {
        case MENU_MODE_MAIN: {
            state.menu_items.push_back(menu_create_text(FONT_WESTERN32_OFFBLACK, "GOLD RUSH", TITLE_TEXT_POSITION));
            state.menu_items.push_back(menu_create_button(MENU_BUTTON_MAIN_PLAY, "PLAY", ivec2(BUTTON_X, BUTTON_Y)));
            state.menu_items.push_back(menu_create_button(MENU_BUTTON_MAIN_EXIT, "EXIT", ivec2(BUTTON_X, BUTTON_Y + BUTTON_Y_DIST)));
            break;
        }
        case MENU_MODE_USERNAME: {
            state.menu_items.push_back(menu_create_text(FONT_WESTERN32_OFFBLACK, "GOLD RUSH", TITLE_TEXT_POSITION));
            state.menu_items.push_back((MenuItem) {
                .type = MENU_ITEM_TEXTBOX,
                .rect = (Rect) { 
                    .x = BUTTON_X - 4, 
                    .y = BUTTON_Y, 
                    .w = 256, 
                    .h = 24 
                },
                .textbox = (MenuItemTextbox) {
                    .prompt = "Username: ",
                    .value = &state.username,
                    .max_length = MAX_USERNAME_LENGTH
                }
            });
            MenuItem back_button = menu_create_button(MENU_BUTTON_USERNAME_BACK, "BACK", ivec2(BUTTON_X, BUTTON_Y + BUTTON_Y_DIST + 4));
            state.menu_items.push_back(back_button);
            state.menu_items.push_back(menu_create_button(MENU_BUTTON_USERNAME_OK, "OK", ivec2(BUTTON_X + back_button.rect.w + 4, BUTTON_Y + BUTTON_Y_DIST + 4)));
            break;
        }
        case MENU_MODE_MATCHLIST: {
            state.menu_items.push_back((MenuItem) {
                .type = MENU_ITEM_FRAME,
                .rect = MATCHLIST_RECT
            });

            if (network_get_lobby_count() == 0) {
                int text_width = render_get_text_size(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...").x;
                state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...", ivec2(MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - (text_width / 2), MATCHLIST_RECT.y + 8)));
            }

            int base_index = (state.matchlist_page * MATCHLIST_PAGE_SIZE);
            for (int lobby_index = base_index; lobby_index < std::min(base_index + MATCHLIST_PAGE_SIZE, (uint32_t)network_get_lobby_count()); lobby_index++) {
                state.menu_items.push_back(menu_create_matchlist_lobby(lobby_index, lobby_index - base_index));
            }

            MenuItem back_button = menu_create_button(MENU_BUTTON_MATCHLIST_BACK, "BACK", ivec2(BUTTON_X, MATCHLIST_RECT.y + MATCHLIST_RECT.h + 4));
            MenuItem host_button = menu_create_button(MENU_BUTTON_MATCHLIST_HOST, "HOST", ivec2(back_button.rect.x + back_button.rect.w + 4, back_button.rect.y));
            MenuItem search_item = (MenuItem) {
                .type = MENU_ITEM_TEXTBOX,
                .rect = (Rect) { 
                    .x = 44, .y = 4, .w = 300, .h = 24 
                },
                .textbox = (MenuItemTextbox) {
                    .prompt = "Search: ",
                    .value = &state.lobby_search_query,
                    .max_length = NETWORK_LOBBY_NAME_BUFFER_SIZE - 1
                }
            };
            MenuItem refresh_item = menu_create_sprite_button(MENU_BUTTON_MATCHLIST_REFRESH, SPRITE_UI_BUTTON_REFRESH, ivec2(search_item.rect.x + search_item.rect.w + 4, search_item.rect.y), false, false);
            MenuItem page_left_item = menu_create_sprite_button(MENU_BUTTON_MATCHLIST_PAGE_LEFT, SPRITE_UI_BUTTON_ARROW, ivec2(refresh_item.rect.x + refresh_item.rect.w + 4, refresh_item.rect.y), state.matchlist_page == 0, true);
            MenuItem page_right_item = menu_create_sprite_button(MENU_BUTTON_MATCHLIST_PAGE_RIGHT, SPRITE_UI_BUTTON_ARROW, ivec2(page_left_item.rect.x + page_left_item.rect.w + 4, refresh_item.rect.y), state.matchlist_page == menu_get_matchlist_page_count(state) - 1, false);

            state.menu_items.push_back(back_button);
            state.menu_items.push_back(host_button);
            state.menu_items.push_back(search_item);
            state.menu_items.push_back(refresh_item);
            state.menu_items.push_back(page_left_item);
            state.menu_items.push_back(page_right_item);
            if (state.item_selected != -1) {
                state.menu_items.push_back(menu_create_button(MENU_BUTTON_MATCHLIST_JOIN, "JOIN", ivec2(host_button.rect.x + host_button.rect.w + 4, host_button.rect.y)));
            }

            break;
        }
        case MENU_MODE_LOBBY: {
            // Playerlist
            state.menu_items.push_back((MenuItem) {
                .type = MENU_ITEM_FRAME,
                .rect = PLAYERLIST_RECT
            });
            int lobby_name_text_width = render_get_text_size(FONT_HACK_GOLD, network_get_lobby_name()).x;
            state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, network_get_lobby_name(), ivec2(PLAYERLIST_RECT.x + (PLAYERLIST_RECT.w / 2) - (lobby_name_text_width / 2), PLAYERLIST_RECT.y + 6)));
            state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, "Name", ivec2(PLAYERLIST_COL_NAME_X, PLAYERLIST_COL_HEADER_Y)));
            state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, "Status", ivec2(PLAYERLIST_COL_STATUS_X, PLAYERLIST_COL_HEADER_Y)));
            state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, "Team", ivec2(PLAYERLIST_COL_TEAM_X, PLAYERLIST_COL_HEADER_Y)));
            state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, "Color", ivec2(PLAYERLIST_COL_COLOR_X, PLAYERLIST_COL_HEADER_Y)));
            uint32_t playerlist_item_index = 0; // used to determine height of a playerlist item
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                const NetworkPlayer& player = network_get_player(player_id);
                if (player.status == NETWORK_PLAYER_STATUS_NONE) {
                    continue;
                }

                int row_y = PLAYERLIST_COL_HEADER_Y + 20 + (PLAYERLIST_ROW_HEIGHT * playerlist_item_index);
                state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, player.name, ivec2(PLAYERLIST_COL_NAME_X, row_y + PLAYERLIST_TEXT_Y_OFFSET)));
                state.menu_items.push_back(menu_create_text(FONT_HACK_GOLD, menu_get_player_status_string((NetworkPlayerStatus)player.status), ivec2(PLAYERLIST_COL_STATUS_X, row_y + PLAYERLIST_TEXT_Y_OFFSET)));
                state.menu_items.push_back(menu_create_team_picker('-', true, ivec2(PLAYERLIST_COL_TEAM_X, row_y)));
                state.menu_items.push_back(menu_create_dropdown(MENU_DROPDOWN_PLAYER_COLOR, player.recolor_id, player_id != network_get_player_id(), ivec2(PLAYERLIST_COL_COLOR_X, row_y)));
                playerlist_item_index++;
            }

            state.menu_items.push_back((MenuItem) {
                .type = MENU_ITEM_FRAME,
                .rect = MATCH_SETTINGS_RECT
            });

            // Chat
            state.menu_items.push_back((MenuItem) {
                .type = MENU_ITEM_FRAME,
                .rect = LOBBY_CHAT_RECT
            });
            state.menu_items.push_back((MenuItem) {
                .type = MENU_ITEM_TEXTBOX,
                .rect = (Rect) { 
                    .x = 16 + 8, 
                    .y = 128 + 158 + 8 + 4, 
                    .w = 432 - 16, 
                    .h = 24 
                },
                .textbox = (MenuItemTextbox) {
                    .prompt = "Chat: ",
                    .value = &state.chat_message,
                    .max_length = MENU_CHAT_MAX_MESSAGE_LENGTH
                }
            });

            MenuItem back_button = menu_create_button(MENU_BUTTON_LOBBY_BACK, "BACK", ivec2(LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12, PLAYERLIST_RECT.y + PLAYERLIST_RECT.h + 4));
            state.menu_items.push_back(back_button);
            if (network_is_server()) {
                state.menu_items.push_back(menu_create_button(MENU_BUTTON_LOBBY_START, "START", ivec2(back_button.rect.x + back_button.rect.w + 4, back_button.rect.y)));
            } else {
                state.menu_items.push_back(menu_create_button(MENU_BUTTON_LOBBY_READY, "READY", ivec2(back_button.rect.x + back_button.rect.w + 4, back_button.rect.y)));
            }

            // Dropdown items, always added last so that they render on top
            if (state.item_selected != -1 && state.menu_items[state.item_selected].type == MENU_ITEM_DROPDOWN) {
                const MenuItem& item = state.menu_items[state.item_selected];
                for (int item_index = 0; item_index < MENU_DROPDOWN_VALUE_STRINGS.at(item.dropdown.name).size(); item_index++) {
                    state.menu_items.push_back(menu_create_dropdown_item(item.dropdown.name, item_index, ivec2(item.rect.x, item.rect.y)));
                }
            }

            break;
        }
        default:
            break;
    }
}

MenuItem menu_create_button(MenuButtonName name, const char* text, ivec2 position) {
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, text);
    if (text_size.x % 8 != 0) {
        text_size.x = ((text_size.x / 8) + 1) * 8;
    }
    text_size.x += 16;

    return (MenuItem) {
        .type = MENU_ITEM_BUTTON,
        .rect = (Rect) { 
            .x = position.x, .y = position.y,
            .w = text_size.x, .h = 21
        },
        .button = (MenuItemButton) {
            .name = name,
            .text = text,
        }
    };
}

MenuItem menu_create_matchlist_lobby(size_t lobby_index, int index) {
    const NetworkLobby& lobby = network_get_lobby(lobby_index);
    char network_lobby_text[64];
    sprintf(network_lobby_text, "* %s (4/4) *", lobby.name);
    ivec2 network_lobby_text_size = render_get_text_size(FONT_HACK_OFFBLACK, network_lobby_text);
    int frame_width = (network_lobby_text_size.x / 15) + 1;
    if (network_lobby_text_size.x % 15 != 0) {
        frame_width++;
    }

    return (MenuItem) {
        .type = MENU_ITEM_LOBBY,
        .rect = (Rect) { 
            .x = MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - ((frame_width * 15) / 2),
            .y = MATCHLIST_RECT.y + (20 * index) + 12,
            .w = frame_width * 15,
            .h = 15
        },
        .lobby = (MenuItemLobby) {
            .index = lobby_index
        }
    };
}

MenuItem menu_create_sprite_button(MenuButtonName name, SpriteName sprite, ivec2 position, bool disabled, bool flip_h) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(sprite);

    return (MenuItem) {
        .type = MENU_ITEM_SPRITE_BUTTON,
        .rect = (Rect) {
            .x = position.x, .y = position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        },
        .sprite = (MenuItemSprite) {
            .name = name,
            .sprite = sprite,
            .disabled = disabled,
            .flip_h = flip_h
        }
    };
}

MenuItem menu_create_team_picker(char value, bool disabled, ivec2 position) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(SPRITE_UI_TEAM_PICKER);
    return (MenuItem) {
        .type = MENU_ITEM_TEAM_PICKER,
        .rect = (Rect) {
            .x = position.x, .y = position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        },
        .team = (MenuItemTeamPicker) {
            .value = value,
            .disabled = disabled
        }
    };
}

MenuItem menu_create_text(FontName font, const char* text, ivec2 position) {
    return (MenuItem) {
        .type = MENU_ITEM_TEXT,
        .rect = (Rect) {
            .x = position.x, .y = position.y,
            .w = 0, .h = 0
        },
        .text = (MenuItemText) {
            .font = font,
            .text = text
        }
    };
}

const char* menu_get_player_status_string(NetworkPlayerStatus status) {
    switch (status) {
        case NETWORK_PLAYER_STATUS_READY:
            return "READY";
        case NETWORK_PLAYER_STATUS_NOT_READY:
            return "NOT READY";
        case NETWORK_PLAYER_STATUS_HOST:
            return "HOST";
        default:
            return "";
    }
}

MenuItem menu_create_dropdown(MenuDropdownName name, int value, bool disabled, ivec2 position) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    return (MenuItem) {
        .type = MENU_ITEM_DROPDOWN,
        .rect = (Rect) {
            .x = position.x, .y = position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        },
        .dropdown = (MenuItemDropdown) {
            .name = name,
            .value = value,
            .disabled = disabled
        }
    };
}

MenuItem menu_create_dropdown_item(MenuDropdownName name, int value, ivec2 dropdown_position) {
    const SpriteInfo& sprite_info = resource_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    return (MenuItem) {
        .type = MENU_ITEM_DROPDOWN_ITEM,
        .rect = (Rect) {
            .x = dropdown_position.x,
            .y = dropdown_position.y + (sprite_info.frame_height * (value + 1)),
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        },
        .dropdown_item = (MenuItemDropdownItem) {
            .name = name,
            .value = value
        }
    };
}

void menu_handle_network_event(MenuState& state, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_RECEIVED_LOBBY_INFO: {
            log_trace("received lobby info event");
            menu_refresh_items(state);
            break;
        }
        case NETWORK_EVENT_CONNECTION_FAILED: {
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            menu_show_status(state, "Connection failed.");
            break;
        }
        case NETWORK_EVENT_INVALID_VERSION: {
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            menu_show_status(state, "Game version does not match server.");
            break;
        }
        case NETWORK_EVENT_JOINED_LOBBY: {
            menu_set_mode(state, MENU_MODE_LOBBY);
            break;
        }
        case NETWORK_EVENT_LOBBY_CHAT: {
            menu_add_chat_message(state, event.lobby_chat.message);
            return;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            if (state.mode == MENU_MODE_LOBBY) {
                menu_refresh_items(state);
            }
            break;
        }
        case NETWORK_EVENT_NEW_PLAYER_CONNECTED: {
            if (state.mode == MENU_MODE_LOBBY) {
                menu_refresh_items(state);
            }
            break;
        }
        default:
            return;
    }
}

void menu_update(MenuState& state) {
    cursor_set(CURSOR_DEFAULT);

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

    state.text_input_cursor_blink_timer--;
    if (state.text_input_cursor_blink_timer == 0) {
        state.text_input_cursor_blink_timer = TEXT_INPUT_BLINK_DURATION;
        state.text_input_show_cursor = !state.text_input_show_cursor;
    }

    if (state.status_timer > 0) {
        state.status_timer--;
    }
    if (state.mode == MENU_MODE_CONNECTING && state.connection_timeout > 0) {
        state.connection_timeout--;
        if (state.connection_timeout == 0) {
            network_disconnect();
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            menu_show_status(state, "Connection timed out.");
        }
    }

    if (input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
        if (input_is_text_input_active()) {
            input_stop_text_input();
        }
        for (int index = 0; index < state.menu_items.size(); index++) {
            // Only select dropdown items when a dropdown is open
            if (state.item_selected != -1 && state.menu_items[state.item_selected].type == MENU_ITEM_DROPDOWN && state.menu_items[index].type != MENU_ITEM_DROPDOWN_ITEM) {
                continue;
            }
            if (!state.menu_items[index].rect.has_point(input_get_mouse_position())) {
                continue;
            }
            switch (state.menu_items[index].type) {
                case MENU_ITEM_BUTTON: {
                    menu_handle_button_press(state, state.menu_items[index].button.name);
                    return;
                }
                case MENU_ITEM_SPRITE_BUTTON: {
                    menu_handle_button_press(state, state.menu_items[index].sprite.name);
                    return;
                }
                case MENU_ITEM_TEXTBOX: {
                    input_start_text_input(state.menu_items[index].textbox.value, state.menu_items[index].textbox.max_length);
                    return;
                }
                case MENU_ITEM_LOBBY: {
                    state.item_selected = index;
                    menu_refresh_items(state);
                    return;
                }
                case MENU_ITEM_DROPDOWN: {
                    if (!state.menu_items[index].dropdown.disabled) {
                        state.item_selected = index;
                        menu_refresh_items(state);
                        return;
                    }
                    break;
                }
                case MENU_ITEM_DROPDOWN_ITEM: {
                    state.item_selected = -1;
                    menu_refresh_items(state);
                    return;
                }
                default:
                    break;
            }
        }
        // If a dropdown menu is open and we clicked a non-item, just close the dropdown 
        if (state.item_selected != -1 && state.menu_items[state.item_selected].type == MENU_ITEM_DROPDOWN) {
            state.item_selected = -1;
            menu_refresh_items(state);
            return;
        }
    }

    if (input_is_text_input_active() && input_is_action_just_pressed(INPUT_ENTER)) {
        if (state.mode == MENU_MODE_USERNAME) {
            menu_handle_button_press(state, MENU_BUTTON_USERNAME_OK);
        } else if (state.mode == MENU_MODE_MATCHLIST) {
            menu_refresh_lobby_search(state);
        }
    }
}

void menu_handle_button_press(MenuState& state, MenuButtonName button) {
    switch (button) {
        case MENU_BUTTON_MAIN_PLAY: {
            menu_set_mode(state, MENU_MODE_USERNAME);
            return;
        }
        case MENU_BUTTON_MAIN_EXIT: {
            menu_set_mode(state, MENU_MODE_EXIT);
            return;
        }
        case MENU_BUTTON_USERNAME_BACK: {
            menu_set_mode(state, MENU_MODE_MAIN);
            return;
        }
        case MENU_BUTTON_USERNAME_OK: {
            if (state.username.length() == 0) {
                menu_show_status(state, "Please enter a username.");
                return;
            }
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            return;
        }
        case MENU_BUTTON_MATCHLIST_BACK: {
            menu_set_mode(state, MENU_MODE_USERNAME);
            return;
        }
        case MENU_BUTTON_MATCHLIST_HOST: {
            if (!network_server_create(state.username.c_str())) {
                menu_show_status(state, "Could not create server.");
                return;
            }
            menu_set_mode(state, MENU_MODE_LOBBY);
            return;
        }
        case MENU_BUTTON_MATCHLIST_JOIN: {
            const NetworkLobby& lobby = network_get_lobby(state.menu_items[state.item_selected].lobby.index);
            if (!network_client_create(state.username.c_str(), lobby.ip, lobby.port)) {
                menu_show_status(state, "Could not create client.");
                return;
            }
            menu_set_mode(state, MENU_MODE_CONNECTING);
            return;
        }
        case MENU_BUTTON_MATCHLIST_REFRESH: {
            menu_refresh_lobby_search(state);
            return;
        }
        case MENU_BUTTON_MATCHLIST_PAGE_LEFT: {
            state.matchlist_page--;
            state.item_selected = -1;
            menu_refresh_items(state);
            return;
        }
        case MENU_BUTTON_MATCHLIST_PAGE_RIGHT: {
            state.matchlist_page++;
            state.item_selected = -1;
            menu_refresh_items(state);
            return;
        }
        case MENU_BUTTON_LOBBY_BACK: {
            network_disconnect();
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            return;
        }
        case MENU_BUTTON_LOBBY_READY: {
            return;
        }
        case MENU_BUTTON_LOBBY_START: {
            return;
        }
    } // End switch button
}

void menu_refresh_lobby_search(MenuState& state) {
    network_scanner_search(state.lobby_search_query.c_str());

    state.matchlist_page = 0;
    state.item_selected = -1;
    menu_refresh_items(state);
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

        if (next.length() + line.length() > MENU_CHAT_MAX_LINE_LENGTH) {
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

void menu_add_chat_message(MenuState& state, const char* message) {
    std::vector<std::string> lines = menu_split_chat_message(std::string(message));
    while (!lines.empty()) {
        if (state.chat.size() == MENU_CHAT_MAX_LINE_COUNT) {
            state.chat.erase(state.chat.begin());
        }
        state.chat.push_back(lines[0]);
        lines.erase(lines.begin());
    }
}

size_t menu_get_matchlist_page_count(const MenuState& state) {
    if (network_get_lobby_count() == 0) {
        return 1;
    }
    return (network_get_lobby_count() / MATCHLIST_PAGE_SIZE) + (int)(network_get_lobby_count() % MATCHLIST_PAGE_SIZE != 0);
}

void menu_render(const MenuState& state) {
    // Sky background
    const SpriteInfo& sky_sprite_info = resource_get_sprite_info(SPRITE_UI_SKY);
    Rect sky_src_rect = (Rect) { 
        .x = sky_sprite_info.atlas_x, 
        .y = sky_sprite_info.atlas_y, 
        .w = sky_sprite_info.frame_width, 
        .h = sky_sprite_info.frame_height 
    };
    render_atlas(sky_sprite_info.atlas, sky_src_rect, (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT }, RENDER_SPRITE_NO_CULL);

    // Tiles
    for (int y = 0; y < MENU_TILE_HEIGHT; y++) {
        for (int x = 0; x < MENU_TILE_WIDTH; x++) {
            SpriteName sprite = (x + y) % 10 == 0 ? SPRITE_TILE_SAND2 : SPRITE_TILE_SAND;
            const SpriteInfo& sprite_info = resource_get_sprite_info(sprite);
            Rect src_rect = (Rect) {
                .x = sprite_info.atlas_x,
                .y = sprite_info.atlas_y,
                .w = sprite_info.frame_width,
                .h = sprite_info.frame_height
            };
            Rect dst_rect = (Rect) {
                .x = (x * TILE_SIZE * 2) - state.parallax_x,
                .y = SCREEN_HEIGHT - ((MENU_TILE_HEIGHT - y) * TILE_SIZE * 2),
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            render_atlas(sprite_info.atlas, src_rect, dst_rect, 0);
        }
    }

    menu_render_decoration(state, 0);
    menu_render_decoration(state, 2);

    // Wagon animation
    const SpriteInfo& sprite_wagon_info = resource_get_sprite_info(SPRITE_UNIT_WAGON);
    // Wagon color defaults to blue
    int wagon_recolor_id = 0;
    if (state.mode == MENU_MODE_LOBBY) { 
        // If we are in lobby, wagon color is player color
        wagon_recolor_id = network_get_player(network_get_player_id()).recolor_id;
        // If player is opening wagon dropdown and hovering a dropdown item, use the hovered color
        // This code works because the dropdown items will only exist if they have the dropdown open in the first place
        for (const MenuItem& item : state.menu_items) {
            if (item.type == MENU_ITEM_DROPDOWN_ITEM && item.dropdown_item.name == MENU_DROPDOWN_PLAYER_COLOR && item.rect.has_point(input_get_mouse_position())) {
                wagon_recolor_id = item.dropdown_item.value;
            }
        }
    }
    Rect wagon_src_rect = (Rect) {
        .x = sprite_wagon_info.atlas_x + (sprite_wagon_info.frame_width * state.wagon_animation.frame.x),
        .y = sprite_wagon_info.atlas_y + (render_get_sprite_image_size(ATLAS_UNIT_WAGON).y * wagon_recolor_id) + (sprite_wagon_info.frame_height * 2),
        .w = sprite_wagon_info.frame_width,
        .h = sprite_wagon_info.frame_height
    };
    Rect wagon_dst_rect = (Rect) {
        .x = state.wagon_x,
        .y = SCREEN_HEIGHT - 88,
        .w = wagon_src_rect.w * 2,
        .h = wagon_src_rect.h * 2
    };
    render_atlas(sprite_wagon_info.atlas, wagon_src_rect, wagon_dst_rect, RENDER_SPRITE_NO_CULL);

    menu_render_decoration(state, 1);

    // Render clouds
    const SpriteInfo& cloud_sprite_info = resource_get_sprite_info(SPRITE_UI_CLOUDS);
    for (int index = 0; index < CLOUD_COUNT; index++) {
        Rect src_rect = (Rect) {
            .x = cloud_sprite_info.atlas_x + (CLOUD_FRAME_X[index] * cloud_sprite_info.frame_width),
            .y = cloud_sprite_info.atlas_y,
            .w = cloud_sprite_info.frame_width,
            .h = cloud_sprite_info.frame_height
        };
        Rect dst_rect = (Rect) {
            .x = CLOUD_COORDS[index].x - state.parallax_cloud_x,
            .y = CLOUD_COORDS[index].y,
            .w = src_rect.w * 2,
            .h = src_rect.h * 2
        };
        render_atlas(cloud_sprite_info.atlas, src_rect, dst_rect, 0);
    }

    /*
        for (int chat_index = 0; chat_index < state.chat.size(); chat_index++) {
            render_text(FONT_HACK_GOLD, state.chat[chat_index].c_str(), ivec2(LOBBY_CHAT_RECT.x + 16, LOBBY_CHAT_RECT.y - 5 + (12 * chat_index)));
        }
        */

    // Render menu items
    bool is_dropdown_open = state.item_selected != -1 && state.menu_items[state.item_selected].type == MENU_ITEM_DROPDOWN;
    for (int index = 0; index < state.menu_items.size(); index++) {
        const MenuItem& item = state.menu_items[index];
        switch (item.type) {
            case MENU_ITEM_BUTTON:
                menu_render_button(item, !is_dropdown_open && item.rect.has_point(input_get_mouse_position()));
                break;
            case MENU_ITEM_TEXTBOX:
                menu_render_textbox(item, state.text_input_show_cursor);
                break;
            case MENU_ITEM_LOBBY:
                menu_render_matchlist_lobby(item, item.rect.has_point(input_get_mouse_position()), index == state.item_selected);
                break;
            case MENU_ITEM_SPRITE_BUTTON:
                menu_render_sprite_button(item, item.rect.has_point(input_get_mouse_position()));
                break;
            case MENU_ITEM_TEXT:
                render_text(item.text.font, item.text.text, ivec2(item.rect.x, item.rect.y));
                break;
            case MENU_ITEM_FRAME:
                render_ninepatch(SPRITE_UI_FRAME, item.rect);
                break;
            case MENU_ITEM_TEAM_PICKER:
                menu_render_team_picker(item, !is_dropdown_open && !item.team.disabled && item.rect.has_point(input_get_mouse_position()));
                break;
            case MENU_ITEM_DROPDOWN: {
                MenuDropdownState dropdown_state = MENU_DROPDOWN_CLOSED;
                if (item.dropdown.disabled) {
                    dropdown_state = MENU_DROPDOWN_DISABLED;
                } else if (state.item_selected == index) {
                    dropdown_state = MENU_DROPDOWN_OPEN;
                } else if (!is_dropdown_open && item.rect.has_point(input_get_mouse_position())) {
                    dropdown_state = MENU_DROPDOWN_HOVERED;
                }
                menu_render_dropdown(item, dropdown_state);
                break;
            }
            case MENU_ITEM_DROPDOWN_ITEM: {
                menu_render_dropdown_item(item, item.rect.has_point(input_get_mouse_position()));
                break;
            }
        }
    }

    // Render version
    char version_text[32];
    sprintf(version_text, "Version %s", APP_VERSION);
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, version_text);
    render_text(FONT_WESTERN8_OFFBLACK, version_text, ivec2(4, SCREEN_HEIGHT - text_size.y - 4));

    // Status text
    if (state.status_timer != 0) {
        ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, state.status_text);
        int rect_width = text_size.x + 32;
        render_ninepatch(SPRITE_UI_FRAME, (Rect) { .x = (SCREEN_WIDTH / 2) - (rect_width / 2), .y = 80, .w = rect_width, .h = 32 });
        render_text(FONT_HACK_GOLD, state.status_text, ivec2((SCREEN_WIDTH / 2) - (text_size.x / 2), 80 + 16 - (text_size.y / 2)));
    }
    if (state.mode == MENU_MODE_CONNECTING) {
        ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, "Connecting...");
        int rect_width = text_size.x + 32;
        render_ninepatch(SPRITE_UI_FRAME, (Rect) { .x = (SCREEN_WIDTH / 2) - (rect_width / 2), .y = 80, .w = rect_width, .h = 32 });
        render_text(FONT_HACK_GOLD, "Connecting...", ivec2((SCREEN_WIDTH / 2) - (text_size.x / 2), 80 + 16 - (text_size.y / 2)));
    }
}

void menu_show_status(MenuState& state, const char* status) {
    strcpy(state.status_text, status);
    state.status_timer = STATUS_DURATION;
}

void menu_render_decoration(const MenuState& state, int index) {
    int cactus_index = ((index * 2) + state.parallax_cactus_offset) % 5;
    if (cactus_index == 2) {
        cactus_index = 0;
    }
    SpriteName sprite = (SpriteName)(SPRITE_TILE_DECORATION0 + cactus_index);
    const SpriteInfo& sprite_info = resource_get_sprite_info(sprite);
    Rect src_rect = (Rect) {
        .x = sprite_info.atlas_x,
        .y = sprite_info.atlas_y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = MENU_DECORATION_COORDS[index].x - state.parallax_x,
        .y = SCREEN_HEIGHT - (MENU_TILE_HEIGHT * TILE_SIZE * 2) + MENU_DECORATION_COORDS[index].y,
        .w = TILE_SIZE * 2,
        .h = TILE_SIZE * 2
    };
    render_atlas(sprite_info.atlas, src_rect, dst_rect, 0);
}

void menu_render_button(const MenuItem& button, bool hovered) {
    // Draw button parchment
    int frame_count = button.rect.w / 8;
    for (int frame = 0; frame < frame_count; frame++) {
        // Determine hframe
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        render_sprite(SPRITE_UI_MENU_BUTTON, ivec2(hframe, (int)hovered), ivec2(button.rect.x + (frame * 8), button.rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
    }

    render_text(hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, button.button.text, ivec2(button.rect.x + 5, button.rect.y + 3 - (int)hovered));
}

void menu_render_textbox(const MenuItem& textbox, bool show_cursor) {
    render_ninepatch(SPRITE_UI_FRAME_SMALL, textbox.rect);
    render_text(FONT_HACK_GOLD, textbox.textbox.prompt, ivec2(textbox.rect.x + 5, textbox.rect.y + 6));
    ivec2 prompt_size = render_get_text_size(FONT_HACK_GOLD, textbox.textbox.prompt);
    render_text(FONT_HACK_GOLD, textbox.textbox.value->c_str(), ivec2(textbox.rect.x + prompt_size.x, textbox.rect.y + 6));
    if (show_cursor && input_is_text_input_active()) {
        int value_text_width = render_get_text_size(FONT_HACK_GOLD, textbox.textbox.value->c_str()).x;
        ivec2 cursor_pos = ivec2(textbox.rect.x + prompt_size.x + value_text_width - 2, textbox.rect.y + 5);
        render_text(FONT_HACK_GOLD, "|", cursor_pos);
    }
}

void menu_render_matchlist_lobby(const MenuItem& item, bool hovered, bool selected) {
    const NetworkLobby& lobby = network_get_lobby(item.lobby.index);
    char network_lobby_text[64];
    sprintf(network_lobby_text, "%s %s (%u/%u) %s", selected ? "*" : "", lobby.name, lobby.player_count, MAX_PLAYERS, selected ? "*" : "");

    const int frame_count = item.rect.w / 15;
    for (int frame = 0; frame < frame_count; frame++) {
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        render_sprite(SPRITE_UI_TEXT_FRAME, ivec2(hframe, 0), ivec2(item.rect.x + (frame * 15), item.rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
    }

    ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, network_lobby_text);
    render_text(hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, network_lobby_text, ivec2(item.rect.x + (item.rect.w / 2) - (text_size.x / 2), item.rect.y + 1 -(int)hovered));
}

void menu_render_sprite_button(const MenuItem& item, bool hovered) {
    int hframe = 0;
    if (item.sprite.disabled) {
        hframe = 2;
    } else if (hovered) {
        hframe = 1;
    }
    uint32_t options = RENDER_SPRITE_NO_CULL;
    if (item.sprite.flip_h) {
        options |= RENDER_SPRITE_FLIP_H;
    }
    render_sprite(item.sprite.sprite, ivec2(hframe, 0), ivec2(item.rect.x, item.rect.y), options, 0);
}

void menu_render_team_picker(const MenuItem& item, bool hovered) {
    render_sprite(SPRITE_UI_TEAM_PICKER, ivec2(hovered ? 1 : 0, 0), ivec2(item.rect.x, item.rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
    char text[2] = { item.team.value, '\0' };
    render_text(hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, text, ivec2(item.rect.x + 5, item.rect.y + PLAYERLIST_TEXT_Y_OFFSET - (int)hovered));
}

void menu_render_dropdown(const MenuItem& item, MenuDropdownState state) {
    render_sprite(SPRITE_UI_DROPDOWN_MINI, ivec2(0, state), ivec2(item.rect.x, item.rect.y), RENDER_SPRITE_NO_CULL, 0);
    render_text(state == MENU_DROPDOWN_HOVERED ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, MENU_DROPDOWN_VALUE_STRINGS.at(item.dropdown.name)[item.dropdown.value], ivec2(item.rect.x + 5, item.rect.y + PLAYERLIST_TEXT_Y_OFFSET));
}

void menu_render_dropdown_item(const MenuItem& item, bool hovered) {
    render_sprite(SPRITE_UI_DROPDOWN_MINI, ivec2(0, 3 + (int)hovered), ivec2(item.rect.x, item.rect.y), RENDER_SPRITE_NO_CULL, 0);
    render_text(hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, MENU_DROPDOWN_VALUE_STRINGS.at(item.dropdown_item.name)[item.dropdown_item.value], ivec2(item.rect.x + 5, item.rect.y + PLAYERLIST_TEXT_Y_OFFSET));
}
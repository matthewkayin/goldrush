#include "menu.h"

#include "core/input.h"
#include "core/cursor.h"
#include "core/logger.h"
#include "render/sprite.h"
#include "render/font.h"
#include "render/render.h"
#include "menu/ui.h"

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

static const int BUTTON_X = 44;
static const int BUTTON_Y = 128;

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

static const int PLAYERLIST_COLUMN_X = PLAYERLIST_RECT.x + 16;
static const int PLAYERLIST_COLUMN_Y = PLAYERLIST_RECT.y + 22;
static const int PLAYERLIST_COLUMN_NAME_WIDTH = 200;
static const int PLAYERLIST_COLUMN_STATUS_WIDTH = 64;
static const int PLAYERLIST_COLUMN_TEAM_WIDTH = 56;
static const int PLAYERLIST_ROW_HEIGHT = 20;

static const uint32_t STATUS_DURATION = 2 * 60; 
static const uint32_t CONNECTION_TIMEOUT = 5 * 60;
static const uint32_t MATCHLIST_PAGE_SIZE = 9;

// defined below menu_update()
void menu_set_mode(MenuState& state, MenuMode mode);
void menu_show_status(MenuState& state, const char* message);
void menu_add_chat_message(MenuState& state, const char* message);
size_t menu_get_matchlist_page_count();
const char* menu_get_player_status_string(NetworkPlayerStatus status);

// defined below menu_render()
void menu_render_decoration(const MenuState& state, int index);

MenuState menu_init() {
    MenuState state;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;
    state.connection_timeout = 0;
    state.matchlist_page = 0;
    state.matchlist_item_selected = MENU_ITEM_NONE;

    state.mode = MENU_MODE_MAIN;
    menu_set_mode(state, state.mode);

    return state;
}

void menu_handle_network_event(MenuState& state, NetworkEvent event) {
    switch (event.type) {
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
            if (state.mode == MENU_MODE_LOBBY) {
                menu_add_chat_message(state, event.lobby_chat.message);
            }
            return;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            if (event.player_disconnected.player_id == 0) {
                network_disconnect();
                menu_set_mode(state, MENU_MODE_MATCHLIST);
                menu_show_status(state, "The host closed the lobby.");
            } else {
                char message[128];
                sprintf(message, "%s left the lobby.", network_get_player(event.player_disconnected.player_id).name);
                menu_add_chat_message(state, message);
            }
            return;
        }
        case NETWORK_EVENT_PLAYER_CONNECTED: {
            char message[128];
            sprintf(message, "%s joined the lobby.", network_get_player(event.player_connected.player_id).name);
            menu_add_chat_message(state, message);
            network_send_lobby_chat(message);
            return;
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

    ui_begin();

    if (state.mode == MENU_MODE_MAIN || state.mode == MENU_MODE_USERNAME) {
        ui_element_position(ivec2(24, 24));
        ui_text(FONT_WESTERN32_OFFBLACK, "GOLD RUSH");
    }

    if (state.mode == MENU_MODE_MAIN) {
        ui_begin_column(ivec2(BUTTON_X, BUTTON_Y), 4);
            if (ui_button("PLAY")) {
                menu_set_mode(state, MENU_MODE_USERNAME);
            }
            if (ui_button("EXIT")) {
                menu_set_mode(state, MENU_MODE_EXIT);
            }
        ui_end_container();
    } else if (state.mode == MENU_MODE_USERNAME) {
        ui_element_position(ivec2(BUTTON_X - 4, BUTTON_Y));
        ui_text_input("Username: ", ivec2(256, 24), &state.username, MAX_USERNAME_LENGTH);

        ui_begin_row(ivec2(BUTTON_X, BUTTON_Y + 29), 4);
            if (ui_button("BACK")) {
                menu_set_mode(state, MENU_MODE_MAIN);
            }
            if (ui_button("OK")) {
                if (state.username.length() == 0) {
                    menu_show_status(state, "Please enter a username.");
                } else {
                    menu_set_mode(state, MENU_MODE_MATCHLIST);
                }
            }
        ui_end_container();
    } else if (state.mode == MENU_MODE_MATCHLIST) {
        ui_frame_rect(MATCHLIST_RECT);

        if (network_get_lobby_count() == 0) {
            int text_width = render_get_text_size(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...").x;
            ui_element_position(ivec2(MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - (text_width / 2), MATCHLIST_RECT.y + 8));
            ui_text(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...");
        }

        // Matchlist search and top buttons
        ui_begin_row(ivec2(44, 4), 4);
            ui_text_input("Search: ", ivec2(300, 24), &state.lobby_search_query, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);
            if (ui_sprite_button(SPRITE_UI_BUTTON_REFRESH, false, false)) {
                network_scanner_search(state.lobby_search_query.c_str());
                state.matchlist_page = 0;
            }
            if (ui_sprite_button(SPRITE_UI_BUTTON_ARROW, state.matchlist_page == 0, true)) {
                state.matchlist_page--;
            }
            if (ui_sprite_button(SPRITE_UI_BUTTON_ARROW, state.matchlist_page == menu_get_matchlist_page_count() - 1, false)) {
                state.matchlist_page++;
            }
        ui_end_container();

        // Matchlist lobby list
        int base_index = (state.matchlist_page * MATCHLIST_PAGE_SIZE);
        for (int lobby_index = base_index; lobby_index < std::min(base_index + MATCHLIST_PAGE_SIZE, (uint32_t)network_get_lobby_count()); lobby_index++) {
            char lobby_text[128];
            bool selected = lobby_index == state.matchlist_item_selected;
            const NetworkLobby& lobby = network_get_lobby(lobby_index);
            sprintf(lobby_text, "%s %s (%u/%u) %s", selected ? "*" : " ", lobby.name, lobby.player_count, MAX_PLAYERS, selected ? "*" : " ");

            ivec2 text_frame_size = ui_text_frame_size(lobby_text);
            ui_element_position(ivec2(MATCHLIST_RECT.x + (MATCHLIST_RECT.w / 2) - (text_frame_size.x / 2), MATCHLIST_RECT.y + 12 + (20 * (lobby_index - base_index))));
            if (ui_text_frame(lobby_text, false)) {
                state.matchlist_item_selected = lobby_index;
            }
        }

        // Matchlist button row
        ui_begin_row(ivec2(BUTTON_X, MATCHLIST_RECT.y + MATCHLIST_RECT.h + 4), 4); 
            if (ui_button("BACK")) {
                menu_set_mode(state, MENU_MODE_USERNAME);
            }
            if (ui_button("HOST")) {
                if (!network_server_create(state.username.c_str())) {
                    menu_show_status(state, "Could not create server.");
                } else {
                    menu_set_mode(state, MENU_MODE_LOBBY);
                }
            }
            if (state.matchlist_item_selected != MENU_ITEM_NONE) {
                if (ui_button("JOIN")) {
                    const NetworkLobby& lobby = network_get_lobby(state.matchlist_item_selected);
                    if (!network_client_create(state.username.c_str(), lobby.ip, lobby.port)) {
                        menu_show_status(state, "Could not create client.");
                    } else {
                        menu_set_mode(state, MENU_MODE_CONNECTING);
                    }
                }
            }
        ui_end_container();
    // End matchlist
    } else if (state.mode == MENU_MODE_LOBBY) {
        // Playerlist
        ui_frame_rect(PLAYERLIST_RECT);
        ivec2 lobby_name_text_size = render_get_text_size(FONT_HACK_GOLD, network_get_lobby_name());
        ui_element_position(ivec2(PLAYERLIST_RECT.x + (PLAYERLIST_RECT.w / 2) - (lobby_name_text_size.x / 2), PLAYERLIST_RECT.y + 6));
        ui_text(FONT_HACK_GOLD, network_get_lobby_name());

        ui_begin_column(ivec2(PLAYERLIST_COLUMN_X, PLAYERLIST_COLUMN_Y), 0);
            // Header row
            ui_element_size(ivec2(0, PLAYERLIST_ROW_HEIGHT));
            ui_begin_row(ivec2(0, 0), 0);
                ui_element_size(ivec2(PLAYERLIST_COLUMN_NAME_WIDTH, 0));
                ui_text(FONT_HACK_GOLD, "Name");

                ui_element_size(ivec2(PLAYERLIST_COLUMN_STATUS_WIDTH, 0));
                ui_text(FONT_HACK_GOLD, "Status");

                ui_element_size(ivec2(PLAYERLIST_COLUMN_TEAM_WIDTH, 0));
                ui_text(FONT_HACK_GOLD, "Team");

                ui_text(FONT_HACK_GOLD, "Color");
            ui_end_container();

            // Player rows
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                const NetworkPlayer& player = network_get_player(player_id);
                if (player.status == NETWORK_PLAYER_STATUS_NONE) {
                    continue;
                }

                ui_element_size(ivec2(0, PLAYERLIST_ROW_HEIGHT));
                ui_begin_row(ivec2(0, 0), 0);
                    ui_element_size(ivec2(PLAYERLIST_COLUMN_NAME_WIDTH, 0));
                    ui_element_position(ivec2(0, 2));
                    ui_text(FONT_HACK_GOLD, player.name);

                    ui_element_size(ivec2(PLAYERLIST_COLUMN_STATUS_WIDTH, 0));
                    ui_element_position(ivec2(0, 2));
                    ui_text(FONT_HACK_GOLD, menu_get_player_status_string((NetworkPlayerStatus)player.status));

                    ui_element_size(ivec2(PLAYERLIST_COLUMN_TEAM_WIDTH, 0));
                    ui_team_picker('-', true);

                    uint32_t player_color = player.recolor_id;
                    const char* items[] = { "Blue", "Red", "Green", "Purple" };
                    if (ui_dropdown(player_id, &player_color, items, MAX_PLAYERS, player_id != network_get_player_id())) {
                        network_set_player_color((uint8_t)player_color);
                    }
                ui_end_container();
            }
        ui_end_container();
        // End playerlist

        ui_frame_rect(MATCH_SETTINGS_RECT);

        // Chat
        ui_frame_rect(LOBBY_CHAT_RECT);
        // Chat messages
        ui_begin_column(ivec2(LOBBY_CHAT_RECT.x + 16, LOBBY_CHAT_RECT.y + 8), 0);
            for (const std::string& message : state.chat) {
                ui_text(FONT_HACK_GOLD, message.c_str());
            }
        ui_end_container();

        // Chat input
        ui_element_position(ivec2(24, 128 + 158 + 8 + 4));
        ui_text_input("Chat: ", ivec2(432 - 16, 24), &state.chat_message, MENU_CHAT_MAX_MESSAGE_LENGTH);
        if (input_is_action_just_pressed(INPUT_ENTER) && input_is_text_input_active()) {
            char chat_message[128];
            sprintf(chat_message, "%s: %s", network_get_player(network_get_player_id()).name, state.chat_message.c_str());
            menu_add_chat_message(state, chat_message);
            network_send_lobby_chat(chat_message);
            state.chat_message = "";
        }

        // Lobby buttons
        ui_begin_row(ivec2(LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12, MATCH_SETTINGS_RECT.y + MATCH_SETTINGS_RECT.h + 4), 4);
            if (ui_button("BACK")) {
                network_disconnect();
                menu_set_mode(state, MENU_MODE_MATCHLIST);
            }
            if (network_is_server()) {
                if (ui_button("START")) {
                    // Make sure all players are ready
                    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                            menu_add_chat_message(state, "Cannot start match: Some players are not ready.");
                            return;
                        }
                    }

                    // Make sure all players have distinct colors
                    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                        const NetworkPlayer& player = network_get_player(player_id);
                        if (player.status == NETWORK_PLAYER_STATUS_NONE) {
                            continue;
                        }
                        for (uint8_t other_id = 0; other_id < MAX_PLAYERS; other_id++) {
                            const NetworkPlayer& other_player = network_get_player(other_id);
                            if (other_player.status == NETWORK_PLAYER_STATUS_NONE || player_id == other_id) {
                                continue;
                            }
                            if (player.recolor_id == other_player.recolor_id) {
                                menu_add_chat_message(state, "Cannot start match: Some players have selected the same color.");
                                return;
                            }
                        }
                    }

                    menu_set_mode(state, MENU_MODE_LOAD_MATCH);
                }
            } else {
                if (ui_button("READY")) {
                    // Toggle player ready status
                    bool is_ready = network_get_player(network_get_player_id()).status == NETWORK_PLAYER_STATUS_READY;
                    network_set_player_ready(!is_ready);
                }
            }
        ui_end_container();
        // End lobby buttons
    } // End lobby
}

void menu_set_mode(MenuState& state, MenuMode mode) {
    state.status_timer = 0;
    state.matchlist_page = 0;
    state.matchlist_item_selected = MENU_ITEM_NONE;

    if (input_is_text_input_active()) {
        input_stop_text_input();
    }

    state.mode = mode;
    if (mode == MENU_MODE_USERNAME) {
        input_start_text_input(&state.username, MAX_USERNAME_LENGTH);
    } else if (mode == MENU_MODE_MATCHLIST) {
        if (!network_scanner_create()) {
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "Error occured while searching for LAN games.");
            return;
        }
        state.lobby_search_query = "";
        network_scanner_search(state.lobby_search_query.c_str());
    } else if (mode == MENU_MODE_CONNECTING) {
        state.connection_timeout = CONNECTION_TIMEOUT;
    }
}

void menu_show_status(MenuState& state, const char* message) {
    strcpy(state.status_text, message);
    state.status_timer = STATUS_DURATION;
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

size_t menu_get_matchlist_page_count() {
    if (network_get_lobby_count() == 0) {
        return 1;
    }
    return (network_get_lobby_count() / MATCHLIST_PAGE_SIZE) + (int)(network_get_lobby_count() % MATCHLIST_PAGE_SIZE != 0);
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

    // Render decorations behind wagon
    menu_render_decoration(state, 0);
    menu_render_decoration(state, 2);

    // Wagon animation
    const SpriteInfo& sprite_wagon_info = resource_get_sprite_info(SPRITE_UNIT_WAGON);
    // Wagon color defaults to blue
    int wagon_recolor_id = 0;
    if (state.mode == MENU_MODE_LOBBY) { 
        // If we are in lobby, wagon color is player color
        wagon_recolor_id = network_get_player(network_get_player_id()).recolor_id;
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

    // Render decorations in front of wagon
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

    ui_render();

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
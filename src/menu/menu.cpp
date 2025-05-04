#include "menu.h"

#include "platform/platform.h"
#include "core/input.h"
#include "core/cursor.h"
#include "core/logger.h"
#include "core/sound.h"
#include "render/sprite.h"
#include "render/font.h"
#include "render/render.h"
#include "menu/ui.h"
#include "../util.h"

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

static const Rect LOBBYLIST_RECT = (Rect) {
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
static const uint32_t LOBBYLIST_PAGE_SIZE = 9;

static const std::vector<std::string> PLAYER_COLOR_STRS = { "Blue", "Red", "Green", "Purple" };

MenuState menu_init() {
    MenuState state;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;
    state.connection_timeout = 0;
    state.lobbylist_page = 0;
    state.lobbylist_item_selected = MENU_ITEM_NONE;

    state.mode = MENU_MODE_MAIN;
    state.options_menu.mode = OPTIONS_MENU_CLOSED;
    menu_set_mode(state, state.mode);

    return state;
}

void menu_handle_network_event(MenuState& state, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_CONNECTION_FAILED: {
            menu_set_mode(state, MENU_MODE_LOBBYLIST);
            menu_show_status(state, "Connection failed.");
            break;
        }
        case NETWORK_EVENT_INVALID_VERSION: {
            menu_set_mode(state, MENU_MODE_LOBBYLIST);
            menu_show_status(state, "Game version does not match server.");
            break;
        }
        case NETWORK_EVENT_JOINED_LOBBY: {
            menu_set_mode(state, MENU_MODE_LOBBY);
            break;
        }
        case NETWORK_EVENT_CHAT: {
            if (state.mode == MENU_MODE_LOBBY) {
                menu_add_chat_message(state, event.chat.message);
            }
            return;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            if (event.player_disconnected.player_id == 0) {
                network_disconnect();
                menu_set_mode(state, MENU_MODE_LOBBYLIST);
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
            network_send_chat(message);
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
            menu_set_mode(state, MENU_MODE_LOBBYLIST);
            menu_show_status(state, "Connection timed out.");
        }
    }

    ui_begin();
    ui_set_input_enabled(state.mode != MENU_MODE_OPTIONS && state.mode != MENU_MODE_REPLAY_RENAME);

    if (state.mode == MENU_MODE_MAIN || state.mode == MENU_MODE_OPTIONS) {
        ui_begin_column(ivec2(BUTTON_X, BUTTON_Y), 4);
            if (ui_button("PLAY")) {
                menu_set_mode(state, MENU_MODE_USERNAME);
            }
            if (ui_button("REPLAYS")) {
                menu_set_mode(state, MENU_MODE_REPLAYS);
            }
            if (ui_button("OPTIONS")) {
                state.options_menu = options_menu_open();
                menu_set_mode(state, MENU_MODE_OPTIONS);
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
                    menu_set_mode(state, MENU_MODE_LOBBYLIST);
                }
            }
        ui_end_container();
    } else if (state.mode == MENU_MODE_LOBBYLIST) {
        ui_frame_rect(LOBBYLIST_RECT);

        if (network_get_lobby_count() == 0) {
            int text_width = render_get_text_size(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...").x;
            ui_element_position(ivec2(LOBBYLIST_RECT.x + (LOBBYLIST_RECT.w / 2) - (text_width / 2), LOBBYLIST_RECT.y + 8));
            ui_text(FONT_HACK_GOLD, "Seems there's no lobbies in these parts...");
        }

        // Lobby list search and top buttons
        ui_begin_row(ivec2(44, 4), 4);
            ui_text_input("Search: ", ivec2(300, 24), &state.lobby_search_query, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);
            if (ui_sprite_button(SPRITE_UI_BUTTON_REFRESH, false, false)) {
                network_scanner_search(state.lobby_search_query.c_str());
                state.lobbylist_page = 0;
            }
            if (ui_sprite_button(SPRITE_UI_BUTTON_ARROW, state.lobbylist_page == 0, true)) {
                state.lobbylist_page--;
            }
            if (ui_sprite_button(SPRITE_UI_BUTTON_ARROW, state.lobbylist_page == menu_get_lobbylist_page_count(network_get_lobby_count()) - 1, false)) {
                state.lobbylist_page++;
            }
        ui_end_container();

        // Lobby list
        int base_index = (state.lobbylist_page * LOBBYLIST_PAGE_SIZE);
        for (int lobby_index = base_index; lobby_index < std::min(base_index + LOBBYLIST_PAGE_SIZE, (uint32_t)network_get_lobby_count()); lobby_index++) {
            char lobby_text[128];
            bool selected = lobby_index == state.lobbylist_item_selected;
            const NetworkLobby& lobby = network_get_lobby(lobby_index);
            sprintf(lobby_text, "%s %s (%u/%u) %s", selected ? "*" : " ", lobby.name, lobby.player_count, MAX_PLAYERS, selected ? "*" : " ");

            ivec2 text_frame_size = ui_text_frame_size(lobby_text);
            ui_element_position(ivec2(LOBBYLIST_RECT.x + (LOBBYLIST_RECT.w / 2) - (text_frame_size.x / 2), LOBBYLIST_RECT.y + 12 + (20 * (lobby_index - base_index))));
            if (ui_text_frame(lobby_text, false)) {
                state.lobbylist_item_selected = lobby_index;
            }
        }

        // Lobbylist button row
        ui_begin_row(ivec2(BUTTON_X, LOBBYLIST_RECT.y + LOBBYLIST_RECT.h + 4), 4); 
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
            if (state.lobbylist_item_selected != MENU_ITEM_NONE) {
                if (ui_button("JOIN")) {
                    const NetworkLobby& lobby = network_get_lobby(state.lobbylist_item_selected);
                    if (!network_client_create(state.username.c_str(), lobby.ip, lobby.port)) {
                        menu_show_status(state, "Could not create client.");
                    } else {
                        menu_set_mode(state, MENU_MODE_CONNECTING);
                    }
                }
            }
        ui_end_container();
    // End lobbylist
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

                    bool teams_enabled = network_get_match_setting(MATCH_SETTING_TEAMS) == MATCH_SETTING_TEAMS_ENABLED;
                    char team_picker_char;
                    if (teams_enabled) {
                        team_picker_char = (char)('1' + (int)player.team);
                    } else {
                        team_picker_char = '-';
                    }
                    ui_element_size(ivec2(PLAYERLIST_COLUMN_TEAM_WIDTH, 0));
                    if (ui_team_picker(team_picker_char, !teams_enabled || player_id != network_get_player_id())) {
                        network_set_player_team(player.team == 0 ? 1 : 0);
                    }

                    uint32_t player_color = player.recolor_id;
                    if (ui_dropdown(UI_DROPDOWN_MINI, &player_color, PLAYER_COLOR_STRS, player_id != network_get_player_id())) {
                        network_set_player_color((uint8_t)player_color);
                    }
                ui_end_container();
            }
        ui_end_container();
        // End playerlist

        // Match settings
        ui_frame_rect(MATCH_SETTINGS_RECT);
        ivec2 match_settings_text_size = render_get_text_size(FONT_HACK_GOLD, "Settings");
        ui_element_position(ivec2(MATCH_SETTINGS_RECT.x + (MATCH_SETTINGS_RECT.w / 2) - (match_settings_text_size.x / 2), MATCH_SETTINGS_RECT.y + 6));
        ui_text(FONT_HACK_GOLD, "Settings");

        ui_begin_column(ivec2(MATCH_SETTINGS_RECT.x + 8, MATCH_SETTINGS_RECT.y + 22), 4);
            int dropdown_width = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI).frame_width;
            const int setting_name_element_size = MATCH_SETTINGS_RECT.w - 16 - dropdown_width;
            for (int index = 0; index < MATCH_SETTING_COUNT; index++) {
                const MatchSettingData& setting_data = match_setting_data((MatchSetting)index);
                ui_begin_row(ivec2(0, 0), 0);
                    ui_element_size(ivec2(setting_name_element_size, 0));
                    ui_element_position(ivec2(0, 2));
                    ui_text(FONT_HACK_GOLD, setting_data.name);

                    uint32_t match_setting_value = network_get_match_setting(index);
                    if (ui_dropdown(UI_DROPDOWN_MINI, &match_setting_value, setting_data.values, !network_is_server())) {
                        network_set_match_setting((uint8_t)index, (uint8_t)match_setting_value);
                    }
                ui_end_container();
            }
        ui_end_container();

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
        if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && input_is_text_input_active()) {
            if (!state.chat_message.empty()) {
                char chat_message[128];
                sprintf(chat_message, "%s: %s", network_get_player(network_get_player_id()).name, state.chat_message.c_str());
                menu_add_chat_message(state, chat_message);
                network_send_chat(chat_message);
                state.chat_message = "";
            }
            sound_play(SOUND_UI_CLICK);
        }

        // Lobby buttons
        ui_begin_row(ivec2(LOBBY_CHAT_RECT.x + LOBBY_CHAT_RECT.w + 12, MATCH_SETTINGS_RECT.y + MATCH_SETTINGS_RECT.h + 4), 4);
            if (ui_button("BACK")) {
                network_disconnect();
                menu_set_mode(state, MENU_MODE_LOBBYLIST);
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
    } else if (state.mode == MENU_MODE_REPLAYS || state.mode == MENU_MODE_REPLAY_RENAME) {
        ui_frame_rect(LOBBYLIST_RECT);

        if (platform_get_replay_file_count() == 0) {
            int text_width = render_get_text_size(FONT_HACK_GOLD, "No replay files found.").x;
            ui_element_position(ivec2(LOBBYLIST_RECT.x + (LOBBYLIST_RECT.w / 2) - (text_width / 2), LOBBYLIST_RECT.y + 8));
            ui_text(FONT_HACK_GOLD, "No replay files found.");
        }

        // Search and top buttons
        ui_begin_row(ivec2(44, 4), 4);
            ui_text_input("Search: ", ivec2(300, 24), &state.lobby_search_query, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);
            if (ui_sprite_button(SPRITE_UI_BUTTON_REFRESH, false, false)) {
                platform_search_replays_folder(state.lobby_search_query.c_str());
                state.lobbylist_page = 0;
            }
            if (ui_sprite_button(SPRITE_UI_BUTTON_ARROW, state.lobbylist_page == 0, true)) {
                state.lobbylist_page--;
            }
            if (ui_sprite_button(SPRITE_UI_BUTTON_ARROW, state.lobbylist_page == menu_get_lobbylist_page_count(platform_get_replay_file_count()) - 1, false)) {
                state.lobbylist_page++;
            }
        ui_end_container();

        // Replay list
        size_t base_index = (state.lobbylist_page * LOBBYLIST_PAGE_SIZE);
        for (size_t lobby_index = base_index; lobby_index < std::min(base_index + LOBBYLIST_PAGE_SIZE, platform_get_replay_file_count()); lobby_index++) {
            char replay_text[128];
            bool selected = lobby_index == state.lobbylist_item_selected;
            sprintf(replay_text, "%s %s %s", selected ? "*" : " ", platform_get_replay_file_name(lobby_index), selected ? "*" : " ");

            ivec2 text_frame_size = ui_text_frame_size(replay_text);
            ui_element_position(ivec2(LOBBYLIST_RECT.x + (LOBBYLIST_RECT.w / 2) - (text_frame_size.x / 2), LOBBYLIST_RECT.y + 12 + (20 * (lobby_index - base_index))));
            if (ui_text_frame(replay_text, false)) {
                state.lobbylist_item_selected = lobby_index;
            }
        }

        // Replaylist button row
        ui_begin_row(ivec2(BUTTON_X, LOBBYLIST_RECT.y + LOBBYLIST_RECT.h + 4), 4); 
            if (ui_button("BACK")) {
                menu_set_mode(state, MENU_MODE_MAIN);
            }
            if (state.lobbylist_item_selected != MENU_ITEM_NONE) {
                if (ui_button("WATCH")) {
                    menu_set_mode(state, MENU_MODE_LOAD_REPLAY);
                }
                if (ui_button("RENAME")) {
                    state.mode = MENU_MODE_REPLAY_RENAME;
                    state.replay_rename = std::string(platform_get_replay_file_name(state.lobbylist_item_selected));
                }
            }
        ui_end_container();
    } // End replaylist

    if (state.mode == MENU_MODE_OPTIONS) {
        options_menu_update(state.options_menu);
        if (state.options_menu.mode == OPTIONS_MENU_CLOSED) {
            menu_set_mode(state, MENU_MODE_MAIN);
        }
    }

    if (state.mode == MENU_MODE_REPLAY_RENAME) {
        ui_set_input_enabled(true);
        ui_screen_shade();
        ui_begin_column(ivec2((SCREEN_WIDTH / 2) - 150, (SCREEN_HEIGHT / 2) - 64), 4);
            ui_text_input("Rename: ", ivec2(300, 24), &state.replay_rename, NETWORK_LOBBY_NAME_BUFFER_SIZE - 1);

            ui_begin_row(ivec2(0, 0), 4);
                if (ui_button("BACK")) {
                    state.mode = MENU_MODE_REPLAYS;
                }
                if (ui_button("OK")) {
                    if (state.replay_rename.empty()) {
                        menu_show_status(state, "Replay name cannot be empty.");
                    } else {
                        if (!filename_ends_in_rep(state.replay_rename)) {
                            state.replay_rename += ".rep";
                        }
                        char old_filename[256];
                        platform_get_replay_path(old_filename, platform_get_replay_file_name(state.lobbylist_item_selected));
                        char new_filename[256];
                        platform_get_replay_path(new_filename, state.replay_rename.c_str());
                        rename(old_filename, new_filename);
                        platform_search_replays_folder(state.lobby_search_query.c_str());
                        state.mode = MENU_MODE_REPLAYS;
                    }
                }
            ui_end_container();
        ui_end_container();
    }
}

void menu_set_mode(MenuState& state, MenuMode mode) {
    state.status_timer = 0;
    state.lobbylist_page = 0;
    if (mode != MENU_MODE_LOAD_REPLAY) {
        state.lobbylist_item_selected = MENU_ITEM_NONE;
    }

    if (input_is_text_input_active()) {
        input_stop_text_input();
    }

    state.mode = mode;
    if (mode == MENU_MODE_LOBBYLIST) {
        if (!network_scanner_create()) {
            menu_set_mode(state, MENU_MODE_MAIN);
            menu_show_status(state, "Error occured while searching for LAN games.");
            return;
        }
        state.lobby_search_query = "";
        network_scanner_search(state.lobby_search_query.c_str());
    } else if (mode == MENU_MODE_CONNECTING) {
        state.connection_timeout = CONNECTION_TIMEOUT;
    } else if (mode == MENU_MODE_REPLAYS) {
        state.lobby_search_query = "";
        platform_search_replays_folder(state.lobby_search_query.c_str());
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

size_t menu_get_lobbylist_page_count(size_t count) {
    if (count == 0) {
        return 1;
    }
    return (count / LOBBYLIST_PAGE_SIZE) + (int)(count % LOBBYLIST_PAGE_SIZE != 0);
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
    render_fill_rect((Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT }, RENDER_COLOR_BLUE);

    // Tiles
    for (int y = 0; y < MENU_TILE_HEIGHT; y++) {
        for (int x = 0; x < MENU_TILE_WIDTH; x++) {
            SpriteName sprite = (x + y) % 10 == 0 ? SPRITE_TILE_SAND2 : SPRITE_TILE_SAND1;
            const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
            Rect src_rect = (Rect) {
                .x = 0,
                .y = 0,
                .w = sprite_info.frame_width,
                .h = sprite_info.frame_height
            };
            Rect dst_rect = (Rect) {
                .x = (x * TILE_SIZE * 2) - state.parallax_x,
                .y = SCREEN_HEIGHT - ((MENU_TILE_HEIGHT - y) * TILE_SIZE * 2),
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            render_sprite(sprite, src_rect, dst_rect, 0);
        }
    }

    // Render decorations behind wagon
    menu_render_decoration(state, 0);
    menu_render_decoration(state, 2);

    // Wagon animation
    const SpriteInfo& sprite_wagon_info = render_get_sprite_info(SPRITE_UNIT_WAGON);
    // Wagon color defaults to blue
    int wagon_recolor_id = 0;
    if (state.mode == MENU_MODE_LOBBY) { 
        // If we are in lobby, wagon color is player color
        wagon_recolor_id = network_get_player(network_get_player_id()).recolor_id;
    }
    Rect wagon_src_rect = (Rect) {
        .x = sprite_wagon_info.frame_width * state.wagon_animation.frame.x,
        .y = (wagon_recolor_id * sprite_wagon_info.frame_height * sprite_wagon_info.vframes) + (sprite_wagon_info.frame_height * 2),
        .w = sprite_wagon_info.frame_width,
        .h = sprite_wagon_info.frame_height
    };
    Rect wagon_dst_rect = (Rect) {
        .x = state.wagon_x,
        .y = SCREEN_HEIGHT - 88,
        .w = wagon_src_rect.w * 2,
        .h = wagon_src_rect.h * 2
    };
    render_sprite(SPRITE_UNIT_WAGON, wagon_src_rect, wagon_dst_rect, RENDER_SPRITE_NO_CULL);

    // Render decorations in front of wagon
    menu_render_decoration(state, 1);

    // Render clouds
    const SpriteInfo& cloud_sprite_info = render_get_sprite_info(SPRITE_UI_CLOUDS);
    for (int index = 0; index < CLOUD_COUNT; index++) {
        Rect src_rect = (Rect) {
            .x = 0 + (CLOUD_FRAME_X[index] * cloud_sprite_info.frame_width),
            .y = 0,
            .w = cloud_sprite_info.frame_width,
            .h = cloud_sprite_info.frame_height
        };
        Rect dst_rect = (Rect) {
            .x = CLOUD_COORDS[index].x - state.parallax_cloud_x,
            .y = CLOUD_COORDS[index].y,
            .w = src_rect.w * 2,
            .h = src_rect.h * 2
        };
        render_sprite(SPRITE_UI_CLOUDS, src_rect, dst_rect, 0);
    }

    // Render title
    if (state.mode == MENU_MODE_MAIN || state.mode == MENU_MODE_USERNAME) {
        render_sprite_frame(SPRITE_UI_TITLE, ivec2(0, 0), ivec2(24, 24), RENDER_SPRITE_NO_CULL, 0);
    }

    // Render version
    char version_text[32];
    sprintf(version_text, "Version %s", APP_VERSION);
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, version_text);
    render_text(FONT_WESTERN8_OFFBLACK, version_text, ivec2(4, SCREEN_HEIGHT - text_size.y - 4));

    ui_render();

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

    render_sprite_batch();
}

void menu_render_decoration(const MenuState& state, int index) {
    int cactus_index = ((index * 2) + state.parallax_cactus_offset) % 5;
    if (cactus_index == 2) {
        cactus_index = 0;
    }
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_DECORATION);
    Rect src_rect = (Rect) {
        .x = 0,
        .y = 0,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    Rect dst_rect = (Rect) {
        .x = MENU_DECORATION_COORDS[index].x - state.parallax_x,
        .y = SCREEN_HEIGHT - (MENU_TILE_HEIGHT * TILE_SIZE * 2) + MENU_DECORATION_COORDS[index].y,
        .w = TILE_SIZE * 2,
        .h = TILE_SIZE * 2
    };
    render_sprite(SPRITE_DECORATION, src_rect, dst_rect, 0);
}
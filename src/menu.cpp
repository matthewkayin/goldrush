#include "menu.h"

#include "engine.h"
#include "defines.h"
#include "util.h"
#include "network.h"
#include "logger.h"
#include <string>
#include <unordered_map>

#ifdef GOLD_DEBUG
    #define GOLD_DEFAULT_JOIN_IP "127.0.0.1"
#else
    #define GOLD_DEFAULT_JOIN_IP ""
#endif

static const int TEXT_INPUT_WIDTH = 264;
static const int TEXT_INPUT_HEIGHT = 35;
static const rect_t TEXT_INPUT_RECT = rect_t(ivec2((SCREEN_WIDTH / 2) - (TEXT_INPUT_WIDTH / 2), (SCREEN_HEIGHT / 2) - (TEXT_INPUT_HEIGHT / 2)), 
                                              ivec2(TEXT_INPUT_WIDTH, TEXT_INPUT_HEIGHT));
static const rect_t PLAYERLIST_RECT = rect_t(ivec2(24, 32), ivec2(256, 128));
static const uint32_t STATUS_TIMER_DURATION = 60;

static const int BUTTON_Y = TEXT_INPUT_RECT.position.y + TEXT_INPUT_RECT.size.y + 16;
static const rect_t HOST_BUTTON_RECT = rect_t(ivec2(250, BUTTON_Y), ivec2(76, 30));
static const rect_t JOIN_BUTTON_RECT = rect_t(ivec2(HOST_BUTTON_RECT.position.x + HOST_BUTTON_RECT.size.x + 8, BUTTON_Y), ivec2(72, 30));
static const rect_t JOIN_IP_BACK_BUTTON_RECT = rect_t(ivec2(200, BUTTON_Y), ivec2(78, 30));
static const rect_t JOIN_IP_CONNECT_BUTTON_RECT = rect_t(ivec2(JOIN_IP_BACK_BUTTON_RECT.position.x + JOIN_IP_BACK_BUTTON_RECT.size.x + 8, BUTTON_Y), ivec2(132, 30));
static const rect_t CONNECTING_BACK_BUTTON_RECT = rect_t(ivec2((SCREEN_WIDTH / 2) - (JOIN_IP_BACK_BUTTON_RECT.size.x / 2), BUTTON_Y), JOIN_IP_BACK_BUTTON_RECT.size);
static const rect_t LOBBY_BACK_RECT = rect_t(PLAYERLIST_RECT.position + ivec2(0, PLAYERLIST_RECT.size.y + 8), JOIN_IP_BACK_BUTTON_RECT.size);
static const rect_t LOBBY_START_RECT = rect_t(LOBBY_BACK_RECT.position + ivec2(LOBBY_BACK_RECT.size.x + 8, 0), ivec2(92, 30));
static const rect_t LOBBY_READY_RECT = rect_t(LOBBY_BACK_RECT.position + ivec2(LOBBY_BACK_RECT.size.x + 8, 0), ivec2(100, 30));

enum MenuButton {
    MENU_BUTTON_NONE,
    MENU_BUTTON_HOST,
    MENU_BUTTON_JOIN,
    MENU_BUTTON_JOIN_IP_BACK,
    MENU_BUTTON_JOIN_IP_CONNECT,
    MENU_BUTTON_CONNECTING_BACK,
    MENU_BUTTON_LOBBY_BACK,
    MENU_BUTTON_LOBBY_START,
    MENU_BUTTON_LOBBY_READY
};

struct menu_button_t {
    const char* text;
    rect_t rect;
};

struct menu_state_t {
    MenuMode mode;

    std::string status_text;
    uint32_t status_timer;

    std::string username;
    std::string ip_address;

    std::unordered_map<MenuButton, menu_button_t> buttons;
    MenuButton button_hovered;
};
static menu_state_t state;

void menu_show_status(const char* message);
void menu_set_mode(MenuMode mode);

bool is_valid_username(const char* username) {
    std::string input_text = std::string(username);
    return input_text.length() != 0;
}

bool is_valid_ip(const char* ip_addr) {
    std::string text_input = std::string(ip_addr);
    uint32_t number_count = 0;
    while (text_input.length() != 0) {
        // Take the next part of the IP address
        size_t dot_index = text_input.find('.');
        std::string part;
        if (dot_index == std::string::npos) {
            part = text_input;
            text_input = "";
        } else {
            part = text_input.substr(0, dot_index);
            text_input = text_input.substr(dot_index + 1);
        }

        // Check that it's numeric
        bool is_part_numeric = true;
        for (char c : part) {
            if (c < '0' || c > '9') {
                is_part_numeric = false;
                break;
            }
        }
        if (!is_part_numeric) {
            break;
        }

        // Check that it's <= 255
        uint32_t part_number = std::stoul(part);
        if (part_number > 255) {
            break;
        }

        number_count++;
    } // End parse text_input

    return number_count == 4;
}

void menu_init() {
    state.username = "";
    state.ip_address = GOLD_DEFAULT_JOIN_IP;
    input_set_text_input_value("");
    menu_set_mode(MENU_MODE_MAIN);
}

void menu_show_status(const char* message) {
    state.status_text = message;
    state.status_timer = STATUS_TIMER_DURATION;
}

MenuMode menu_get_mode() {
    return state.mode;
}

void menu_set_mode(MenuMode mode) {
    // Set mode
    state.mode = mode;
    state.status_timer = 0;
    state.button_hovered = MENU_BUTTON_NONE;

    // Set buttons
    state.buttons.clear();
    switch(state.mode) {
        case MENU_MODE_MAIN:
            state.buttons[MENU_BUTTON_HOST] = (menu_button_t) { .text = "HOST", .rect = HOST_BUTTON_RECT };
            state.buttons[MENU_BUTTON_JOIN] = (menu_button_t) { .text = "JOIN", .rect = JOIN_BUTTON_RECT };
            input_set_text_input_value(state.username.c_str());
            break;
        case MENU_MODE_JOIN_IP:
            state.buttons[MENU_BUTTON_JOIN_IP_BACK] = (menu_button_t) { .text = "BACK", .rect = JOIN_IP_BACK_BUTTON_RECT };
            state.buttons[MENU_BUTTON_JOIN_IP_CONNECT] = (menu_button_t) { .text = "CONNECT", .rect = JOIN_IP_CONNECT_BUTTON_RECT };
            input_set_text_input_value(state.ip_address.c_str());
            break;
        case MENU_MODE_JOIN_CONNECTING:
            state.buttons[MENU_BUTTON_CONNECTING_BACK] = (menu_button_t) { .text = "BACK", .rect = CONNECTING_BACK_BUTTON_RECT };
            break;
        case MENU_MODE_LOBBY:
            state.buttons[MENU_BUTTON_LOBBY_BACK] = (menu_button_t) { .text = "BACK", .rect = LOBBY_BACK_RECT };
            if (network_is_server()) {
                state.buttons[MENU_BUTTON_LOBBY_START] = (menu_button_t) { .text = "START", .rect = LOBBY_START_RECT };
            } else {
                state.buttons[MENU_BUTTON_LOBBY_READY] = (menu_button_t) { .text = "READY", .rect = LOBBY_READY_RECT };
            }
        default:
            break;
    }
}

void menu_update() {
    if (state.mode == MENU_MODE_MATCH_START) {
        return;
    }

    // Status timer
    if (state.status_timer > 0) {
        state.status_timer--;
    }

    network_service();
    network_event_t network_event;
    while (network_poll_events(&network_event)) {
        switch (network_event.type) {
            case NETWORK_EVENT_CONNECTION_FAILED: {
                menu_set_mode(MENU_MODE_JOIN_IP);
                menu_show_status("Failed to connect to server.");
                break;
            }
            case NETWORK_EVENT_SERVER_DISCONNECTED: {
                menu_set_mode(MENU_MODE_MAIN);
                menu_show_status("Server disconnected.");
                break;
            }
            case NETWORK_EVENT_LOBBY_FULL: {
                menu_set_mode(MENU_MODE_MAIN);
                menu_show_status("That lobby is full.");
                break;
            }
            case NETWORK_EVENT_INVALID_VERSION: {
                menu_set_mode(MENU_MODE_MAIN);
                menu_show_status("Client version does not match.");
                break;
            }
            case NETWORK_EVENT_JOINED_LOBBY: {
                menu_set_mode(MENU_MODE_LOBBY);
                break;
            }
            case NETWORK_EVENT_CLIENT_READY: {
                network_server_broadcast_playerlist();
                break;
            }
            case NETWORK_EVENT_MATCH_LOAD: {
                menu_set_mode(MENU_MODE_MATCH_START);
                break;
            }
            default:
                break;
        }
    }

    // Button hover
    ivec2 mouse_position = input_get_mouse_position();
    state.button_hovered = MENU_BUTTON_NONE;
    for (auto it : state.buttons) {
        if (it.second.rect.has_point(mouse_position)) {
            state.button_hovered = it.first;
        }
    }

    // Mouse pressed
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        // Text input focus
        if (state.mode == MENU_MODE_MAIN || state.mode == MENU_MODE_JOIN_IP) {
            bool input_text_focused = TEXT_INPUT_RECT.has_point(mouse_position);
            bool is_text_input_active = input_is_text_input_active();
            if (input_text_focused && !is_text_input_active) {
                input_start_text_input(TEXT_INPUT_RECT, state.mode == MENU_MODE_MAIN ? MAX_USERNAME_LENGTH : 16);
            } else if (!input_text_focused && is_text_input_active) {
                input_stop_text_input();
            }
        }

        switch (state.button_hovered) {
            case MENU_BUTTON_HOST:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        menu_show_status("Invalid username.");
                        break;
                    }

                    state.username = std::string(input_get_text_input_value());
                    if (!network_server_create(state.username.c_str())) {
                        menu_show_status("Could not create server.");
                        break;
                    }
                    menu_set_mode(MENU_MODE_LOBBY);
                    break;
                }
            case MENU_BUTTON_JOIN:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        menu_show_status("Invalid username.");
                        break;
                    }

                    state.username = std::string(input_get_text_input_value());
                    menu_set_mode(MENU_MODE_JOIN_IP);
                    break;
                }
            case MENU_BUTTON_JOIN_IP_BACK:
                menu_set_mode(MENU_MODE_MAIN);
                break;
            case MENU_BUTTON_JOIN_IP_CONNECT:
                {
                    if (!is_valid_ip(input_get_text_input_value())) {
                        menu_show_status("Invalid IP address.");
                        break;
                    }

                    state.ip_address = std::string(input_get_text_input_value());
                    network_client_create(state.username.c_str(), state.ip_address.c_str());
                    menu_set_mode(MENU_MODE_JOIN_CONNECTING);

                    break;
                }
            case MENU_BUTTON_CONNECTING_BACK:
                network_disconnect();
                menu_set_mode(MENU_MODE_JOIN_IP);
                break;
            case MENU_BUTTON_LOBBY_BACK:
                network_disconnect();
                menu_set_mode(MENU_MODE_MAIN);
                break;
            case MENU_BUTTON_LOBBY_READY:
                network_client_toggle_ready();
                break;
            case MENU_BUTTON_LOBBY_START:
                network_server_start_loading();
                break;
            default:
                break;
        }
    }
}

void menu_render() {
    if (state.mode == MENU_MODE_MATCH_START) {
        return;
    }

    render_rect(rect_t(ivec2(0, 0), ivec2(SCREEN_WIDTH, SCREEN_HEIGHT)), COLOR_SAND, true);

    if (state.mode != MENU_MODE_LOBBY) {
        render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_BLACK, ivec2(RENDER_TEXT_CENTERED, 24));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_BLACK, ivec2(4, SCREEN_HEIGHT - 14));

    if (state.status_timer > 0) {
        render_text(FONT_WESTERN8, state.status_text.c_str(), COLOR_RED, ivec2(RENDER_TEXT_CENTERED, TEXT_INPUT_RECT.position.y - 38));
    }

    if (state.mode == MENU_MODE_MAIN || state.mode == MENU_MODE_JOIN_IP) {
        const char* prompt = state.mode == MENU_MODE_MAIN ? "USERNAME" : "IP ADDRESS";
        render_text(FONT_WESTERN8, prompt, COLOR_BLACK, TEXT_INPUT_RECT.position + ivec2(1, -13));
        render_rect(TEXT_INPUT_RECT, COLOR_BLACK);

        render_text(FONT_WESTERN16, input_get_text_input_value(), COLOR_BLACK, TEXT_INPUT_RECT.position + ivec2(2, TEXT_INPUT_RECT.size.y - 4), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (state.mode == MENU_MODE_JOIN_CONNECTING) {
        render_text(FONT_WESTERN16, "Connecting...", COLOR_BLACK, ivec2(RENDER_TEXT_CENTERED, RENDER_TEXT_CENTERED));
    }

    if (state.mode == MENU_MODE_LOBBY) {
        render_rect(PLAYERLIST_RECT, COLOR_BLACK);

        uint32_t player_index = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            /*
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }
            */

            std::string player_name_text = std::string(player.name);
            player_name_text = "BREADBREADFISH";
            if (player.status == PLAYER_STATUS_HOST) {
                player_name_text += ": HOST";
            } else if (player.status == PLAYER_STATUS_NOT_READY) {
                player_name_text += ": NOT READY";
            } else if (player.status == PLAYER_STATUS_READY) {
                player_name_text += ": READY";
            }

            int line_y = 16 * (player_index + 1);
            render_text(FONT_WESTERN8, player_name_text.c_str(), COLOR_BLACK, PLAYERLIST_RECT.position + ivec2(4, line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            render_line(PLAYERLIST_RECT.position + ivec2(0, line_y), PLAYERLIST_RECT.position + ivec2(PLAYERLIST_RECT.size.x - 1, line_y), COLOR_BLACK);
            player_index++;
        }

        if (network_is_server()) {
            ivec2 side_text_pos = PLAYERLIST_RECT.position + ivec2(PLAYERLIST_RECT.size.x + 2, 0);
            render_text(FONT_WESTERN8, "You are the host.", COLOR_BLACK, side_text_pos);
        }
    }

    for (auto it : state.buttons) {
        color_t button_color = state.button_hovered == it.first ? COLOR_WHITE : COLOR_BLACK;
        render_text(FONT_WESTERN16, it.second.text, button_color, it.second.rect.position + ivec2(4, 4));
        render_rect(it.second.rect, button_color);
    }
}
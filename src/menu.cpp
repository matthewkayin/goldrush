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
static const rect TEXT_INPUT_RECT = rect(xy((SCREEN_WIDTH / 2) - (TEXT_INPUT_WIDTH / 2), (SCREEN_HEIGHT / 2) - (TEXT_INPUT_HEIGHT / 2)), 
                                              xy(TEXT_INPUT_WIDTH, TEXT_INPUT_HEIGHT));
static const rect PLAYERLIST_RECT = rect(xy(24, 32), xy(384, 242));
static const unsigned int STATUS_TIMER_DURATION = 60;

static const int BUTTON_Y = 232;
static const rect HOST_BUTTON_RECT = rect(xy(300, BUTTON_Y), xy(76, 30));
static const rect JOIN_BUTTON_RECT = rect(xy(HOST_BUTTON_RECT.position.x + HOST_BUTTON_RECT.size.x + 8, BUTTON_Y), xy(72, 30));
static const rect JOIN_IP_BACK_BUTTON_RECT = rect(xy(248, BUTTON_Y), xy(78, 30));
static const rect JOIN_IP_CONNECT_BUTTON_RECT = rect(xy(JOIN_IP_BACK_BUTTON_RECT.position.x + JOIN_IP_BACK_BUTTON_RECT.size.x + 8, BUTTON_Y), xy(132, 30));
static const rect CONNECTING_BACK_BUTTON_RECT = rect(xy((SCREEN_WIDTH / 2) - (JOIN_IP_BACK_BUTTON_RECT.size.x / 2), BUTTON_Y), JOIN_IP_BACK_BUTTON_RECT.size);
static const rect LOBBY_BACK_RECT = rect(PLAYERLIST_RECT.position + xy(0, PLAYERLIST_RECT.size.y + 8), JOIN_IP_BACK_BUTTON_RECT.size);
static const rect LOBBY_START_RECT = rect(LOBBY_BACK_RECT.position + xy(LOBBY_BACK_RECT.size.x + 8, 0), xy(92, 30));
static const rect LOBBY_READY_RECT = rect(LOBBY_BACK_RECT.position + xy(LOBBY_BACK_RECT.size.x + 8, 0), xy(100, 30));

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
    rect _rect;
};

struct menu_state_t {
    MenuMode mode;

    std::string status_text;
    unsigned int status_timer;

    std::string username;
    std::string ip_address;
    std::string host_ip_address;

    std::unordered_map<MenuButton, menu_button_t> buttons;
    MenuButton button_hovered;
};

bool is_valid_username(const char* username) {
    std::string input_text = std::string(username);
    return input_text.length() != 0;
}

bool is_valid_ip(const char* ip_addr) {
    std::string text_input = std::string(ip_addr);
    unsigned int number_count = 0;
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
        unsigned int part_number = std::stoul(part);
        if (part_number > 255) {
            break;
        }

        number_count++;
    } // End parse text_input

    return number_count == 4;
}

menu_t::menu_t() {
    state = std::make_unique<menu_state_t>();
    state->username = "";
    state->ip_address = GOLD_DEFAULT_JOIN_IP;
    set_mode(MENU_MODE_MAIN);
}

menu_t::~menu_t() {}

void menu_t::show_status(const char* message) {
    state->status_text = message;
    state->status_timer = STATUS_TIMER_DURATION;
}

void menu_t::set_mode(MenuMode mode) {
    // Set mode
    state->mode = mode;
    state->status_timer = 0;
    state->button_hovered = MENU_BUTTON_NONE;

    // Set buttons
    state->buttons.clear();
    switch(state->mode) {
        case MENU_MODE_MAIN:
            state->buttons[MENU_BUTTON_HOST] = (menu_button_t) { .text = "HOST", ._rect = HOST_BUTTON_RECT };
            state->buttons[MENU_BUTTON_JOIN] = (menu_button_t) { .text = "JOIN", ._rect = JOIN_BUTTON_RECT };
            input_set_text_input_value(state->username.c_str());
            break;
        case MENU_MODE_JOIN_IP:
            state->buttons[MENU_BUTTON_JOIN_IP_BACK] = (menu_button_t) { .text = "BACK", ._rect = JOIN_IP_BACK_BUTTON_RECT };
            state->buttons[MENU_BUTTON_JOIN_IP_CONNECT] = (menu_button_t) { .text = "CONNECT", ._rect = JOIN_IP_CONNECT_BUTTON_RECT };
            input_set_text_input_value(state->ip_address.c_str());
            break;
        case MENU_MODE_JOIN_CONNECTING:
            state->buttons[MENU_BUTTON_CONNECTING_BACK] = (menu_button_t) { .text = "BACK", ._rect = CONNECTING_BACK_BUTTON_RECT };
            break;
        case MENU_MODE_LOBBY:
            state->buttons[MENU_BUTTON_LOBBY_BACK] = (menu_button_t) { .text = "BACK", ._rect = LOBBY_BACK_RECT };
            if (network_is_server()) {
                state->buttons[MENU_BUTTON_LOBBY_START] = (menu_button_t) { .text = "START", ._rect = LOBBY_START_RECT };
            } else {
                state->buttons[MENU_BUTTON_LOBBY_READY] = (menu_button_t) { .text = "READY", ._rect = LOBBY_READY_RECT };
            }
        default:
            break;
    }
}

void menu_t::update() {
    // Status timer
    if (state->status_timer > 0) {
        state->status_timer--;
    }

    if (state->mode == MENU_MODE_JOIN_CONNECTING && network_get_status() == NETWORK_STATUS_OFFLINE) {
        network_disconnect();
        set_mode(MENU_MODE_JOIN_IP);
        show_status("Failed to connect to server.");
    } else if (state->mode == MENU_MODE_JOIN_CONNECTING && network_get_status() == NETWORK_STATUS_CONNECTED) {
        set_mode(MENU_MODE_LOBBY);
    }

    // Button hover
    xy mouse_position = input_get_mouse_position();
    state->button_hovered = MENU_BUTTON_NONE;
    for (auto it : state->buttons) {
        if (it.second._rect.has_point(mouse_position)) {
            state->button_hovered = it.first;
        }
    }

    // Mouse pressed
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        // Text input focus
        if (state->mode == MENU_MODE_MAIN || state->mode == MENU_MODE_JOIN_IP) {
            bool input_text_focused = TEXT_INPUT_RECT.has_point(mouse_position);
            bool is_text_input_active = input_is_text_input_active();
            if (input_text_focused && !is_text_input_active) {
                input_start_text_input(TEXT_INPUT_RECT);
            } else if (!input_text_focused && is_text_input_active) {
                input_stop_text_input();
            }
        }

        switch (state->button_hovered) {
            case MENU_BUTTON_HOST:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        show_status("Invalid username.");
                        break;
                    }

                    state->username = std::string(input_get_text_input_value());
                    if (!network_server_create()) {
                        show_status("Could not create server.");
                        break;
                    }
                    state->host_ip_address = "who knows, man.";
                    set_mode(MENU_MODE_LOBBY);
                    break;
                }
            case MENU_BUTTON_JOIN:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        show_status("Invalid username.");
                        break;
                    }

                    state->username = std::string(input_get_text_input_value());
                    set_mode(MENU_MODE_JOIN_IP);
                    break;
                }
            case MENU_BUTTON_JOIN_IP_BACK:
                set_mode(MENU_MODE_MAIN);
                break;
            case MENU_BUTTON_JOIN_IP_CONNECT:
                {
                    if (!is_valid_ip(input_get_text_input_value())) {
                        show_status("Invalid IP address.");
                        break;
                    }

                    state->ip_address = std::string(input_get_text_input_value());
                    network_client_create(state->ip_address.c_str());
                    set_mode(MENU_MODE_JOIN_CONNECTING);

                    break;
                }
            case MENU_BUTTON_CONNECTING_BACK:
                network_disconnect();
                set_mode(MENU_MODE_JOIN_IP);
                break;
            case MENU_BUTTON_LOBBY_BACK:
                network_disconnect();
                set_mode(MENU_MODE_MAIN);
                break;
            default:
                break;
        }
    }
}

void menu_t::render() const {
    render_rect(rect(xy(0, 0), xy(SCREEN_WIDTH, SCREEN_HEIGHT)), COLOR_SAND, true);

    if (state->mode != MENU_MODE_LOBBY) {
        render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_BLACK, xy(RENDER_TEXT_CENTERED, 36));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_BLACK, xy(4, SCREEN_HEIGHT - 14));

    if (state->status_timer > 0) {
        render_text(FONT_WESTERN8, state->status_text.c_str(), COLOR_RED, xy(RENDER_TEXT_CENTERED, TEXT_INPUT_RECT.position.y - 48));
    }

    if (state->mode == MENU_MODE_MAIN || state->mode == MENU_MODE_JOIN_IP) {
        const char* prompt = state->mode == MENU_MODE_MAIN ? "USERNAME" : "IP ADDRESS";
        render_text(FONT_WESTERN8, prompt, COLOR_BLACK, TEXT_INPUT_RECT.position + xy(1, -13));
        render_rect(TEXT_INPUT_RECT, COLOR_BLACK);

        render_text(FONT_WESTERN16, input_get_text_input_value(), COLOR_BLACK, TEXT_INPUT_RECT.position + xy(2, TEXT_INPUT_RECT.size.y - 4), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (state->mode == MENU_MODE_JOIN_CONNECTING) {
        render_text(FONT_WESTERN16, "Connecting...", COLOR_BLACK, xy(RENDER_TEXT_CENTERED, RENDER_TEXT_CENTERED));
    }

    if (state->mode == MENU_MODE_LOBBY) {
        render_rect(PLAYERLIST_RECT, COLOR_BLACK);

        std::string players[] = {
            "Banana",
            "Goat",
            "Yeet"
        };
        for (unsigned int player_index = 0; player_index < 3; player_index++) {
            int line_y = 16 * (player_index + 1);
            render_text(FONT_WESTERN8, players[player_index].c_str(), COLOR_BLACK, PLAYERLIST_RECT.position + xy(4, line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            render_line(PLAYERLIST_RECT.position + xy(0, line_y), PLAYERLIST_RECT.position + xy(PLAYERLIST_RECT.size.x - 1, line_y), COLOR_BLACK);
        }

        if (network_is_server()) {
            xy side_text_pos = PLAYERLIST_RECT.position + xy(PLAYERLIST_RECT.size.x + 2, 0);
            render_text(FONT_WESTERN8, "You are the host.", COLOR_BLACK, side_text_pos);
            render_text(FONT_WESTERN8, (std::string("Your IP is ") + state->host_ip_address).c_str(), COLOR_BLACK, side_text_pos + xy(0, 16));
        }
    }

    for (auto it : state->buttons) {
        color_t button_color = state->button_hovered == it.first ? COLOR_WHITE : COLOR_BLACK;
        render_text(FONT_WESTERN16, it.second.text, button_color, it.second._rect.position + xy(4, 4));
        render_rect(it.second._rect, button_color);
    }
}
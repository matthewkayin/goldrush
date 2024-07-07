#include "menu.h"

#include "defines.h"
#include "util.h"
#include "input.h"
#include "network.h"
#include "logger.h"

#ifdef GOLD_DEBUG
    #define GOLD_DEFAULT_JOIN_IP "127.0.0.1"
#else
    #define GOLD_DEFAULT_JOIN_IP ""
#endif

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

menu_t::menu_t() {
    username = "";
    ip_address = GOLD_DEFAULT_JOIN_IP;
    input_set_text_input_value("");
    set_mode(MENU_MODE_MAIN);
}

void menu_t::show_status(const char* message) {
    status_text = message;
    status_timer = STATUS_TIMER_DURATION;
}

MenuMode menu_t::get_mode() const {
    return mode;
}

void menu_t::set_mode(MenuMode mode) {
    // Set mode
    this->mode = mode;
    status_timer = 0;
    button_hovered = MENU_BUTTON_NONE;

    // Set buttons
    buttons.clear();
    switch(mode) {
        case MENU_MODE_MAIN:
            buttons[MENU_BUTTON_HOST] = (menu_button_t) { .text = "HOST", .rect = HOST_BUTTON_RECT };
            buttons[MENU_BUTTON_JOIN] = (menu_button_t) { .text = "JOIN", .rect = JOIN_BUTTON_RECT };
            input_set_text_input_value(username.c_str());
            break;
        case MENU_MODE_JOIN_IP:
            buttons[MENU_BUTTON_JOIN_IP_BACK] = (menu_button_t) { .text = "BACK", .rect = JOIN_IP_BACK_BUTTON_RECT };
            buttons[MENU_BUTTON_JOIN_IP_CONNECT] = (menu_button_t) { .text = "CONNECT", .rect = JOIN_IP_CONNECT_BUTTON_RECT };
            input_set_text_input_value(ip_address.c_str());
            break;
        case MENU_MODE_JOIN_CONNECTING:
            buttons[MENU_BUTTON_CONNECTING_BACK] = (menu_button_t) { .text = "BACK", .rect = CONNECTING_BACK_BUTTON_RECT };
            break;
        case MENU_MODE_LOBBY:
            buttons[MENU_BUTTON_LOBBY_BACK] = (menu_button_t) { .text = "BACK", .rect = LOBBY_BACK_RECT };
            if (network_is_server()) {
                buttons[MENU_BUTTON_LOBBY_START] = (menu_button_t) { .text = "START", .rect = LOBBY_START_RECT };
            } else {
                buttons[MENU_BUTTON_LOBBY_READY] = (menu_button_t) { .text = "READY", .rect = LOBBY_READY_RECT };
            }
        default:
            break;
    }
}

void menu_t::update() {
    if (mode == MENU_MODE_MATCH_START) {
        return;
    }

    // Status timer
    if (status_timer > 0) {
        status_timer--;
    }

    network_service();
    network_event_t network_event;
    while (network_poll_events(&network_event)) {
        switch (network_event.type) {
            case NETWORK_EVENT_CONNECTION_FAILED: {
                set_mode(MENU_MODE_JOIN_IP);
                show_status("Failed to connect to server.");
                break;
            }
            case NETWORK_EVENT_SERVER_DISCONNECTED: {
                set_mode(MENU_MODE_MAIN);
                show_status("Server disconnected.");
                break;
            }
            case NETWORK_EVENT_LOBBY_FULL: {
                set_mode(MENU_MODE_MAIN);
                show_status("That lobby is full.");
                break;
            }
            case NETWORK_EVENT_INVALID_VERSION: {
                set_mode(MENU_MODE_MAIN);
                show_status("Client version does not match.");
                break;
            }
            case NETWORK_EVENT_JOINED_LOBBY: {
                set_mode(MENU_MODE_LOBBY);
                break;
            }
            case NETWORK_EVENT_CLIENT_READY: {
                network_server_broadcast_playerlist();
                break;
            }
            case NETWORK_EVENT_MATCH_LOAD: {
                set_mode(MENU_MODE_MATCH_START);
                break;
            }
            default:
                break;
        }
    }

    // Button hover
    ivec2 mouse_position = input_get_mouse_position();
    button_hovered = MENU_BUTTON_NONE;
    for (auto it : buttons) {
        if (it.second.rect.has_point(mouse_position)) {
            button_hovered = it.first;
        }
    }

    // Mouse pressed
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        // Text input focus
        if (mode == MENU_MODE_MAIN || mode == MENU_MODE_JOIN_IP) {
            bool input_text_focused = TEXT_INPUT_RECT.has_point(mouse_position);
            bool is_text_input_active = input_is_text_input_active();
            if (input_text_focused && !is_text_input_active) {
                input_start_text_input(TEXT_INPUT_RECT, mode == MENU_MODE_MAIN ? MAX_USERNAME_LENGTH : 16);
            } else if (!input_text_focused && is_text_input_active) {
                input_stop_text_input();
            }
        }

        switch (button_hovered) {
            case MENU_BUTTON_HOST:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        show_status("Invalid username.");
                        break;
                    }

                    username = std::string(input_get_text_input_value());
                    if (!network_server_create(username.c_str())) {
                        show_status("Could not create server.");
                        break;
                    }
                    set_mode(MENU_MODE_LOBBY);
                    break;
                }
            case MENU_BUTTON_JOIN:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        show_status("Invalid username.");
                        break;
                    }

                    username = std::string(input_get_text_input_value());
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

                    ip_address = std::string(input_get_text_input_value());
                    network_client_create(username.c_str(), ip_address.c_str());
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

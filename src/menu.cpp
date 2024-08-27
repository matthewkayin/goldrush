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

menu_state_t menu_init() {
    menu_state_t state;
    state.username = "";
    state.ip_address = GOLD_DEFAULT_JOIN_IP;
    input_set_text_input_value("");
    menu_set_mode(state, MENU_MODE_MAIN);
    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_HALF_SPEED);
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;
    return state;
}

void menu_show_status(menu_state_t& state, const char* message) {
    state.status_text = message;
    state.status_timer = STATUS_TIMER_DURATION;
}

void menu_set_mode(menu_state_t& state, MenuMode mode) {
    // Set mode
    state.mode = mode;
    state.status_timer = 0;
    state.button_hovered = MENU_BUTTON_NONE;

    // Set buttons
    state.buttons.clear();
    switch(mode) {
        case MENU_MODE_OPTIONS:
        case MENU_MODE_MAIN:
            state.buttons[MENU_BUTTON_PLAY] = (menu_button_t) { .text = "PLAY", .rect = PLAY_BUTTON_RECT };
            state.buttons[MENU_BUTTON_OPTIONS] = (menu_button_t) { .text = "OPTIONS", .rect = OPTIONS_BUTTON_RECT };
            state.buttons[MENU_BUTTON_EXIT] = (menu_button_t) { .text = "EXIT", .rect = EXIT_BUTTON_RECT };
            break;
        case MENU_MODE_PLAY:
            state.buttons[MENU_BUTTON_HOST] = (menu_button_t) { .text = "HOST", .rect = HOST_BUTTON_RECT };
            state.buttons[MENU_BUTTON_JOIN] = (menu_button_t) { .text = "JOIN", .rect = JOIN_BUTTON_RECT };
            state.buttons[MENU_BUTTON_PLAY_BACK] = (menu_button_t) { .text = "BACK", .rect = PLAY_BACK_BUTTON_RECT };
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

void menu_update(menu_state_t& state) {
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
                menu_set_mode(state, MENU_MODE_JOIN_IP);
                menu_show_status(state, "Failed to connect to server.");
                break;
            }
            case NETWORK_EVENT_SERVER_DISCONNECTED: {
                menu_set_mode(state, MENU_MODE_MAIN);
                menu_show_status(state, "Server disconnected.");
                break;
            }
            case NETWORK_EVENT_LOBBY_FULL: {
                menu_set_mode(state, MENU_MODE_MAIN);
                menu_show_status(state, "That lobby is full.");
                break;
            }
            case NETWORK_EVENT_INVALID_VERSION: {
                menu_set_mode(state, MENU_MODE_MAIN);
                menu_show_status(state, "Client version does not match.");
                break;
            }
            case NETWORK_EVENT_JOINED_LOBBY: {
                menu_set_mode(state, MENU_MODE_LOBBY);
                break;
            }
            case NETWORK_EVENT_CLIENT_READY: {
                network_server_broadcast_playerlist();
                break;
            }
            case NETWORK_EVENT_MATCH_LOAD: {
                menu_set_mode(state, MENU_MODE_MATCH_START);
                break;
            }
            default:
                break;
        }
    }

    // Button hover
    xy mouse_position = input_get_mouse_position();
    state.button_hovered = MENU_BUTTON_NONE;
    for (auto it : state.buttons) {
        if (it.second.rect.has_point(mouse_position) && state.mode != MENU_MODE_OPTIONS) {
            state.button_hovered = it.first;
        }
    }

    // Mouse pressed
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT) && state.mode != MENU_MODE_OPTIONS) {
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
                        menu_show_status(state, "Invalid username.");
                        break;
                    }

                    state.username = std::string(input_get_text_input_value());
                    if (!network_server_create(state.username.c_str())) {
                        menu_show_status(state, "Could not create server.");
                        break;
                    }
                    menu_set_mode(state, MENU_MODE_LOBBY);
                    break;
                }
            case MENU_BUTTON_JOIN:
                {
                    if (!is_valid_username(input_get_text_input_value())) {
                        menu_show_status(state, "Invalid username.");
                        break;
                    }

                    state.username = std::string(input_get_text_input_value());
                    menu_set_mode(state, MENU_MODE_JOIN_IP);
                    break;
                }
            case MENU_BUTTON_JOIN_IP_BACK:
                menu_set_mode(state, MENU_MODE_MAIN);
                break;
            case MENU_BUTTON_JOIN_IP_CONNECT:
                {
                    if (!is_valid_ip(input_get_text_input_value())) {
                        menu_show_status(state, "Invalid IP address.");
                        break;
                    }

                    state.ip_address = std::string(input_get_text_input_value());
                    network_client_create(state.username.c_str(), state.ip_address.c_str());
                    menu_set_mode(state, MENU_MODE_JOIN_CONNECTING);

                    break;
                }
            case MENU_BUTTON_CONNECTING_BACK:
                network_disconnect();
                menu_set_mode(state, MENU_MODE_JOIN_IP);
                break;
            case MENU_BUTTON_LOBBY_BACK:
                network_disconnect();
                menu_set_mode(state, MENU_MODE_MAIN);
                break;
            case MENU_BUTTON_LOBBY_READY:
                network_client_toggle_ready();
                break;
            case MENU_BUTTON_LOBBY_START: {
                if (network_are_all_players_ready()) {
                    network_server_start_loading();
                    menu_set_mode(state, MENU_MODE_MATCH_START);
                }
                break;
            }
            case MENU_BUTTON_EXIT: {
                menu_set_mode(state, MENU_MODE_EXIT);
                break;
            }
            case MENU_BUTTON_PLAY: {
                menu_set_mode(state, MENU_MODE_PLAY);
                break;
            }
            case MENU_BUTTON_OPTIONS: {
                state.option_menu_state = option_menu_init();
                menu_set_mode(state, MENU_MODE_OPTIONS);
                break;
            }
            case MENU_BUTTON_PLAY_BACK: {
                menu_set_mode(state, MENU_MODE_MAIN);
                break;
            }
            default:
                break;
        }
    }

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

    if (state.mode == MENU_MODE_OPTIONS) {
        option_menu_update(state.option_menu_state);
        if (state.option_menu_state.mode == OPTION_MENU_CLOSED) {
            menu_set_mode(state, MENU_MODE_MAIN);
        }
    }
}

#pragma once

#include "defines.h"
#include "util.h"
#include "sprite.h"
#include "options.h"
#include <string>
#include <unordered_map>

const int TEXT_INPUT_WIDTH = 264;
const int TEXT_INPUT_HEIGHT = 35;
const rect_t TEXT_INPUT_RECT = rect_t(xy(48, (SCREEN_HEIGHT / 2) - (TEXT_INPUT_HEIGHT / 2) - 42), 
                                              xy(TEXT_INPUT_WIDTH, TEXT_INPUT_HEIGHT));
const rect_t PLAYERLIST_RECT = rect_t(xy(24, 32), xy(256, 128));
const uint32_t STATUS_TIMER_DURATION = 60;

const int BUTTON_Y = TEXT_INPUT_RECT.position.y + TEXT_INPUT_RECT.size.y + 16;
const rect_t PLAY_BUTTON_RECT = rect_t(xy(48, 128), xy(84, 30));
const rect_t OPTIONS_BUTTON_RECT = rect_t(xy(48, PLAY_BUTTON_RECT.position.y + 42), xy(120, 30));
const rect_t EXIT_BUTTON_RECT = rect_t(xy(48, OPTIONS_BUTTON_RECT.position.y + 42), xy(72, 30));
const rect_t HOST_BUTTON_RECT = rect_t(xy(48, BUTTON_Y), xy(76, 30));
const rect_t JOIN_BUTTON_RECT = rect_t(xy(HOST_BUTTON_RECT.position.x + HOST_BUTTON_RECT.size.x + 8, BUTTON_Y), xy(72, 30));
const rect_t PLAY_BACK_BUTTON_RECT = rect_t(xy(48, HOST_BUTTON_RECT.position.y + 42), xy(78, 30));
const rect_t JOIN_IP_BACK_BUTTON_RECT = rect_t(xy(48, BUTTON_Y + 42), xy(78, 30));
const rect_t JOIN_IP_CONNECT_BUTTON_RECT = rect_t(xy(48, BUTTON_Y), xy(132, 30));
const rect_t CONNECTING_BACK_BUTTON_RECT = rect_t(xy(48, BUTTON_Y + 42), JOIN_IP_BACK_BUTTON_RECT.size);
const rect_t LOBBY_BACK_RECT = rect_t(PLAYERLIST_RECT.position + xy(0, PLAYERLIST_RECT.size.y + 8), JOIN_IP_BACK_BUTTON_RECT.size);
const rect_t LOBBY_START_RECT = rect_t(LOBBY_BACK_RECT.position + xy(LOBBY_BACK_RECT.size.x + 8, 0), xy(92, 30));
const rect_t LOBBY_READY_RECT = rect_t(LOBBY_BACK_RECT.position + xy(LOBBY_BACK_RECT.size.x + 8, 0), xy(100, 30));

const int PARALLAX_TIMER_DURATION = 2;

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_PLAY,
    MENU_MODE_JOIN_IP,
    MENU_MODE_JOIN_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_MATCH_START,
    MENU_MODE_EXIT,
    MENU_MODE_OPTIONS
};

enum MenuButton {
    MENU_BUTTON_NONE,
    MENU_BUTTON_PLAY,
    MENU_BUTTON_OPTIONS,
    MENU_BUTTON_EXIT,
    MENU_BUTTON_HOST,
    MENU_BUTTON_JOIN,
    MENU_BUTTON_JOIN_IP_BACK,
    MENU_BUTTON_JOIN_IP_CONNECT,
    MENU_BUTTON_CONNECTING_BACK,
    MENU_BUTTON_LOBBY_BACK,
    MENU_BUTTON_LOBBY_START,
    MENU_BUTTON_LOBBY_READY,
    MENU_BUTTON_PLAY_BACK
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

    animation_t wagon_animation;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;

    option_menu_state_t option_menu_state;
};

menu_state_t menu_init();
void menu_update(menu_state_t& state);
void menu_show_status(menu_state_t& state, const char* message);
void menu_set_mode(menu_state_t& state, MenuMode mode);
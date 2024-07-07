#pragma once

#include "defines.h"
#include "util.h"
#include <string>
#include <unordered_map>

const int TEXT_INPUT_WIDTH = 264;
const int TEXT_INPUT_HEIGHT = 35;
const rect_t TEXT_INPUT_RECT = rect_t(ivec2((SCREEN_WIDTH / 2) - (TEXT_INPUT_WIDTH / 2), (SCREEN_HEIGHT / 2) - (TEXT_INPUT_HEIGHT / 2)), 
                                              ivec2(TEXT_INPUT_WIDTH, TEXT_INPUT_HEIGHT));
const rect_t PLAYERLIST_RECT = rect_t(ivec2(24, 32), ivec2(256, 128));
const uint32_t STATUS_TIMER_DURATION = 60;

const int BUTTON_Y = TEXT_INPUT_RECT.position.y + TEXT_INPUT_RECT.size.y + 16;
const rect_t HOST_BUTTON_RECT = rect_t(ivec2(250, BUTTON_Y), ivec2(76, 30));
const rect_t JOIN_BUTTON_RECT = rect_t(ivec2(HOST_BUTTON_RECT.position.x + HOST_BUTTON_RECT.size.x + 8, BUTTON_Y), ivec2(72, 30));
const rect_t JOIN_IP_BACK_BUTTON_RECT = rect_t(ivec2(200, BUTTON_Y), ivec2(78, 30));
const rect_t JOIN_IP_CONNECT_BUTTON_RECT = rect_t(ivec2(JOIN_IP_BACK_BUTTON_RECT.position.x + JOIN_IP_BACK_BUTTON_RECT.size.x + 8, BUTTON_Y), ivec2(132, 30));
const rect_t CONNECTING_BACK_BUTTON_RECT = rect_t(ivec2((SCREEN_WIDTH / 2) - (JOIN_IP_BACK_BUTTON_RECT.size.x / 2), BUTTON_Y), JOIN_IP_BACK_BUTTON_RECT.size);
const rect_t LOBBY_BACK_RECT = rect_t(PLAYERLIST_RECT.position + ivec2(0, PLAYERLIST_RECT.size.y + 8), JOIN_IP_BACK_BUTTON_RECT.size);
const rect_t LOBBY_START_RECT = rect_t(LOBBY_BACK_RECT.position + ivec2(LOBBY_BACK_RECT.size.x + 8, 0), ivec2(92, 30));
const rect_t LOBBY_READY_RECT = rect_t(LOBBY_BACK_RECT.position + ivec2(LOBBY_BACK_RECT.size.x + 8, 0), ivec2(100, 30));

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_JOIN_IP,
    MENU_MODE_JOIN_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_MATCH_START
};

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

struct menu_t {
    MenuMode mode;

    std::string status_text;
    uint32_t status_timer;

    std::string username;
    std::string ip_address;

    std::unordered_map<MenuButton, menu_button_t> buttons;
    MenuButton button_hovered;

    menu_t();
    void show_status(const char* message);
    MenuMode get_mode() const;
    void set_mode(MenuMode mode);
    void update();
};
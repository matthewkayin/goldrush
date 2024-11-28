#pragma once

#include "defines.h"
#include "engine.h"

#include <string>

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_USERNAME,
    MENU_MODE_MATCHLIST,
    MENU_MODE_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_LOBBY_HOST,
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
    MENU_BUTTON_MATCHLIST_BACK,
    MENU_BUTTON_USERNAME_BACK,
    MENU_BUTTON_USERNAME_OK,
    MENU_BUTTON_LOBBY_BACK,
    MENU_BUTTON_LOBBY_READY,
    MENU_BUTTON_LOBBY_START,
    MENU_BUTTON_CONNECTING_BACK
};

struct menu_state_t {
    MenuMode mode;
    MenuButton button_hovered;

    std::string status_text;
    uint32_t status_timer;

    std::string username;
    int item_hovered;
    int item_selected;

    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;
};

menu_state_t menu_init();
void menu_handle_input(menu_state_t& state, SDL_Event event);
void menu_update(menu_state_t& state);
void menu_show_status(menu_state_t& state, const char* text);
void menu_render(const menu_state_t& state);
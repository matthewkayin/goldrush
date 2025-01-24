#pragma once

#include "defines.h"
#include "engine.h"
#include "network.h"
#include "animation.h"

#include <string>

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_USERNAME,
    MENU_MODE_MATCHLIST,
    MENU_MODE_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_EXIT,
    MENU_MODE_OPTIONS,
    MENU_MODE_LOAD_MATCH
};

enum MenuButton {
    MENU_BUTTON_NONE,
    MENU_BUTTON_PLAY,
    MENU_BUTTON_REPLAYS,
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

enum MenuHoverType {
    MENU_HOVER_NONE,
    MENU_HOVER_BUTTON,
    MENU_HOVER_ITEM,
    MENU_HOVER_REFRESH
};

struct menu_hover_t {
    MenuHoverType type;
    union {
        MenuButton button;
        int item;
    };
};

struct menu_state_t {
    MenuMode mode;
    struct menu_hover_t hover;

    std::string status_text;
    uint32_t status_timer;

    std::string username;
    int item_selected;

    animation_t wagon_animation;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;
};

menu_state_t menu_init();
void menu_handle_input(menu_state_t& state, SDL_Event event);
void menu_update(menu_state_t& state);
void menu_show_status(menu_state_t& state, const char* text);
void menu_set_mode(menu_state_t& state, MenuMode mode);
bool menu_is_button_disabled(const menu_state_t& state, MenuButton button);
void menu_render(const menu_state_t& state);
SDL_Rect menu_get_lobby_text_frame_rect(int lobby_index);
void menu_render_lobby_text(const menu_state_t& state, int lobby_index);
void menu_render_menu_button(MenuButton button, bool hovered);
SDL_Rect menu_get_button_rect(MenuButton button);
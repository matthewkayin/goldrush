#pragma once

#include "core/animation.h"

enum menu_mode {
    MENU_MODE_MAIN,
    MENU_MODE_USERNAME,
    MENU_MODE_MATCHLIST,
    MENU_MODE_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_EXIT,
    MENU_MODE_OPTIONS,
    MENU_MODE_LOAD_MATCH,
    MENU_MODE_COUNT
};

enum menu_item_name {
    MENU_ITEM_MAIN_BUTTON_PLAY,
    MENU_ITEM_MAIN_BUTTON_EXIT,
    MENU_ITEM_USERNAME_TEXTBOX,
    MENU_ITEM_USERNAME_BUTTON_BACK,
    MENU_ITEM_USERNAME_BUTTON_OK,
    MENU_ITEM_COUNT
};

struct menu_state_t {
    menu_mode mode;

    animation_t wagon_animation;
    int wagon_x;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;

    char username[MAX_USERNAME_LENGTH + 4];
    size_t username_length;

    bool text_input_show_cursor;
    uint32_t text_input_cursor_blink_timer;
};

menu_state_t menu_init();
void menu_update(menu_state_t& state);
void menu_render(const menu_state_t& state);
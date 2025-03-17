#pragma once

#include "core/animation.h"
#include <vector>

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

enum menu_button_name {
    MENU_BUTTON_PLAY,
    MENU_BUTTON_EXIT
};

struct menu_button_t {
    menu_button_name name;
    const char* text;
    rect_t rect;
};

struct menu_state_t {
    menu_mode mode;

    animation_t wagon_animation;
    int wagon_x;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;

    std::vector<menu_button_t> menu_buttons[MENU_MODE_COUNT];
};

menu_state_t menu_init();
void menu_update(menu_state_t& state);
void menu_render(const menu_state_t& state);
#pragma once

#include <SDL2/SDL.h>
#include "engine.h"

enum OptionMenuMode {
    OPTION_MENU_CLOSED,
    OPTION_MENU_OPEN,
    OPTION_MENU_DROPDOWN,
    OPTION_MENU_CONFIRM_CHANGES
};

enum OptionMenuHover {
    OPTION_HOVER_NONE,
    OPTION_HOVER_BACK,
    OPTION_HOVER_APPLY,
    OPTION_HOVER_DROPDOWN,
    OPTION_HOVER_DROPDOWN_ITEM,
    OPTION_HOVER_CONFIRM_NO,
    OPTION_HOVER_CONFIRM_YES
};

struct options_menu_state_t {
    OptionMenuMode mode;
    OptionMenuHover hover;
    int hover_subindex;
    int dropdown_chosen;
    std::unordered_map<Option, int> pending_changes;
    uint32_t confirm_changes_timer;
};

options_menu_state_t options_menu_open();
void options_menu_handle_input(options_menu_state_t& state, SDL_Event event);
void options_menu_update(options_menu_state_t& state);
void options_menu_render(const options_menu_state_t& state);
SDL_Rect options_get_dropdown_rect(int index);
SDL_Rect options_get_dropdown_item_rect(int option, int value);
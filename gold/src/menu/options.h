#pragma once

#include "core/options.h"
#include "core/input.h"
#include <unordered_map>
#include <SDL2/SDL.h>

enum OptionsMenuMode {
    OPTIONS_MENU_OPEN,
    OPTIONS_MENU_HOTKEYS,
    OPTIONS_MENU_CLOSED
};

struct OptionsMenuState {
    OptionsMenuMode mode;
    SDL_Keycode hotkey_pending_changes[INPUT_ACTION_COUNT];
    int hotkey_group_selected;
    int hotkey_index_selected;
};

OptionsMenuState options_menu_open();
void options_menu_update(OptionsMenuState& state);
void options_menu_render(const OptionsMenuState& state);
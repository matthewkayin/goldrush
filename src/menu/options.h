#pragma once

#include "core/options.h"
#include "core/input.h"
#include <unordered_map>
#include <SDL3/SDL.h>

enum OptionsMenuMode {
    OPTIONS_MENU_OPEN,
    OPTIONS_MENU_HOTKEYS,
    OPTIONS_MENU_CLOSED
};

enum OptionsMenuSaveStatus {
    OPTIONS_MENU_SAVE_STATUS_NONE,
    OPTIONS_MENU_SAVE_STATUS_UNSAVED_CHANGES,
    OPTIONS_MENU_SAVE_STATUS_SAVED,
    OPTIONS_MENU_SAVE_STATUS_ERRORS,
};

struct OptionsMenuState {
    OptionsMenuMode mode;
    SDL_Scancode hotkey_pending_changes[INPUT_ACTION_COUNT];
    int hotkey_group_selected;
    int hotkey_index_selected;
    OptionsMenuSaveStatus save_status;
};

OptionsMenuState options_menu_open();
void options_menu_update(OptionsMenuState& state);
#pragma once

#include <SDL2/SDL.h>
#include "engine.h"

enum OptionMenuMode {
    OPTION_MENU_CLOSED,
    OPTION_MENU_OPEN,
    OPTION_MENU_DROPDOWN,
    OPTION_MENU_HOTKEYS
};

enum OptionMenuHover {
    OPTION_HOVER_NONE,
    OPTION_HOVER_BACK,
    OPTION_HOVER_HOTKEYS,
    OPTION_HOVER_HOTKEYS_SAVE,
    OPTION_HOVER_HOTKEY_GROUP,
    OPTION_HOVER_HOTKEYS_HOTKEY,
    OPTION_HOVER_HOTKEYS_DEFAULTS,
    OPTION_HOVER_HOTKEYS_GRID,
    OPTION_HOVER_DROPDOWN,
    OPTION_HOVER_DROPDOWN_ITEM
};

enum OptionHotkeyGroup {
    HOTKEY_GROUP_MINER,
    HOTKEY_GROUP_MINER_BUILD,
    HOTKEY_GROUP_MINER_BUILD2,
    HOTKEY_GROUP_WAGON,
    HOTKEY_GROUP_SAPPER,
    HOTKEY_GROUP_TINKER,
    HOTKEY_GROUP_HALL,
    HOTKEY_GROUP_SALOON,
    HOTKEY_GROUP_SMITH,
    HOTKEY_GROUP_BARRACKS,
    HOTKEY_GROUP_COOP,
    HOTKEY_GROUP_SHERIFFS,
    HOTKEY_GROUP_COUNT
};

struct options_menu_state_t {
    OptionMenuMode mode;
    OptionMenuHover hover;
    int hover_subindex;
    int dropdown_chosen;
    int slider_chosen;
    int hotkey_group_chosen;
    int hotkey_chosen;
    std::unordered_map<UiButton, SDL_Keycode> hotkeys;
    int hotkey_save_status_timer;
    bool hotkey_save_successful;
};

options_menu_state_t options_menu_open();
void options_menu_handle_input(options_menu_state_t& state, SDL_Event event);
void options_menu_update(options_menu_state_t& state);
void options_menu_render(const options_menu_state_t& state);
SDL_Rect options_get_dropdown_rect(int index);
SDL_Rect options_get_dropdown_item_rect(int option, int value);
const char* option_value_str(Option option, int value);
xy option_hotkey_group_position(int button);
xy option_hotkey_position(int button);
void option_get_hotkey_str(char* str_ptr, UiButton button, const std::unordered_map<UiButton, SDL_Keycode>& hotkeys);
bool option_is_hotkey_group_valid(const std::unordered_map<UiButton, SDL_Keycode>& hotkeys, int group);
#pragma once

#include "core/options.h"
#include <unordered_map>

enum OptionsMenuMode {
    OPTIONS_MENU_OPEN,
    OPTIONS_MENU_CLOSED
};

struct OptionsMenuState {
    OptionsMenuMode mode;
    std::unordered_map<OptionName, int> pending_changes;
};

OptionsMenuState options_menu_open();
void options_menu_update(OptionsMenuState& state);
void options_menu_render(const OptionsMenuState& state);
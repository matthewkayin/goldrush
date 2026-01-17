#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/trigger.h"
#include <vector>
#include <string>

struct EditorMenuTriggerEffect {
    uint32_t effect_index;
    TriggerAction effect;
    std::vector<std::string> effect_type_items;
    std::string hint_value;
};

EditorMenuTriggerEffect editor_menu_trigger_effect_open(const TriggerAction& effect, uint32_t effect_index);
void editor_menu_trigger_effect_update(EditorMenuTriggerEffect& menu, UI& ui, EditorMenuMode& mode);

#endif
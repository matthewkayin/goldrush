#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/trigger.h"
#include <vector>
#include <string>

struct EditorMenuTriggerAction {
    uint32_t action_index;
    TriggerAction action;
    std::vector<std::string> action_type_items;
    std::string hint_value;
};

EditorMenuTriggerAction editor_menu_trigger_action_open(const TriggerAction& action, uint32_t action_index);
void editor_menu_trigger_action_update(EditorMenuTriggerAction& menu, UI& ui, EditorMenuMode& mode);

#endif
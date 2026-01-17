#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/trigger.h"
#include "scenario/scenario.h"
#include <vector>
#include <string>

struct EditorMenuTriggerAction {
    uint32_t action_index;
    TriggerAction action;
    std::vector<std::string> action_type_items;
    std::vector<std::string> objective_items;
    std::vector<std::string> message_type_items;
    std::string chat_prefix_value;
    std::string chat_message_value;
};

EditorMenuTriggerAction editor_menu_trigger_action_open(const Scenario* scenario, const TriggerAction& action, uint32_t action_index);
void editor_menu_trigger_action_update(EditorMenuTriggerAction& menu, UI& ui, EditorMenuMode& mode);

#endif
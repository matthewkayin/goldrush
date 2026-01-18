#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/scenario.h"
#include "scenario/trigger.h"
#include <vector>
#include <string>

struct EditorMenuTriggerCondition {
    uint32_t condition_index;
    TriggerCondition condition;
    std::vector<std::string> condition_type_items;
    std::vector<std::string> objective_dropdown_items;
};

EditorMenuTriggerCondition editor_menu_trigger_condition_open(const Scenario* scenario, const TriggerCondition& condition, uint32_t condition_index);
void editor_menu_trigger_condition_update(EditorMenuTriggerCondition& menu, UI& ui, EditorMenuMode& mode);

#endif
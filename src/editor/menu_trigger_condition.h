#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/scenario.h"
#include "scenario/trigger.h"
#include <vector>
#include <string>

enum EditorMenuTriggerConditionRequest {
    EDITOR_MENU_TRIGGER_CONDITION_REQUEST_NONE,
    EDITOR_MENU_TRIGGER_CONDITION_REQUEST_AREA
};

struct EditorMenuTriggerCondition {
    EditorMenuTriggerConditionRequest request;
    uint32_t condition_index;
    TriggerCondition condition;
    std::vector<std::string> condition_type_items;
    std::vector<std::string> objective_dropdown_items;
};

EditorMenuTriggerCondition editor_menu_trigger_condition_open(const Scenario* scenario, const TriggerCondition& condition, uint32_t condition_index);
void editor_menu_trigger_condition_update(EditorMenuTriggerCondition& menu, UI& ui, EditorMenuMode& mode);
void editor_menu_trigger_condition_set_request_area(EditorMenuTriggerCondition& menu, ivec2 cell, int cell_size);

#endif
#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"

enum EditorMenuObjectiveMode {
    EDITOR_MENU_OBJECTIVE_CLOSED,
    EDITOR_MENU_OBJECTIVE_OPEN,
    EDITOR_MENU_OBJECTIVE_SAVE
};

struct EditorMenuObjective {
    EditorMenuObjectiveMode mode;
    std::vector<std::string> objective_type_items;

    uint32_t objective_type;
    uint32_t objective_gold_target;
};

EditorMenuObjective editor_menu_objective_open(const Scenario* scenario);
void editor_menu_objective_update(EditorMenuObjective& menu, UI& ui);
ScenarioObjective editor_menu_objective_get_objective(const EditorMenuObjective& menu);

#endif
#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/scenario.h"

struct EditorMenuObjective {
    uint32_t objective_index;
    std::vector<std::string> text_values;
    std::vector<std::string> objective_counter_type_dropdown_items;
    int scroll;
};

EditorMenuObjective editor_menu_objective_open(const Scenario* scenario);
void editor_menu_objective_update(EditorMenuObjective& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario);

#endif
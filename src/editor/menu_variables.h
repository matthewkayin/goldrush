#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/scenario.h"

struct EditorMenuVariables {
    uint32_t variable_index;
    std::vector<std::string> text_values;
    int scroll;
};

EditorMenuVariables editor_menu_variables_open(const Scenario* scenario);
void editor_menu_variables_update(EditorMenuVariables& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario);

#endif
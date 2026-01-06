#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"

enum EditorMenuAllowedTechMode {
    EDITOR_MENU_ALLOWED_TECH_MODE_CLOSED,
    EDITOR_MENU_ALLOWED_TECH_MODE_OPEN,
    EDITOR_MENU_ALLOWED_TECH_MODE_SAVE
};

struct EditorMenuAllowedTech {
    EditorMenuAllowedTechMode mode;
    bool allowed_tech[ENTITY_TYPE_COUNT];
};

EditorMenuAllowedTech editor_menu_allowed_tech_open(const Scenario* scenario);
void editor_menu_allowed_tech_update(EditorMenuAllowedTech& menu, UI& ui);

#endif
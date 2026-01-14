#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"
#include "editor/menu.h"

struct EditorMenuAllowedTech {
    bool allowed_entities[ENTITY_TYPE_COUNT];
    bool allowed_upgrades[UPGRADE_COUNT];
};

EditorMenuAllowedTech editor_menu_allowed_tech_open(const Scenario* scenario);
void editor_menu_allowed_tech_update(EditorMenuAllowedTech& menu, UI& ui, EditorMenuMode& mode);

#endif
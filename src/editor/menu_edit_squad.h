#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"
#include "editor/menu.h"

struct EditorMenuEditSquad {
    std::string squad_name;
    uint32_t squad_player;
    uint32_t squad_type;
    std::vector<std::string> squad_player_items;
    std::vector<std::string> squad_type_items;
};

EditorMenuEditSquad editor_menu_edit_squad_open(const ScenarioSquad& squad, const Scenario* scenario);
void editor_menu_edit_squad_update(EditorMenuEditSquad& menu, UI& ui, EditorMenuMode& mode);

#endif
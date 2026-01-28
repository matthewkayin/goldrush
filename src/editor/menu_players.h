#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"
#include "editor/menu.h"

enum EditorMenuPlayersMode {
    EDITOR_MENU_PLAYERS_MODE_PLAYERS,
    EDITOR_MENU_PLAYERS_MODE_TECH
};

struct EditorMenuPlayers {
    EditorMenuPlayersMode mode;
    uint32_t selected_player_id;
    std::string player_name_string;
    int scroll;
};

EditorMenuPlayers editor_menu_players_open(const Scenario* scenario);
void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario);

#endif
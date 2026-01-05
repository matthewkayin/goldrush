#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"

enum EditorMenuPlayersMode {
    EDITOR_MENU_PLAYERS_CLOSED,
    EDITOR_MENU_PLAYERS_OPEN
};

struct EditorMenuPlayers {
    EditorMenuPlayersMode mode;
    uint32_t starting_gold[MAX_PLAYERS];
    std::string player_names[MAX_PLAYERS - 1];
};

EditorMenuPlayers editor_menu_players_open(const Scenario* scenario);
void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, Scenario* scenario);

#endif
#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "editor/document.h"

enum EditorMenuPlayersMode {
    EDITOR_MENU_PLAYERS_CLOSED,
    EDITOR_MENU_PLAYERS_OPEN
};

struct EditorMenuPlayers {
    EditorMenuPlayersMode mode;
    uint32_t starting_gold[MAX_PLAYERS];
    std::string player_names[MAX_PLAYERS - 1];
};

EditorMenuPlayers editor_menu_players_open(const EditorDocument* document);
void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, EditorDocument* document);

#endif
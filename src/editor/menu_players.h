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
};

EditorMenuPlayers editor_menu_players_open();
void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, EditorDocument* document);

#endif
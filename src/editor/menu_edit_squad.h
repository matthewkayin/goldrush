#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "editor/document.h"

enum EditorMenuEditSquadMode {
    EDITOR_MENU_EDIT_SQUAD_CLOSED,
    EDITOR_MENU_EDIT_SQUAD_OPEN,
    EDITOR_MENU_EDIT_SQUAD_SAVE
};

struct EditorMenuEditSquad {
    EditorMenuEditSquadMode mode;
    std::string squad_name;
    uint32_t squad_player;
    uint32_t squad_type;
    std::vector<std::string> squad_player_items;
    std::vector<std::string> squad_type_items;
};

EditorMenuEditSquad editor_menu_edit_squad_open(const EditorSquad& squad, const EditorDocument* document);
void editor_menu_edit_squad_update(EditorMenuEditSquad& menu, UI& ui);

#endif
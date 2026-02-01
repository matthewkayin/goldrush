#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "core/ui.h"

struct EditorMenuMapSize {
    uint32_t map_size;
};

EditorMenuMapSize editor_menu_map_size_open(int map_width);
void editor_menu_map_size_update(EditorMenuMapSize& menu, UI& ui, EditorMenuMode& mode);

#endif
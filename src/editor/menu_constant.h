#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/scenario.h"

struct EditorMenuConstant {
    std::string value;
};

EditorMenuConstant editor_menu_constant_open(const char* constant_name);
void editor_menu_constant_update(EditorMenuConstant& menu, UI& ui, EditorMenuMode& mode);

#endif
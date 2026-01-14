#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"
#include "editor/menu.h"

struct EditorMenuRenameTrigger {
    std::string trigger_name;
};

EditorMenuRenameTrigger editor_menu_rename_trigger_open(const Trigger& trigger);
void editor_menu_rename_trigger_update(EditorMenuRenameTrigger& menu, UI& ui, EditorMenuMode& mode);

#endif
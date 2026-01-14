#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"
#include "editor/menu.h"

struct EditorMenuTriggers {
    std::string trigger_name;
};

EditorMenuTriggers editor_menu_triggers_open(const Trigger& trigger);
void editor_menu_triggers_update(EditorMenuTriggers& menu, UI& ui, EditorMenuMode& mode);

#endif
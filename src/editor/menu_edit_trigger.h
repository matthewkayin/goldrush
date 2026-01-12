#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"

enum EditorMenuEditTriggerMode {
    EDITOR_MENU_EDIT_TRIGGER_CLOSED,
    EDITOR_MENU_EDIT_TRIGGER_OPEN,
    EDITOR_MENU_EDIT_TRIGGER_SAVE
};

struct EditorMenuEditTrigger {
    EditorMenuEditTriggerMode mode;
    std::string trigger_name;
};

EditorMenuEditTrigger editor_menu_edit_trigger_open(const Trigger& trigger);
void editor_menu_edit_trigger_update(EditorMenuEditTrigger& menu, UI& ui);

#endif
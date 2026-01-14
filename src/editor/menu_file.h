#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"
#include "editor/menu.h"

enum EditorMenuFileType {
    EDITOR_MENU_FILE_TYPE_SAVE,
    EDITOR_MENU_FILE_TYPE_OPEN
};

struct EditorMenuFile {
    EditorMenuFileType type;
    std::string path;
};

EditorMenuFile editor_menu_file_save_open(const char* previous_path);
EditorMenuFile editor_menu_file_open_open();
void editor_menu_file_update(EditorMenuFile& menu, UI& ui, EditorMenuMode& mode);

#endif
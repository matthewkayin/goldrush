#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"
#include "scenario/scenario.h"

enum EditorMenuFileMode {
    EDITOR_MENU_FILE_CLOSED,
    EDITOR_MENU_FILE_SAVE_OPEN,
    EDITOR_MENU_FILE_OPEN_OPEN,
    EDITOR_MENU_FILE_SAVE_FINISHED,
    EDITOR_MENU_FILE_OPEN_FINISHED
};

struct EditorMenuFile {
    EditorMenuFileMode mode;
    std::string path;
};

EditorMenuFile editor_menu_file_save_open(const char* previous_path);
EditorMenuFile editor_menu_file_open_open();
void editor_menu_file_update(EditorMenuFile& menu, UI& ui);

#endif
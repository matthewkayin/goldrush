#include "menu_file.h"

#ifdef GOLD_DEBUG

#include "util/util.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 128
};

EditorMenuFile editor_menu_file_save_open(const char* previous_path) {
    EditorMenuFile menu;
    menu.type = EDITOR_MENU_FILE_TYPE_SAVE;
    menu.path = std::string(previous_path);

    return menu;
}

EditorMenuFile editor_menu_file_open_open() {
    EditorMenuFile menu;
    menu.type = EDITOR_MENU_FILE_TYPE_OPEN;
    menu.path = "";

    return menu;
}

void editor_menu_file_update(EditorMenuFile& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, menu.type == EDITOR_MENU_FILE_TYPE_SAVE ? "Save Map" : "Open Map");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        ui_text_input(ui, "Path: ", ivec2(300 - 16, 24), &menu.path, 128);
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT) {
        input_stop_text_input();
        if (!string_ends_with(menu.path, ".scn")) {
            menu.path = menu.path + ".scn";
        }
    }
}

#endif
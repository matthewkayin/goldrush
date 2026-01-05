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
    menu.mode = EDITOR_MENU_FILE_SAVE_OPEN;
    menu.path = std::string(previous_path);

    return menu;
}

EditorMenuFile editor_menu_file_open_open() {
    EditorMenuFile menu;
    menu.mode = EDITOR_MENU_FILE_OPEN_OPEN;
    menu.path = "";

    return menu;
}

void editor_menu_file_update(EditorMenuFile& menu, UI& ui) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_RECT);

    const char* header_text = menu.mode == EDITOR_MENU_FILE_SAVE_OPEN ? "Save" : "Open";
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, header_text);
    ui_element_position(ui, ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (header_text_size.x / 2), MENU_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, header_text);

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        ui_text_input(ui, "Path: ", ivec2(300 - 16, 24), &menu.path, 128);
    ui_end_container(ui);

    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_FILE_CLOSED; 
    }

    ui_element_position(ui, ui_button_position_frame_bottom_right(MENU_RECT, header_text));
    if (ui_button(ui, header_text)) {
        input_stop_text_input();
        if (!string_ends_with(menu.path, ".scn")) {
            menu.path = menu.path + ".scn";
        }
        menu.mode = menu.mode == EDITOR_MENU_FILE_SAVE_OPEN
            ? EDITOR_MENU_FILE_SAVE_FINISHED
            : EDITOR_MENU_FILE_OPEN_FINISHED;
    }
}

#endif
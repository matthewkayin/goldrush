#include "menu_constant.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 112
};

EditorMenuConstant editor_menu_constant_open(const char* constant_name) {
    EditorMenuConstant menu;
    menu.value = std::string(constant_name);

    return menu;
}

void editor_menu_constant_update(EditorMenuConstant& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Rename Constant");

    ui_element_position(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30));
    ui_text_input(ui, "Name: ", ivec2(MENU_RECT.w - 32, 24), &menu.value, SCENARIO_CONSTANT_NAME_BUFFER_LENGTH - 1);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);
}

#endif
#include "menu_triggers.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 112
};

EditorMenuTriggers editor_menu_triggers_open(const Trigger& trigger) {
    EditorMenuTriggers menu;
    menu.trigger_name = std::string(trigger.name);

    return menu;
}

void editor_menu_triggers_update(EditorMenuTriggers& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Triggers");

    // Name
    ui_element_position(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30));
    ui_text_input(ui, "Name: ", ivec2(MENU_RECT.w - 32, 24), &menu.trigger_name, TRIGGER_NAME_MAX_LENGTH - 1);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);
}

#endif
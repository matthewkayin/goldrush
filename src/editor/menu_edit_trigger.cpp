#include "menu_edit_trigger.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 112
};

EditorMenuEditTrigger editor_menu_edit_trigger_open(const Trigger& trigger) {
    EditorMenuEditTrigger menu;
    menu.mode = EDITOR_MENU_EDIT_TRIGGER_OPEN;
    menu.trigger_name = std::string(trigger.name);

    return menu;
}

void editor_menu_edit_trigger_update(EditorMenuEditTrigger& menu, UI& ui) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_RECT);

    // Header
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Edit Trigger");
    ui_element_position(ui, ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (header_text_size.x / 2), MENU_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, "Edit Trigger");

    // Name
    ui_element_position(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30));
    ui_text_input(ui, "Name: ", ivec2(MENU_RECT.w - 32, 24), &menu.trigger_name, TRIGGER_NAME_MAX_LENGTH - 1);

    // Buttons
    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_EDIT_TRIGGER_CLOSED;
    }

    ui_element_position(ui, ui_button_position_frame_bottom_right(MENU_RECT, "Save"));
    if (ui_button(ui, "Save")) {
        menu.mode = EDITOR_MENU_EDIT_TRIGGER_SAVE;
    }
}

#endif
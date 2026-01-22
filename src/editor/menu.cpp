#include "menu.h"

#ifdef GOLD_DEBUG

bool editor_menu_dropdown(UI& ui, const char* prompt, uint32_t* selection, const std::vector<std::string>& items, const Rect& rect, int scroll_max_visible_items) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);

    bool dropdown_clicked = false;

    ui_element_size(ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(ui, ivec2(0, 0), 0);
        ui_element_position(ui, ivec2(0, 3));
        ui_text(ui, FONT_HACK_GOLD, prompt);

        ui_element_position(ui, ivec2(rect.w - 16 - dropdown_sprite_info.frame_width, 0));
        dropdown_clicked = ui_dropdown(ui, UI_DROPDOWN_MINI, selection, items, false, scroll_max_visible_items);
    ui_end_container(ui);

    return dropdown_clicked;
}

bool editor_menu_slider(UI& ui, const char* prompt, uint32_t* value, const UiSliderParams& params, const Rect& rect) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN);

    bool slider_value_changed = false;
    ui_element_size(ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(ui, ivec2(0, 0), 0);
        ui_element_position(ui, ivec2(0, 3));
        ui_text(ui, FONT_HACK_GOLD, prompt);

        ui_element_position(ui, ivec2(rect.w - 16 - dropdown_sprite_info.frame_width, 0));
        slider_value_changed = ui_slider(ui, value, NULL, params);
    ui_end_container(ui);

    return slider_value_changed;
}

bool editor_menu_prompt_and_button(UI& ui, const char* prompt, const char* button, Rect rect) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);

    bool button_pressed = false;
    ui_element_size(ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(ui, ivec2(0, 0), 0);
        ui_element_position(ui, ivec2(0, 3));
        ui_text(ui, FONT_HACK_GOLD, prompt);

        ui_element_position(ui, ivec2(rect.w - 16 - dropdown_sprite_info.frame_width, 0));
        button_pressed = ui_slim_button(ui, button);
    ui_end_container(ui);

    return button_pressed;
}

void editor_menu_header(UI& ui, Rect rect, const char* header_text) {
    ui.input_enabled = true;
    ui_frame_rect(ui, rect);

    // Header
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, header_text);
    ui_element_position(ui, ivec2(rect.x + (rect.w / 2) - (header_text_size.x / 2), rect.y + 6));
    ui_text(ui, FONT_HACK_GOLD, header_text);
}

void editor_menu_back_save_buttons(UI& ui, Rect rect, EditorMenuMode& mode) {
    ui_element_position(ui, ui_button_position_frame_bottom_left(rect));
    if (ui_button(ui, "Back")) {
        mode = EDITOR_MENU_MODE_CLOSED;
    }

    ui_element_position(ui, ui_button_position_frame_bottom_right(rect, "Save"));
    if (ui_button(ui, "Save")) {
        mode = EDITOR_MENU_MODE_SUBMIT;
    }
}

#endif
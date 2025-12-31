#include "ui.h"

bool editor_menu_dropdown(UI& ui, const char* prompt, uint32_t* selection, const std::vector<std::string>& items, const Rect& rect) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);

    bool dropdown_clicked = false;

    ui_element_size(ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(ui, ivec2(0, 0), 0);
        ui_element_position(ui, ivec2(0, 3));
        ui_text(ui, FONT_HACK_GOLD, prompt);

        ui_element_position(ui, ivec2(rect.w - 16 - dropdown_sprite_info.frame_width, 0));
        dropdown_clicked = ui_dropdown(ui, UI_DROPDOWN_MINI, selection, items, false);
    ui_end_container(ui);

    return dropdown_clicked;
}

void editor_menu_slider(UI& ui, const char* prompt, uint32_t* value, const Rect& rect) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN);

    ui_element_size(ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(ui, ivec2(0, 0), 0);
        ui_element_position(ui, ivec2(0, 3));
        ui_text(ui, FONT_HACK_GOLD, prompt);

        ui_element_position(ui, ivec2(rect.w - 16 - dropdown_sprite_info.frame_width, 0));
        ui_slider(ui, value, NULL, 0, 100, UI_SLIDER_DISPLAY_RAW_VALUE);
    ui_end_container(ui);
}
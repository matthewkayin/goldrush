#include "ui.h"

#ifdef GOLD_DEBUG

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

#endif
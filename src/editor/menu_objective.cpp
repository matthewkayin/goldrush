#include "menu_objective.h"

#ifdef GOLD_DEBUG

#include "editor/ui.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 256
};

EditorMenuObjective editor_menu_objective_open(const EditorDocument* document) {
    EditorMenuObjective menu;
    menu.mode = EDITOR_MENU_OBJECTIVE_OPEN;

    for (uint32_t objective_type = 0; objective_type < OBJECTIVE_TYPE_COUNT; objective_type++) {
        menu.objective_type_items.push_back(objective_type_str((ObjectiveType)objective_type));
    }

    menu.objective_type = document->objective.type;
    menu.objective_gold_target = document->objective.type == OBJECTIVE_TYPE_GOLD 
        ? document->objective.gold.target 
        : 0;

    return menu;
}

void editor_menu_objective_update(EditorMenuObjective& menu, UI& ui) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_RECT);

    // Header
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Edit Objective");
    ui_element_position(ui, ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (header_text_size.x / 2), MENU_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, "Edit Objective");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Objective type
        editor_menu_dropdown(ui, "Type:", &menu.objective_type, menu.objective_type_items, MENU_RECT);

        // Gold
        if (menu.objective_type == OBJECTIVE_TYPE_GOLD) {
            UiSliderParams params = (UiSliderParams) {
                .display = UI_SLIDER_DISPLAY_RAW_VALUE,
                .size = UI_SLIDER_SIZE_NORMAL,
                .min = 0,
                .max = 15000,
                .step = 100
            };
            editor_menu_slider(ui, "Target:", &menu.objective_gold_target, params, MENU_RECT);
        }
    ui_end_container(ui);

    // Buttons
    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_OBJECTIVE_CLOSED;
    }

    ui_element_position(ui, ui_button_position_frame_bottom_right(MENU_RECT, "Save"));
    if (ui_button(ui, "Save")) {
        menu.mode = EDITOR_MENU_OBJECTIVE_SAVE;
    }
}

Objective editor_menu_objective_get_objective(const EditorMenuObjective& menu) {
    Objective objective;
    objective.type = (ObjectiveType)menu.objective_type;

    switch (menu.objective_type) {
        case OBJECTIVE_TYPE_GOLD: {
            objective.gold.target = menu.objective_gold_target;
            break;
        }
        default:
            break;
    }

    return objective;
}

#endif
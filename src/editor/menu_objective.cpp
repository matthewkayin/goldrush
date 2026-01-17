#include "menu_objective.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"
#include <algorithm>

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

static const UiSliderParams OBJECTIVE_COUNTER_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 0,
    .max = 10,
    .step = 1
};

static const uint32_t VISIBLE_ROW_COUNT = 7;
static const int ICON_ROW_SIZE = 8;
static const uint32_t OBJECTIVE_INDEX_NONE = UINT32_MAX;

void editor_menu_objective_save(const EditorMenuObjective& menu, Scenario* scenario);

EditorMenuObjective editor_menu_objective_open(const Scenario* scenario) {
    EditorMenuObjective menu;
    menu.objective_index = OBJECTIVE_INDEX_NONE;

    for (uint32_t objective_index = 0; objective_index < scenario->objectives.size(); objective_index++) {
        menu.text_values.push_back(std::string(scenario->objectives[objective_index].description));
    }

    for (uint32_t objective_counter_type = 0; objective_counter_type < OBJECTIVE_COUNTER_TYPE_COUNT; objective_counter_type++) {
        menu.objective_counter_type_dropdown_items.push_back(objective_counter_type_str((ObjectiveCounterType)objective_counter_type));
    }

    menu.scroll = 0;

    return menu;
}

void editor_menu_objective_update(EditorMenuObjective& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario) {
    editor_menu_header(ui, MENU_RECT, "Edit Objectives");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        if (menu.objective_index == OBJECTIVE_INDEX_NONE) {
            const SpriteInfo& edit_button_info = render_get_sprite_info(SPRITE_UI_EDITOR_EDIT);

            for (uint32_t index = 0; index < VISIBLE_ROW_COUNT; index++) {
                uint32_t objective_index = (uint32_t)menu.scroll + index;

                if (objective_index < scenario->objectives.size()) {
                    ui_begin_row(ui, ivec2(0, 0), 4);
                        char prompt[8];
                        sprintf(prompt, "%u: ", objective_index);
                        ui_text_input(ui, prompt, ivec2(MENU_RECT.w - 64, 24), &menu.text_values[objective_index], OBJECTIVE_TITLE_BUFFER_LENGTH - 1);

                        ui_element_size(ui, ivec2(edit_button_info.frame_width, edit_button_info.frame_height));
                        ui_element_position(ui, ivec2(0, 3));
                        if (ui_sprite_button(ui, SPRITE_UI_EDITOR_EDIT, false, false)) {
                            editor_menu_objective_save(menu, scenario);
                            menu.objective_index = objective_index;
                        }

                        ui_element_size(ui, ivec2(edit_button_info.frame_width, edit_button_info.frame_height));
                        ui_element_position(ui, ivec2(0, 3));
                        if (ui_sprite_button(ui, SPRITE_UI_EDITOR_TRASH, false, false)) {
                            scenario->objectives.erase(scenario->objectives.begin() + objective_index);
                            menu.text_values.erase(menu.text_values.begin() + objective_index);
                            index--;
                        }
                    ui_end_container(ui);
                } else if (objective_index == scenario->objectives.size()) {
                    if (ui_button(ui, "+ Objective")) {
                        Objective new_objective = objective_init();
                        scenario->objectives.push_back(new_objective);
                        menu.text_values.push_back(std::string(new_objective.description));
                    }
                }
            }

            menu.scroll = std::clamp(menu.scroll - input_get_mouse_scroll(), 0, std::max(0, (int)(scenario->objectives.size() + 1) - (int)VISIBLE_ROW_COUNT));
        } else {
            Objective& objective = scenario->objectives[menu.objective_index];

            // Objective text
            char objective_text[128];
            sprintf(objective_text, "Objective %u: %s", menu.objective_index, objective.description);
            ui_text(ui, FONT_HACK_GOLD, objective_text);

            // Counter type
            uint32_t counter_type = objective.counter_type;
            if (editor_menu_dropdown(ui, "Counter Type:", &counter_type, menu.objective_counter_type_dropdown_items, MENU_RECT)) {
                objective.counter_type = (ObjectiveCounterType)counter_type;
                if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_ENTITY) {
                    objective.counter_value = 0;
                }
            }

            // Counter value variable
            if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_VARIABLE) {
                editor_menu_slider(ui, "Counter Value:", &objective.counter_value, OBJECTIVE_COUNTER_SLIDER_PARAMS, MENU_RECT);
            } 

            // Counter value entity
            if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_ENTITY) {
                char entity_type_text[64];
                sprintf(entity_type_text, "Counter Entity Type: %s", entity_get_data((EntityType)objective.counter_value).name);
                ui_text(ui, FONT_HACK_GOLD, entity_type_text);

                uint32_t row_count = ENTITY_TYPE_COUNT / ICON_ROW_SIZE;
                if (ENTITY_TYPE_COUNT % ICON_ROW_SIZE != 0) {
                    row_count++;
                }

                for (uint32_t row = 0; row < row_count; row++) {
                    ui_begin_row(ui, ivec2(4, 0), 2);
                        for (uint32_t col = 0; col < ICON_ROW_SIZE; col++) {
                            uint32_t entity_type_index = col + (row * ICON_ROW_SIZE);
                            if (entity_type_index >= ENTITY_TYPE_COUNT) {
                                continue;
                            }

                            SpriteName icon = entity_get_data((EntityType)entity_type_index).icon;
                            if (ui_icon_button(ui, icon, objective.counter_value == entity_type_index)) {
                                objective.counter_value = entity_type_index;
                            }
                        }
                    ui_end_container(ui);
                }
            }

            // Counter target
            editor_menu_slider(ui, "Counter Target:", &objective.counter_target, OBJECTIVE_COUNTER_SLIDER_PARAMS, MENU_RECT);
        }
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT || mode == EDITOR_MENU_MODE_CLOSED) {
        if (menu.objective_index == OBJECTIVE_INDEX_NONE) {
            editor_menu_objective_save(menu, scenario);
        } else {
            menu.objective_index = OBJECTIVE_INDEX_NONE;
            mode = EDITOR_MENU_MODE_OPEN;
        }
    }
}

void editor_menu_objective_save(const EditorMenuObjective& menu, Scenario* scenario) {
    for (uint32_t objective_index = 0; objective_index < scenario->objectives.size(); objective_index++) {
        strncpy(scenario->objectives[objective_index].description, menu.text_values[objective_index].c_str(), OBJECTIVE_TITLE_BUFFER_LENGTH);
    }
}

#endif
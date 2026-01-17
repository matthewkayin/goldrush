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

static const uint32_t VISIBLE_ROW_COUNT = 7;

void editor_menu_objective_save(const EditorMenuObjective& menu, Scenario* scenario);

EditorMenuObjective editor_menu_objective_open(const Scenario* scenario) {
    EditorMenuObjective menu;
    menu.objective_index = 0;

    for (uint32_t objective_index = 0; objective_index < scenario->objectives.size(); objective_index++) {
        menu.text_values.push_back(std::string(scenario->objectives[objective_index].description));
    }

    menu.scroll = 0;

    return menu;
}

void editor_menu_objective_update(EditorMenuObjective& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario) {
    editor_menu_header(ui, MENU_RECT, "Edit Objectives");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        for (uint32_t index = 0; index < VISIBLE_ROW_COUNT; index++) {
            uint32_t objective_index = (uint32_t)menu.scroll + index;

            if (objective_index < scenario->objectives.size()) {
                ui_begin_row(ui, ivec2(0, 0), 4);
                    char prompt[8];
                    sprintf(prompt, "%u: ", objective_index);
                    ui_text_input(ui, prompt, ivec2(MENU_RECT.w - 64, 24), &menu.text_values[objective_index], OBJECTIVE_TITLE_BUFFER_LENGTH - 1);

                    ui_element_position(ui, ivec2(0, 3));
                    if (ui_sprite_button(ui, SPRITE_UI_EDITOR_TRASH, false, false)) {
                        scenario->objectives.erase(scenario->objectives.begin() + objective_index);
                        menu.text_values.erase(menu.text_values.begin() + objective_index);
                        index--;
                    }
                ui_end_container(ui);
            } else if (objective_index == scenario->objectives.size()) {
                if (ui_button(ui, "+ Objective")) {
                    Objective new_objective;
                    new_objective.is_finished = false;
                    sprintf(new_objective.description, "");
                    scenario->objectives.push_back(new_objective);
                    menu.text_values.push_back(std::string(new_objective.description));
                }
            }
        }
    ui_end_container(ui);

    menu.scroll = std::clamp(menu.scroll - input_get_mouse_scroll(), 0, std::max(0, (int)(scenario->objectives.size() + 1) - (int)VISIBLE_ROW_COUNT));

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT || mode == EDITOR_MENU_MODE_CLOSED) {
        editor_menu_objective_save(menu, scenario);
    }
}

void editor_menu_objective_save(const EditorMenuObjective& menu, Scenario* scenario) {
    for (uint32_t objective_index = 0; objective_index < scenario->objectives.size(); objective_index++) {
        strncpy(scenario->objectives[objective_index].description, menu.text_values[objective_index].c_str(), OBJECTIVE_TITLE_BUFFER_LENGTH);
    }
}

#endif
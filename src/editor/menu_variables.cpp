#include "menu_variables.h"

#ifdef GOLD_DEBUG

#include <algorithm>

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

static const uint32_t VISIBLE_ROW_COUNT = 7;

EditorMenuVariables editor_menu_variables_open(const Scenario* scenario) {
    EditorMenuVariables menu;

    for (uint32_t variable_index = 0; variable_index < scenario->variables.size(); variable_index++) {
        menu.text_values.push_back(std::string(scenario->variables[variable_index].name));
    }
    menu.scroll = 0;

    return menu;
}

void editor_menu_variables_update(EditorMenuVariables& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario) {
    editor_menu_header(ui, MENU_RECT, "Edit Variables");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        for (uint32_t index = 0; index < VISIBLE_ROW_COUNT; index++) {
            uint32_t variable_index = (uint32_t)menu.scroll + index;
            if (variable_index < scenario->variables.size()) {
                char prompt[8];
                sprintf(prompt, "%u: ", variable_index);
                ui_text_input(ui, prompt, ivec2(MENU_RECT.w - 32, 24), &menu.text_values[variable_index], SCENARIO_VARIABLE_NAME_BUFFER_LENGTH - 1);
            } else if (variable_index == scenario->variables.size()) {
                if (ui_button(ui, "+ Variable")) {
                    ScenarioVariable variable;
                    sprintf(variable.name, "");
                    variable.type = SCENARIO_VARIABLE_TYPE_SQUAD;
                    variable.squad.player_id = 0;
                    variable.squad.squad_id = 0;
                    scenario->variables.push_back(variable);
                    menu.text_values.push_back(std::string(variable.name));
                }
            }
        }
    ui_end_container(ui);

    menu.scroll = std::clamp(menu.scroll - input_get_mouse_scroll(), 0, std::max(0, (int)(scenario->variables.size() + 1) - (int)VISIBLE_ROW_COUNT));

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT || mode == EDITOR_MENU_MODE_CLOSED) {
        for (uint32_t variable_index = 0; variable_index < scenario->variables.size(); variable_index++) {
            strncpy(scenario->variables[variable_index].name, menu.text_values[variable_index].c_str(), SCENARIO_VARIABLE_NAME_BUFFER_LENGTH);
        }
    }
}

#endif
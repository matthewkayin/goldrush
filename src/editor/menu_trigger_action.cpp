#include "menu_trigger_action.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

static const UiSliderParams ACTION_WAIT_SECONDS_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 1,
    .max = 60,
    .step = 1
};

void editor_menu_trigger_action_set_action_type(EditorMenuTriggerAction& menu, TriggerActionType action_type);

EditorMenuTriggerAction editor_menu_trigger_action_open(const Scenario* scenario, const TriggerAction& action, uint32_t action_index) {
    EditorMenuTriggerAction menu;
    menu.action = action;
    menu.action_index = action_index;

    for (uint32_t effect_type = 0; effect_type < TRIGGER_ACTION_TYPE_COUNT; effect_type++) {
        menu.action_type_items.push_back(trigger_action_type_str((TriggerActionType)effect_type));
    }

    for (uint32_t objective_index = 0; objective_index < scenario->objectives.size(); objective_index++) {
        char text[64];
        sprintf(text, "%u: %s", objective_index, scenario->objectives[objective_index].description);
        menu.objective_items.push_back(std::string(text));
    }

    if (action.type == TRIGGER_ACTION_TYPE_HINT) {
        menu.hint_value = std::string(action.hint.message);
    }

    return menu;
}

void editor_menu_trigger_action_update(EditorMenuTriggerAction& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Action");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        uint32_t action_type = menu.action.type;
        if (editor_menu_dropdown(ui, "Action Type:", &action_type, menu.action_type_items, MENU_RECT)) {
            editor_menu_trigger_action_set_action_type(menu, (TriggerActionType)action_type);
            if (menu.action.type == TRIGGER_ACTION_TYPE_HINT) {
                menu.hint_value = std::string(menu.action.hint.message);
            }
        }

        switch (menu.action.type) {
            case TRIGGER_ACTION_TYPE_HINT: {
                ui_text_input(ui, "Message: ", ivec2(MENU_RECT.w - 32, 24), &menu.hint_value, TRIGGER_ACTION_HINT_MESSAGE_BUFFER_LENGTH - 1);

                break;
            }
            case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE: {
                editor_menu_dropdown(ui, "Objective:", &menu.action.add_objective.objective_index, menu.objective_items, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_FINISH_OBJECTIVE: {
                editor_menu_dropdown(ui, "Objective:", &menu.action.finish_objective.objective_index, menu.objective_items, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_WAIT: {
                editor_menu_slider(ui, "Seconds:", &menu.action.wait.seconds, ACTION_WAIT_SECONDS_SLIDER_PARAMS, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT) {
        if (menu.action.type == TRIGGER_ACTION_TYPE_HINT) {
            strncpy(menu.action.hint.message, menu.hint_value.c_str(), TRIGGER_ACTION_HINT_MESSAGE_BUFFER_LENGTH - 1);
        }
    }
}

void editor_menu_trigger_action_set_action_type(EditorMenuTriggerAction& menu, TriggerActionType action_type) {
    TriggerAction action;
    action.type = action_type;

    switch (action_type) {
        case TRIGGER_ACTION_TYPE_HINT: {
            sprintf(action.hint.message, "");
            break;
        }
        case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE: {
            action.add_objective.objective_index = 0;
            break;
        }
        case TRIGGER_ACTION_TYPE_FINISH_OBJECTIVE: {
            action.finish_objective.objective_index = 0;
            break;
        }
        case TRIGGER_ACTION_TYPE_WAIT: {
            action.wait.seconds = 1;
            break;
        }
        case TRIGGER_ACTION_TYPE_COUNT: 
            GOLD_ASSERT(false);
            break;
    }

    menu.action = action;
}

#endif
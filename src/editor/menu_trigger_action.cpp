#include "menu_trigger_action.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 264
};

static const Rect SIDE_MENU_RECT = (Rect) {
    .x = MENU_RECT.x + MENU_RECT.w + 4,
    .y = MENU_RECT.y,
    .w = 116,
    .h = MENU_RECT.h
};

static const UiSliderParams ACTION_WAIT_SECONDS_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 1,
    .max = 60,
    .step = 1
};

static const std::vector<std::string> YES_NO_ITEMS = { "No", "Yes" };

static const uint32_t ICON_ROW_SIZE = 8U;

void editor_menu_trigger_action_set_action_type(EditorMenuTriggerAction& menu, TriggerActionType action_type);

EditorMenuTriggerAction editor_menu_trigger_action_open(const Scenario* scenario, const TriggerAction& action, uint32_t action_index) {
    EditorMenuTriggerAction menu;
    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_NONE;
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

    for (uint32_t message_type = 0; message_type < TRIGGER_ACTION_CHAT_PREFIX_TYPE_COUNT; message_type++) {
        menu.message_type_items.push_back(trigger_action_chat_prefix_type_str((TriggerActionChatPrefixType)message_type));
    }

    for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
        char text[64];
        sprintf(text, "%u: %s", player_id, scenario->players[player_id].name);
        menu.spawn_units_player_items.push_back(std::string(text));
    }

    if (action.type == TRIGGER_ACTION_TYPE_CHAT) {
        menu.chat_prefix_value = std::string(action.chat.prefix);
        menu.chat_message_value = std::string(action.chat.message);
    }

    return menu;
}

void editor_menu_trigger_action_update(EditorMenuTriggerAction& menu, UI& ui, EditorMenuMode& mode) {
    if (menu.request != EDITOR_MENU_TRIGGER_ACTION_REQUEST_NONE) {
        return;
    }

    editor_menu_header(ui, MENU_RECT, "Edit Action");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        uint32_t action_type = menu.action.type;
        if (editor_menu_dropdown(ui, "Action Type:", &action_type, menu.action_type_items, MENU_RECT)) {
            editor_menu_trigger_action_set_action_type(menu, (TriggerActionType)action_type);
            if (menu.action.type == TRIGGER_ACTION_TYPE_CHAT) {
                menu.chat_prefix_value = std::string(menu.action.chat.prefix);
                menu.chat_message_value = std::string(menu.action.chat.message);
            }
        }

        switch (menu.action.type) {
            case TRIGGER_ACTION_TYPE_CHAT: {
                uint32_t prefix_type = menu.action.chat.prefix_type;
                if (editor_menu_dropdown(ui, "Prefix Type:", &prefix_type, menu.message_type_items, MENU_RECT)) {
                    menu.action.chat.prefix_type = (TriggerActionChatPrefixType)prefix_type;
                }
                if (menu.action.chat.prefix_type != TRIGGER_ACTION_CHAT_PREFIX_TYPE_NONE) {
                    ui_text_input(ui, "Prefix: ", ivec2(MENU_RECT.w - 32, 24), &menu.chat_prefix_value, TRIGGER_ACTION_CHAT_PREFIX_BUFFER_LENGTH - 1);
                }
                ui_text_input(ui, "Message: ", ivec2(MENU_RECT.w - 32, 24), &menu.chat_message_value, TRIGGER_ACTION_CHAT_MESSAGE_BUFFER_LENGTH - 1);

                break;
            }
            case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE: {
                editor_menu_dropdown(ui, "Objective:", &menu.action.add_objective.objective_index, menu.objective_items, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_COMPLETE_OBJECTIVE: {
                editor_menu_dropdown(ui, "Objective:", &menu.action.finish_objective.objective_index, menu.objective_items, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_WAIT: {
                editor_menu_slider(ui, "Seconds:", &menu.action.wait.seconds, ACTION_WAIT_SECONDS_SLIDER_PARAMS, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_FOG_REVEAL: {
                char prompt_str[32];
                sprintf(prompt_str, "Cell: <%i, %i> Size: %i Sight: %i", menu.action.fog.cell.x, menu.action.fog.cell.y, menu.action.fog.cell_size, menu.action.fog.sight);
                if (editor_menu_prompt_and_button(ui, prompt_str, "Edit", MENU_RECT)) {
                    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_FOG_REVEAL;
                }

                editor_menu_slider(ui, "Duration:", &menu.action.fog.duration_seconds, ACTION_WAIT_SECONDS_SLIDER_PARAMS, MENU_RECT);
                break;
            }
            case TRIGGER_ACTION_TYPE_ALERT: {
                char prompt_str[32];
                sprintf(prompt_str, "Cell: <%i, %i> Size: %i", menu.action.alert.cell.x, menu.action.alert.cell.y, menu.action.alert.cell_size);
                if (editor_menu_prompt_and_button(ui, prompt_str, "Edit", MENU_RECT)) {
                    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_ALERT;
                }

                break;
            }
            case TRIGGER_ACTION_TYPE_SPAWN_UNITS: {
                // Player dropdown
                uint32_t player_selection = menu.action.spawn_units.player_id - 1;
                if (editor_menu_dropdown(ui, "Player:", &player_selection, menu.spawn_units_player_items, MENU_RECT)) {
                    menu.action.spawn_units.player_id = player_selection + 1;
                }

                // Target cell
                char prompt_str[32];
                sprintf(prompt_str, "Target Cell: <%i, %i>", menu.action.spawn_units.target_cell.x, menu.action.spawn_units.target_cell.y);
                if (editor_menu_prompt_and_button(ui, prompt_str, "Edit", MENU_RECT)) {
                    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_TARGET_CELL;
                }

                // Spawn cell
                sprintf(prompt_str, "Spawn Cell: <%i, %i>", menu.action.spawn_units.spawn_cell.x, menu.action.spawn_units.spawn_cell.y);
                if (editor_menu_prompt_and_button(ui, prompt_str, "Edit", MENU_RECT)) {
                    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_SPAWN_CELL;
                }

                // Units
                ui_text(ui, FONT_HACK_GOLD, "Current Units:");
                for (uint32_t row = 0; row < 3; row++) {
                    ui_begin_row(ui, ivec2(0, 0), 2);
                        for (uint32_t col = 0; col < ICON_ROW_SIZE; col++) {
                            uint32_t index = col + (row * ICON_ROW_SIZE);
                            if (index >= menu.action.spawn_units.entity_count) {
                                continue;
                            }

                            EntityType entity_type = menu.action.spawn_units.entity_types[index];
                            if (ui_icon_button(ui, entity_get_data(entity_type).icon, false)) {
                                for (uint32_t forward_index = index + 1; forward_index < menu.action.spawn_units.entity_count; forward_index++) {
                                    menu.action.spawn_units.entity_types[forward_index - 1] = menu.action.spawn_units.entity_types[forward_index];
                                }
                                menu.action.spawn_units.entity_count--;
                            }
                        }
                    ui_end_container(ui);
                }
                break;
            }
            case TRIGGER_ACTION_TYPE_SET_LOSE_CONDITION: {
                uint32_t lose_on_buildings_destroyed = (uint32_t)menu.action.set_lose_condition.lose_on_buildings_destroyed;
                if (editor_menu_dropdown(ui, "Lose on Buildings Destroyed:", &lose_on_buildings_destroyed, YES_NO_ITEMS, MENU_RECT)) {
                    menu.action.set_lose_condition.lose_on_buildings_destroyed = (bool)lose_on_buildings_destroyed;
                }

                break;
            }
            case TRIGGER_ACTION_TYPE_SHOW_ENEMY_GOLD:
            case TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES:
                break;
            case TRIGGER_ACTION_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }
    ui_end_container(ui);

    // Side menu
    if (menu.action.type == TRIGGER_ACTION_TYPE_SPAWN_UNITS) {
        ui_frame_rect(ui, SIDE_MENU_RECT);
        ui_begin_column(ui, ivec2(SIDE_MENU_RECT.x + 8, SIDE_MENU_RECT.y + 30), 4);

            ui_text(ui, FONT_HACK_GOLD, "Add Units:");
            const uint32_t unit_type_count = ENTITY_HALL - ENTITY_MINER;
            for (uint32_t row = 0; row < 4; row++) {
                ui_begin_row(ui, ivec2(0, 0), 2);
                    for (uint32_t col = 0; col < 3; col++) {
                        uint32_t index = col + (row * 3);
                        if (index >= unit_type_count) {
                            continue;
                        }

                        EntityType entity_type = (EntityType)(ENTITY_MINER + index);
                        if (ui_icon_button(ui, entity_get_data(entity_type).icon, false) && menu.action.spawn_units.entity_count < TRIGGER_ACTION_SPAWN_UNITS_ENTITY_COUNT_MAX) {
                            menu.action.spawn_units.entity_types[menu.action.spawn_units.entity_count] = entity_type;
                            menu.action.spawn_units.entity_count++;
                        }
                    }
                ui_end_container(ui);
            }
        ui_end_container(ui);
    }

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT) {
        if (menu.action.type == TRIGGER_ACTION_TYPE_CHAT) {
            if (menu.action.chat.prefix_type == TRIGGER_ACTION_CHAT_PREFIX_TYPE_NONE) {
                sprintf(menu.action.chat.prefix, "");
            } else {
                strncpy(menu.action.chat.prefix, menu.chat_prefix_value.c_str(), TRIGGER_ACTION_CHAT_PREFIX_BUFFER_LENGTH - 1);
            }
            strncpy(menu.action.chat.message, menu.chat_message_value.c_str(), TRIGGER_ACTION_CHAT_MESSAGE_BUFFER_LENGTH - 1);
        }
    }
}

void editor_menu_trigger_action_set_fog_cell(EditorMenuTriggerAction& menu, ivec2 cell, int cell_size, int sight) {
    menu.action.fog.cell = cell;
    menu.action.fog.cell_size = cell_size;
    menu.action.fog.sight = sight;
    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_NONE;
}

void editor_menu_trigger_action_set_alert(EditorMenuTriggerAction& menu, ivec2 cell, int cell_size) {
    menu.action.alert.cell = cell;
    menu.action.alert.cell_size = cell_size;
    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_NONE;
}

void editor_menu_trigger_action_set_request_cell(EditorMenuTriggerAction& menu, ivec2 cell) {
    if (menu.request == EDITOR_MENU_TRIGGER_ACTION_REQUEST_SPAWN_CELL) {
        menu.action.spawn_units.spawn_cell = cell;
    } else if (menu.request == EDITOR_MENU_TRIGGER_ACTION_REQUEST_TARGET_CELL) {
        menu.action.spawn_units.target_cell = cell;
    }
    menu.request = EDITOR_MENU_TRIGGER_ACTION_REQUEST_NONE;
}

void editor_menu_trigger_action_set_action_type(EditorMenuTriggerAction& menu, TriggerActionType action_type) {
    TriggerAction action;
    action.type = action_type;

    switch (action_type) {
        case TRIGGER_ACTION_TYPE_CHAT: {
            action.chat.prefix_type = TRIGGER_ACTION_CHAT_PREFIX_TYPE_NONE;
            sprintf(action.chat.prefix, "");
            sprintf(action.chat.message, "");
            break;
        }
        case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE: {
            action.add_objective.objective_index = 0;
            break;
        }
        case TRIGGER_ACTION_TYPE_COMPLETE_OBJECTIVE: {
            action.finish_objective.objective_index = 0;
            break;
        }
        case TRIGGER_ACTION_TYPE_WAIT: {
            action.wait.seconds = 1;
            break;
        }
        case TRIGGER_ACTION_TYPE_FOG_REVEAL: {
            action.fog.cell = ivec2(10, 10);
            action.fog.cell_size = 1;
            action.fog.sight = 4;
            action.fog.duration_seconds = 3;
            break;
        }
        case TRIGGER_ACTION_TYPE_ALERT: {
            action.alert.cell = ivec2(10, 10);
            action.alert.cell_size = 1;
            break;
        }
        case TRIGGER_ACTION_TYPE_SPAWN_UNITS: {
            action.spawn_units.player_id = 1;
            action.spawn_units.spawn_cell = ivec2(0, 0);
            action.spawn_units.target_cell = ivec2(0, 0);
            action.spawn_units.entity_count = 0;
            break;
        }
        case TRIGGER_ACTION_TYPE_SET_LOSE_CONDITION: {
            action.set_lose_condition.lose_on_buildings_destroyed = true;
        }
        case TRIGGER_ACTION_TYPE_SHOW_ENEMY_GOLD:
        case TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES:
            break;
        case TRIGGER_ACTION_TYPE_COUNT: 
            GOLD_ASSERT(false);
            break;
    }

    menu.action = action;
}

#endif
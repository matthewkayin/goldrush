#include "menu_trigger_condition.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

static const UiSliderParams ENTITY_COUNT_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 1,
    .max = 20,
    .step = 1,
};

void editor_menu_trigger_condition_set_condition_type(EditorMenuTriggerCondition& menu, TriggerConditionType condition_type);
void editor_menu_trigger_condition_entity_type_picker(UI& ui, EntityType& entity_type);

EditorMenuTriggerCondition editor_menu_trigger_condition_open(const Scenario* scenario, const TriggerCondition& condition, uint32_t condition_index) {
    EditorMenuTriggerCondition menu;
    menu.condition = condition;
    menu.condition_index = condition_index;
    
    for (uint32_t condition_type = 0; condition_type < TRIGGER_CONDITION_TYPE_COUNT; condition_type++) {
        menu.condition_type_items.push_back(
            std::string(trigger_condition_type_str((TriggerConditionType)condition_type))
        );
    }

    for (uint32_t objective_index = 0; objective_index < scenario->objectives.size(); objective_index++) {
        char objective_text[64];
        sprintf(objective_text, "%u: %s", objective_index, scenario->objectives[objective_index].description);
        menu.objective_dropdown_items.push_back(std::string(objective_text));
    }

    return menu;
}

void editor_menu_trigger_condition_update(EditorMenuTriggerCondition& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Condition");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        uint32_t condition_type = menu.condition.type;
        if (editor_menu_dropdown(ui, "Condition Type:", &condition_type, menu.condition_type_items, MENU_RECT)) {
            editor_menu_trigger_condition_set_condition_type(menu, (TriggerConditionType)condition_type);
        }

        switch (menu.condition.type) {
            case TRIGGER_CONDITION_TYPE_ENTITY_COUNT: {
                char text[128];
                sprintf(text, "Entity Type: %s", entity_get_data(menu.condition.entity_count.entity_type).name);
                ui_text(ui, FONT_HACK_GOLD, text);

                editor_menu_trigger_condition_entity_type_picker(ui, menu.condition.entity_count.entity_type);
                editor_menu_slider(ui, "Entity Count: ", &menu.condition.entity_count.entity_count, ENTITY_COUNT_SLIDER_PARAMS, MENU_RECT);

                break;
            }
            case TRIGGER_CONDITION_TYPE_OBJECTIVE_COMPLETE: {
                editor_menu_dropdown(ui, "Objective:", &menu.condition.objective_complete.objective_index, menu.objective_dropdown_items, MENU_RECT);
                break;
            }
            case TRIGGER_CONDITION_TYPE_COUNT:
                GOLD_ASSERT(false);
                break;
        }        
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);
}

void editor_menu_trigger_condition_set_condition_type(EditorMenuTriggerCondition& menu, TriggerConditionType condition_type) {
    TriggerCondition condition;
    condition.type = condition_type;

    switch (condition_type) {
        case TRIGGER_CONDITION_TYPE_ENTITY_COUNT: {
            condition.entity_count.entity_type = ENTITY_MINER;
            condition.entity_count.entity_count = 1;
            break;
        }
        case TRIGGER_CONDITION_TYPE_OBJECTIVE_COMPLETE: {
            condition.objective_complete.objective_index = 0;
            break;
        }
        case TRIGGER_CONDITION_TYPE_COUNT:
            GOLD_ASSERT(false);
            break;
    }

    menu.condition = condition;
}

void editor_menu_trigger_condition_entity_type_picker(UI& ui, EntityType& entity_type) {
    const uint32_t ICON_ROW_SIZE = 8;
    uint32_t row_count = ENTITY_TYPE_COUNT / ICON_ROW_SIZE;
    if (ENTITY_TYPE_COUNT % ICON_ROW_SIZE != 0) {
        row_count++;
    }

    for (uint32_t row = 0; row < row_count; row++) {
        ui_begin_row(ui, ivec2(4, 0), 2);
            for (uint32_t col = 0; col < ICON_ROW_SIZE; col++) {
                uint32_t entity_type_index =  col + (row * ICON_ROW_SIZE);
                if (entity_type_index >= ENTITY_TYPE_COUNT) {
                    continue;
                }

                SpriteName icon = entity_get_data((EntityType)entity_type_index).icon;
                if (ui_icon_button(ui, icon, entity_type == entity_type_index)) {
                    entity_type = (EntityType)entity_type_index;
                }
            }
        ui_end_container(ui);
    }
}

#endif
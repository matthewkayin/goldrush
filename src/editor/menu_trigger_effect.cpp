#include "menu_trigger_effect.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

void editor_menu_trigger_effect_set_effect_type(EditorMenuTriggerEffect& menu, TriggerEffectType effect_type);

EditorMenuTriggerEffect editor_menu_trigger_effect_open(const TriggerEffect& effect, uint32_t effect_index) {
    EditorMenuTriggerEffect menu;
    menu.effect = effect;
    menu.effect_index = effect_index;

    for (uint32_t effect_type = 0; effect_type < TRIGGER_EFFECT_TYPE_COUNT; effect_type++) {
        menu.effect_type_items.push_back(trigger_effect_type_str((TriggerEffectType)effect_type));
    }

    if (effect.type == TRIGGER_EFFECT_TYPE_HINT) {
        menu.hint_value = std::string(effect.hint.message);
    }

    return menu;
}

void editor_menu_trigger_effect_update(EditorMenuTriggerEffect& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Effect");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        uint32_t effect_type = menu.effect.type;
        if (editor_menu_dropdown(ui, "Effect Type:", &effect_type, menu.effect_type_items, MENU_RECT)) {
            editor_menu_trigger_effect_set_effect_type(menu, (TriggerEffectType)effect_type);
            if (menu.effect.type == TRIGGER_EFFECT_TYPE_HINT) {
                menu.hint_value = std::string(menu.effect.hint.message);
            }
        }

        switch (menu.effect.type) {
            case TRIGGER_EFFECT_TYPE_HINT: {
                ui_text_input(ui, "Message: ", ivec2(MENU_RECT.w - 32, 24), &menu.hint_value, TRIGGER_EFFECT_HINT_MESSAGE_MAX_LENGTH - 1);

                break;
            }
            case TRIGGER_EFFECT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    if (mode == EDITOR_MENU_MODE_SUBMIT) {
        if (menu.effect.type == TRIGGER_EFFECT_TYPE_HINT) {
            strncpy(menu.effect.hint.message, menu.hint_value.c_str(), TRIGGER_EFFECT_HINT_MESSAGE_MAX_LENGTH - 1);
        }
    }
}

void editor_menu_trigger_effect_set_effect_type(EditorMenuTriggerEffect& menu, TriggerEffectType effect_type) {
    TriggerEffect effect;
    effect.type = effect_type;

    switch (effect_type) {
        case TRIGGER_EFFECT_TYPE_HINT: {
            sprintf(effect.hint.message, "");
            break;
        }
        case TRIGGER_EFFECT_TYPE_COUNT: 
            GOLD_ASSERT(false);
            break;
    }

    menu.effect = effect;
}

#endif
#include "menu_allowed_tech.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 256
};

static const int ICON_ROW_SIZE = 8;

EditorMenuAllowedTech editor_menu_allowed_tech_open(const Scenario* scenario) {
    EditorMenuAllowedTech menu;
    memcpy(menu.allowed_entities, scenario->allowed_entities, sizeof(menu.allowed_entities));
    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        uint32_t flag = 1U << upgrade_index;
        menu.allowed_upgrades[upgrade_index] = (scenario->allowed_upgrades & flag) == flag;
    }

    return menu;
}

void editor_menu_allowed_tech_update(EditorMenuAllowedTech& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Allowed Tech");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        const uint32_t TECH_COUNT = ENTITY_TYPE_COUNT + UPGRADE_COUNT;
        uint32_t row_count = TECH_COUNT / ICON_ROW_SIZE;
        if (TECH_COUNT % ICON_ROW_SIZE != 0) {
            row_count++;
        }

        for (uint32_t row = 0; row < row_count; row++) {
            ui_begin_row(ui, ivec2(4, 0), 2);
                for (uint32_t col = 0; col < ICON_ROW_SIZE; col++) {
                    uint32_t tech_index = col + (row * ICON_ROW_SIZE);
                    if (tech_index >= TECH_COUNT) {
                        continue;
                    }

                    const SpriteName icon = tech_index <= ENTITY_TYPE_COUNT
                        ? entity_get_data((EntityType)tech_index).icon
                        : upgrade_get_data(tech_index - ENTITY_TYPE_COUNT).icon;
                    bool* is_tech_allowed = tech_index <= ENTITY_TYPE_COUNT
                        ? menu.allowed_entities + tech_index
                        : menu.allowed_upgrades + (tech_index - ENTITY_TYPE_COUNT);
                    if (ui_icon_button(ui, icon, *is_tech_allowed)) {
                        *is_tech_allowed = !(*is_tech_allowed);
                    }
                }
            ui_end_container(ui);
        }
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);
}

#endif
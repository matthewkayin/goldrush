#include "menu_map_size.h"

#ifdef GOLD_DEBUG

#include "core/match_setting.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 164
};

EditorMenuMapSize editor_menu_map_size_open(int map_width) {
    EditorMenuMapSize menu;

    uint32_t map_size;
    for (map_size = 0; map_size < MAP_SIZE_COUNT; map_size++) {
        if (match_setting_get_map_size((MapSize)map_size) == map_width) {
            break;
        }
    }
    GOLD_ASSERT(map_size != MAP_SIZE_COUNT);
    menu.map_size = map_size;

    return menu;
}

void editor_menu_map_size_update(EditorMenuMapSize& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Map Size");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Map size
        editor_menu_dropdown(ui, "Map Size:", &menu.map_size, match_setting_data(MATCH_SETTING_MAP_SIZE).values, MENU_RECT);
    ui_end_container(ui);
    
    editor_menu_back_save_buttons(ui, MENU_RECT, mode);
}

#endif
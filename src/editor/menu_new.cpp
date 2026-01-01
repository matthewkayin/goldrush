#include "menu_new.h"

#ifdef GOLD_DEBUG

#include "editor/ui.h"

static const Rect MENU_NEW_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 256
};

void editor_menu_new_set_map_type(EditorMenuNew& menu, MapType map_type);

EditorMenuNew editor_menu_new_open() {
    EditorMenuNew menu;
    menu.mode = EDITOR_MENU_NEW_OPEN;
    editor_menu_new_set_map_type(menu, MAP_TYPE_TOMBSTONE);
    menu.map_size = MAP_SIZE_SMALL;
    menu.use_noise_gen_params = 0;

    return menu;
}

void editor_menu_new_update(EditorMenuNew& menu, UI& ui) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_NEW_RECT);

    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "New Map");
    ui_element_position(ui, ivec2(MENU_NEW_RECT.x + (MENU_NEW_RECT.w / 2) - (header_text_size.x / 2), MENU_NEW_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, "New Map");

    ui_begin_column(ui, ivec2(MENU_NEW_RECT.x + 8, MENU_NEW_RECT.y + 30), 4);
        // Map type
        if (editor_menu_dropdown(ui, "Map Type:", &menu.map_type, match_setting_data(MATCH_SETTING_MAP_TYPE).values, MENU_NEW_RECT)) {
            editor_menu_new_set_map_type(menu, (MapType)menu.map_type);
        }

        // Map size
        editor_menu_dropdown(ui, "Map Size:", &menu.map_size, match_setting_data(MATCH_SETTING_MAP_SIZE).values, MENU_NEW_RECT);

        // Use noise gen params
        editor_menu_dropdown(ui, "Generation Style:", &menu.use_noise_gen_params, { "Blank", "Noise" }, MENU_NEW_RECT);

        // Noise gen params
        if (menu.use_noise_gen_params) {
            editor_menu_dropdown(ui, "Noise Inverted:", &menu.noise_gen_inverted, { "No", "Yes" }, MENU_NEW_RECT);
            editor_menu_slider(ui, menu.noise_gen_inverted ? "Highground Threshold:" : "Water Threshold:", &menu.noise_gen_water_threshold, MENU_NEW_RECT);
            editor_menu_slider(ui, "Lowground Threshold:", &menu.noise_gen_lowground_threshold, MENU_NEW_RECT);
            if (menu.map_type == MAP_TYPE_KLONDIKE) {
                editor_menu_slider(ui, "Forest Threshold:", &menu.noise_gen_forest_threshold, MENU_NEW_RECT);
            }
        }
    ui_end_container(ui);

    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_NEW_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_NEW_CLOSED;
    }
    ui_element_position(ui, ui_button_position_frame_bottom_right(MENU_NEW_RECT, "Create"));
    if (ui_button(ui, "Create")) {
        menu.mode = EDITOR_MENU_NEW_CREATE;
    }
}

void editor_menu_new_set_map_type(EditorMenuNew& menu, MapType map_type) {
    menu.map_type = map_type;
    // Set threshold values based on defaults
    NoiseGenParams params = noise_create_noise_gen_params(map_type, MAP_SIZE_SMALL, 0, 0);
    menu.noise_gen_inverted = (uint32_t)params.map_inverted;
    menu.noise_gen_water_threshold = params.water_threshold;
    menu.noise_gen_lowground_threshold = params.lowground_threshold;
    menu.noise_gen_forest_threshold = params.forest_threshold;
}

NoiseGenParams editor_menu_new_create_noise_gen_params(const EditorMenuNew& menu) {
    uint64_t map_seed = rand();
    uint64_t forest_seed = rand();
    NoiseGenParams params = noise_create_noise_gen_params((MapType)menu.map_type, (MapSize)menu.map_size, map_seed, forest_seed);

    params.map_inverted = menu.noise_gen_inverted;
    params.water_threshold = menu.noise_gen_water_threshold;
    params.lowground_threshold = menu.noise_gen_lowground_threshold;
    params.forest_threshold = menu.noise_gen_forest_threshold;

    return params;
}

#endif
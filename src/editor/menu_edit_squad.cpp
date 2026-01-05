#include "menu_edit_squad.h"

#ifdef GOLD_DEBUG

#include "editor/ui.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 256
};

EditorMenuEditSquad editor_menu_edit_squad_open(const ScenarioSquad& squad, const Scenario* scenario) {
    EditorMenuEditSquad menu;
    menu.mode = EDITOR_MENU_EDIT_SQUAD_OPEN;
    menu.squad_name = std::string(squad.name);
    menu.squad_player = squad.player_id - 1;
    menu.squad_type = squad.type;

    for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
        char player_text[64];
        sprintf(player_text, "%u: %s", player_id, scenario->players[player_id].name);
        menu.squad_player_items.push_back(std::string(player_text));
    }

    for (uint32_t squad_type = 0; squad_type < SCENARIO_SQUAD_TYPE_COUNT; squad_type++) {
        menu.squad_type_items.push_back(std::string(scenario_squad_type_str((ScenarioSquadType)squad_type)));
    }

    return menu;
}

void editor_menu_edit_squad_update(EditorMenuEditSquad& menu, UI& ui) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_RECT);
    
    // Header
    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Edit Squad");
    ui_element_position(ui, ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (header_text_size.x / 2), MENU_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, "Edit Squad");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Name
        ui_text_input(ui, "Name: ", ivec2(MENU_RECT.w - 32, 24), &menu.squad_name, MAX_USERNAME_LENGTH);

        // Player dropdown
        editor_menu_dropdown(ui, "Player:", &menu.squad_player, menu.squad_player_items, MENU_RECT);

        // Squad type
        editor_menu_dropdown(ui, "Type:", &menu.squad_type, menu.squad_type_items, MENU_RECT);
    ui_end_container(ui);

    // Buttons
    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_EDIT_SQUAD_CLOSED;
    }

    ui_element_position(ui, ui_button_position_frame_bottom_right(MENU_RECT, "Save"));
    if (ui_button(ui, "Save")) {
        menu.mode = EDITOR_MENU_EDIT_SQUAD_SAVE;
    }
}

#endif
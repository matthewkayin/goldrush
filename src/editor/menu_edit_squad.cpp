#include "menu_edit_squad.h"

#ifdef GOLD_DEBUG

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 64,
    .w = 300,
    .h = 256
};

EditorMenuEditSquad editor_menu_edit_squad_open(const ScenarioSquad& squad, const Scenario* scenario) {
    EditorMenuEditSquad menu;
    menu.squad_name = std::string(squad.name);
    menu.squad_player = squad.player_id - 1;

    for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
        char player_text[64];
        sprintf(player_text, "%u: %s", player_id, scenario->players[player_id].name);
        menu.squad_player_items.push_back(std::string(player_text));
    }

    menu.squad_type_values = {
        BOT_SQUAD_TYPE_DEFEND,
        BOT_SQUAD_TYPE_LANDMINES,
        BOT_SQUAD_TYPE_PATROL,
        BOT_SQUAD_TYPE_HOLD_POSITION
    };

    for (BotSquadType squad_type : menu.squad_type_values) {
        menu.squad_type_items.push_back(std::string(bot_squad_type_str(squad_type)));
    }

    uint32_t index;
    for (index = 0; index < menu.squad_type_values.size(); index++) {
        if (squad.type == menu.squad_type_values[index]) {
            break;
        }
    }
    GOLD_ASSERT(index != menu.squad_type_values.size());
    menu.squad_type_index = index;

    return menu;
}

void editor_menu_edit_squad_update(EditorMenuEditSquad& menu, UI& ui, EditorMenuMode& mode) {
    editor_menu_header(ui, MENU_RECT, "Edit Squad");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        // Name
        ui_text_input(ui, "Name: ", ivec2(MENU_RECT.w - 32, 24), &menu.squad_name, MAX_USERNAME_LENGTH);

        // Player dropdown
        editor_menu_dropdown(ui, "Player:", &menu.squad_player, menu.squad_player_items, MENU_RECT);

        // Squad type
        editor_menu_dropdown(ui, "Type:", &menu.squad_type_index, menu.squad_type_items, MENU_RECT);
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);
}

BotSquadType editor_menu_edit_squad_get_selected_squad_type(const EditorMenuEditSquad& menu) {
    return menu.squad_type_values[menu.squad_type_index];
}

#endif
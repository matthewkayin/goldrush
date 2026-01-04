#include "menu_players.h"

#ifdef GOLD_DEBUG

#include "editor/ui.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 48,
    .w = 300,
    .h = 264
};

static const UiSliderParams STARTING_GOLD_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 0,
    .max = 1500,
    .step = 50
};

EditorMenuPlayers editor_menu_players_open(const EditorDocument* document) {
    EditorMenuPlayers menu;

    menu.mode = EDITOR_MENU_PLAYERS_OPEN;

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        menu.starting_gold[player_id] = document->players[player_id].starting_gold;
        if (player_id != 0) {
            menu.player_names[player_id - 1] = std::string(document->players[player_id].name);
        }
    }

    return menu;
}

void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, EditorDocument* document) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_RECT);

    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Edit Players");
    ui_element_position(ui, ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (header_text_size.x / 2), MENU_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, "Edit Players");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        ui_text(ui, FONT_HACK_GOLD, document->players[0].name);
        editor_menu_slider(ui, "Starting Gold:", &menu.starting_gold[0], STARTING_GOLD_SLIDER_PARAMS, MENU_RECT);

        for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
            char prompt[16];
            sprintf(prompt, "Player %u: ", player_id);
            ui_text_input(ui, prompt, ivec2(MENU_RECT.w - 32, 24), &menu.player_names[player_id - 1], MAX_USERNAME_LENGTH);
            editor_menu_slider(ui, "Starting Gold:", &menu.starting_gold[player_id], STARTING_GOLD_SLIDER_PARAMS, MENU_RECT);
        }
    ui_end_container(ui);

    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_PLAYERS_CLOSED;
        input_stop_text_input();
    }

    ui_element_position(ui, ui_button_position_frame_bottom_right(MENU_RECT, "Save"));
    if (ui_button(ui, "Save")) {
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            document->players[player_id].starting_gold = menu.starting_gold[player_id];
            if (player_id != 0) {
                strncpy(document->players[player_id].name, menu.player_names[player_id - 1].c_str(), MAX_USERNAME_LENGTH);
            }
        }
        menu.mode = EDITOR_MENU_PLAYERS_CLOSED;
        input_stop_text_input();
    }
}

#endif
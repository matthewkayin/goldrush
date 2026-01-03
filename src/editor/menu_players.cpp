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
    .min = 0,
    .max = 1500,
    .step = 10
};

EditorMenuPlayers editor_menu_players_open() {
    EditorMenuPlayers menu;

    menu.mode = EDITOR_MENU_PLAYERS_OPEN;

    return menu;
}

void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, EditorDocument* document) {
    ui.input_enabled = true;
    ui_frame_rect(ui, MENU_RECT);

    ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Edit Players");
    ui_element_position(ui, ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (header_text_size.x / 2), MENU_RECT.y + 6));
    ui_text(ui, FONT_HACK_GOLD, "Edit Players");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        ui_text(ui, FONT_HACK_GOLD, document->players[0].name.c_str());
        editor_menu_slider(ui, "Starting Gold:", &document->players[0].starting_gold, STARTING_GOLD_SLIDER_PARAMS, MENU_RECT);

        for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
            char prompt[16];
            sprintf(prompt, "Player %u: ", player_id);
            ui_text_input(ui, prompt, ivec2(MENU_RECT.w - 32, 24), &document->players[player_id].name, MAX_USERNAME_LENGTH);
            editor_menu_slider(ui, "Starting Gold:", &document->players[player_id].starting_gold, STARTING_GOLD_SLIDER_PARAMS, MENU_RECT);
        }
    ui_end_container(ui);

    ui_element_position(ui, ui_button_position_frame_bottom_left(MENU_RECT));
    if (ui_button(ui, "Back")) {
        menu.mode = EDITOR_MENU_PLAYERS_CLOSED;
    }
}

#endif
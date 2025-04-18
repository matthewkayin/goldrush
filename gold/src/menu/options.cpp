#include "options.h"

#include "render/render.h"
#include "ui.h"

static const int OPTIONS_MENU_WIDTH = 350;
static const int OPTIONS_MENU_HEIGHT = 300;
static const Rect OPTIONS_FRAME_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (OPTIONS_MENU_WIDTH / 2),
    .y = (SCREEN_HEIGHT / 2) - (OPTIONS_MENU_HEIGHT / 2),
    .w = OPTIONS_MENU_WIDTH, 
    .h = OPTIONS_MENU_HEIGHT
};
static const ivec2 BACK_BUTTON_POSITION = ivec2(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const ivec2 SAVE_BUTTON_POSITION = ivec2(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - 56, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);

OptionsMenuState options_menu_open() {
    OptionsMenuState state;

    state.mode = OPTIONS_MENU_OPEN;
    for (int option = 0; option < OPTION_COUNT; option++) {
        state.pending_changes[(OptionName)option] = option_get_value((OptionName)option);
    }

    return state;
}

void options_menu_update(OptionsMenuState& state) {
    ui_begin(UI_OPTIONS, true);
    ui_frame_rect(OPTIONS_FRAME_RECT);

    ui_element_position(BACK_BUTTON_POSITION);
    if (ui_button("BACK")) {
        state.mode = OPTIONS_MENU_CLOSED;
    }

    ui_element_position(SAVE_BUTTON_POSITION);
    if (ui_button("SAVE")) {
    }
}

void options_menu_render(const OptionsMenuState& state) {
    Rect SCREEN_RECT = (Rect) {
        .x = 0, .y = 0,
        .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT
    };
    render_fill_rect(SCREEN_RECT, RENDER_COLOR_OFFBLACK_TRANSPARENT);

    ui_render(UI_OPTIONS);
}
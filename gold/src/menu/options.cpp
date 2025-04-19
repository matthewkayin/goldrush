#include "options.h"

#include "core/logger.h"
#include "render/render.h"
#include "ui.h"
#include <unordered_map>

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

static const char* const OPTION_DISPLAY_VALUES[] = { "Windowed", "Fullscreen", "Borderless" };
static const char* const OPTION_VSYNC_VALUES[] = { "Disabled", "Enabled", "Adaptive" };
static const char* const OPTION_UNIT_VOICES_VALUES[] = { "Off", "On" };
static const char* const OPTION_HOTKEYS_VALUES[] = { "Default", "Grid", "Custom" };
const char* const* option_value_strings(OptionName option) {
    switch (option) {
        case OPTION_DISPLAY:
            return OPTION_DISPLAY_VALUES;
        case OPTION_VSYNC:
            return OPTION_VSYNC_VALUES;
        case OPTION_UNIT_VOICES:
            return OPTION_UNIT_VOICES_VALUES;
        case OPTION_HOTKEYS:
            return OPTION_HOTKEYS_VALUES;
        default:
            return {};
    }
}

OptionsMenuState options_menu_open() {
    OptionsMenuState state;

    state.mode = OPTIONS_MENU_OPEN;

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

    ui_begin_column(ivec2(OPTIONS_FRAME_RECT.x + 8, OPTIONS_FRAME_RECT.y + 16), 25);
        const SpriteInfo& dropdown_info = render_get_sprite_info(SPRITE_UI_DROPDOWN);
        for (uint32_t option = 0; option < OPTION_COUNT; option++) {
            const OptionData& option_data = option_get_data((OptionName)option);
            ui_begin_row(ivec2(0, 0), 0);
                ui_element_position(ivec2(0, 3));
                ui_text(FONT_WESTERN8_GOLD, option_data.name);

                if (option_data.type == OPTION_TYPE_DROPDOWN) {
                    ui_element_position(ivec2(OPTIONS_FRAME_RECT.w - 16 - dropdown_info.frame_width, 0));
                    uint32_t value = option_get_value((OptionName)option);
                    if (ui_dropdown((int)option, UI_DROPDOWN, &value, option_value_strings((OptionName)option), option_data.max_value, false)) {
                        option_set_value((OptionName)option, value);
                    }
                }
            ui_end_container();
        }
    ui_end_container();
}

void options_menu_render(const OptionsMenuState& state) {
    Rect SCREEN_RECT = (Rect) {
        .x = 0, .y = 0,
        .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT
    };
    render_fill_rect(SCREEN_RECT, RENDER_COLOR_OFFBLACK_TRANSPARENT);

    ui_render(UI_OPTIONS);
}
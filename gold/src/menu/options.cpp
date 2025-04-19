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

static const char* const OPTION_DISPLAY_VALUES[] = { "Windowed", "Fullscreen", "Borderless" };
static const char* const OPTION_VSYNC_VALUES[] = { "Disabled", "Enabled", "Adaptive" };
static const char* const OPTION_UNIT_VOICES_VALUES[] = { "Off", "On" };
const char* const* option_value_strings(OptionName option) {
    switch (option) {
        case OPTION_DISPLAY:
            return OPTION_DISPLAY_VALUES;
        case OPTION_VSYNC:
            return OPTION_VSYNC_VALUES;
        case OPTION_UNIT_VOICES:
            return OPTION_UNIT_VOICES_VALUES;
        default:
            return {};
    }
}

static const int HOTKEY_GROUP_NONE = -1;
static const int HOTKEY_INDEX_NONE = -1;

enum HotkeyGroupName {
    HOTKEY_GROUP_MINER,
    HOTKEY_GROUP_BUILD,
    HOTKEY_GROUP_BUILD2,
    HOTKEY_GROUP_WAGON,
    HOTKEY_GROUP_PYRO,
    HOTKEY_GROUP_DETECTIVE,
    HOTKEY_GROUP_HALL,
    HOTKEY_GROUP_SALOON,
    HOTKEY_GROUP_WORKSHOP,
    HOTKEY_GROUP_SMITH,
    HOTKEY_GROUP_BARRACKS,
    HOTKEY_GROUP_COOP,
    HOTKEY_GROUP_COUNT
};

struct HotkeyGroup {
    const char* name;
    SpriteName icon;
    InputAction hotkeys[HOTKEY_GROUP_SIZE];
};

static const std::unordered_map<HotkeyGroupName, HotkeyGroup> HOTKEY_GROUPS = {
    { HOTKEY_GROUP_MINER, (HotkeyGroup) {
        .name = "Miner",
        .icon = SPRITE_BUTTON_ICON_MINER,
        .hotkeys = { 
            INPUT_HOTKEY_ATTACK, INPUT_HOTKEY_STOP, INPUT_HOTKEY_DEFEND,
            INPUT_HOTKEY_REPAIR, INPUT_HOTKEY_BUILD, INPUT_HOTKEY_BUILD2 
        }
    }},
    { HOTKEY_GROUP_BUILD, (HotkeyGroup) {
        .name = "Build",
        .icon = SPRITE_BUTTON_ICON_BUILD,
        .hotkeys = { 
            INPUT_HOTKEY_HALL, INPUT_HOTKEY_HOUSE, INPUT_HOTKEY_SALOON,
            INPUT_HOTKEY_BUNKER, INPUT_HOTKEY_WORKSHOP, INPUT_HOTKEY_CANCEL 
        }
    }},
    { HOTKEY_GROUP_BUILD2, (HotkeyGroup) {
        .name = "Advanced Build",
        .icon = SPRITE_BUTTON_ICON_BUILD2,
        .hotkeys = { 
            INPUT_HOTKEY_SMITH, INPUT_HOTKEY_COOP, INPUT_HOTKEY_BARRACKS,
            INPUT_HOTKEY_SHERIFFS, INPUT_HOTKEY_NONE, INPUT_HOTKEY_CANCEL 
        }
    }},
    { HOTKEY_GROUP_WAGON, (HotkeyGroup) {
        .name = "Wagon",
        .icon = SPRITE_BUTTON_ICON_WAGON,
        .hotkeys = { 
            INPUT_HOTKEY_ATTACK, INPUT_HOTKEY_STOP, INPUT_HOTKEY_DEFEND,
            INPUT_HOTKEY_UNLOAD, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_PYRO, (HotkeyGroup) {
        .name = "Pyro",
        .icon = SPRITE_BUTTON_ICON_PYRO,
        .hotkeys = { 
            INPUT_HOTKEY_ATTACK, INPUT_HOTKEY_STOP, INPUT_HOTKEY_DEFEND,
            INPUT_HOTKEY_MOLOTOV, INPUT_HOTKEY_LANDMINE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_DETECTIVE, (HotkeyGroup) {
        .name = "Detective",
        .icon = SPRITE_BUTTON_ICON_DETECTIVE,
        .hotkeys = { 
            INPUT_HOTKEY_ATTACK, INPUT_HOTKEY_STOP, INPUT_HOTKEY_DEFEND,
            INPUT_HOTKEY_CAMO, INPUT_HOTKEY_LANDMINE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_HALL, (HotkeyGroup) {
        .name = "Town Hall",
        .icon = SPRITE_BUTTON_ICON_HALL,
        .hotkeys = { 
            INPUT_HOTKEY_MINER, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE,
            INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_SALOON, (HotkeyGroup) {
        .name = "Saloon",
        .icon = SPRITE_BUTTON_ICON_SALOON,
        .hotkeys = { 
            INPUT_HOTKEY_COWBOY, INPUT_HOTKEY_BANDIT, INPUT_HOTKEY_DETECTIVE,
            INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_WORKSHOP, (HotkeyGroup) {
        .name = "Workshop",
        .icon = SPRITE_BUTTON_ICON_WORKSHOP,
        .hotkeys = { 
            INPUT_HOTKEY_SAPPER, INPUT_HOTKEY_PYRO, INPUT_HOTKEY_BALLOON,
            INPUT_HOTKEY_RESEARCH_LANDMINES, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_SMITH, (HotkeyGroup) {
        .name = "Blacksmith",
        .icon = SPRITE_BUTTON_ICON_SMITH,
        .hotkeys = { 
            INPUT_HOTKEY_RESEARCH_WAGON_ARMOR, INPUT_HOTKEY_RESEARCH_BAYONETS, INPUT_HOTKEY_NONE,
            INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_BARRACKS, (HotkeyGroup) {
        .name = "Barracks",
        .icon = SPRITE_BUTTON_ICON_BARRACKS,
        .hotkeys = { 
            INPUT_HOTKEY_SOLDIER, INPUT_HOTKEY_CANNON, INPUT_HOTKEY_NONE,
            INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_COOP, (HotkeyGroup) {
        .name = "Chicken Coop",
        .icon = SPRITE_BUTTON_ICON_COOP,
        .hotkeys = { 
            INPUT_HOTKEY_JOCKEY, INPUT_HOTKEY_WAGON, INPUT_HOTKEY_NONE,
            INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }}
};

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
        if (state.mode == OPTIONS_MENU_OPEN) {
            state.mode = OPTIONS_MENU_CLOSED;
        } else if (state.mode == OPTIONS_MENU_HOTKEYS) {
            state.mode = OPTIONS_MENU_OPEN;
        }
    }

    if (state.mode == OPTIONS_MENU_OPEN) {
        ui_begin_column(ivec2(OPTIONS_FRAME_RECT.x + 8, OPTIONS_FRAME_RECT.y + 16), 25);
            const SpriteInfo& dropdown_info = render_get_sprite_info(SPRITE_UI_DROPDOWN);
            for (uint32_t option = 0; option < OPTION_COUNT; option++) {
                const OptionData& option_data = option_get_data((OptionName)option);
                ui_begin_row(ivec2(0, 0), 0);
                    ui_element_position(ivec2(0, 3));
                    ui_text(FONT_WESTERN8_GOLD, option_data.name);

                    ui_element_position(ivec2(OPTIONS_FRAME_RECT.w - 16 - dropdown_info.frame_width, 0));
                    uint32_t value = option_get_value((OptionName)option);
                    if (option_data.type == OPTION_TYPE_DROPDOWN) {
                        if (ui_dropdown((int)option, UI_DROPDOWN, &value, option_value_strings((OptionName)option), option_data.max_value, false)) {
                            option_set_value((OptionName)option, value);
                        }
                    } else if (option_data.type == OPTION_TYPE_PERCENT_SLIDER || option_data.type == OPTION_TYPE_SLIDER) {
                        if (ui_slider((int)option, &value, option_data.min_value, option_data.max_value, option_data.type == OPTION_TYPE_SLIDER ? UI_SLIDER_DISPLAY_RAW_VALUE : UI_SLIDER_DISPLAY_PERCENT)) {
                            option_set_value((OptionName)option, value);
                        }
                    }
                ui_end_container();
            }

            // Hotkeys row
            ui_begin_row(ivec2(0, 0), 0);
                ui_element_position(ivec2(0, 3));
                ui_text(FONT_WESTERN8_GOLD, "Hotkeys");

                ivec2 edit_button_size = ui_button_size("Edit");
                ui_element_position(ivec2(OPTIONS_FRAME_RECT.w - 16 - edit_button_size.x, 0));
                if (ui_button("Edit")) {
                    state.mode = OPTIONS_MENU_HOTKEYS;
                    for (int input = INPUT_HOTKEY_NONE; input < INPUT_ACTION_COUNT; input++) {
                        state.hotkey_pending_changes[input] = input_get_hotkey_mapping((InputAction)input);
                        state.hotkey_group_selected = HOTKEY_GROUP_NONE;
                        state.hotkey_index_selected = HOTKEY_INDEX_NONE;
                    }
                }
            ui_end_container();
        ui_end_container();
    } else if (state.mode == OPTIONS_MENU_HOTKEYS) {
        ui_begin_row(ivec2(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + 16), 8);
            ui_begin_column(ivec2(0, 0), 4);
                ui_text(FONT_HACK_GOLD, "Hotkey Groups:");
                int hotkey_group = 0;
                while (hotkey_group < HOTKEY_GROUP_COUNT) {
                    ui_begin_row(ivec2(0, 0), 4);
                    for (int column = 0; column < 3; column++) {
                        const HotkeyGroup& group = HOTKEY_GROUPS.at((HotkeyGroupName)hotkey_group);
                        if (ui_icon_button(group.icon, hotkey_group == state.hotkey_group_selected)) {
                            state.hotkey_group_selected = hotkey_group;
                        }

                        hotkey_group++;
                        if (hotkey_group == HOTKEY_GROUP_COUNT) {
                            break;
                        }
                    }
                    ui_end_container();
                }
            ui_end_container();
            if (state.hotkey_group_selected != HOTKEY_GROUP_NONE) {
                const HotkeyGroup& group = HOTKEY_GROUPS.at((HotkeyGroupName)state.hotkey_group_selected);
                ui_begin_column(ivec2(0, 0), 4);
                    ui_text(FONT_HACK_GOLD, group.name);
                ui_end_container();
            }
        ui_end_container();
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
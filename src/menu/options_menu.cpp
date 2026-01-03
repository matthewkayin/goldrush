#include "options_menu.h"

#include "core/logger.h"
#include "render/render.h"
#include "match/hotkey.h" 
#include <unordered_map>

static const int OPTIONS_MENU_WIDTH = 364;
static const int OPTIONS_MENU_HEIGHT = 300;
static const Rect OPTIONS_FRAME_RECT = {
    .x = (SCREEN_WIDTH / 2) - (OPTIONS_MENU_WIDTH / 2),
    .y = (SCREEN_HEIGHT / 2) - (OPTIONS_MENU_HEIGHT / 2),
    .w = OPTIONS_MENU_WIDTH, 
    .h = OPTIONS_MENU_HEIGHT
};
static const ivec2 BACK_BUTTON_POSITION = ivec2(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const ivec2 SAVE_BUTTON_POSITION = ivec2(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - 56, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);

static const std::unordered_map<OptionName, std::vector<std::string>> OPTION_VALUE_STRINGS = {
    { OPTION_DISPLAY, { "Windowed", "Fullscreen" }},
    { OPTION_VSYNC, { "Disabled", "Enabled", "Adaptive" }}
};

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
    HOTKEY_GROUP_SHERIFFS,
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
            INPUT_HOTKEY_CAMO, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
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
            INPUT_HOTKEY_RESEARCH_LANDMINES, INPUT_HOTKEY_RESEARCH_TAILWIND, INPUT_HOTKEY_NONE
        }
    }},
    { HOTKEY_GROUP_SMITH, (HotkeyGroup) {
        .name = "Blacksmith",
        .icon = SPRITE_BUTTON_ICON_SMITH,
        .hotkeys = { 
            INPUT_HOTKEY_RESEARCH_SERRATED_KNIVES, INPUT_HOTKEY_RESEARCH_BAYONETS, INPUT_HOTKEY_RESEARCH_WAGON_ARMOR,
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
    }},
    { HOTKEY_GROUP_SHERIFFS, (HotkeyGroup) {
        .name = "Sheriff's Office",
        .icon = SPRITE_BUTTON_ICON_SHERIFFS,
        .hotkeys = { 
            INPUT_HOTKEY_RESEARCH_PRIVATE_EYE, INPUT_HOTKEY_RESEARCH_STAKEOUT, INPUT_HOTKEY_NONE,
            INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE, INPUT_HOTKEY_NONE
        }
    }}
};

bool options_menu_is_hotkey_group_valid(const OptionsMenuState& state, HotkeyGroupName group);
bool options_menu_are_all_hotkey_groups_valid(const OptionsMenuState& state);
bool options_menu_has_unsaved_hotkey_changes(const OptionsMenuState& state);
void options_menu_save_hotkey_changes(OptionsMenuState& state);
void options_menu_set_hotkey_mapping_to_grid(SDL_Scancode* hotkey_mapping);
const char* options_menu_get_save_status_str(OptionsMenuSaveStatus status);

OptionsMenuState options_menu_open() {
    OptionsMenuState state;

    state.mode = OPTIONS_MENU_OPEN;

    return state;
}

void options_menu_update(OptionsMenuState& state, UI& ui) {
    ui.input_enabled = true;
    ui_screen_shade(ui);
    ui_frame_rect(ui, OPTIONS_FRAME_RECT);

    ui_element_position(ui, BACK_BUTTON_POSITION);
    if (ui_button(ui, "Back")) {
        if (state.mode == OPTIONS_MENU_OPEN) {
            state.mode = OPTIONS_MENU_CLOSED;
        } else if (state.mode == OPTIONS_MENU_HOTKEYS) {
            state.mode = OPTIONS_MENU_OPEN;
        }
    }

    if (state.mode == OPTIONS_MENU_OPEN) {
        ui_begin_column(ui, ivec2(OPTIONS_FRAME_RECT.x + 8, OPTIONS_FRAME_RECT.y + 16), 25);
            const SpriteInfo& dropdown_info = render_get_sprite_info(SPRITE_UI_DROPDOWN);
            for (uint32_t option = 0; option < OPTION_COUNT; option++) {
                const OptionData& option_data = option_get_data((OptionName)option);
                ui_begin_row(ui, ivec2(0, 0), 0);
                    ui_element_position(ui, ivec2(0, 3));
                    ui_text(ui, FONT_WESTERN8_GOLD, option_data.name);

                    ui_element_position(ui, ivec2(OPTIONS_FRAME_RECT.w - 16 - dropdown_info.frame_width, 0));
                    uint32_t value = option_get_value((OptionName)option);
                    if (option_data.type == OPTION_TYPE_DROPDOWN) {
                        if (ui_dropdown(ui, UI_DROPDOWN, &value, OPTION_VALUE_STRINGS.at((OptionName)option), false)) {
                            option_set_value((OptionName)option, value);
                        }
                    } else if (option_data.type == OPTION_TYPE_PERCENT_SLIDER || option_data.type == OPTION_TYPE_SLIDER) {
                        UiSliderParams params = (UiSliderParams) {
                            .display = option_data.type == OPTION_TYPE_SLIDER
                                ? UI_SLIDER_DISPLAY_RAW_VALUE
                                : UI_SLIDER_DISPLAY_PERCENT,
                            .min = option_data.min_value,
                            .max = option_data.max_value,
                            .step = 1
                        };
                        if (ui_slider(ui, &value, NULL, params)) {
                            option_set_value((OptionName)option, value);
                        }
                    }
                ui_end_container(ui);
            }

            // Hotkeys row
            ui_begin_row(ui, ivec2(0, 0), 0);
                ui_element_position(ui, ivec2(0, 3));
                ui_text(ui, FONT_WESTERN8_GOLD, "Hotkeys");

                ivec2 edit_button_size = ui_button_size("Edit");
                ui_element_position(ui, ivec2(OPTIONS_FRAME_RECT.w - 16 - edit_button_size.x, 0));
                if (ui_button(ui, "Edit")) {
                    state.mode = OPTIONS_MENU_HOTKEYS;
                    for (int input = INPUT_HOTKEY_NONE; input < INPUT_ACTION_COUNT; input++) {
                        state.hotkey_pending_changes[input] = input_get_hotkey_mapping((InputAction)input);
                        state.hotkey_group_selected = HOTKEY_GROUP_NONE;
                        state.hotkey_index_selected = HOTKEY_INDEX_NONE;
                        state.save_status = OPTIONS_MENU_SAVE_STATUS_NONE;
                    }
                }
            ui_end_container(ui);
        ui_end_container(ui);
    } else if (state.mode == OPTIONS_MENU_HOTKEYS) {
        ui_begin_row(ui, ivec2(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + 16), -4);
            ui_begin_column(ui, ivec2(0, 0), 4);
                ui_text(ui, FONT_HACK_GOLD, "Hotkey Groups:");
                int hotkey_group = 0;
                while (hotkey_group < HOTKEY_GROUP_COUNT) {
                    ui_begin_row(ui, ivec2(0, 0), 4);
                    for (int column = 0; column < 3; column++) {
                        const HotkeyGroup& group = HOTKEY_GROUPS.at((HotkeyGroupName)hotkey_group);
                        if (ui_icon_button(ui, group.icon, hotkey_group == state.hotkey_group_selected)) {
                            state.hotkey_group_selected = hotkey_group;
                            state.hotkey_index_selected = HOTKEY_INDEX_NONE;
                        }

                        hotkey_group++;
                        if (hotkey_group == HOTKEY_GROUP_COUNT) {
                            break;
                        }
                    }
                    ui_end_container(ui);
                }

                ui_text(ui, FONT_HACK_GOLD, "Use Preset:");
                ui_begin_row(ui, ivec2(0, 0), 4);
                if (ui_button(ui, "Default")) {
                    input_set_hotkey_mapping_to_default(state.hotkey_pending_changes);
                    state.save_status = options_menu_has_unsaved_hotkey_changes(state)
                                            ? OPTIONS_MENU_SAVE_STATUS_UNSAVED_CHANGES
                                            : OPTIONS_MENU_SAVE_STATUS_NONE;
                }
                if (ui_button(ui, "Grid")) {
                    options_menu_set_hotkey_mapping_to_grid(state.hotkey_pending_changes);
                    state.save_status = options_menu_has_unsaved_hotkey_changes(state)
                                            ? OPTIONS_MENU_SAVE_STATUS_UNSAVED_CHANGES
                                            : OPTIONS_MENU_SAVE_STATUS_NONE;
                }
                ui_end_container(ui);
            ui_end_container(ui);
            if (state.hotkey_group_selected != HOTKEY_GROUP_NONE) {
                const HotkeyGroup& group = HOTKEY_GROUPS.at((HotkeyGroupName)state.hotkey_group_selected);
                ui_element_size(ui, ivec2(36 * 3, 0));
                ui_begin_column(ui, ivec2(0, 0), 4);
                    ui_text(ui, FONT_HACK_GOLD, group.name);

                    // Hotkey rows
                    for (int hotkey_row = 0; hotkey_row < 2; hotkey_row++) {
                        ui_begin_row(ui, ivec2(0, 0), 4);
                            for (int hotkey_index = (hotkey_row * 3); hotkey_index < (hotkey_row * 3) + 3; hotkey_index++) {
                                SpriteName icon_button_sprite = group.hotkeys[hotkey_index] == INPUT_HOTKEY_NONE
                                                                    ? UI_ICON_BUTTON_EMPTY
                                                                    : hotkey_get_sprite(group.hotkeys[hotkey_index]);
                                if (ui_icon_button(ui, icon_button_sprite, hotkey_index == state.hotkey_index_selected)) {
                                    // ui_icon_button will not return true if the button is empty, so we don't need to handle that case here
                                    state.hotkey_index_selected = hotkey_index;
                                }
                            }
                        ui_end_container(ui);
                    }

                    if (!options_menu_is_hotkey_group_valid(state, (HotkeyGroupName)state.hotkey_group_selected)) {
                        ui_begin_column(ui, ivec2(0, 0), 0);
                            ui_text(ui, FONT_HACK_WHITE, "Error: multiple actions in this");
                            ui_text(ui, FONT_HACK_WHITE, "group are mapped to the same key.");
                        ui_end_container(ui);
                    }

                    if (state.hotkey_index_selected != HOTKEY_INDEX_NONE) {
                        char hotkey_text[128];
                        char* hotkey_text_ptr = hotkey_text;
                        InputAction hotkey = group.hotkeys[state.hotkey_index_selected];

                        hotkey_text_ptr += hotkey_get_name(hotkey_text_ptr, hotkey);
                        hotkey_text_ptr += sprintf(hotkey_text_ptr, " (");
                        hotkey_text_ptr += input_sprintf_sdl_scancode_str(hotkey_text_ptr, state.hotkey_pending_changes[hotkey]);
                        hotkey_text_ptr += sprintf(hotkey_text_ptr, ")");
                        ui_text(ui, FONT_HACK_GOLD,  hotkey_text);
                        ivec2 text_size = render_get_text_size(FONT_HACK_WHITE, "Press any key to");
                        ui_element_size(ui, ivec2(text_size.x, text_size.y - 4));
                        ui_text(ui, FONT_HACK_WHITE, "Press any key to");
                        ui_text(ui, FONT_HACK_WHITE, "change this hotkey.");

                        SDL_Scancode key_just_pressed = input_get_key_just_pressed();
                        if (input_is_key_valid_hotkey_mapping(key_just_pressed)) {
                            state.hotkey_pending_changes[hotkey] = key_just_pressed;
                            state.save_status = options_menu_has_unsaved_hotkey_changes(state)
                                                    ? OPTIONS_MENU_SAVE_STATUS_UNSAVED_CHANGES
                                                    : OPTIONS_MENU_SAVE_STATUS_NONE;
                        }
                    }
                ui_end_container(ui);
            } // End if selected hotkey group != none
        ui_end_container(ui);

        if (state.save_status != OPTIONS_MENU_SAVE_STATUS_NONE) {
            const char* status_text = options_menu_get_save_status_str(state.save_status);
            ivec2 text_size = render_get_text_size(FONT_HACK_WHITE, status_text);
            ivec2 text_pos = ivec2(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - text_size.x, SAVE_BUTTON_POSITION.y - 4 - text_size.y);
            ui_element_position(ui, text_pos);
            ui_text(ui, FONT_HACK_WHITE, status_text);
        }
        ui_element_position(ui, SAVE_BUTTON_POSITION);
        if (ui_button(ui, "Save")) {
            if (options_menu_are_all_hotkey_groups_valid(state)) {
                options_menu_save_hotkey_changes(state);
                state.save_status = OPTIONS_MENU_SAVE_STATUS_SAVED;
            } else {
                state.save_status = OPTIONS_MENU_SAVE_STATUS_ERRORS;
            }
        }
    }
}

bool options_menu_is_hotkey_group_valid(const OptionsMenuState& state, HotkeyGroupName name) {
    const HotkeyGroup& group = HOTKEY_GROUPS.at(name);
    for (int hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
        for (int other_index = 0; other_index < HOTKEY_GROUP_SIZE; other_index++) {
            if (hotkey_index == other_index ||
                    group.hotkeys[hotkey_index] == INPUT_HOTKEY_NONE ||
                    group.hotkeys[other_index] == INPUT_HOTKEY_NONE) {
                continue;
            }

            if (state.hotkey_pending_changes[group.hotkeys[hotkey_index]] == 
                    state.hotkey_pending_changes[group.hotkeys[other_index]]) {
                return false;
            }
        }
    }

    return true;
}

bool options_menu_are_all_hotkey_groups_valid(const OptionsMenuState& state) {
    for (int group = 0; group < HOTKEY_GROUP_COUNT; group++) {
        if (!options_menu_is_hotkey_group_valid(state, (HotkeyGroupName)group)) {
            return false;
        }
    }

    return true;
}

bool options_menu_has_unsaved_hotkey_changes(const OptionsMenuState& state) {
    for (int hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
        if (input_get_hotkey_mapping((InputAction)hotkey) != state.hotkey_pending_changes[hotkey]) {
            return true;
        }
    }

    return false;
}

void options_menu_save_hotkey_changes(OptionsMenuState& state) {
    for (int hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
        input_set_hotkey_mapping((InputAction)hotkey, state.hotkey_pending_changes[hotkey]);
    }
}

void options_menu_set_hotkey_mapping_to_grid(SDL_Scancode* hotkey_mapping) {
    static const SDL_Scancode keys[HOTKEY_GROUP_SIZE] = {
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E,
        SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D
    };

    for (int group_index = 0; group_index < HOTKEY_GROUP_COUNT; group_index++) {
        const HotkeyGroup& group = HOTKEY_GROUPS.at((HotkeyGroupName)group_index);
        for (int index = 0; index < HOTKEY_GROUP_SIZE; index++) {
            InputAction hotkey = group.hotkeys[index];
            if (hotkey == INPUT_HOTKEY_NONE || hotkey == INPUT_HOTKEY_CANCEL) {
                continue;
            }

            hotkey_mapping[hotkey] = keys[index];
        }
    }

    hotkey_mapping[INPUT_HOTKEY_CANCEL] = SDL_SCANCODE_ESCAPE;
}

const char* options_menu_get_save_status_str(OptionsMenuSaveStatus status) {
    switch (status) {
        case OPTIONS_MENU_SAVE_STATUS_NONE:
            return "";
        case OPTIONS_MENU_SAVE_STATUS_UNSAVED_CHANGES:
            return "You have unsaved changes.";
        case OPTIONS_MENU_SAVE_STATUS_SAVED:
            return "Hotkeys saved.";
        case OPTIONS_MENU_SAVE_STATUS_ERRORS:
            return "One or more hotkey groups has errors.";
    }
}
#include "options.h"

#include "logger.h"

static const int OPTIONS_MENU_WIDTH = 350;
static const int OPTIONS_MENU_HEIGHT = 300;
static const SDL_Rect OPTIONS_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (OPTIONS_MENU_WIDTH / 2),
    .y = (SCREEN_HEIGHT / 2) - (OPTIONS_MENU_HEIGHT / 2),
    .w = OPTIONS_MENU_WIDTH,
    .h = OPTIONS_MENU_HEIGHT
};
static const xy BACK_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const xy HOTKEYS_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - 120, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const xy SAVE_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - 56, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const xy DEFAULTS_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + 16, BACK_BUTTON_POSITION.y - 84);
static const xy GRID_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + 16, DEFAULTS_BUTTON_POSITION.y + 21 + 4);

static const int SAVE_STATUS_DURATION = 60 * 5;

struct hotkey_group_t {
    const char* name;
    UiButton icon;
    UiButton hotkeys[6];
};

static const std::unordered_map<OptionHotkeyGroup, hotkey_group_t> HOTKEY_GROUPS = {
    { HOTKEY_GROUP_MINER, (hotkey_group_t) {
        .name = "Miner:",
        .icon = UI_BUTTON_UNIT_MINER,
        .hotkeys = { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                     UI_BUTTON_REPAIR, UI_BUTTON_BUILD, UI_BUTTON_BUILD2 }
    }},
    { HOTKEY_GROUP_MINER_BUILD, (hotkey_group_t) {
        .name = "Build:",
        .icon = UI_BUTTON_BUILD,
        .hotkeys = { UI_BUTTON_BUILD_HALL, UI_BUTTON_BUILD_HOUSE, UI_BUTTON_BUILD_CAMP,
                     UI_BUTTON_BUILD_SALOON, UI_BUTTON_BUILD_BUNKER, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_MINER_BUILD2, (hotkey_group_t) {
        .name = "Advanced Build:",
        .icon = UI_BUTTON_BUILD2,
        .hotkeys = { UI_BUTTON_BUILD_SMITH, UI_BUTTON_BUILD_COOP, UI_BUTTON_BUILD_BARRACKS,
                     UI_BUTTON_BUILD_SHERIFFS, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_WAGON, (hotkey_group_t) {
        .name = "Wagon:",
        .icon = UI_BUTTON_UNIT_WAGON,
        .hotkeys = { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                     UI_BUTTON_UNLOAD, UI_BUTTON_NONE, UI_BUTTON_NONE }
    }},
    { HOTKEY_GROUP_SAPPER, (hotkey_group_t) {
        .name = "Sapper:",
        .icon = UI_BUTTON_UNIT_SAPPER,
        .hotkeys = { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                     UI_BUTTON_EXPLODE, UI_BUTTON_NONE, UI_BUTTON_NONE }
    }},
    { HOTKEY_GROUP_TINKER, (hotkey_group_t) {
        .name = "Tinker:",
        .icon = UI_BUTTON_UNIT_TINKER,
        .hotkeys = { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                     UI_BUTTON_BUILD_MINE, UI_BUTTON_SMOKE, UI_BUTTON_NONE }
    }},
    { HOTKEY_GROUP_HALL, (hotkey_group_t) {
        .name = "Town Hall:",
        .icon = UI_BUTTON_BUILD_HALL,
        .hotkeys = { UI_BUTTON_UNIT_MINER, UI_BUTTON_NONE, UI_BUTTON_NONE,
                     UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_SALOON, (hotkey_group_t) {
        .name = "Saloon:",
        .icon = UI_BUTTON_BUILD_SALOON,
        .hotkeys = { UI_BUTTON_UNIT_COWBOY, UI_BUTTON_UNIT_BANDIT, UI_BUTTON_UNIT_TINKER,
                     UI_BUTTON_UNIT_SAPPER, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_SMITH, (hotkey_group_t) {
        .name = "Blacksmith:",
        .icon = UI_BUTTON_BUILD_SMITH,
        .hotkeys = { UI_BUTTON_RESEARCH_WAR_WAGON, UI_BUTTON_RESEARCH_EXPLOSIVES, UI_BUTTON_RESEARCH_BAYONETS,
                     UI_BUTTON_RESEARCH_SMOKE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_COOP, (hotkey_group_t) {
        .name = "Chicken Coop:",
        .icon = UI_BUTTON_BUILD_COOP,
        .hotkeys = { UI_BUTTON_UNIT_JOCKEY, UI_BUTTON_UNIT_WAGON, UI_BUTTON_NONE,
                     UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_BARRACKS, (hotkey_group_t) {
        .name = "Barracks:",
        .icon = UI_BUTTON_BUILD_BARRACKS,
        .hotkeys = { UI_BUTTON_UNIT_SOLDIER, UI_BUTTON_UNIT_CANNON, UI_BUTTON_NONE,
                     UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }},
    { HOTKEY_GROUP_SHERIFFS, (hotkey_group_t) {
        .name = "Sheriff's Office:",
        .icon = UI_BUTTON_BUILD_SHERIFFS,
        .hotkeys = { UI_BUTTON_UNIT_SPY, UI_BUTTON_NONE, UI_BUTTON_NONE,
                     UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }
    }}
};

options_menu_state_t options_menu_open() {
    options_menu_state_t state;

    state.mode = OPTION_MENU_OPEN;
    state.hover = OPTION_HOVER_NONE;
    state.hover_subindex = 0;
    state.dropdown_chosen = -1;
    state.slider_chosen = -1;
    state.hotkey_group_chosen = -1;
    state.hotkey_chosen = -1;
    state.hotkey_save_status_timer = 0;
    state.hotkey_save_successful = false;

    return state;
}

void options_menu_handle_input(options_menu_state_t& state, SDL_Event event) {
    if (state.mode == OPTION_MENU_CLOSED) {
        return;
    }

    if (event.type == SDL_KEYDOWN && state.mode == OPTION_MENU_HOTKEYS && state.hotkey_chosen != -1) {
        SDL_Keycode keycode = event.key.keysym.sym;
        if ((keycode >= SDLK_a && keycode <= SDLK_z) || 
                keycode == SDLK_RIGHTBRACKET || 
                keycode == SDLK_LEFTBRACKET ||
                keycode == SDLK_BACKSLASH ||
                keycode == SDLK_BACKQUOTE ||
                keycode == SDLK_MINUS ||
                keycode == SDLK_EQUALS ||
                keycode == SDLK_COMMA ||
                keycode == SDLK_PERIOD ||
                keycode == SDLK_SLASH ||
                keycode == SDLK_SEMICOLON ||
                keycode == SDLK_QUOTE ||
                keycode == SDLK_ESCAPE) {
            state.hotkeys[HOTKEY_GROUPS.at((OptionHotkeyGroup)state.hotkey_group_chosen).hotkeys[state.hotkey_chosen]] = event.key.keysym.sym;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (state.hover != OPTION_HOVER_NONE) {
            sound_play(SOUND_UI_SELECT);
        }

        if (state.mode == OPTION_MENU_HOTKEYS) {
            if (state.hover == OPTION_HOVER_BACK) {
                state.mode = OPTION_MENU_OPEN;
                state.hotkey_group_chosen = -1;
                state.hotkey_chosen = -1;
                state.hotkey_save_status_timer = 0;
                return;
            }

            if (state.hover == OPTION_HOVER_HOTKEYS_DEFAULTS) {
                for (auto it : DEFAULT_HOTKEYS) {
                    state.hotkeys[it.first] = it.second;
                }
                return;
            }

            if (state.hover == OPTION_HOVER_HOTKEYS_GRID) {
                for (auto it : GRID_HOTKEYS) {
                    state.hotkeys[it.first] = it.second;
                }
                return;
            }

            if (state.hover == OPTION_HOVER_HOTKEYS_SAVE) {
                state.hotkey_save_successful = true;
                for (int group = 0; group < HOTKEY_GROUP_COUNT; group++) {
                    if (!option_is_hotkey_group_valid(state.hotkeys, group)) {
                        state.hotkey_save_successful = false;
                        break;
                    }
                }

                if (state.hotkey_save_successful) {
                    for (auto it : state.hotkeys) {
                        engine.hotkeys[it.first] = it.second;
                    }
                }
                state.hotkey_save_status_timer = SAVE_STATUS_DURATION;
            }

            if (state.hover == OPTION_HOVER_HOTKEY_GROUP) {
                state.hotkey_group_chosen = state.hover_subindex;
                state.hotkey_chosen = -1;
            }

            if (state.hover == OPTION_HOVER_HOTKEYS_HOTKEY) {
                state.hotkey_chosen = state.hover_subindex;
            }
        }

        if (state.dropdown_chosen != -1) {
            if (state.hover == OPTION_HOVER_DROPDOWN_ITEM) {
                engine_apply_option((Option)state.dropdown_chosen, state.hover_subindex);
            }
            state.dropdown_chosen = -1;
            return;
        }

        if (state.hover == OPTION_HOVER_BACK) {
            state.mode = OPTION_MENU_CLOSED;
            return;
        } else if (state.hover == OPTION_HOVER_HOTKEYS) {
            state.mode = OPTION_MENU_HOTKEYS;
            for (auto it : engine.hotkeys) {
                state.hotkeys[it.first] = it.second;
            }
            return;
        } else if (state.hover == OPTION_HOVER_DROPDOWN) {
            state.dropdown_chosen = state.hover_subindex;
            state.hover = OPTION_HOVER_NONE;
            return;
        } 

        for (int option = 0; option < OPTION_COUNT; option++) {
            SDL_Rect dropdown_rect = options_get_dropdown_rect(option);
            if (sdl_rect_has_point(dropdown_rect, engine.mouse_position)) {
                const option_data_t& option_data = OPTION_DATA.at((Option)option);
                if (option_data.type == OPTION_TYPE_SLIDER || option_data.type == OPTION_TYPE_SLIDER_PERCENT) {
                    state.slider_chosen = option;
                    return;
                }
            }
        }
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        state.slider_chosen = -1;
    }
}

void options_menu_update(options_menu_state_t& state) {
    if (state.mode == OPTION_MENU_CLOSED) {
        return;
    }

    state.hover = OPTION_HOVER_NONE;

    if (state.mode == OPTION_MENU_HOTKEYS) {
        if (state.hotkey_save_status_timer > 0) {
            state.hotkey_save_status_timer--;
        }

        SDL_Rect back_button_rect = render_get_button_rect("BACK", BACK_BUTTON_POSITION);
        if (sdl_rect_has_point(back_button_rect, engine.mouse_position)) {
            state.hover = OPTION_HOVER_BACK;
        } 

        SDL_Rect save_button_rect = render_get_button_rect("SAVE", SAVE_BUTTON_POSITION);
        if (sdl_rect_has_point(save_button_rect, engine.mouse_position)) {
            state.hover = OPTION_HOVER_HOTKEYS_SAVE;
        } 

        SDL_Rect default_button_rect = render_get_button_rect("SET TO DEFAULT LAYOUT", DEFAULTS_BUTTON_POSITION);
        if (sdl_rect_has_point(default_button_rect, engine.mouse_position)) {
            state.hover = OPTION_HOVER_HOTKEYS_DEFAULTS;
        } 

        SDL_Rect grid_button_rect = render_get_button_rect("SET TO GRID LAYOUT", GRID_BUTTON_POSITION);
        if (sdl_rect_has_point(grid_button_rect, engine.mouse_position)) {
            state.hover = OPTION_HOVER_HOTKEYS_GRID;
        } 

        for (int hotkey_group = 0; hotkey_group < HOTKEY_GROUP_COUNT; hotkey_group++) {
            xy hotkey_group_position = option_hotkey_group_position(hotkey_group);
            SDL_Rect hotkey_group_rect = (SDL_Rect) {
                .x = hotkey_group_position.x, .y = hotkey_group_position.y,
                .w = engine.sprites[SPRITE_UI_BUTTON].frame_size.x, .h = engine.sprites[SPRITE_UI_BUTTON].frame_size.y
            };
            if (sdl_rect_has_point(hotkey_group_rect, engine.mouse_position)) {
                state.hover = OPTION_HOVER_HOTKEY_GROUP;
                state.hover_subindex = hotkey_group;
            }
        }

        if (state.hover == OPTION_HOVER_NONE && state.hotkey_group_chosen != -1) {
            const hotkey_group_t& hotkey_group = HOTKEY_GROUPS.at((OptionHotkeyGroup)state.hotkey_group_chosen);
            for (int hotkey = 0; hotkey < 6; hotkey++) {
                if (hotkey_group.hotkeys[hotkey] == UI_BUTTON_NONE) {
                    continue;
                }
                xy hotkey_position = option_hotkey_position(hotkey);
                SDL_Rect hotkey_rect = (SDL_Rect) {
                    .x = hotkey_position.x, .y = hotkey_position.y,
                    .w = engine.sprites[SPRITE_UI_BUTTON].frame_size.x, .h = engine.sprites[SPRITE_UI_BUTTON].frame_size.y
                };
                if (sdl_rect_has_point(hotkey_rect, engine.mouse_position)) {
                    state.hover = OPTION_HOVER_HOTKEYS_HOTKEY;
                    state.hover_subindex = hotkey;
                }
            }
        }

        return;
    }

    if (state.slider_chosen != -1) {
        SDL_Rect dropdown_rect = options_get_dropdown_rect(state.slider_chosen);
        int mouse_x = engine.mouse_position.x;
        if (mouse_x < dropdown_rect.x) {
            mouse_x = dropdown_rect.x;
        } else if (mouse_x > dropdown_rect.x + dropdown_rect.w) {
            mouse_x = dropdown_rect.x + dropdown_rect.w;
        }
        mouse_x -= dropdown_rect.x;

        const option_data_t& option_data = OPTION_DATA.at((Option)state.slider_chosen);
        engine_apply_option((Option)state.slider_chosen, option_data.min_value + ((mouse_x * option_data.max_value) / dropdown_rect.w));
        return;
    }

    SDL_Rect back_button_rect = render_get_button_rect("BACK", BACK_BUTTON_POSITION);
    if (sdl_rect_has_point(back_button_rect, engine.mouse_position)) {
        state.hover = OPTION_HOVER_BACK;
    } 
    SDL_Rect hotkey_button_rect = render_get_button_rect("EDIT HOTKEYS", HOTKEYS_BUTTON_POSITION);
    if (sdl_rect_has_point(hotkey_button_rect, engine.mouse_position)) {
        state.hover = OPTION_HOVER_HOTKEYS;
    } 

    if (state.hover == OPTION_HOVER_NONE && state.dropdown_chosen == -1) {
        for (int option = 0; option < OPTION_COUNT; option++) {
            SDL_Rect dropdown_rect = options_get_dropdown_rect(option);
            if (sdl_rect_has_point(dropdown_rect, engine.mouse_position)) {
                const option_data_t& option_data = OPTION_DATA.at((Option)option);
                if (option_data.type == OPTION_TYPE_DROPDOWN) {
                    state.hover = OPTION_HOVER_DROPDOWN;
                    state.hover_subindex = option;
                } 
                break;
            }
        }
    }

    if (state.hover == OPTION_HOVER_NONE && state.dropdown_chosen != -1) {
        for (int value = 0; value < OPTION_DATA.at((Option)state.dropdown_chosen).max_value; value++) {
            SDL_Rect item_rect = options_get_dropdown_item_rect(state.dropdown_chosen, value);
            if (sdl_rect_has_point(item_rect, engine.mouse_position)) {
                state.hover = OPTION_HOVER_DROPDOWN_ITEM;
                state.hover_subindex = value;
                break;
            }
        }
    }
}

void options_menu_render(const options_menu_state_t& state) {
    if (state.mode == OPTION_MENU_CLOSED) {
        return;
    }

    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 128);
    SDL_RenderFillRect(engine.renderer, &SCREEN_RECT);
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);

    // HOTKEYS RENDER
    if (state.mode == OPTION_MENU_HOTKEYS) {
        render_ninepatch(SPRITE_UI_FRAME, OPTIONS_FRAME_RECT, 16);

        render_text(FONT_WESTERN8_GOLD, "Hotkey Groups:", xy(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + 16));

        for (int hotkey_group = 0; hotkey_group < HOTKEY_GROUP_COUNT; hotkey_group++) {
            int frame = 2;
            if (state.hover == OPTION_HOVER_HOTKEY_GROUP && state.hover_subindex == hotkey_group) {
                frame = 1;
            } else if (state.hotkey_group_chosen == hotkey_group) {
                frame = 0;
            }
            render_sprite(SPRITE_UI_BUTTON, xy(frame, 0), option_hotkey_group_position(hotkey_group), RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(HOTKEY_GROUPS.at((OptionHotkeyGroup)hotkey_group).icon - 1, frame), option_hotkey_group_position(hotkey_group), RENDER_SPRITE_NO_CULL);
        }

        if (state.hotkey_group_chosen != -1) {
            const hotkey_group_t& hotkey_group = HOTKEY_GROUPS.at((OptionHotkeyGroup)state.hotkey_group_chosen);
            render_text(FONT_WESTERN8_GOLD, hotkey_group.name, xy(option_hotkey_position(0).x, OPTIONS_FRAME_RECT.y + 16));

            for (int hotkey = 0; hotkey < 6; hotkey++) {
                int frame = 0;
                if (state.hover == OPTION_HOVER_HOTKEYS_HOTKEY && state.hover_subindex == hotkey) {
                    frame = 1;
                } else if (state.hotkey_chosen == hotkey) {
                    frame = 2;
                }
                render_sprite(SPRITE_UI_BUTTON, xy(frame, 0), option_hotkey_position(hotkey), RENDER_SPRITE_NO_CULL);
                if (hotkey_group.hotkeys[hotkey] != UI_BUTTON_NONE) {
                    render_sprite(SPRITE_UI_BUTTON_ICON, xy(hotkey_group.hotkeys[hotkey] - 1, frame), option_hotkey_position(hotkey), RENDER_SPRITE_NO_CULL);
                }
            }

            if (state.hotkey_chosen != -1) {
                char hotkey_str[64];
                option_get_hotkey_str(hotkey_str, hotkey_group.hotkeys[state.hotkey_chosen], state.hotkeys);
                render_text(FONT_WESTERN8_GOLD, hotkey_str, option_hotkey_position(3) + xy(0, 36));
                render_text(FONT_WESTERN8_GOLD, "Press the desired key.", option_hotkey_position(3) + xy(0, 36 + 16));
            }

            if (!option_is_hotkey_group_valid(state.hotkeys, state.hotkey_group_chosen)) {
                render_text(FONT_WESTERN8_RED, "Multiple buttons in this group", option_hotkey_position(3) + xy(0, 36 + 36 + 4));
                render_text(FONT_WESTERN8_RED, "have the same hotkey!", option_hotkey_position(3) + xy(0, 36 + 36 + 16 + 4));
            }
        }

        render_menu_button("SET TO DEFAULT LAYOUT", DEFAULTS_BUTTON_POSITION, state.hover == OPTION_HOVER_HOTKEYS_DEFAULTS);
        render_menu_button("SET TO GRID LAYOUT", GRID_BUTTON_POSITION, state.hover == OPTION_HOVER_HOTKEYS_GRID);

        if (state.hotkey_save_status_timer != 0) {
            if (state.hotkey_save_successful) {
                xy save_status_size = render_get_text_size(FONT_WESTERN8_WHITE, "Hotkeys saved.");
                render_text(FONT_WESTERN8_WHITE, "Hotkeys saved.", xy(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - save_status_size.x, SAVE_BUTTON_POSITION.y - 24));
            } else {
                xy save_status_size = render_get_text_size(FONT_WESTERN8_WHITE, "Some hotkeys have conflicts!");
                render_text(FONT_WESTERN8_RED, "Some hotkeys have conflicts!", xy(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 16 - save_status_size.x, SAVE_BUTTON_POSITION.y - 24));
            }
        }

        render_menu_button("SAVE", SAVE_BUTTON_POSITION, state.hover == OPTION_HOVER_HOTKEYS_SAVE);
        render_menu_button("BACK", BACK_BUTTON_POSITION, state.hover == OPTION_HOVER_BACK);
    // OPTIONS RENDER
    } else {
        render_ninepatch(SPRITE_UI_FRAME, OPTIONS_FRAME_RECT, 16);

        for (int option = 0; option < OPTION_COUNT; option++) {
            int dropdown_vframe = 0;
            if (state.hover == OPTION_HOVER_DROPDOWN && state.hover_subindex == option) {
                dropdown_vframe = 1;
            } else if (state.dropdown_chosen == option) {
                dropdown_vframe = 2;
            }

            SDL_Rect dropdown_rect = options_get_dropdown_rect(option);
            const option_data_t& option_data = OPTION_DATA.at((Option)option);
            render_text(FONT_WESTERN8_GOLD, option_data.name, xy(OPTIONS_FRAME_RECT.x + 8, dropdown_rect.y + 5));

            if (option_data.type == OPTION_TYPE_DROPDOWN) {
                render_sprite(SPRITE_UI_OPTIONS_DROPDOWN, xy(0, dropdown_vframe), xy(dropdown_rect.x, dropdown_rect.y), RENDER_SPRITE_NO_CULL);
                render_text(dropdown_vframe == 1 ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, option_value_str((Option)option, engine.options.at((Option)option)), xy(dropdown_rect.x + 5, dropdown_rect.y + 5));
            } else if (option_data.type == OPTION_TYPE_SLIDER || option_data.type == OPTION_TYPE_SLIDER_PERCENT) {
                char option_value_str[8];
                if (option_data.type == OPTION_TYPE_SLIDER_PERCENT) {
                    sprintf(option_value_str, "%i%%", (int)(((float)engine.options[(Option)option] * 100.0f) / (float)option_data.max_value));
                } else {
                    sprintf(option_value_str, "%i", engine.options[(Option)option]);
                }
                xy option_value_str_size = render_get_text_size(FONT_WESTERN8_GOLD, option_value_str);
                render_text(FONT_WESTERN8_GOLD, option_value_str, xy(dropdown_rect.x - 4 - option_value_str_size.x, dropdown_rect.y + 5));

                static const int SLIDER_RECT_HEIGHT = 5;
                static const int NOTCH_WIDTH = 5;
                static const int NOTCH_HEIGHT = SLIDER_RECT_HEIGHT + 9;
                SDL_Rect slider_rect = (SDL_Rect) { .x = dropdown_rect.x, .y = dropdown_rect.y + (dropdown_rect.h / 2) - (SLIDER_RECT_HEIGHT / 2) + 1, .w = dropdown_rect.w, .h = SLIDER_RECT_HEIGHT };

                SDL_SetRenderDrawColor(engine.renderer, COLOR_DARKBLACK.r, COLOR_DARKBLACK.g, COLOR_DARKBLACK.b, COLOR_DARKBLACK.a);
                SDL_RenderFillRect(engine.renderer, &slider_rect);

                slider_rect.x += 1;
                slider_rect.w = ((dropdown_rect.w - 2) * (engine.options[(Option)option] - option_data.min_value)) / option_data.max_value;
                SDL_Rect notch_rect = (SDL_Rect) { .x = slider_rect.x + slider_rect.w - (NOTCH_WIDTH / 2), .y = slider_rect.y + (SLIDER_RECT_HEIGHT / 2) - (NOTCH_HEIGHT / 2), .w = NOTCH_WIDTH, .h = NOTCH_HEIGHT };

                SDL_SetRenderDrawColor(engine.renderer, COLOR_GOLD.r, COLOR_GOLD.g, COLOR_GOLD.b, COLOR_GOLD.a);
                SDL_RenderFillRect(engine.renderer, &slider_rect);

                slider_rect.x = dropdown_rect.x;
                slider_rect.w = dropdown_rect.w;

                SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
                SDL_RenderDrawRect(engine.renderer, &slider_rect);

                SDL_SetRenderDrawColor(engine.renderer, COLOR_GOLD.r, COLOR_GOLD.g, COLOR_GOLD.b, COLOR_GOLD.a);
                SDL_RenderFillRect(engine.renderer, &notch_rect);
                SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
                SDL_RenderDrawRect(engine.renderer, &notch_rect);
            }
        }

        if (state.dropdown_chosen != -1) {
            const option_data_t& option_data = OPTION_DATA.at((Option)state.dropdown_chosen);
            for (int value = 0; value < option_data.max_value; value++) {
                SDL_Rect item_rect = options_get_dropdown_item_rect(state.dropdown_chosen, value);
                int item_vframe = state.hover == OPTION_HOVER_DROPDOWN_ITEM && state.hover_subindex == value ? 4 : 3;
                render_sprite(SPRITE_UI_OPTIONS_DROPDOWN, xy(0, item_vframe), xy(item_rect.x, item_rect.y));
                render_text(item_vframe == 3 ? FONT_WESTERN8_OFFBLACK : FONT_WESTERN8_WHITE, option_value_str((Option)state.dropdown_chosen, value), xy(item_rect.x + 5, item_rect.y + 5));
            }
        }

        render_menu_button("EDIT HOTKEYS", HOTKEYS_BUTTON_POSITION, state.hover == OPTION_HOVER_HOTKEYS);
        render_menu_button("BACK", BACK_BUTTON_POSITION, state.hover == OPTION_HOVER_BACK);
    }
}

SDL_Rect options_get_dropdown_rect(int index) {
    return (SDL_Rect) {
        .x = OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 8 - engine.sprites[SPRITE_UI_OPTIONS_DROPDOWN].frame_size.x,
        .y = OPTIONS_FRAME_RECT.y + 16 + ((engine.sprites[SPRITE_UI_OPTIONS_DROPDOWN].frame_size.y + 4) * index),
        .w = engine.sprites[SPRITE_UI_OPTIONS_DROPDOWN].frame_size.x,
        .h = engine.sprites[SPRITE_UI_OPTIONS_DROPDOWN].frame_size.y
    };
}

SDL_Rect options_get_dropdown_item_rect(int option, int value) {
    SDL_Rect dropdown_rect = options_get_dropdown_rect(option);
    dropdown_rect.y += engine.sprites[SPRITE_UI_OPTIONS_DROPDOWN].frame_size.y * (value + 1);
    return dropdown_rect;
}

const char* option_value_str(Option option, int value) {
    switch (option) {
        case OPTION_DISPLAY: {
            switch ((OptionValueDisplay)value) {
                case DISPLAY_WINDOWED:
                    return "Windowed";
                case DISPLAY_FULLSCREEN:
                    return "Fullscreen";
                case DISPLAY_BORDERLESS:
                    return "Borderless";
                default:
                    return "";
            }
        }
        case OPTION_VSYNC: {
            switch ((OptionValueVsync)value) {
                case VSYNC_DISABLED:
                    return "Disabled";
                case VSYNC_ENABLED:
                    return "Enabled";
                default:
                    return "";
            }
        }
        default:
            return "";
    }
}

xy option_hotkey_group_position(int group) {
    int row = group / 3;
    int col = group % 3;
    return xy(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + 32) + 
            xy((engine.sprites[SPRITE_UI_BUTTON].frame_size.x + 4) * col, 
                        (engine.sprites[SPRITE_UI_BUTTON].frame_size.y + 4) * row);
}

xy option_hotkey_position(int button) {
    int frame_size = engine.sprites[SPRITE_UI_BUTTON].frame_size.x;
    return xy(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 40 - frame_size - ((frame_size + 4) * 4), OPTIONS_FRAME_RECT.y + 32) +
            xy((frame_size + 4) * (button % 3), (frame_size + 4) * (button / 3));
}

void option_get_hotkey_str(char* str_ptr, UiButton button, const std::unordered_map<UiButton, SDL_Keycode>& hotkeys) {
    switch (button) {
        case UI_BUTTON_STOP:
            str_ptr += sprintf(str_ptr, "Stop");
            break;
        case UI_BUTTON_ATTACK:
            str_ptr += sprintf(str_ptr, "Attack");
            break;
        case UI_BUTTON_BUILD:
            str_ptr += sprintf(str_ptr, "Build");
            break;
        case UI_BUTTON_BUILD2:
            str_ptr += sprintf(str_ptr, "Advanced Build");
            break;
        case UI_BUTTON_REPAIR:
            str_ptr += sprintf(str_ptr, "Repair");
            break;
        case UI_BUTTON_DEFEND:
            str_ptr += sprintf(str_ptr, "Defend");
            break;
        case UI_BUTTON_UNLOAD:
            str_ptr += sprintf(str_ptr, "Unload");
            break;
        case UI_BUTTON_EXPLODE:
            str_ptr += sprintf(str_ptr, "Explode");
            break;
        case UI_BUTTON_SMOKE:
            str_ptr += sprintf(str_ptr, "Smoke Bomb");
            break;
        case UI_BUTTON_CANCEL:
            str_ptr += sprintf(str_ptr, "Cancel");
            break;
        case UI_BUTTON_UNIT_MINER:
            str_ptr += sprintf(str_ptr, "Hire Miner");
            break;
        case UI_BUTTON_UNIT_COWBOY:
            str_ptr += sprintf(str_ptr, "Hire Cowboy");
            break;
        case UI_BUTTON_UNIT_BANDIT:
            str_ptr += sprintf(str_ptr, "Hire Bandit");
            break;
        case UI_BUTTON_UNIT_WAGON:
            str_ptr += sprintf(str_ptr, "Hire Wagon");
            break;
        case UI_BUTTON_UNIT_WAR_WAGON:
            str_ptr += sprintf(str_ptr, "Hire War Wagon");
            break;
        case UI_BUTTON_UNIT_JOCKEY:
            str_ptr += sprintf(str_ptr, "Hire Jockey");
            break;
        case UI_BUTTON_UNIT_SAPPER:
            str_ptr += sprintf(str_ptr, "Hire Sapper");
            break;
        case UI_BUTTON_UNIT_TINKER:
            str_ptr += sprintf(str_ptr, "Hire Tinker");
            break;
        case UI_BUTTON_UNIT_SOLDIER:
            str_ptr += sprintf(str_ptr, "Hire Soldier");
            break;
        case UI_BUTTON_UNIT_CANNON:
            str_ptr += sprintf(str_ptr, "Hire Cannoneer");
            break;
        case UI_BUTTON_UNIT_SPY:
            str_ptr += sprintf(str_ptr, "Hire Detective");
            break;
        case UI_BUTTON_BUILD_HALL:
            str_ptr += sprintf(str_ptr, "Build Town Hall");
            break;
        case UI_BUTTON_BUILD_HOUSE:
            str_ptr += sprintf(str_ptr, "Build House");
            break;
        case UI_BUTTON_BUILD_CAMP:
            str_ptr += sprintf(str_ptr, "Build Mining Camp");
            break;
        case UI_BUTTON_BUILD_SALOON:
            str_ptr += sprintf(str_ptr, "Build Saloon");
            break;
        case UI_BUTTON_BUILD_BUNKER:
            str_ptr += sprintf(str_ptr, "Build Bunker");
            break;
        case UI_BUTTON_BUILD_COOP:
            str_ptr += sprintf(str_ptr, "Build Coop");
            break;
        case UI_BUTTON_BUILD_SMITH:
            str_ptr += sprintf(str_ptr, "Build Blacksmith");
            break;
        case UI_BUTTON_BUILD_BARRACKS:
            str_ptr += sprintf(str_ptr, "Build Barracks");
            break;
        case UI_BUTTON_BUILD_SHERIFFS:
            str_ptr += sprintf(str_ptr, "Build Sheriff's Office");
            break;
        case UI_BUTTON_BUILD_MINE:
            str_ptr += sprintf(str_ptr, "Lay Mine");
            break;
        case UI_BUTTON_RESEARCH_WAR_WAGON:
            str_ptr += sprintf(str_ptr, "Research War Wagon");
            break;
        case UI_BUTTON_RESEARCH_EXPLOSIVES:
            str_ptr += sprintf(str_ptr, "Research Explosives");
            break;
        case UI_BUTTON_RESEARCH_BAYONETS:
            str_ptr += sprintf(str_ptr, "Research Bayonets");
            break;
        case UI_BUTTON_RESEARCH_SMOKE:
            str_ptr += sprintf(str_ptr, "Research Smoke");
            break;
        default:
            break;
    }

    str_ptr += sprintf(str_ptr, " (");
    SDL_Keycode hotkey = hotkeys.at(button);
    str_ptr += engine_sdl_key_str(str_ptr, hotkey);
    str_ptr += sprintf(str_ptr, ")");
}

bool option_is_hotkey_group_valid(const std::unordered_map<UiButton, SDL_Keycode>& hotkeys, int group) {
    const hotkey_group_t& hotkey_group = HOTKEY_GROUPS.at((OptionHotkeyGroup)group);
    for (int index = 0; index < 6; index++) {
        if (hotkey_group.hotkeys[index] == UI_BUTTON_NONE) {
            continue;
        }
        for (int other = 0; other < 6; other++) {
            if (hotkey_group.hotkeys[other] == UI_BUTTON_NONE) {
                continue;
            }
            if (index == other) {
                continue;
            }

            if (hotkeys.at(hotkey_group.hotkeys[index]) == hotkeys.at(hotkey_group.hotkeys[other])) {
                return false;
            }
        }
    }

    return true;
}
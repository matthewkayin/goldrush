#include "options.h"

#include "logger.h"

static const int OPTIONS_MENU_WIDTH = 250;
static const int OPTIONS_MENU_HEIGHT = 300;
static const SDL_Rect OPTIONS_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (OPTIONS_MENU_WIDTH / 2),
    .y = (SCREEN_HEIGHT / 2) - (OPTIONS_MENU_HEIGHT / 2),
    .w = OPTIONS_MENU_WIDTH,
    .h = OPTIONS_MENU_HEIGHT
};
static const xy BACK_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);

options_menu_state_t options_menu_open() {
    options_menu_state_t state;

    state.mode = OPTION_MENU_OPEN;
    state.hover = OPTION_HOVER_NONE;
    state.hover_subindex = 0;
    state.dropdown_chosen = -1;
    state.slider_chosen = -1;

    return state;
}

void options_menu_handle_input(options_menu_state_t& state, SDL_Event event) {
    if (state.mode == OPTION_MENU_CLOSED) {
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (state.hover != OPTION_HOVER_NONE) {
            sound_play(SOUND_UI_SELECT);
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
        } else if (state.hover == OPTION_HOVER_DROPDOWN) {
            state.dropdown_chosen = state.hover_subindex;
            state.hover = OPTION_HOVER_NONE;
            return;
        } 

        for (int option = 0; option < OPTION_COUNT; option++) {
            SDL_Rect dropdown_rect = options_get_dropdown_rect(option);
            if (sdl_rect_has_point(dropdown_rect, engine.mouse_position)) {
                const option_data_t& option_data = OPTION_DATA.at((Option)option);
                if (option_data.type == OPTION_TYPE_SLIDER) {
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

    if (state.slider_chosen != -1) {
        SDL_Rect dropdown_rect = options_get_dropdown_rect(state.slider_chosen);
        int mouse_x = engine.mouse_position.x;
        if (mouse_x < dropdown_rect.x) {
            mouse_x = dropdown_rect.x;
        } else if (mouse_x > dropdown_rect.x + dropdown_rect.w) {
            mouse_x = dropdown_rect.x + dropdown_rect.w;
        }
        mouse_x -= dropdown_rect.x;

        engine_apply_option((Option)state.slider_chosen, (mouse_x * OPTION_DATA.at((Option)state.slider_chosen).max_value) / dropdown_rect.w);
        return;
    }

    SDL_Rect back_button_rect = render_get_button_rect("BACK", BACK_BUTTON_POSITION);
    if (sdl_rect_has_point(back_button_rect, engine.mouse_position)) {
        state.hover = OPTION_HOVER_BACK;
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
        } else if (option_data.type == OPTION_TYPE_SLIDER) {
            static const int SLIDER_RECT_HEIGHT = 5;
            static const int NOTCH_WIDTH = 5;
            static const int NOTCH_HEIGHT = SLIDER_RECT_HEIGHT + 9;
            SDL_Rect slider_rect = (SDL_Rect) { .x = dropdown_rect.x, .y = dropdown_rect.y + (dropdown_rect.h / 2) - (SLIDER_RECT_HEIGHT / 2) + 1, .w = dropdown_rect.w, .h = SLIDER_RECT_HEIGHT };

            SDL_SetRenderDrawColor(engine.renderer, COLOR_DARKBLACK.r, COLOR_DARKBLACK.g, COLOR_DARKBLACK.b, COLOR_DARKBLACK.a);
            SDL_RenderFillRect(engine.renderer, &slider_rect);

            slider_rect.x += 1;
            slider_rect.w = ((dropdown_rect.w - 2) * engine.options[(Option)option]) / option_data.max_value;
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

    render_menu_button("BACK", BACK_BUTTON_POSITION, state.hover == OPTION_HOVER_BACK);
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
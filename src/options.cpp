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
static const int CONFIRM_CHANGES_WIDTH = 264;
static const int CONFIRM_CHANGES_HEIGHT = 84;
static const SDL_Rect OPTIONS_CONFIRM_CHANGES_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (CONFIRM_CHANGES_WIDTH / 2),
    .y = (SCREEN_HEIGHT / 2) - (CONFIRM_CHANGES_HEIGHT / 2),
    .w = CONFIRM_CHANGES_WIDTH,
    .h = CONFIRM_CHANGES_HEIGHT
};
static const xy APPLY_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + OPTIONS_FRAME_RECT.w - 64 - 16, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const xy BACK_BUTTON_POSITION = xy(OPTIONS_FRAME_RECT.x + 16, OPTIONS_FRAME_RECT.y + OPTIONS_FRAME_RECT.h - 21 - 8);
static const int CONFIRM_CHANGES_DURATION = 60 * 30;
static const xy CONFIRM_NO_BUTTON_POSITION = xy(OPTIONS_CONFIRM_CHANGES_RECT.x + 16, OPTIONS_CONFIRM_CHANGES_RECT.y + OPTIONS_CONFIRM_CHANGES_RECT.h - 21 - 8);
static const xy CONFIRM_YES_BUTTON_POSITION = xy(OPTIONS_CONFIRM_CHANGES_RECT.x + OPTIONS_CONFIRM_CHANGES_RECT.w - 48 - 16, OPTIONS_CONFIRM_CHANGES_RECT.y + OPTIONS_CONFIRM_CHANGES_RECT.h - 21 - 8);

options_menu_state_t options_menu_open() {
    options_menu_state_t state;

    state.mode = OPTION_MENU_OPEN;
    state.hover = OPTION_HOVER_NONE;
    state.hover_subindex = 0;
    state.dropdown_chosen = -1;
    state.pending_changes = engine.options;
    state.confirm_changes_timer = 0;

    for (int option = 0; option < OPTION_COUNT; option++) {
        state.pending_changes[(Option)option] = engine.options[(Option)option];
    }

    return state;
}

void options_menu_handle_input(options_menu_state_t& state, SDL_Event event) {
    if (state.mode == OPTION_MENU_CLOSED) {
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (state.dropdown_chosen != -1) {
            if (state.hover == OPTION_HOVER_DROPDOWN_ITEM) {
                state.pending_changes[(Option)state.dropdown_chosen] = state.hover_subindex;
            }
            state.dropdown_chosen = -1;
            sound_play(SOUND_UI_SELECT);
            return;
        }

        if (state.hover == OPTION_HOVER_NONE) {
            return;
        }
        sound_play(SOUND_UI_SELECT);

        if (state.hover == OPTION_HOVER_BACK) {
            state.mode = OPTION_MENU_CLOSED;
            return;
        } else if (state.hover == OPTION_HOVER_APPLY) {
            bool request_changes = false;
            for (int option = 0; option < OPTION_COUNT; option++) {
                if (engine.options[(Option)option] != state.pending_changes[(Option)option] && OPTION_DATA.at((Option)option).confirm_required) {
                    request_changes = true;
                    break;
                }
            }
            if (request_changes) {
                SDL_SetWindowGrab(engine.window, SDL_FALSE);
                engine_destroy_renderer();
                engine_init_renderer(state.pending_changes);
                SDL_SetWindowGrab(engine.window, SDL_TRUE);
                state.confirm_changes_timer = CONFIRM_CHANGES_DURATION;
                state.mode = OPTION_MENU_CONFIRM_CHANGES;
            }
            return;
        } else if (state.hover == OPTION_HOVER_DROPDOWN) {
            state.dropdown_chosen = state.hover_subindex;
            state.hover = OPTION_HOVER_NONE;
            return;
        } else if (state.hover == OPTION_HOVER_CONFIRM_NO) {
            for (int option = 0; option < OPTION_COUNT; option++) {
                state.pending_changes[(Option)option] = engine.options[(Option)option];
            }
            SDL_SetWindowGrab(engine.window, SDL_FALSE);
            engine_destroy_renderer();
            engine_init_renderer(state.pending_changes);
            SDL_SetWindowGrab(engine.window, SDL_TRUE);
            state.confirm_changes_timer = 0;
            state.mode = OPTION_MENU_OPEN;
            return;
        } else if (state.hover == OPTION_HOVER_CONFIRM_YES) {
            for (int option = 0; option < OPTION_COUNT; option++) {
                engine.options[(Option)option] = state.pending_changes[(Option)option];
            }
            state.confirm_changes_timer = 0;
            state.mode = OPTION_MENU_OPEN;
        }
    }
}

void options_menu_update(options_menu_state_t& state) {
    if (state.mode == OPTION_MENU_CLOSED) {
        return;
    }

    if (state.mode == OPTION_MENU_CONFIRM_CHANGES) {
        state.confirm_changes_timer--;
        if (state.confirm_changes_timer == 0) {
            SDL_SetWindowGrab(engine.window, SDL_FALSE);
            engine_destroy_renderer();
            engine_init_renderer(engine.options);
            SDL_SetWindowGrab(engine.window, SDL_TRUE);
            state.mode = OPTION_MENU_OPEN;
            return;
        }

        SDL_Rect no_button_rect = render_get_button_rect("NO", CONFIRM_NO_BUTTON_POSITION);
        SDL_Rect yes_button_rect = render_get_button_rect("YES", CONFIRM_YES_BUTTON_POSITION);
        if (sdl_rect_has_point(no_button_rect, engine.mouse_position)) {
            state.hover = OPTION_HOVER_CONFIRM_NO;
        } else if (sdl_rect_has_point(yes_button_rect, engine.mouse_position)) {
            state.hover = OPTION_HOVER_CONFIRM_YES;
        }
        return;
    }

    state.hover = OPTION_HOVER_NONE;
    SDL_Rect back_button_rect = render_get_button_rect("BACK", BACK_BUTTON_POSITION);
    SDL_Rect apply_button_rect = render_get_button_rect("APPLY", APPLY_BUTTON_POSITION);
    if (sdl_rect_has_point(back_button_rect, engine.mouse_position)) {
        state.hover = OPTION_HOVER_BACK;
    } else if (sdl_rect_has_point(apply_button_rect, engine.mouse_position)) {
        state.hover = OPTION_HOVER_APPLY;
    }

    if (state.hover == OPTION_HOVER_NONE && state.dropdown_chosen == -1) {
        for (int option = 0; option < OPTION_COUNT; option++) {
            SDL_Rect dropdown_rect = options_get_dropdown_rect(option);
            if (sdl_rect_has_point(dropdown_rect, engine.mouse_position)) {
                state.hover = OPTION_HOVER_DROPDOWN;
                state.hover_subindex = option;
                break;
            }
        }
    }

    if (state.hover == OPTION_HOVER_NONE && state.dropdown_chosen != -1) {
        for (int value = 0; value < OPTION_DATA.at((Option)state.dropdown_chosen).value_count; value++) {
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
        render_text(FONT_WESTERN8_GOLD, engine_option_name_str((Option)option), xy(OPTIONS_FRAME_RECT.x + 8, dropdown_rect.y + 5));
        render_sprite(SPRITE_UI_OPTIONS_DROPDOWN, xy(0, dropdown_vframe), xy(dropdown_rect.x, dropdown_rect.y), RENDER_SPRITE_NO_CULL);
        render_text(dropdown_vframe == 1 ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, engine_option_value_str((Option)option, state.pending_changes.at((Option)option)), xy(dropdown_rect.x + 5, dropdown_rect.y + 5));
    }

    if (state.dropdown_chosen != -1) {
        for (int value = 0; value < OPTION_DATA.at((Option)state.dropdown_chosen).value_count; value++) {
            SDL_Rect item_rect = options_get_dropdown_item_rect(state.dropdown_chosen, value);
            int item_vframe = state.hover == OPTION_HOVER_DROPDOWN_ITEM && state.hover_subindex == value ? 4 : 3;
            render_sprite(SPRITE_UI_OPTIONS_DROPDOWN, xy(0, item_vframe), xy(item_rect.x, item_rect.y));
            render_text(item_vframe == 3 ? FONT_WESTERN8_OFFBLACK : FONT_WESTERN8_WHITE, engine_option_value_str((Option)state.dropdown_chosen, value), xy(item_rect.x + 5, item_rect.y + 5));
        }
    }

    render_menu_button("APPLY", APPLY_BUTTON_POSITION, state.hover == OPTION_HOVER_APPLY);
    render_menu_button("BACK", BACK_BUTTON_POSITION, state.hover == OPTION_HOVER_BACK);

    if (state.mode == OPTION_MENU_CONFIRM_CHANGES) {
        render_ninepatch(SPRITE_UI_FRAME, OPTIONS_CONFIRM_CHANGES_RECT, 16);
        render_text(FONT_WESTERN8_GOLD, "Do you want to keep these changes?", xy(SCREEN_WIDTH / 2, OPTIONS_CONFIRM_CHANGES_RECT.y + 8), TEXT_ANCHOR_TOP_CENTER);
        char countdown_text[8];
        sprintf(countdown_text, "%u", state.confirm_changes_timer / 60);
        render_text(FONT_WESTERN8_GOLD, countdown_text, xy(SCREEN_WIDTH / 2, OPTIONS_CONFIRM_CHANGES_RECT.y + 32), TEXT_ANCHOR_TOP_CENTER);

        render_menu_button("NO", CONFIRM_NO_BUTTON_POSITION, state.hover == OPTION_HOVER_CONFIRM_NO);
        render_menu_button("YES", CONFIRM_YES_BUTTON_POSITION, state.hover == OPTION_HOVER_CONFIRM_YES);
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
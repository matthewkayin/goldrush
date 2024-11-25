#include "menu.h"

#include <cstring>
#include <unordered_map>

static const std::unordered_map<MenuMode, std::vector<MenuButton>> MODE_BUTTONS = {
    { MENU_MODE_MAIN, { MENU_BUTTON_PLAY, MENU_BUTTON_OPTIONS, MENU_BUTTON_EXIT } },
    { MENU_MODE_PLAY, { MENU_BUTTON_HOST, MENU_BUTTON_JOIN, MENU_BUTTON_BACK } }
};

struct menu_button_t {
    const char* text;
    SDL_Rect rect;
};

static const std::unordered_map<MenuButton, menu_button_t> MENU_BUTTON_DATA = {
    { MENU_BUTTON_PLAY, (menu_button_t) {
        .text = "PLAY",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128,
            .w = 84, .h = 30
    }}},
    { MENU_BUTTON_OPTIONS, (menu_button_t) {
        .text = "OPTIONS",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42,
            .w = 120, .h = 30
    }}},
    { MENU_BUTTON_EXIT, (menu_button_t) {
        .text = "EXIT",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42 + 42,
            .w = 72, .h = 30
    }}},
    { MENU_BUTTON_HOST, (menu_button_t) {
        .text = "HOST",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42,
            .w = 76, .h = 30
    }}},
    { MENU_BUTTON_JOIN, (menu_button_t) {
        .text = "JOIN",
        .rect = (SDL_Rect) {
            .x = 48 + 76 + 8, .y = 128 + 42,
            .w = 72, .h = 30
    }}},
    { MENU_BUTTON_BACK, (menu_button_t) {
        .text = "BACK",
        .rect = (SDL_Rect) {
            .x = 48, .y = 128 + 42 + 42,
            .w = 78, .h = 30
    }}}
};

static const SDL_Rect TEXT_INPUT_RECT = (SDL_Rect) {
    .x = 48, .y = 120, .w = 264, .h = 35 
};

menu_state_t menu_init() {
    menu_state_t state;

    state.mode = MENU_MODE_MAIN;
    state.status_text = "";
    state.username = "";
    state.status_timer = 0;
    state.button_hovered = MENU_BUTTON_NONE;

    return state;
}

void menu_handle_input(menu_state_t& state, SDL_Event event) {
    // Mouse pressed
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        // Button pressed
        switch (state.button_hovered) {
            case MENU_BUTTON_PLAY: {
                state.mode = MENU_MODE_PLAY;
                break;
            }
            case MENU_BUTTON_OPTIONS: {
                break;
            }
            case MENU_BUTTON_EXIT: {
                state.mode = MENU_MODE_EXIT;
                break;
            }
            case MENU_BUTTON_BACK: {
                switch (state.mode) {
                    case MENU_MODE_PLAY:
                        state.mode = MENU_MODE_MAIN;
                        break;
                    default:
                        break;
                }
            }
            default:
                break;
        }

        // Text input pressed
        if (state.button_hovered == MENU_BUTTON_NONE && state.mode == MENU_MODE_PLAY &&
            sdl_rect_has_point(TEXT_INPUT_RECT, engine.mouse_position)) {
            SDL_SetTextInputRect(&TEXT_INPUT_RECT);
            SDL_StartTextInput();
        }
    } else if (event.type == SDL_TEXTINPUT) {
        if (state.mode == MENU_MODE_PLAY) {
            state.username += std::string(event.text.text);
            if (state.username.length() > MAX_USERNAME_LENGTH) {
                state.username = state.username.substr(0, MAX_USERNAME_LENGTH);
            }
        }
    } else if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
        if (state.mode == MENU_MODE_PLAY && state.username.length() > 0) {
            state.username.pop_back();
        }
    }
}

void menu_update(menu_state_t& state) {
    state.button_hovered = MENU_BUTTON_NONE;
    if (SDL_GetWindowMouseGrab(engine.window) == SDL_TRUE) {
        auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
        if (mode_buttons_it != MODE_BUTTONS.end()) {
            for (MenuButton button : mode_buttons_it->second) {
                if (sdl_rect_has_point(MENU_BUTTON_DATA.at(button).rect, engine.mouse_position)) {
                    state.button_hovered = button;
                    break;
                }
            }
        }
    }
}

void menu_render(const menu_state_t& state) {
    SDL_Rect background_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_SetRenderDrawColor(engine.renderer, COLOR_SKY_BLUE.r, COLOR_SKY_BLUE.g, COLOR_SKY_BLUE.b, COLOR_SKY_BLUE.a);
    SDL_RenderFillRect(engine.renderer, &background_rect);

    render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_OFFBLACK, xy(24, 24));

    if (state.mode == MENU_MODE_PLAY) {
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(engine.renderer, &TEXT_INPUT_RECT);
        render_text(FONT_WESTERN16, state.username.c_str(), COLOR_BLACK, xy(TEXT_INPUT_RECT.x + 4, TEXT_INPUT_RECT.y + 31), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    auto mode_buttons_it = MODE_BUTTONS.find(state.mode);
    if (mode_buttons_it != MODE_BUTTONS.end()) {
        for (MenuButton button : mode_buttons_it->second) {
            SDL_Color button_color = button == state.button_hovered ? COLOR_WHITE : COLOR_BLACK;
            SDL_SetRenderDrawColor(engine.renderer, button_color.r, button_color.g, button_color.b, button_color.a);

            const menu_button_t& button_data = MENU_BUTTON_DATA.at(button);
            SDL_RenderDrawRect(engine.renderer, &button_data.rect);
            render_text(FONT_WESTERN16, button_data.text, button_color, xy(button_data.rect.x + 4, button_data.rect.y + 4));
        }
    }
}
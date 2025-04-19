#include "input.h"

#include "defines.h"
#include "core/logger.h"
#include <cstring>

struct InputState {
    SDL_Window* window;
    ivec2 window_size;
    ivec2 screen_position;
    int scaled_screen_y;

    ivec2 mouse_position;
    bool current[INPUT_ACTION_COUNT];
    bool previous[INPUT_ACTION_COUNT];
    bool user_requests_exit;

    std::string* text_input_str;
    size_t text_input_max_length;

    SDL_Keycode hotkey_mapping[INPUT_ACTION_COUNT];

    SDL_Keycode key_just_pressed;
};
static InputState state;

void input_update_window_size();

void input_init(SDL_Window* window) {
    memset(&state, 0, sizeof(state));
    state.window = window;
    input_update_window_size();
    input_stop_text_input();
    input_set_hotkey_mapping_to_default(state.hotkey_mapping);
}

void input_update_window_size() {
    SDL_GetWindowSize(state.window, &state.window_size.x, &state.window_size.y);
    float aspect = (float)state.window_size.x / (float)SCREEN_WIDTH;
    float scaled_y = (float)SCREEN_HEIGHT * aspect;
    float border_size = ((float)state.window_size.y - scaled_y) / 2.0;
    state.scaled_screen_y = (int)scaled_y;
    state.screen_position = ivec2(0, (int)border_size);
}

void input_poll_events() {
    memcpy(&state.previous, &state.current, sizeof(state.current));
    state.key_just_pressed = INPUT_KEY_NONE;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle quit
        if (event.type == SDL_QUIT) {
            state.user_requests_exit = true;
            break;
        }

        // Capture mouse
        if (SDL_GetWindowGrab(state.window) == SDL_FALSE) {
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_SetWindowGrab(state.window, SDL_TRUE);
                continue;
            }
            // If the mouse is not captured, don't handle any other input
            // Breaking here also prevents the initial click into the window from triggering any action
            break;
        }

        #ifdef GOLD_DEBUG
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
                SDL_SetWindowGrab(state.window, SDL_FALSE);
                break;
            }
        #endif

        switch (event.type) {
            case SDL_MOUSEMOTION: {
                state.mouse_position = ivec2(event.motion.x - state.screen_position.x, event.motion.y - state.screen_position.y);
                state.mouse_position = ivec2((state.mouse_position.x * SCREEN_WIDTH) / state.window_size.x, (state.mouse_position.y * SCREEN_HEIGHT) / state.scaled_screen_y);
                break;
            }
            case SDL_MOUSEBUTTONDOWN: 
            case SDL_MOUSEBUTTONUP: {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        state.current[INPUT_ACTION_LEFT_CLICK] = event.type == SDL_MOUSEBUTTONDOWN;
                        break;
                    case SDL_BUTTON_RIGHT:
                        state.current[INPUT_ACTION_RIGHT_CLICK] = event.type == SDL_MOUSEBUTTONDOWN;
                        break;
                    default:
                        break;
                } 
                break;
            }
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                if (event.type == SDL_KEYDOWN) {
                    state.key_just_pressed = event.key.keysym.sym;
                }
                switch (event.key.keysym.sym) {
                    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:
                        state.current[INPUT_ACTION_SHIFT] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_LCTRL:
                    case SDLK_RCTRL:
                        state.current[INPUT_ACTION_CTRL] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_SPACE:
                        state.current[INPUT_ACTION_SPACE] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_F3:
                        state.current[INPUT_ACTION_F3] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_F10:
                        state.current[INPUT_ACTION_MATCH_MENU] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_RETURN: 
                        state.current[INPUT_ACTION_ENTER] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_BACKSPACE: {
                        if (event.type == SDL_KEYDOWN && SDL_IsTextInputActive() && state.text_input_str->length() > 0) {
                            state.text_input_str->pop_back();
                        }
                        break;
                    }
                    default: {
                        if (event.key.keysym.sym >= SDLK_0 && event.key.keysym.sym <= SDLK_9) {
                            int key_index = event.key.keysym.sym - SDLK_0;
                            state.current[INPUT_ACTION_NUM0 + key_index] = event.type == SDL_KEYDOWN;
                        } else {
                            for (uint32_t hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
                                if (event.key.keysym.sym == state.hotkey_mapping[hotkey]) {
                                    state.current[hotkey] = event.type == SDL_KEYDOWN;
                                }
                            }
                        }
                        break;
                    }
                }
                break;
            }
            case SDL_TEXTINPUT: {
                (*state.text_input_str) += std::string(event.text.text);
                if (state.text_input_str->length() > state.text_input_max_length) {
                    (*state.text_input_str) = state.text_input_str->substr(0, state.text_input_max_length);
                }
                break;
            }
            default: 
                break;
        }
    }
}

void input_start_text_input(std::string* str, size_t max_length) {
    SDL_StartTextInput();
    state.text_input_str = str;
    state.text_input_max_length = max_length;
}

void input_stop_text_input() {
    SDL_StopTextInput();
    state.text_input_str = NULL;
}

bool input_is_text_input_active() {
    return SDL_IsTextInputActive();
}

ivec2 input_get_mouse_position() {
    return state.mouse_position;
}

bool input_user_requests_exit() {
    return state.user_requests_exit;
}

bool input_is_action_pressed(InputAction action) {
    return state.current[action];
}

bool input_is_action_just_pressed(InputAction action) {
    return state.current[action] && !state.previous[action];
}

bool input_is_action_just_released(InputAction action) {
    return state.previous[action] && !state.current[action];
}

int input_sprintf_sdl_key_str(char* str_ptr, SDL_Keycode key) {
    if (key == SDLK_ESCAPE) {
        return sprintf(str_ptr, "ESC");
    } else if (key >= SDLK_a && key <= SDLK_z) {
        return sprintf(str_ptr, "%c", (char)(key - 32));
    } else if (key == SDLK_LEFTBRACKET) {
        return sprintf(str_ptr, "[");
    } else if (key == SDLK_RIGHTBRACKET) {
        return sprintf(str_ptr, "]");
    } else if (key == SDLK_BACKSLASH) {
        return sprintf(str_ptr, "\\");
    } else if (key == SDLK_BACKQUOTE) {
        return sprintf(str_ptr, "`");
    } else if (key == SDLK_MINUS) {
        return sprintf(str_ptr, "-");
    } else if (key == SDLK_EQUALS) {
        return sprintf(str_ptr, "=");
    } else if (key == SDLK_COMMA) {
        return sprintf(str_ptr, ",");
    } else if (key == SDLK_PERIOD) {
        return sprintf(str_ptr, ".");
    } else if (key == SDLK_SLASH) {
        return sprintf(str_ptr, "/");
    } else if (key == SDLK_SEMICOLON) {
        return sprintf(str_ptr, ";");
    } else if (key == SDLK_QUOTE) {
        return sprintf(str_ptr, "'");
    } else {
        return 0;
    }
}

SDL_Keycode input_get_hotkey_mapping(InputAction hotkey) {
    return state.hotkey_mapping[hotkey];
}

void input_set_hotkey_mapping(InputAction hotkey, SDL_Keycode key) {
    state.hotkey_mapping[hotkey] = key;
}

void input_set_hotkey_mapping_to_default(SDL_Keycode* hotkey_mapping) {
    // Unit
    hotkey_mapping[INPUT_HOTKEY_ATTACK] = SDLK_a;
    hotkey_mapping[INPUT_HOTKEY_STOP] = SDLK_s;
    hotkey_mapping[INPUT_HOTKEY_DEFEND] = SDLK_d;

    // Miner
    hotkey_mapping[INPUT_HOTKEY_BUILD] = SDLK_b;
    hotkey_mapping[INPUT_HOTKEY_BUILD2] = SDLK_v;
    hotkey_mapping[INPUT_HOTKEY_REPAIR] = SDLK_r;

    // Unload
    hotkey_mapping[INPUT_HOTKEY_UNLOAD] = SDLK_x;

    // Esc
    hotkey_mapping[INPUT_HOTKEY_CANCEL] = SDLK_ESCAPE;

    // Build 1
    hotkey_mapping[INPUT_HOTKEY_HALL] = SDLK_t;
    hotkey_mapping[INPUT_HOTKEY_HOUSE] = SDLK_e;
    hotkey_mapping[INPUT_HOTKEY_SALOON] = SDLK_s;
    hotkey_mapping[INPUT_HOTKEY_BUNKER] = SDLK_b;
    hotkey_mapping[INPUT_HOTKEY_WORKSHOP] = SDLK_w;

    // Build 2
    hotkey_mapping[INPUT_HOTKEY_SMITH] = SDLK_s;
    hotkey_mapping[INPUT_HOTKEY_COOP] = SDLK_c;
    hotkey_mapping[INPUT_HOTKEY_BARRACKS] = SDLK_b;
    hotkey_mapping[INPUT_HOTKEY_SHERIFFS] = SDLK_e;

    // Hall
    hotkey_mapping[INPUT_HOTKEY_MINER] = SDLK_e;

    // Saloon
    hotkey_mapping[INPUT_HOTKEY_COWBOY] = SDLK_c;
    hotkey_mapping[INPUT_HOTKEY_BANDIT] = SDLK_b;
    hotkey_mapping[INPUT_HOTKEY_DETECTIVE] = SDLK_d;

    // Workshop
    hotkey_mapping[INPUT_HOTKEY_SAPPER] = SDLK_s;
    hotkey_mapping[INPUT_HOTKEY_PYRO] = SDLK_r;
    hotkey_mapping[INPUT_HOTKEY_BALLOON] = SDLK_b;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_LANDMINES] = SDLK_e;

    // Coop
    hotkey_mapping[INPUT_HOTKEY_WAGON] = SDLK_w;
    hotkey_mapping[INPUT_HOTKEY_WAR_WAGON] = SDLK_w;
    hotkey_mapping[INPUT_HOTKEY_JOCKEY] = SDLK_c;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_WAGON_ARMOR] = SDLK_a;

    // Barracks
    hotkey_mapping[INPUT_HOTKEY_SOLDIER] = SDLK_s;
    hotkey_mapping[INPUT_HOTKEY_CANNON] = SDLK_c;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_BAYONETS] = SDLK_b;

    // Pyro
    hotkey_mapping[INPUT_HOTKEY_MOLOTOV] = SDLK_v;
    hotkey_mapping[INPUT_HOTKEY_LANDMINE] = SDLK_e;

    // Detective
    hotkey_mapping[INPUT_HOTKEY_CAMO] = SDLK_c;
}

SDL_Keycode input_get_key_just_pressed() {
    return state.key_just_pressed;
}

bool input_is_key_valid_hotkey_mapping(SDL_Keycode key) {
    return (
        key == SDLK_ESCAPE ||
        (key >= SDLK_a && key <= SDLK_z) ||
        key == SDLK_LEFTBRACKET ||
        key == SDLK_RIGHTBRACKET ||
        key == SDLK_BACKSLASH ||
        key == SDLK_BACKQUOTE ||
        key == SDLK_MINUS ||
        key == SDLK_EQUALS ||
        key == SDLK_COMMA ||
        key == SDLK_PERIOD ||
        key == SDLK_SLASH ||
        key == SDLK_SEMICOLON ||
        key == SDLK_QUOTE
    );
}
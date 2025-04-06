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

    InputAction hotkey_group[HOTKEY_GROUP_SIZE];
    SDL_Keycode hotkey_mapping[INPUT_ACTION_COUNT];
};
static InputState state;

void input_update_window_size();

void input_init(SDL_Window* window) {
    memset(&state, 0, sizeof(state));
    state.window = window;
    input_update_window_size();
    input_stop_text_input();
    input_use_hotkey_mapping_default();
    for (uint32_t index = 0; index < HOTKEY_GROUP_SIZE; index++) {
        state.hotkey_group[index] = INPUT_HOTKEY_NONE;
    }
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
            case SDL_WINDOWEVENT_RESIZED: {
                input_update_window_size();
                break;
            }
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

void input_set_hotkey_group(uint32_t group) {
    for (uint32_t index = 0; index < HOTKEY_GROUP_SIZE; index++) {
        state.hotkey_group[index] = INPUT_HOTKEY_NONE;
    }

    if ((group & INPUT_HOTKEY_GROUP_UNIT) == INPUT_HOTKEY_GROUP_UNIT) {
        state.hotkey_group[0] = INPUT_HOTKEY_ATTACK;
        state.hotkey_group[1] = INPUT_HOTKEY_STOP;
        state.hotkey_group[2] = INPUT_HOTKEY_DEFEND;
    }
    if ((group & INPUT_HOTKEY_GROUP_MINER) == INPUT_HOTKEY_GROUP_MINER) {
        state.hotkey_group[3] = INPUT_HOTKEY_REPAIR;
        state.hotkey_group[4] = INPUT_HOTKEY_BUILD;
        state.hotkey_group[5] = INPUT_HOTKEY_BUILD2;
    }
    if ((group & INPUT_HOTKEY_GROUP_CANCEL) == INPUT_HOTKEY_GROUP_CANCEL) {
        state.hotkey_group[5] = INPUT_HOTKEY_CANCEL;
    }
    if ((group & INPUT_HOTKEY_GROUP_BUILD) == INPUT_HOTKEY_GROUP_BUILD) {
        state.hotkey_group[0] = INPUT_HOTKEY_HALL;
        state.hotkey_group[1] = INPUT_HOTKEY_HOUSE;
        state.hotkey_group[2] = INPUT_HOTKEY_SALOON;
        state.hotkey_group[3] = INPUT_HOTKEY_SMITH;
        state.hotkey_group[4] = INPUT_HOTKEY_BUNKER;
        state.hotkey_group[5] = INPUT_HOTKEY_CANCEL;
    }
    if ((group & INPUT_HOTKEY_GROUP_HALL) == INPUT_HOTKEY_GROUP_HALL) {
        state.hotkey_group[0] = INPUT_HOTKEY_MINER;
    }
    if ((group & INPUT_HOTKEY_GROUP_SALOON) == INPUT_HOTKEY_GROUP_SALOON) {
        state.hotkey_group[0] = INPUT_HOTKEY_COWBOY;
        state.hotkey_group[1] = INPUT_HOTKEY_BANDIT;
    } 
}

InputAction input_get_hotkey(uint32_t index) {
    return state.hotkey_group[index];
}

int input_sprintf_hotkey_str(char* str_ptr, InputAction hotkey) {
    SDL_Keycode key = state.hotkey_mapping[hotkey];
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

void input_use_hotkey_mapping_default() {
    state.hotkey_mapping[INPUT_HOTKEY_ATTACK] = SDLK_a;
    state.hotkey_mapping[INPUT_HOTKEY_STOP] = SDLK_s;
    state.hotkey_mapping[INPUT_HOTKEY_DEFEND] = SDLK_d;
    state.hotkey_mapping[INPUT_HOTKEY_BUILD] = SDLK_b;
    state.hotkey_mapping[INPUT_HOTKEY_BUILD2] = SDLK_v;
    state.hotkey_mapping[INPUT_HOTKEY_REPAIR] = SDLK_r;
    state.hotkey_mapping[INPUT_HOTKEY_CANCEL] = SDLK_ESCAPE;
    state.hotkey_mapping[INPUT_HOTKEY_HALL] = SDLK_t;
    state.hotkey_mapping[INPUT_HOTKEY_HOUSE] = SDLK_e;
    state.hotkey_mapping[INPUT_HOTKEY_SALOON] = SDLK_s;
    state.hotkey_mapping[INPUT_HOTKEY_SMITH] = SDLK_b;
    state.hotkey_mapping[INPUT_HOTKEY_BUNKER] = SDLK_r;
    state.hotkey_mapping[INPUT_HOTKEY_MINER] = SDLK_e;
    state.hotkey_mapping[INPUT_HOTKEY_COWBOY] = SDLK_c;
    state.hotkey_mapping[INPUT_HOTKEY_BANDIT] = SDLK_b;
}
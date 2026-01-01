#include "input.h"

#include "defines.h"
#include "core/logger.h"
#include <cstring>
#include <algorithm>

struct InputState {
    SDL_Window* window;
    ivec2 scaled_screen_size;
    ivec2 scaled_screen_position;

    ivec2 mouse_position;
    bool current[INPUT_ACTION_COUNT];
    bool previous[INPUT_ACTION_COUNT];
    bool user_requests_exit;
    bool should_capture_mouse;

    std::string* text_input_str;
    size_t text_input_max_length;

    SDL_Scancode hotkey_mapping[INPUT_ACTION_COUNT];

    SDL_Scancode key_just_pressed;
};
static InputState state;

void input_update_screen_scale();

void input_init(SDL_Window* window) {
    memset(&state, 0, sizeof(state));
    state.window = window;
    input_update_screen_scale();
    input_stop_text_input();
    input_set_hotkey_mapping_to_default(state.hotkey_mapping);

    state.should_capture_mouse = true;

    #ifndef GOLD_DEBUG
        SDL_SetWindowMouseGrab(state.window, true);
    #endif
}

void input_update_screen_scale() {
    ivec2 window_size;
    SDL_GetWindowSize(state.window, &window_size.x, &window_size.y);

    int scale_width = window_size.x / SCREEN_WIDTH;
    int scale_height = window_size.y / SCREEN_HEIGHT;
    int scale = std::min(scale_width, scale_height);

    state.scaled_screen_size = ivec2(SCREEN_WIDTH, SCREEN_HEIGHT) * scale;
    state.scaled_screen_position = ivec2(
        (window_size.x / 2) - (state.scaled_screen_size.x / 2),
        (window_size.y / 2) - (state.scaled_screen_size.y / 2));
}

void input_set_mouse_capture_enabled(bool value) {
    state.should_capture_mouse = value;
}

void input_poll_events() {
    memcpy(&state.previous, &state.current, sizeof(state.current));
    state.key_just_pressed = INPUT_KEY_NONE;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle quit
        if (event.type == SDL_EVENT_QUIT) {
            state.user_requests_exit = true;
            break;
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED || 
                event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            input_update_screen_scale();
        }

        // Capture mouse
        if (state.should_capture_mouse && !SDL_GetWindowMouseGrab(state.window)) {
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_SetWindowMouseGrab(state.window, true);
                continue;
            }
            // If the mouse is not captured, don't handle any other input
            // Breaking here also prevents the initial click into the window from triggering any action
            break;
        }

        #ifdef GOLD_DEBUG
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_TAB) {
                SDL_SetWindowMouseGrab(state.window, false);
                break;
            }
        #endif

        switch (event.type) {
            case SDL_EVENT_WINDOW_FOCUS_LOST: 
            case SDL_EVENT_WINDOW_FOCUS_GAINED: {
                memset(state.current, 0, sizeof(state.current));
                break;
            }
            case SDL_EVENT_MOUSE_MOTION: {
                state.mouse_position = ivec2((int)event.motion.x - state.scaled_screen_position.x, (int)event.motion.y - state.scaled_screen_position.y);
                state.mouse_position = ivec2((state.mouse_position.x * SCREEN_WIDTH) / state.scaled_screen_size.x, (state.mouse_position.y * SCREEN_HEIGHT) / state.scaled_screen_size.y);
                state.mouse_position.x = std::clamp(state.mouse_position.x, 0, SCREEN_WIDTH);
                state.mouse_position.y = std::clamp(state.mouse_position.y, 0, SCREEN_HEIGHT);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN: 
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        state.current[INPUT_ACTION_LEFT_CLICK] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                        break;
                    case SDL_BUTTON_RIGHT:
                        state.current[INPUT_ACTION_RIGHT_CLICK] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                        break;
                    default:
                        break;
                } 
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    state.key_just_pressed = event.key.scancode;
                }
                switch (event.key.scancode) {
                    case SDL_SCANCODE_LSHIFT:
                    case SDL_SCANCODE_RSHIFT:
                        state.current[INPUT_ACTION_SHIFT] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_LCTRL:
                    case SDL_SCANCODE_RCTRL:
                    #ifdef PLATFORM_MACOS
                    case SDL_SCANCODE_LGUI:
                    case SDL_SCANCODE_RGUI:
                    #endif
                        state.current[INPUT_ACTION_CTRL] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_SPACE:
                        state.current[INPUT_ACTION_SPACE] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    #ifdef GOLD_DEBUG
                    case SDL_SCANCODE_F9:
                        state.current[INPUT_ACTION_TURBO] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    #endif
                    case SDL_SCANCODE_F10:
                        state.current[INPUT_ACTION_MATCH_MENU] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_RETURN: 
                        state.current[INPUT_ACTION_ENTER] = event.type == SDL_EVENT_KEY_DOWN;
                        break;
                    case SDL_SCANCODE_BACKSPACE: {
                        if (event.type == SDL_EVENT_KEY_DOWN && SDL_TextInputActive(state.window) && state.text_input_str->length() > 0) {
                            state.text_input_str->pop_back();
                        }
                        break;
                    }
                    default: {
                    // Map editor shortcuts
                    #ifdef GOLD_DEBUG
                        const bool ctrl_is_pressed = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_LCTRL];
                        if (event.key.scancode == SDL_SCANCODE_S && ctrl_is_pressed) {
                            state.current[INPUT_ACTION_EDITOR_SAVE] = event.type == SDL_EVENT_KEY_DOWN;
                            break;
                        }
                        if (event.key.scancode == SDL_SCANCODE_Z && ctrl_is_pressed) {
                            state.current[INPUT_ACTION_EDITOR_UNDO] = event.type == SDL_EVENT_KEY_DOWN;
                            break;
                        }
                        if (event.key.scancode == SDL_SCANCODE_R && ctrl_is_pressed) {
                            state.current[INPUT_ACTION_EDITOR_REDO] = event.type == SDL_EVENT_KEY_DOWN;
                            break;
                        }
                        if (event.key.scancode == SDL_SCANCODE_B) {
                            state.current[INPUT_ACTION_EDITOR_TOOL_BRUSH] = event.type == SDL_EVENT_KEY_DOWN;
                            break;
                        }
                    #endif

                        if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_0) {
                            int key_index = event.key.scancode - SDL_SCANCODE_1;
                            state.current[INPUT_ACTION_NUM1 + key_index] = event.type == SDL_EVENT_KEY_DOWN;
                        } else if (event.key.scancode >= SDL_SCANCODE_F1 && event.key.scancode <= SDL_SCANCODE_F6) {
                            int key_index = event.key.scancode - SDL_SCANCODE_F1;
                            state.current[INPUT_ACTION_F1 + key_index] = event.type == SDL_EVENT_KEY_DOWN;
                        } else {
                            for (uint32_t hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
                                if (event.key.scancode == state.hotkey_mapping[hotkey]) {
                                    state.current[hotkey] = event.type == SDL_EVENT_KEY_DOWN;
                                }
                            }
                        }
                        break;
                    }
                }
                break;
            }
            case SDL_EVENT_TEXT_INPUT: {
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
    SDL_StartTextInput(state.window);
    state.text_input_str = str;
    state.text_input_max_length = max_length;
}

void input_stop_text_input() {
    SDL_StopTextInput(state.window);
    state.text_input_str = NULL;
}

bool input_is_text_input_active() {
    return SDL_TextInputActive(state.window);
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

int input_sprintf_sdl_scancode_str(char* str_ptr, SDL_Scancode scancode) {
    if (scancode == SDL_SCANCODE_ESCAPE) {
        return sprintf(str_ptr, "ESC");
    }
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        uint8_t letter_index = (uint8_t)scancode - (uint8_t)SDL_SCANCODE_A;
        return sprintf(str_ptr, "%c", (char)((uint8_t)'A' + letter_index));
    }
    if (scancode >= SDL_SCANCODE_MINUS && scancode <= SDL_SCANCODE_SLASH) {
        static const char* scancode_chars = "-=[]\\\\;'`,./";
        return sprintf(str_ptr, "%c", scancode_chars[scancode - SDL_SCANCODE_MINUS]);
    }
    return 0;
}

SDL_Scancode input_get_hotkey_mapping(InputAction hotkey) {
    return state.hotkey_mapping[hotkey];
}

void input_set_hotkey_mapping(InputAction hotkey, SDL_Scancode key) {
    state.hotkey_mapping[hotkey] = key;
}

void input_set_hotkey_mapping_to_default(SDL_Scancode* hotkey_mapping) {
    // Unit
    hotkey_mapping[INPUT_HOTKEY_ATTACK] = SDL_SCANCODE_A;
    hotkey_mapping[INPUT_HOTKEY_STOP] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_DEFEND] = SDL_SCANCODE_D;

    // Miner
    hotkey_mapping[INPUT_HOTKEY_BUILD] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_BUILD2] = SDL_SCANCODE_V;
    hotkey_mapping[INPUT_HOTKEY_REPAIR] = SDL_SCANCODE_R;

    // Unload
    hotkey_mapping[INPUT_HOTKEY_UNLOAD] = SDL_SCANCODE_X;

    // Esc
    hotkey_mapping[INPUT_HOTKEY_CANCEL] = SDL_SCANCODE_ESCAPE;

    // Build 1
    hotkey_mapping[INPUT_HOTKEY_HALL] = SDL_SCANCODE_T;
    hotkey_mapping[INPUT_HOTKEY_HOUSE] = SDL_SCANCODE_E;
    hotkey_mapping[INPUT_HOTKEY_SALOON] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_BUNKER] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_WORKSHOP] = SDL_SCANCODE_W;

    // Build 2
    hotkey_mapping[INPUT_HOTKEY_SMITH] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_COOP] = SDL_SCANCODE_C;
    hotkey_mapping[INPUT_HOTKEY_BARRACKS] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_SHERIFFS] = SDL_SCANCODE_E;

    // Hall
    hotkey_mapping[INPUT_HOTKEY_MINER] = SDL_SCANCODE_E;

    // Saloon
    hotkey_mapping[INPUT_HOTKEY_COWBOY] = SDL_SCANCODE_C;
    hotkey_mapping[INPUT_HOTKEY_BANDIT] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_DETECTIVE] = SDL_SCANCODE_D;

    // Workshop
    hotkey_mapping[INPUT_HOTKEY_SAPPER] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_PYRO] = SDL_SCANCODE_R;
    hotkey_mapping[INPUT_HOTKEY_BALLOON] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_LANDMINES] = SDL_SCANCODE_E;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_TAILWIND] = SDL_SCANCODE_W;

    // Coop
    hotkey_mapping[INPUT_HOTKEY_WAGON] = SDL_SCANCODE_W;
    hotkey_mapping[INPUT_HOTKEY_JOCKEY] = SDL_SCANCODE_C;

    // Smith
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_WAGON_ARMOR] = SDL_SCANCODE_W;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_BAYONETS] = SDL_SCANCODE_B;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_SERRATED_KNIVES] = SDL_SCANCODE_S;

    // Barracks
    hotkey_mapping[INPUT_HOTKEY_SOLDIER] = SDL_SCANCODE_S;
    hotkey_mapping[INPUT_HOTKEY_CANNON] = SDL_SCANCODE_C;

    // Sheriff's Office
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_PRIVATE_EYE] = SDL_SCANCODE_E;
    hotkey_mapping[INPUT_HOTKEY_RESEARCH_STAKEOUT] = SDL_SCANCODE_S;

    // Pyro
    hotkey_mapping[INPUT_HOTKEY_MOLOTOV] = SDL_SCANCODE_V;
    hotkey_mapping[INPUT_HOTKEY_LANDMINE] = SDL_SCANCODE_E;

    // Detective
    hotkey_mapping[INPUT_HOTKEY_CAMO] = SDL_SCANCODE_C;
}

SDL_Scancode input_get_key_just_pressed() {
    return state.key_just_pressed;
}

bool input_is_key_valid_hotkey_mapping(SDL_Scancode key) {
    return key == SDL_SCANCODE_ESCAPE ||
                (key >= SDL_SCANCODE_A && key <= SDL_SCANCODE_Z) ||
                (key >= SDL_SCANCODE_MINUS && key <= SDL_SCANCODE_SLASH);
}
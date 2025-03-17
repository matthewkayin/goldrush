#include "input.h"

#include <cstring>

struct input_state_t {
    ivec2 mouse_position;
    bool current[INPUT_COUNT];
    bool previous[INPUT_COUNT];
    bool user_requests_exit;
};
static input_state_t state;

void input_init() {
    memset(&state, 0, sizeof(state));
}

void input_poll_events(SDL_Window* window) {
    memcpy(&state.current, &state.previous, sizeof(state.current));

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle quit
        if (event.type == SDL_QUIT) {
            state.user_requests_exit = true;
            break;
        }

        // Capture mouse
        if (SDL_GetWindowGrab(window) == SDL_FALSE) {
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_SetWindowGrab(window, SDL_TRUE);
                continue;
            }
            // If the mouse is not captured, don't handle any other input
            // Breaking here also prevents the initial click into the window from triggering any action
            break;
        }

        #ifdef GOLD_DEBUG
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
                SDL_SetWindowGrab(window, SDL_FALSE);
                break;
            }
        #endif

        switch (event.type) {
            case SDL_MOUSEMOTION: {
                state.mouse_position = ivec2(event.motion.x, event.motion.y);
                break;
            }
            case SDL_MOUSEBUTTONDOWN: 
            case SDL_MOUSEBUTTONUP: {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT:
                        state.current[INPUT_LEFT_CLICK] = event.type == SDL_MOUSEBUTTONDOWN;
                        break;
                    case SDL_BUTTON_RIGHT:
                        state.current[INPUT_RIGHT_CLICK] = event.type == SDL_MOUSEBUTTONDOWN;
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
                        state.current[INPUT_SHIFT] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_LCTRL:
                    case SDLK_RCTRL:
                        state.current[INPUT_CTRL] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_SPACE:
                        state.current[INPUT_SPACE] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_ESCAPE:
                        state.current[INPUT_ESC] = event.type == SDL_KEYDOWN;
                        break;
                    case SDLK_F3:
                        state.current[INPUT_F3] = event.type == SDL_KEYDOWN;
                        break;
                    default:
                        break;
                }
                break;
            }
            default: 
                break;
        }
    }
}

ivec2 input_get_mouse_position() {
    return state.mouse_position;
}

bool input_user_requests_exit() {
    return state.user_requests_exit;
}

bool input_is_action_pressed(input_action action) {
    return state.current[action];
}

bool input_is_action_just_pressed(input_action action) {
    return state.current[action] && !state.previous[action];
}

bool input_is_action_just_released(input_action action) {
    return state.previous[action] && !state.current[action];
}
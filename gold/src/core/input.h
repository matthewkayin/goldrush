#pragma once

#include "math/gmath.h"
#include <SDL2/SDL.h>
#include <string>

enum input_action {
    INPUT_LEFT_CLICK,
    INPUT_RIGHT_CLICK,
    INPUT_SHIFT,
    INPUT_CTRL,
    INPUT_SPACE,
    INPUT_ESC,
    INPUT_F3,
    INPUT_ENTER,
    INPUT_COUNT
};

void input_init(SDL_Window* window);
void input_poll_events();

void input_start_text_input(std::string* str, size_t max_length);
void input_stop_text_input();
bool input_is_text_input_active();

ivec2 input_get_mouse_position();
bool input_user_requests_exit();
bool input_is_action_pressed(input_action action);
bool input_is_action_just_pressed(input_action action);
bool input_is_action_just_released(input_action action);
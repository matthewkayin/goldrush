#pragma once

#include "util.h"
#include "sprite.h"

const uint8_t MOUSE_BUTTON_LEFT = 1;
const uint8_t MOUSE_BUTTON_MIDDLE = 2;
const uint8_t MOUSE_BUTTON_RIGHT = 3;

enum Cursor {
    CURSOR_DEFAULT,
    CURSOR_TARGET,
    CURSOR_COUNT
};

bool input_is_mouse_button_pressed(uint8_t button);
bool input_is_mouse_button_just_pressed(uint8_t button);
bool input_is_mouse_button_just_released(uint8_t button);

xy input_get_mouse_position();

bool input_is_ui_hotkey_just_pressed(UiButton button);

void input_start_text_input(const rect_t& text_input_rect, size_t input_length_limit);
void input_stop_text_input();
bool input_is_text_input_active();
void input_set_text_input_value(const char* value);
const char* input_get_text_input_value();
size_t input_get_text_input_length();
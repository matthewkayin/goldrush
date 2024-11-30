#pragma once

#include "defines.h"
#include <SDL2/SDL.h>
#include <queue>

enum UiMode {
    UI_MODE_MATCH_NOT_STARTED,
    UI_MODE_NONE
};

enum InputType: uint8_t {
    INPUT_NONE,
};

struct input_t {
    uint8_t type;
};

struct match_state_t {
    UiMode ui_mode;

    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;
};

match_state_t match_init();
void match_handle_input(match_state_t& state, SDL_Event e);
void match_update(match_state_t& state);
void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);
void match_render(const match_state_t& state);
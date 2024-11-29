#pragma once

#include <SDL2/SDL.h>

struct match_state_t {

};

match_state_t match_init();
void match_handle_input(match_state_t& state, SDL_Event e);
void match_update(match_state_t& state);
void match_render(const match_state_t& state);
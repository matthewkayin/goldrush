#pragma once

#include "defines.h"
#include "engine.h"

struct menu_state_t {

};

menu_state_t menu_init();
void menu_handle_input(menu_state_t& state, SDL_Event event);
void menu_update(menu_state_t& state);
void menu_render(const menu_state_t& state);
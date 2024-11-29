#include "match.h"

#include "engine.h"

match_state_t match_init() {
    match_state_t state;

    return state;
}

void match_handle_input(match_state_t& state, SDL_Event e) {

}

void match_update(match_state_t& state) {

}

void match_render(const match_state_t& state) {
    SDL_SetRenderDrawColor(engine.renderer, 0, 255, 255, 255);
    SDL_RenderClear(engine.renderer);
}
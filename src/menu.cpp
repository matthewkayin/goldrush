#include "menu.h"

menu_state_t menu_init() {
    menu_state_t state;

    return state;
}

void menu_handle_input(menu_state_t& state, SDL_Event event) {

}

void menu_update(menu_state_t& state) {

}

void menu_render(const menu_state_t& state) {
    SDL_Rect background_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_SetRenderDrawColor(engine.renderer, COLOR_SKY_BLUE.r, COLOR_SKY_BLUE.g, COLOR_SKY_BLUE.b, COLOR_SKY_BLUE.a);
    SDL_RenderFillRect(engine.renderer, &background_rect);

    render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_OFFBLACK, xy(24, 24));
}
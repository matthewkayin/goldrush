#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include "util.h"

#define RENDER_TEXT_CENTERED -1

// COLORS

const SDL_Color COLOR_BLACK = (SDL_Color) { .r = 0, .g = 0, .b = 0, .a = 255 };
const SDL_Color COLOR_OFFBLACK = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 };
const SDL_Color COLOR_DARKBLACK = (SDL_Color) { .r = 16, .g = 15, .b = 18, .a = 255 };
const SDL_Color COLOR_WHITE = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 };
const SDL_Color COLOR_SKY_BLUE = (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 };
const SDL_Color COLOR_DIM_BLUE = (SDL_Color) { .r = 70, .g = 100, .b = 115, .a = 255 };
const SDL_Color COLOR_SAND_DARK = (SDL_Color) { .r = 204, .g = 162, .b = 139, .a = 255 };
const SDL_Color COLOR_RED = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 };
const SDL_Color COLOR_GREEN = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 };
const SDL_Color COLOR_GOLD = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 };
const SDL_Color COLOR_DARK_GRAY = (SDL_Color) { .r = 94, .g = 88, .b = 89, .a = 255 };

// FONT

enum Font {
    FONT_HACK,
    FONT_WESTERN8,
    FONT_WESTERN16,
    FONT_WESTERN32,
    FONT_M3X6,
    FONT_COUNT
};

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;

    std::vector<TTF_Font*> fonts;

    xy mouse_position;
};
extern engine_t engine;

bool engine_init();
bool engine_init_renderer();
void engine_destroy_renderer();
void engine_quit();

enum TextAnchor {
    TEXT_ANCHOR_TOP_LEFT,
    TEXT_ANCHOR_BOTTOM_LEFT,
    TEXT_ANCHOR_TOP_RIGHT,
    TEXT_ANCHOR_BOTTOM_RIGHT,
    TEXT_ANCHOR_TOP_CENTER
};

void render_text(Font font, const char* text, SDL_Color color, xy position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);
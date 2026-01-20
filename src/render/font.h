#pragma once

#include "defines.h"
#include <SDL3/SDL.h>

enum FontName {
    FONT_HACK_OFFBLACK,
    FONT_HACK_WHITE,
    FONT_HACK_GOLD,
    FONT_HACK_GOLD_SATURATED,
    FONT_HACK_PLAYER0,
    FONT_HACK_PLAYER1,
    FONT_HACK_PLAYER2,
    FONT_HACK_PLAYER3,
    FONT_HACK_SHADOW,
    FONT_WESTERN8_OFFBLACK,
    FONT_WESTERN8_WHITE,
    FONT_WESTERN8_GOLD,
    FONT_WESTERN8_RED,
    FONT_M3X6_OFFBLACK,
    FONT_M3X6_DARKBLACK,
    FONT_M3X6_WHITE,
    FONT_COUNT
};

const uint32_t FONT_OPTION_IGNORE_BEARING = 1;

struct FontParams {
    uint32_t options;
    const char* path;
    int size;
    SDL_Color color;
};

const FontParams& resource_get_font_params(FontName name);

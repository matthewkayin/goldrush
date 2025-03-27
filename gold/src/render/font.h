#pragma once

#include "defines.h"

enum FontName {
    FONT_HACK_OFFBLACK,
    FONT_HACK_WHITE,
    FONT_HACK_GOLD,
    FONT_HACK_PLAYER0,
    FONT_HACK_PLAYER1,
    FONT_HACK_PLAYER2,
    FONT_HACK_PLAYER3,
    FONT_WESTERN8_OFFBLACK,
    FONT_WESTERN8_WHITE,
    FONT_WESTERN8_GOLD,
    FONT_WESTERN8_RED,
    FONT_COUNT
};

struct FontParams {
    const char* path;
    int size;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool ignore_bearing;
};

const FontParams& resource_get_font_params(FontName name);

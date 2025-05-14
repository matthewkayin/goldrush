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
    FONT_M3X6_OFFBLACK,
    FONT_M3X6_DARKBLACK,
    FONT_M3X6_WHITE,
    FONT_COUNT
};

enum FontImportStrategy {
    FONT_IMPORT_DEFAULT,
    FONT_IMPORT_IGNORE_BEARING,
    FONT_IMPORT_NUMERIC_ONLY
};

struct FontParams {
    FontImportStrategy strategy;
    const char* path;
    int size;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

const FontParams& resource_get_font_params(FontName name);

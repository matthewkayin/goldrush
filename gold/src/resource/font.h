#pragma once

#include <cstdint>

enum font_name {
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
    FONT_WESTERN32_OFFBLACK,
    FONT_COUNT
};

struct font_params_t {
    const char* path;
    int size;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    bool ignore_bearing;
};

const font_params_t& resource_get_font_params(font_name name);

#pragma once

#include <cstdint>

enum font_name {
    FONT_HACK_BLACK,
    FONT_WESTERN8_BLACK,
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

#include "font.h"

#include <unordered_map>

static const std::unordered_map<font_name, font_params_t> FONT_PARAMS = {
    { FONT_HACK_OFFBLACK, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 40, .g = 37, .b = 45,
        .ignore_bearing = true
    }},
    { FONT_HACK_WHITE, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 255, .g = 255, .b = 255,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER0, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 92, .g = 132, .b = 153,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER1, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 186, .g = 97, .b = 95,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER2, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 77, .g = 135, .b = 115,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER3, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 144, .g = 119, .b = 153,
        .ignore_bearing = true
    }},
    { FONT_WESTERN8_OFFBLACK, (font_params_t) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 40, .g = 37, .b = 45,
        .ignore_bearing = false
    }},
    { FONT_WESTERN8_WHITE, (font_params_t) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 255, .g = 255, .b = 255,
        .ignore_bearing = false
    }},
    { FONT_WESTERN8_GOLD, (font_params_t) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 238, .g = 209, .b = 158,
        .ignore_bearing = false
    }},
    { FONT_WESTERN8_RED, (font_params_t) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 186, .g = 97, .b = 95,
        .ignore_bearing = false
    }},
    { FONT_WESTERN32_OFFBLACK, (font_params_t) { 
        .path = "western.ttf", 
        .size = 32,
        .r = 40, .g = 37, .b = 45,
        .ignore_bearing = false
    }}
};

const font_params_t& resource_get_font_params(font_name name) {
    return FONT_PARAMS.at(name);
}
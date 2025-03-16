#include "font.h"

#include <unordered_map>

static const std::unordered_map<font_name, font_params_t> FONT_PARAMS = {
    { FONT_HACK_BLACK, (font_params_t) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 0, .g = 0, .b = 0,
        .ignore_bearing = true
    }},
    { FONT_WESTERN8_BLACK, (font_params_t) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 0, .g = 0, .b = 0,
        .ignore_bearing = false
    }}
};

const font_params_t& resource_get_font_params(font_name name) {
    return FONT_PARAMS.at(name);
}
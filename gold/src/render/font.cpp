#include "font.h"

#include <unordered_map>

static const std::unordered_map<FontName, FontParams> FONT_PARAMS = {
    { FONT_HACK_OFFBLACK, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 40, .g = 37, .b = 45,
        .ignore_bearing = true
    }},
    { FONT_HACK_WHITE, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 255, .g = 255, .b = 255,
        .ignore_bearing = true
    }},
    { FONT_HACK_GOLD, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 238, .g = 209, .b = 158,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER0, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 92, .g = 132, .b = 153,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER1, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 186, .g = 97, .b = 95,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER2, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 77, .g = 135, .b = 115,
        .ignore_bearing = true
    }},
    { FONT_HACK_PLAYER3, (FontParams) { 
        .path = "hack.ttf", 
        .size = 10,
        .r = 144, .g = 119, .b = 153,
        .ignore_bearing = true
    }},
    { FONT_WESTERN8_OFFBLACK, (FontParams) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 40, .g = 37, .b = 45,
        .ignore_bearing = false
    }},
    { FONT_WESTERN8_WHITE, (FontParams) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 255, .g = 255, .b = 255,
        .ignore_bearing = false
    }},
    { FONT_WESTERN8_GOLD, (FontParams) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 238, .g = 209, .b = 158,
        .ignore_bearing = false
    }},
    { FONT_WESTERN8_RED, (FontParams) { 
        .path = "western.ttf", 
        .size = 8,
        .r = 186, .g = 97, .b = 95,
        .ignore_bearing = false
    }},
    { FONT_WESTERN32_OFFBLACK, (FontParams) { 
        .path = "western.ttf", 
        .size = 32,
        .r = 40, .g = 37, .b = 45,
        .ignore_bearing = false
    }}
};

const FontParams& resource_get_font_params(FontName name) {
    return FONT_PARAMS.at(name);
}
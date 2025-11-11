#include "font.h"

#include <unordered_map>

static const std::unordered_map<FontName, FontParams> FONT_PARAMS = {
    { FONT_HACK_OFFBLACK, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 40, .g = 37, .b = 45,
    }},
    { FONT_HACK_WHITE, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 255, .g = 255, .b = 255,
    }},
    { FONT_HACK_GOLD, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 238, .g = 209, .b = 158,
    }},
    { FONT_HACK_PLAYER0, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 92, .g = 132, .b = 153,
    }},
    { FONT_HACK_PLAYER1, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 186, .g = 97, .b = 95,
    }},
    { FONT_HACK_PLAYER2, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 77, .g = 135, .b = 115,
    }},
    { FONT_HACK_PLAYER3, { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .r = 144, .g = 119, .b = 153,
    }},
    { FONT_WESTERN8_OFFBLACK, { 
        .strategy = FONT_IMPORT_DEFAULT,
        .path = "western.ttf", 
        .size = 8,
        .r = 40, .g = 37, .b = 45,
    }},
    { FONT_WESTERN8_WHITE, { 
        .strategy = FONT_IMPORT_DEFAULT,
        .path = "western.ttf", 
        .size = 8,
        .r = 255, .g = 255, .b = 255,
    }},
    { FONT_WESTERN8_GOLD, { 
        .strategy = FONT_IMPORT_DEFAULT,
        .path = "western.ttf", 
        .size = 8,
        .r = 238, .g = 209, .b = 158,
    }},
    { FONT_WESTERN8_RED, { 
        .strategy = FONT_IMPORT_DEFAULT,
        .path = "western.ttf", 
        .size = 8,
        .r = 186, .g = 97, .b = 95,
    }},
    { FONT_M3X6_OFFBLACK, { 
        .strategy = FONT_IMPORT_NUMERIC_ONLY,
        .path = "m3x6.ttf", 
        .size = 16,
        .r = 40, .g = 37, .b = 45,
    }},
    { FONT_M3X6_DARKBLACK, { 
        .strategy = FONT_IMPORT_NUMERIC_ONLY,
        .path = "m3x6.ttf", 
        .size = 16,
        .r = 16, .g = 15, .b = 18,
    }},
    { FONT_M3X6_WHITE, { 
        .strategy = FONT_IMPORT_NUMERIC_ONLY,
        .path = "m3x6.ttf", 
        .size = 16,
        .r = 255, .g = 255, .b = 255,
    }}
};

const FontParams& resource_get_font_params(FontName name) {
    return FONT_PARAMS.at(name);
}
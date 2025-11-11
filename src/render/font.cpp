#include "font.h"

#include <unordered_map>

static const std::unordered_map<FontName, FontParams> FONT_PARAMS = {
    { FONT_HACK, (FontParams) { 
        .strategy = FONT_IMPORT_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
    }},
    { FONT_WESTERN8, (FontParams) { 
        .strategy = FONT_IMPORT_DEFAULT,
        .path = "western.ttf", 
        .size = 8,
    }},
    { FONT_M3X6, (FontParams) { 
        .strategy = FONT_IMPORT_NUMERIC_ONLY,
        .path = "m3x6.ttf", 
        .size = 16,
    }}
};

const FontParams& resource_get_font_params(FontName name) {
    return FONT_PARAMS.at(name);
}
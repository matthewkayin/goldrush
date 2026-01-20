#include "font.h"

#include "render/ui_color.h"
#include <unordered_map>

static const std::unordered_map<FontName, FontParams> FONT_PARAMS = {
    { FONT_HACK_OFFBLACK, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 } 
    }},
    { FONT_HACK_SHADOW, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 200 }
    }},
    { FONT_HACK_WHITE, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }
    }},
    { FONT_HACK_GOLD, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 }
    }},
    { FONT_HACK_GOLD_SATURATED, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = (SDL_Color) { .r = 237, .g = 200, .b = 135, .a = 255 }
    }},
    { FONT_HACK_PLAYER0, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = PLAYER_UI_COLOR[0]
    }},
    { FONT_HACK_PLAYER1, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = PLAYER_UI_COLOR[1]
    }},
    { FONT_HACK_PLAYER2, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = PLAYER_UI_COLOR[2]
    }},
    { FONT_HACK_PLAYER3, (FontParams) { 
        .options = FONT_OPTION_IGNORE_BEARING,
        .path = "hack.ttf", 
        .size = 10,
        .color = PLAYER_UI_COLOR[3]
    }},
    { FONT_WESTERN8_OFFBLACK, (FontParams) { 
        .options = 0,
        .path = "western.ttf", 
        .size = 8,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }
    }},
    { FONT_WESTERN8_WHITE, (FontParams) { 
        .options = 0,
        .path = "western.ttf", 
        .size = 8,
        .color = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }
    }},
    { FONT_WESTERN8_GOLD, (FontParams) { 
        .options = 0,
        .path = "western.ttf", 
        .size = 8,
        .color = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 }
    }},
    { FONT_WESTERN8_RED, (FontParams) { 
        .options = 0,
        .path = "western.ttf", 
        .size = 8,
        .color = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 }
    }},
    { FONT_M3X6_OFFBLACK, (FontParams) { 
        .options = 0,
        .path = "m3x6.ttf", 
        .size = 16,
        .color = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 }
    }},
    { FONT_M3X6_DARKBLACK, (FontParams) { 
        .options = 0,
        .path = "m3x6.ttf", 
        .size = 16,
        .color = (SDL_Color) { .r = 16, .g = 15, .b = 18, .a = 255 }
    }},
    { FONT_M3X6_WHITE, (FontParams) { 
        .options = 0,
        .path = "m3x6.ttf", 
        .size = 16,
        .color = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 }
    }}
};

const FontParams& resource_get_font_params(FontName name) {
    return FONT_PARAMS.at(name);
}
#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<atlas_name, atlas_params_t> ATLAS_PARAMS = {
    { ATLAS_UI, (atlas_params_t) {
        .path = "ui.png",
        .strategy = ATLAS_IMPORT_DEFAULT
    }}
};

static const std::unordered_map<sprite_name, sprite_info_t> SPRITE_INFO = {
    { SPRITE_UI_MINIMAP, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 0, .atlas_y = 0,
        .frame_width = 136, .frame_height = 136
    }},
    { SPRITE_UI_BUTTON_PANEL, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 136, .atlas_y = 0,
        .frame_width = 132, .frame_height = 106
    }},
    { SPRITE_UI_BOTTOM_PANEL, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 0, .atlas_y = 136,
        .frame_width = 372, .frame_height = 88
    }},
    { SPRITE_UI_FRAME, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 268, .atlas_y = 0,
        .frame_width = 16, .frame_height = 16
    }},
    { SPRITE_UI_FRAME_SMALL, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 316, .atlas_y = 0,
        .frame_width = 24, .frame_height = 24
    }},
    { SPRITE_UI_BUTTON_REFRESH, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 340, .atlas_y = 0,
        .frame_width = 24, .frame_height = 24
    }},
    { SPRITE_UI_BUTTON_ARROW, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 316, .atlas_y = 24,
        .frame_width = 24, .frame_height = 24
    }},
    { SPRITE_UI_BUTTON_BURGER, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 269, .atlas_y = 49,
        .frame_width = 19, .frame_height = 18
    }},
    { SPRITE_UI_GOLD_ICON, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 269, .atlas_y = 68,
        .frame_width = 16, .frame_height = 16
    }},
    { SPRITE_UI_HOUSE_ICON, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 286, .atlas_y = 68,
        .frame_width = 19, .frame_height = 16
    }},
    { SPRITE_UI_MINER_ICON, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 307, .atlas_y = 68,
        .frame_width = 12, .frame_height = 10
    }},
    { SPRITE_UI_DROPDOWN, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 0, .atlas_y = 224,
        .frame_width = 112, .frame_height = 21
    }},
    { SPRITE_UI_DROPDOWN_MINI, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 113, .atlas_y = 224,
        .frame_width = 80, .frame_height = 16
    }},
    { SPRITE_UI_TEAM_PICKER, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 192, .atlas_y = 224,
        .frame_width = 16, .frame_height = 16
    }},
    { SPRITE_UI_PANEL_BUTTON, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 219, .atlas_y = 241,
        .frame_width = 32, .frame_height = 32
    }},
    { SPRITE_UI_MENU_BUTTON, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 194, .atlas_y = 241,
        .frame_width = 24, .frame_height = 21
    }},
    { SPRITE_UI_TEXT_FRAME, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 227, .atlas_y = 224,
        .frame_width = 45, .frame_height = 15
    }},
    { SPRITE_UI_CLOUDS, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 0, .atlas_y = 330,
        .frame_width = 70, .frame_height = 16
    }}
};

atlas_params_t resource_get_atlas_params(atlas_name atlas) {
    return ATLAS_PARAMS.at(atlas);
}

sprite_info_t resource_get_sprite_info(sprite_name name) {
    return SPRITE_INFO.at(name);
}
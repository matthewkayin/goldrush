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
    }}
};

atlas_params_t resource_get_atlas_params(atlas_name atlas) {
    return ATLAS_PARAMS.at(atlas);
}

sprite_info_t resource_get_sprite_info(sprite_name name) {
    return SPRITE_INFO.at(name);
}
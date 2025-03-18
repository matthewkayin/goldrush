#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<atlas_name, atlas_params_t> ATLAS_PARAMS = {
    { ATLAS_UI, (atlas_params_t) {
        .path = "ui.png",
        .strategy = ATLAS_IMPORT_DEFAULT
    }},
    { ATLAS_TILESET, (atlas_params_t) {
        .path = "tileset.png",
        .strategy = ATLAS_IMPORT_TILESET,
        .tileset = (atlas_params_tileset_t) {
            .begin = SPRITE_TILE_NULL,
            .end = SPRITE_TILE_DECORATION4
        }
    }},
    { ATLAS_FONT, (atlas_params_t) {
        .path = "",
        .strategy = ATLAS_IMPORT_FONTS,
    }},
    { ATLAS_UNIT_WAGON, (atlas_params_t) {
        .path = "unit_wagon.png",
        .strategy = ATLAS_IMPORT_RECOLOR,
    }}
};

static std::unordered_map<sprite_name, sprite_info_t> SPRITE_INFO = {
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
        .frame_width = 8, .frame_height = 8
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
        .atlas_x = 227, .atlas_y = 224,
        .frame_width = 8, .frame_height = 21
    }},
    { SPRITE_UI_TEXT_FRAME, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 252, .atlas_y = 224,
        .frame_width = 15, .frame_height = 15
    }},
    { SPRITE_UI_CLOUDS, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 0, .atlas_y = 330,
        .frame_width = 70, .frame_height = 16
    }},
    { SPRITE_UI_SKY, (sprite_info_t) { 
        .atlas = ATLAS_UI, 
        .atlas_x = 0, .atlas_y = 352,
        .frame_width = 16, .frame_height = 16
    }},
    { SPRITE_UNIT_WAGON, (sprite_info_t) { 
        .atlas = ATLAS_UNIT_WAGON, 
        .atlas_x = 0, .atlas_y = 0,
        .frame_width = 40, .frame_height = 34
    }}
};

static const std::unordered_map<sprite_name, tile_data_t> TILE_DATA = {
    { SPRITE_TILE_NULL, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 16
    }},
    { SPRITE_TILE_SAND, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 0,
        .source_y = 0
    }},
    { SPRITE_TILE_SAND2, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 16,
        .source_y = 0
    }},
    { SPRITE_TILE_SAND3, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 0
    }},
    { SPRITE_TILE_WATER, (tile_data_t) {
        .type = TILE_TYPE_AUTO,
        .source_x = 0,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_NW_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_NE_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_SW_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_SE_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_NORTH_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_WEST_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_EAST_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_SOUTH_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_SW_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_SOUTH_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_SE_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_NW_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_NE_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_SW_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_SE_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 128,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 128,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 176,
        .source_y = 48
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_TOP, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_BOTTOM, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 32
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_TOP, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 0
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 16
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_BOTTOM, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 32
    }},
    { SPRITE_TILE_DECORATION0, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 0,
        .source_y = 64
    }},
    { SPRITE_TILE_DECORATION1, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 16,
        .source_y = 64
    }},
    { SPRITE_TILE_DECORATION2, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 64
    }},
    { SPRITE_TILE_DECORATION3, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 64
    }},
    { SPRITE_TILE_DECORATION4, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 64
    }}
};

const atlas_params_t& resource_get_atlas_params(atlas_name atlas) {
    return ATLAS_PARAMS.at(atlas);
}

const sprite_info_t& resource_get_sprite_info(sprite_name name) {
    return SPRITE_INFO.at(name);
}

void resource_set_sprite_info(sprite_name name, sprite_info_t sprite_info) {
    SPRITE_INFO[name] = sprite_info;
}

const tile_data_t& resource_get_tile_data(sprite_name tile) {
    return TILE_DATA.at(tile);
}
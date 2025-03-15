#pragma once

#include <cstdint>

enum atlas_name {
    ATLAS_UI,
    ATLAS_COUNT
};

enum atlas_import_strategy {
    ATLAS_IMPORT_DEFAULT,
    ATLAS_IMPORT_RECOLOR,
    ATLAS_IMPORT_RECOLOR_AND_LOW_ALPHA
};

struct atlas_params_t {
    const char* path;
    atlas_import_strategy strategy;
};

enum sprite_name {
    SPRITE_UI_MINIMAP,
    SPRITE_UI_BUTTON_PANEL,
    SPRITE_UI_BOTTOM_PANEL,
    SPRITE_UI_FRAME,
    SPRITE_COUNT
};

struct sprite_info_t {
    atlas_name atlas;
    int atlas_x;
    int atlas_y;
    int frame_width;
    int frame_height;
};

atlas_params_t resource_get_atlas_params(atlas_name atlas);
sprite_info_t resource_get_sprite_info(sprite_name name);

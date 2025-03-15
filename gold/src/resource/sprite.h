#pragma once

#include "math/gmath.h"
#include <cstdint>

enum sprite_name {
    SPRITE_DECORATION,
    SPRITE_UNIT_MINER,
    SPRITE_COUNT
};

enum sprite_import_strategy {
    SPRITE_IMPORT_DEFAULT,
    SPRITE_IMPORT_RECOLOR,
    SPRITE_IMPORT_RECOLOR_AND_LOW_ALPHA
};

struct sprite_params_t {
    const char* path;
    sprite_import_strategy strategy;
    int hframes;
    int vframes;
};

sprite_params_t resource_get_sprite_params(sprite_name name);

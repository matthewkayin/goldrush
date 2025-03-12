#pragma once

#include "math/gmath.h"
#include <cstdint>

enum sprite_name {
    SPRITE_DECORATION,
    SPRITE_COUNT
};

struct sprite_params_t {
    const char* path;
    int hframes;
    int vframes;
};

sprite_params_t resource_get_sprite_params(sprite_name name);

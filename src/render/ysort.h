#pragma once

#include "math/gmath.h"
#include "render/sprite.h"
#include <vector>

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    int ysort_position;
    uint32_t options;
    int recolor_id;
};

void ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high);
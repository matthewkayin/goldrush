#pragma once

#include "math/gmath.h"
#include "render/sprite.h"
#include "profile/profile.h"
#include <vector>

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    int ysort_position;
    uint32_t options;
    int recolor_id;
};

// This is the actual function that does the work
void _ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high);

#define ysort_render_params(params, low, high)           \
    {                                                    \
        GOLD_PROFILE_SCOPE_NAME("ysort_render_params");  \
        _ysort_render_params(params, low, high);         \
    }
#pragma once

#include "util.h"
#include <cstdint>

enum Animation {
    ANIMATION_UI_MOVE,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_UNIT_BUILD
};

struct animation_state_t {
    Animation animation;
    uint32_t timer;
    ivec2 frame;
    bool is_playing = false;
};

animation_state_t animation_start(Animation animation);
void animation_update(animation_state_t& state);
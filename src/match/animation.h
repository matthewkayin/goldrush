#pragma once

#include "util.h"
#include <cstdint>

enum Animation {
    ANIMATION_UI_MOVE,
    ANIMATION_UI_MOVE_GOLD,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_UNIT_BUILD
};

struct animation_state_t {
    Animation animation;
    uint32_t timer;
    ivec2 frame;
    int loops_remaining = 0;
};

animation_state_t animation_start(Animation animation);
bool animation_is_playing(const animation_state_t& state);
void animation_update(animation_state_t& state);
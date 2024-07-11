#pragma once

#include "util.h"
#include <cstdint>

enum Animation {
    ANIMATION_UI_MOVE,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_UI_BUTTON_HOVER,
    ANIMATION_UI_BUTTON_UNHOVER,
};

struct animation_t {
    Animation animation;
    uint32_t timer;
    ivec2 frame = ivec2(0, 0);
    bool is_playing = false;

    void play(Animation animation);
    void update();
    void stop();
};
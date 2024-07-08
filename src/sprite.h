#pragma once

#include "util.h"

enum Sprite {
    SPRITE_TILES,
    SPRITE_UI_FRAME,
    SPRITE_UI_FRAME_BOTTOM,
    SPRITE_UI_MINIMAP,
    SPRITE_UI_MOVE,
    SPRITE_SELECT_RING,
    SPRITE_UNIT_MINER,
    SPRITE_COUNT
};

enum Animation {
    ANIMATION_UI_MOVE
};

struct animation_player_t {
    Animation animation;
    uint32_t timer;
    ivec2 frame;
    bool is_playing = false;
    bool looping;

    void play(Animation animation, bool looping);
    void update();
    void stop();
};
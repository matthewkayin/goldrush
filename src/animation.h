#pragma once

#include "util.h"
#include <cstdint>
#include <unordered_map>

enum AnimationName {
    ANIMATION_UI_MOVE_CELL,
    ANIMATION_UI_MOVE_ENTITY,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_MOVE_HALF_SPEED,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_UNIT_BUILD,
    ANIMATION_UNIT_DEATH,
    ANIMATION_UNIT_DEATH_FADE,
    ANIMATION_RALLY_FLAG,
    ANIMATION_PARTICLE_SPARKS,
    ANIMATION_PARTICLE_BUNKER_COWBOY
};

const int ANIMATION_LOOPS_INDEFINITELY = -1;

struct animation_data_t {
    int vframe;
    int hframe_start;
    int hframe_end;
    uint32_t frame_duration;
    int loops;
};

extern const std::unordered_map<uint32_t, animation_data_t> ANIMATION_DATA;

struct animation_t {
    AnimationName name;
    uint32_t timer;
    xy frame;
    int loops_remaining = 0;
};

animation_t animation_create(AnimationName name);
bool animation_is_playing(const animation_t& animation);
void animation_update(animation_t& animation);
void animation_stop(animation_t& animation);
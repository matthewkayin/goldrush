#pragma once

#include "util.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

enum AnimationName {
    ANIMATION_UI_MOVE_CELL,
    ANIMATION_UI_MOVE_ENTITY,
    ANIMATION_UI_MOVE_ATTACK_ENTITY,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_MOVE_SLOW,
    ANIMATION_UNIT_MOVE_CANNON,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_SOLDIER_RANGED_ATTACK,
    ANIMATION_CANNON_ATTACK,
    ANIMATION_UNIT_MINE,
    ANIMATION_UNIT_BUILD,
    ANIMATION_UNIT_DEATH,
    ANIMATION_UNIT_DEATH_FADE,
    ANIMATION_CANNON_DEATH,
    ANIMATION_CANNON_DEATH_FADE,
    ANIMATION_RALLY_FLAG,
    ANIMATION_PARTICLE_SPARKS,
    ANIMATION_PARTICLE_BUNKER_COWBOY,
    ANIMATION_PARTICLE_EXPLOSION,
    ANIMATION_PARTICLE_CANNON_EXPLOSION,
    ANIMATION_PARTICLE_SMOKE_START,
    ANIMATION_PARTICLE_SMOKE,
    ANIMATION_PARTICLE_SMOKE_END,
    ANIMATION_SMITH_BEGIN,
    ANIMATION_SMITH_LOOP,
    ANIMATION_SMITH_END,
    ANIMATION_MINE_PRIME
};

const int ANIMATION_LOOPS_INDEFINITELY = -1;

struct animation_frame_t {
    int hframe;
    uint32_t duration;
};

struct animation_data_t {
    int vframe;
    std::vector<animation_frame_t> frames;
    int loops;
};

struct animation_t {
    AnimationName name;
    uint32_t timer;
    uint32_t frame_index;
    xy frame;
    int loops_remaining = 0;
};

void animation_init();
animation_t animation_create(AnimationName name);
bool animation_is_playing(const animation_t& animation);
void animation_update(animation_t& animation);
void animation_stop(animation_t& animation);
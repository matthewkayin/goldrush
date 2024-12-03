#include "animation.h"

#include "asserts.h"

const std::unordered_map<uint32_t, animation_data_t> ANIMATION_DATA = {
    { ANIMATION_UI_MOVE_CELL, (animation_data_t) {
        .vframe = 0,
        .hframe_start = 0, .hframe_end = 4,
        .frame_duration = 4,
        .loops = 1
    }},
    { ANIMATION_UI_MOVE_ENTITY, (animation_data_t) {
        .vframe = 0,
        .hframe_start = 0, .hframe_end = 1,
        .frame_duration = 8,
        .loops = 2
    }},
    { ANIMATION_UNIT_IDLE, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 0, .hframe_end = 0,
        .frame_duration = 0,
        .loops = 0
    }},
    { ANIMATION_UNIT_MOVE, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 1, .hframe_end = 4,
        .frame_duration = 8,
        .loops = ANIMATION_LOOPS_INDEFINITELY
    }},
    { ANIMATION_UNIT_MOVE_HALF_SPEED, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 1, .hframe_end = 4,
        .frame_duration = 10,
        .loops = ANIMATION_LOOPS_INDEFINITELY
    }},
    { ANIMATION_UNIT_ATTACK, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 5, .hframe_end = 7,
        .frame_duration = 8,
        .loops = 1
    }},
    { ANIMATION_UNIT_BUILD, (animation_data_t) {
        .vframe = 0,
        .hframe_start = 0, .hframe_end = 1,
        .frame_duration = 8,
        .loops = ANIMATION_LOOPS_INDEFINITELY
    }},
    { ANIMATION_UNIT_DEATH, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 8, .hframe_end = 11,
        .frame_duration = 4,
        .loops = 1
    }},
    { ANIMATION_UNIT_DEATH_FADE, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 12, .hframe_end = 14,
        .frame_duration = 180,
        .loops = 1
    }},
    { ANIMATION_RALLY_FLAG, (animation_data_t) {
        .vframe = 0,
        .hframe_start = 0, .hframe_end = 5,
        .frame_duration = 6,
        .loops = ANIMATION_LOOPS_INDEFINITELY
    }},
    { ANIMATION_PARTICLE_SPARKS, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 0, .hframe_end = 3,
        .frame_duration = 3,
        .loops = 1
    }},
    { ANIMATION_PARTICLE_BUNKER_COWBOY, (animation_data_t) {
        .vframe = -1,
        .hframe_start = 0, .hframe_end = 1,
        .frame_duration = 8,
        .loops = 1
    }}
};

animation_t animation_create(AnimationName name) {
    auto it = ANIMATION_DATA.find(name);
    GOLD_ASSERT(it != ANIMATION_DATA.end());

    animation_t animation;
    animation.name = name;
    animation.timer = 0;
    animation.frame = xy(it->second.hframe_start, it->second.vframe != -1 ? it->second.vframe : 0);
    animation.loops_remaining = it->second.loops;

    return animation;
}

bool animation_is_playing(const animation_t& animation) {
    return animation.loops_remaining != 0;
}

void animation_update(animation_t& animation) {
    auto it = ANIMATION_DATA.find(animation.name);
    GOLD_ASSERT(it != ANIMATION_DATA.end());

    animation.timer++;
    if (animation.timer == it->second.frame_duration) {
        animation.timer = 0;
        if (animation.frame.x < it->second.hframe_end) {
            animation.frame.x++;
        } else {
            if (it->second.loops != ANIMATION_LOOPS_INDEFINITELY) {
                animation.loops_remaining--;
            }
            if (animation.loops_remaining != 0) {
                // Restart animation
                animation.frame.x = it->second.hframe_start;
            }
        }
    }
}

void animation_stop(animation_t& animation) {
    animation.loops_remaining = 0;
}
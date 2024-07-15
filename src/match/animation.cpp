#include "animation.h"

#include "asserts.h"
#include <unordered_map>

struct animation_data_t {
    int v_frame;
    int h_frame_start;
    int h_frame_end;
    uint32_t frame_duration;
    bool is_looping;
};

static const std::unordered_map<uint32_t, animation_data_t> animation_data = {
    { ANIMATION_UI_MOVE, (animation_data_t) {
        .v_frame = 0,
        .h_frame_start = 0, .h_frame_end = 4,
        .frame_duration = 4,
        .is_looping = false
    }},
    { ANIMATION_UNIT_IDLE, (animation_data_t) {
        .v_frame = -1,
        .h_frame_start = 0, .h_frame_end = 0,
        .frame_duration = 0,
        .is_looping = false
    }},
    { ANIMATION_UNIT_MOVE, (animation_data_t) {
        .v_frame = -1,
        .h_frame_start = 1, .h_frame_end = 4,
        .frame_duration = 8,
        .is_looping = true
    }},
    { ANIMATION_UNIT_ATTACK, (animation_data_t) {
        .v_frame = -1,
        .h_frame_start = 5, .h_frame_end = 7,
        .frame_duration = 8,
        .is_looping = false
    }},
    { ANIMATION_UNIT_BUILD, (animation_data_t) {
        .v_frame = 0,
        .h_frame_start = 0, .h_frame_end = 1,
        .frame_duration = 8,
        .is_looping = true
    }}
};

animation_state_t animation_start(Animation animation) {
    auto it = animation_data.find(animation);
    GOLD_ASSERT(it != animation_data.end());

    animation_state_t state;
    state.animation = animation;
    state.timer = 0;
    state.frame = ivec2(it->second.h_frame_start, it->second.v_frame != -1 ? it->second.v_frame : 0);
    // Only set is playing to true if the animation has multiple frames
    state.is_playing = it->second.h_frame_start != it->second.h_frame_end;

    return state;
}

void animation_update(animation_state_t& state) {
    animation_data_t data = animation_data.at(state.animation);

    state.timer++;
    if (state.timer == data.frame_duration) {
        state.timer = 0;
        state.frame.x++;
        if (state.frame.x == data.h_frame_end + 1) {
            if (data.is_looping) {
                state.frame.x = data.h_frame_start;
            } else {
                state.frame.x--;
                state.is_playing = false;
            }
        }
    }
}
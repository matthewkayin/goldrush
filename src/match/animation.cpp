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
    }}
};

void animation_t::play(Animation animation) {
    if (this->animation == animation && is_playing) {
        return;
    }
    this->animation = animation;

    auto it = animation_data.find(animation);
    GOLD_ASSERT(it != animation_data.end());

    timer = 0;
    frame.x = it->second.h_frame_start;
    if (it->second.v_frame != -1) {
        frame.y = it->second.v_frame;
    }
    
    // If h_frame_start == h_frame_end, then this is a single-frame "animation" so we shouldn't play it
    if (it->second.h_frame_start == it->second.h_frame_end) {
        is_playing = false;
    } else {
        is_playing = true;
    }
}

void animation_t::update() {
    if (!is_playing) {
        return;
    }
    
    auto it = animation_data.find(animation);

    timer++;
    if (timer == it->second.frame_duration) {
        timer = 0;
        int direction = it->second.h_frame_start < it->second.h_frame_end ? 1 : -1;
        frame.x += direction;
        if (frame.x == it->second.h_frame_end + 1) {
            if (it->second.is_looping) {
                frame.x = it->second.h_frame_start;
            } else {
                frame.x--;
                is_playing = false;
            }
        }
    }
}

void animation_t::stop() {
    is_playing = false;
}

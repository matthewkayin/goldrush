#include "animation.h"

#include "asserts.h"
#include <unordered_map>
#include <vector>

struct AnimationFrame {
    int hframe;
    uint32_t duration;
};

struct AnimationData {
    int vframe;
    std::vector<AnimationFrame> frames;
    int loops;
};

static std::unordered_map<AnimationName, AnimationData> ANIMATION_DATA;
static bool initialized = false;

std::vector<AnimationFrame> animation_frame_range(int start_hframe, int end_hframe, uint32_t duration) {
    std::vector<AnimationFrame> frames;
    for (int hframe = start_hframe; hframe < end_hframe + 1; hframe++) {
        frames.push_back((AnimationFrame) { .hframe = hframe, .duration = duration });
    }

    return frames;
}

void animation_init() {
    ANIMATION_DATA[ANIMATION_UI_MOVE_CELL] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 3, 4),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UI_MOVE_ENTITY] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 1, 8),
        .loops = 2
    };
    ANIMATION_DATA[ANIMATION_UI_HIGHLIGHT_ENTITY] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 1, 16),
        .loops = 5
    };
    ANIMATION_DATA[ANIMATION_UI_MOVE_ATTACK_ENTITY] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 1, 8),
        .loops = 2
    };
    ANIMATION_DATA[ANIMATION_UNIT_IDLE] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 0 }
        },
        .loops = 0
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(1, 4, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE_SLOW] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(1, 4, 10),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE_CANNON] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(1, 8, 4),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_ATTACK] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(5, 7, 8),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SOLDIER_RANGED_ATTACK] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 15, .duration = 8 },
            (AnimationFrame) { .hframe = 16, .duration = 32 },
            (AnimationFrame) { .hframe = 17, .duration = 32 },
            (AnimationFrame) { .hframe = 18, .duration = 8 },
            (AnimationFrame) { .hframe = 19, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SOLDIER_CHARGE] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 20, .duration = 8 },
            (AnimationFrame) { .hframe = 21, .duration = 8 },
            (AnimationFrame) { .hframe = 22, .duration = 8 },
            (AnimationFrame) { .hframe = 23, .duration = 8 },
            (AnimationFrame) { .hframe = 24, .duration = 8 },
            (AnimationFrame) { .hframe = 25, .duration = 16 },
            (AnimationFrame) { .hframe = 24, .duration = 8 },
            (AnimationFrame) { .hframe = 23, .duration = 8 },
            (AnimationFrame) { .hframe = 22, .duration = 8 },
            (AnimationFrame) { .hframe = 21, .duration = 8 },
            (AnimationFrame) { .hframe = 20, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_ATTACK] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(9, 13, 8),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_MINE] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 5, .duration = 8 },
            (AnimationFrame) { .hframe = 6, .duration = 8 },
            (AnimationFrame) { .hframe = 7, .duration = 24 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_BUILD] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 1, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_DEATH] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(8, 11, 4),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_DEATH_FADE] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(12, 14, 180),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_DEATH] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(14, 17, 4),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_DEATH_FADE] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(18, 20, 180),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_BALLOON_MOVE] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(0, 1, 30),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_BALLOON_DEATH_START] = (AnimationData) {
        .vframe = -1,
        .frames = { (AnimationFrame) { .hframe = 2, .duration = 4 } },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_BALLOON_DEATH] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(3, 4, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_RALLY_FLAG] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 5, 6),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SPARKS] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(0, 3, 3),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_BUNKER_COWBOY] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(0, 1, 4),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_EXPLOSION] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(0, 5, 4),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_CANNON_EXPLOSION] = (AnimationData) {
        .vframe = -1,
        .frames = animation_frame_range(0, 3, 4),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_BLEED] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 3, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_SMITH_BEGIN] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 3, 8),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SMITH_LOOP] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(4, 6, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_SMITH_END] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(7, 8, 8),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_MINE_PRIME] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 1, 6),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_WORKSHOP] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(3, 18, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_FIRE_START] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(0, 1, 8),
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_FIRE_BURN] = (AnimationData) {
        .vframe = 0,
        .frames = animation_frame_range(2, 3, 8),
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };

    initialized = true;
}

Animation animation_create(AnimationName name) {
    if (!initialized) {
        animation_init();
    }

    auto it = ANIMATION_DATA.find(name);
    GOLD_ASSERT(it != ANIMATION_DATA.end());

    Animation animation;
    animation.name = name;
    animation.timer = 0;
    animation.frame_index = 0;
    animation.frame = ivec2(it->second.frames[0].hframe, it->second.vframe != -1 ? it->second.vframe : 0);
    animation.loops_remaining = it->second.loops;

    return animation;
}

bool animation_is_playing(const Animation& animation) {
    return animation.loops_remaining != 0;
}

void animation_update(Animation& animation) {
    auto it = ANIMATION_DATA.find(animation.name);
    GOLD_ASSERT(it != ANIMATION_DATA.end());

    animation.timer++;
    if (animation.timer == it->second.frames[animation.frame_index].duration) {
        animation.timer = 0;
        if (animation.frame_index < it->second.frames.size() - 1) {
            animation.frame_index++;
            animation.frame.x++;
        } else {
            if (it->second.loops != ANIMATION_LOOPS_INDEFINITELY) {
                animation.loops_remaining--;
            }
            if (animation.loops_remaining != 0) {
                // Restart animation
                animation.frame_index = 0;
            }
        }
        animation.frame.x = it->second.frames[animation.frame_index].hframe;
    }
}

void animation_stop(Animation& animation) {
    animation.loops_remaining = 0;
}
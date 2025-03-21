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

void animation_init() {
    ANIMATION_DATA[ANIMATION_UI_MOVE_CELL] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 4 },
            (AnimationFrame) { .hframe = 1, .duration = 4 },
            (AnimationFrame) { .hframe = 2, .duration = 4 },
            (AnimationFrame) { .hframe = 3, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UI_MOVE_ENTITY] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 8 },
            (AnimationFrame) { .hframe = 1, .duration = 8 }
        },
        .loops = 2
    };
    ANIMATION_DATA[ANIMATION_UI_MOVE_ATTACK_ENTITY] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 8 },
            (AnimationFrame) { .hframe = 1, .duration = 8 }
        },
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
        .frames = {
            (AnimationFrame) { .hframe = 1, .duration = 8 },
            (AnimationFrame) { .hframe = 2, .duration = 8 },
            (AnimationFrame) { .hframe = 3, .duration = 8 },
            (AnimationFrame) { .hframe = 4, .duration = 8 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE_SLOW] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 1, .duration = 10 },
            (AnimationFrame) { .hframe = 2, .duration = 10 },
            (AnimationFrame) { .hframe = 3, .duration = 10 },
            (AnimationFrame) { .hframe = 4, .duration = 10 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE_CANNON] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 1, .duration = 4 },
            (AnimationFrame) { .hframe = 2, .duration = 4 },
            (AnimationFrame) { .hframe = 3, .duration = 4 },
            (AnimationFrame) { .hframe = 4, .duration = 4 },
            (AnimationFrame) { .hframe = 5, .duration = 4 },
            (AnimationFrame) { .hframe = 6, .duration = 4 },
            (AnimationFrame) { .hframe = 7, .duration = 4 },
            (AnimationFrame) { .hframe = 8, .duration = 4 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_ATTACK] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 5, .duration = 8 },
            (AnimationFrame) { .hframe = 6, .duration = 8 },
            (AnimationFrame) { .hframe = 7, .duration = 8 }
        },
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
        .frames = {
            (AnimationFrame) { .hframe = 9, .duration = 8 },
            (AnimationFrame) { .hframe = 10, .duration = 8 },
            (AnimationFrame) { .hframe = 11, .duration = 8 },
            (AnimationFrame) { .hframe = 12, .duration = 8 },
            (AnimationFrame) { .hframe = 13, .duration = 8 }
        },
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
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 8 },
            (AnimationFrame) { .hframe = 1, .duration = 8 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_DEATH] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 8, .duration = 4 },
            (AnimationFrame) { .hframe = 9, .duration = 4 },
            (AnimationFrame) { .hframe = 10, .duration = 4 },
            (AnimationFrame) { .hframe = 11, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_DEATH_FADE] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 12, .duration = 180 },
            (AnimationFrame) { .hframe = 13, .duration = 180 },
            (AnimationFrame) { .hframe = 14, .duration = 180 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_DEATH] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 14, .duration = 4 },
            (AnimationFrame) { .hframe = 15, .duration = 4 },
            (AnimationFrame) { .hframe = 16, .duration = 4 },
            (AnimationFrame) { .hframe = 17, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_DEATH_FADE] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 18, .duration = 180 },
            (AnimationFrame) { .hframe = 19, .duration = 180 },
            (AnimationFrame) { .hframe = 20, .duration = 180 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_RALLY_FLAG] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 6 },
            (AnimationFrame) { .hframe = 1, .duration = 6 },
            (AnimationFrame) { .hframe = 2, .duration = 6 },
            (AnimationFrame) { .hframe = 3, .duration = 6 },
            (AnimationFrame) { .hframe = 4, .duration = 6 },
            (AnimationFrame) { .hframe = 5, .duration = 6 },
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SPARKS] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 3 },
            (AnimationFrame) { .hframe = 1, .duration = 3 },
            (AnimationFrame) { .hframe = 2, .duration = 3 },
            (AnimationFrame) { .hframe = 3, .duration = 3 },
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_BUNKER_COWBOY] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 4 },
            (AnimationFrame) { .hframe = 1, .duration = 4 },
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_EXPLOSION] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 4 },
            (AnimationFrame) { .hframe = 1, .duration = 4 },
            (AnimationFrame) { .hframe = 2, .duration = 4 },
            (AnimationFrame) { .hframe = 3, .duration = 4 },
            (AnimationFrame) { .hframe = 4, .duration = 4 },
            (AnimationFrame) { .hframe = 5, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_CANNON_EXPLOSION] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 4 },
            (AnimationFrame) { .hframe = 1, .duration = 4 },
            (AnimationFrame) { .hframe = 2, .duration = 4 },
            (AnimationFrame) { .hframe = 3, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SMOKE_START] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 4 },
            (AnimationFrame) { .hframe = 1, .duration = 4 },
            (AnimationFrame) { .hframe = 2, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SMOKE] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 3, .duration = 30 },
            (AnimationFrame) { .hframe = 4, .duration = 30 },
        },
        .loops = 36 
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SMOKE_END] = (AnimationData) {
        .vframe = -1,
        .frames = {
            (AnimationFrame) { .hframe = 5, .duration = 4 },
            (AnimationFrame) { .hframe = 6, .duration = 4 },
            (AnimationFrame) { .hframe = 7, .duration = 4 },
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SMITH_BEGIN] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 0, .duration = 8 },
            (AnimationFrame) { .hframe = 1, .duration = 8 },
            (AnimationFrame) { .hframe = 2, .duration = 8 },
            (AnimationFrame) { .hframe = 3, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SMITH_LOOP] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 4, .duration = 8 },
            (AnimationFrame) { .hframe = 5, .duration = 8 },
            (AnimationFrame) { .hframe = 6, .duration = 8 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_SMITH_END] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 7, .duration = 8 },
            (AnimationFrame) { .hframe = 8, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_MINE_PRIME] = (AnimationData) {
        .vframe = 0,
        .frames = {
            (AnimationFrame) { .hframe = 1, .duration = 6 },
            (AnimationFrame) { .hframe = 0, .duration = 6 },
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
}

Animation animation_create(AnimationName name) {
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
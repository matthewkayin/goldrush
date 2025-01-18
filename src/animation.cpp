#include "animation.h"

#include "asserts.h"

static std::unordered_map<uint32_t, animation_data_t> ANIMATION_DATA; 

void animation_init() {
    ANIMATION_DATA[ANIMATION_UI_MOVE_CELL] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 4 },
            (animation_frame_t) { .hframe = 1, .duration = 4 },
            (animation_frame_t) { .hframe = 2, .duration = 4 },
            (animation_frame_t) { .hframe = 3, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UI_MOVE_ENTITY] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 8 },
            (animation_frame_t) { .hframe = 1, .duration = 8 }
        },
        .loops = 2
    };
    ANIMATION_DATA[ANIMATION_UI_MOVE_ATTACK_ENTITY] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 8 },
            (animation_frame_t) { .hframe = 1, .duration = 8 }
        },
        .loops = 2
    };
    ANIMATION_DATA[ANIMATION_UNIT_IDLE] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 0 }
        },
        .loops = 0
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 1, .duration = 8 },
            (animation_frame_t) { .hframe = 2, .duration = 8 },
            (animation_frame_t) { .hframe = 3, .duration = 8 },
            (animation_frame_t) { .hframe = 4, .duration = 8 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE_SLOW] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 1, .duration = 10 },
            (animation_frame_t) { .hframe = 2, .duration = 10 },
            (animation_frame_t) { .hframe = 3, .duration = 10 },
            (animation_frame_t) { .hframe = 4, .duration = 10 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_MOVE_CANNON] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 1, .duration = 4 },
            (animation_frame_t) { .hframe = 2, .duration = 4 },
            (animation_frame_t) { .hframe = 3, .duration = 4 },
            (animation_frame_t) { .hframe = 4, .duration = 4 },
            (animation_frame_t) { .hframe = 5, .duration = 4 },
            (animation_frame_t) { .hframe = 6, .duration = 4 },
            (animation_frame_t) { .hframe = 7, .duration = 4 },
            (animation_frame_t) { .hframe = 8, .duration = 4 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_ATTACK] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 5, .duration = 8 },
            (animation_frame_t) { .hframe = 6, .duration = 8 },
            (animation_frame_t) { .hframe = 7, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SOLDIER_RANGED_ATTACK] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 15, .duration = 8 },
            (animation_frame_t) { .hframe = 16, .duration = 32 },
            (animation_frame_t) { .hframe = 17, .duration = 32 },
            (animation_frame_t) { .hframe = 18, .duration = 8 },
            (animation_frame_t) { .hframe = 19, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_ATTACK] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 9, .duration = 8 },
            (animation_frame_t) { .hframe = 10, .duration = 8 },
            (animation_frame_t) { .hframe = 11, .duration = 8 },
            (animation_frame_t) { .hframe = 12, .duration = 8 },
            (animation_frame_t) { .hframe = 13, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_MINE] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 5, .duration = 8 },
            (animation_frame_t) { .hframe = 6, .duration = 8 },
            (animation_frame_t) { .hframe = 7, .duration = 16 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_BUILD] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 8 },
            (animation_frame_t) { .hframe = 1, .duration = 8 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_UNIT_DEATH] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 8, .duration = 4 },
            (animation_frame_t) { .hframe = 9, .duration = 4 },
            (animation_frame_t) { .hframe = 10, .duration = 4 },
            (animation_frame_t) { .hframe = 11, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_UNIT_DEATH_FADE] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 12, .duration = 180 },
            (animation_frame_t) { .hframe = 13, .duration = 180 },
            (animation_frame_t) { .hframe = 14, .duration = 180 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_DEATH] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 14, .duration = 4 },
            (animation_frame_t) { .hframe = 15, .duration = 4 },
            (animation_frame_t) { .hframe = 16, .duration = 4 },
            (animation_frame_t) { .hframe = 17, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_CANNON_DEATH_FADE] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 18, .duration = 180 },
            (animation_frame_t) { .hframe = 19, .duration = 180 },
            (animation_frame_t) { .hframe = 20, .duration = 180 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_RALLY_FLAG] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 6 },
            (animation_frame_t) { .hframe = 1, .duration = 6 },
            (animation_frame_t) { .hframe = 2, .duration = 6 },
            (animation_frame_t) { .hframe = 3, .duration = 6 },
            (animation_frame_t) { .hframe = 4, .duration = 6 },
            (animation_frame_t) { .hframe = 5, .duration = 6 },
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SPARKS] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 3 },
            (animation_frame_t) { .hframe = 1, .duration = 3 },
            (animation_frame_t) { .hframe = 2, .duration = 3 },
            (animation_frame_t) { .hframe = 3, .duration = 3 },
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_BUNKER_COWBOY] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 4 },
            (animation_frame_t) { .hframe = 1, .duration = 4 },
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_EXPLOSION] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 4 },
            (animation_frame_t) { .hframe = 1, .duration = 4 },
            (animation_frame_t) { .hframe = 2, .duration = 4 },
            (animation_frame_t) { .hframe = 3, .duration = 4 },
            (animation_frame_t) { .hframe = 4, .duration = 4 },
            (animation_frame_t) { .hframe = 5, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_CANNON_EXPLOSION] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 4 },
            (animation_frame_t) { .hframe = 1, .duration = 4 },
            (animation_frame_t) { .hframe = 2, .duration = 4 },
            (animation_frame_t) { .hframe = 3, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SMOKE_START] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 4 },
            (animation_frame_t) { .hframe = 1, .duration = 4 },
            (animation_frame_t) { .hframe = 2, .duration = 4 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SMOKE] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 3, .duration = 30 },
            (animation_frame_t) { .hframe = 4, .duration = 30 },
        },
        .loops = 36 
    };
    ANIMATION_DATA[ANIMATION_PARTICLE_SMOKE_END] = (animation_data_t) {
        .vframe = -1,
        .frames = {
            (animation_frame_t) { .hframe = 5, .duration = 4 },
            (animation_frame_t) { .hframe = 6, .duration = 4 },
            (animation_frame_t) { .hframe = 7, .duration = 4 },
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SMITH_BEGIN] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 0, .duration = 8 },
            (animation_frame_t) { .hframe = 1, .duration = 8 },
            (animation_frame_t) { .hframe = 2, .duration = 8 },
            (animation_frame_t) { .hframe = 3, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_SMITH_LOOP] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 4, .duration = 8 },
            (animation_frame_t) { .hframe = 5, .duration = 8 },
            (animation_frame_t) { .hframe = 6, .duration = 8 }
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
    ANIMATION_DATA[ANIMATION_SMITH_END] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 7, .duration = 8 },
            (animation_frame_t) { .hframe = 8, .duration = 8 }
        },
        .loops = 1
    };
    ANIMATION_DATA[ANIMATION_MINE_PRIME] = (animation_data_t) {
        .vframe = 0,
        .frames = {
            (animation_frame_t) { .hframe = 1, .duration = 6 },
            (animation_frame_t) { .hframe = 0, .duration = 6 },
        },
        .loops = ANIMATION_LOOPS_INDEFINITELY
    };
}

animation_t animation_create(AnimationName name) {
    auto it = ANIMATION_DATA.find(name);
    GOLD_ASSERT(it != ANIMATION_DATA.end());

    animation_t animation;
    animation.name = name;
    animation.timer = 0;
    animation.frame_index = 0;
    animation.frame = xy(it->second.frames[0].hframe, it->second.vframe != -1 ? it->second.vframe : 0);
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

void animation_stop(animation_t& animation) {
    animation.loops_remaining = 0;
}
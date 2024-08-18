#include "sprite.h"

const std::unordered_map<uint32_t, sprite_params_t> SPRITE_PARAMS = {
    { SPRITE_TILES, (sprite_params_t) {
        .path = "sprite/tiles.png",
        .hframes = -1,
        .vframes = -1,
        .recolor = false
    }},
    { SPRITE_TILE_GOLD, (sprite_params_t) {
        .path = "sprite/tile_gold.png",
        .hframes = -1,
        .vframes = -1,
        .recolor = false
    }},
    { SPRITE_UI_FRAME, (sprite_params_t) {
        .path = "sprite/frame.png",
        .hframes = 3,
        .vframes = 3,
        .recolor = false
    }},
    { SPRITE_UI_FRAME_BOTTOM, (sprite_params_t) {
        .path = "sprite/ui_frame_bottom.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_UI_FRAME_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_frame_buttons.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_UI_MINIMAP, (sprite_params_t) {
        .path = "sprite/ui_minimap.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_UI_BUTTON, (sprite_params_t) {
        .path = "sprite/ui_button.png",
        .hframes = 2,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_UI_BUTTON_ICON, (sprite_params_t) {
        .path = "sprite/ui_button_icon.png",
        .hframes = UI_BUTTON_COUNT - 1,
        .vframes = 2,
        .recolor = false
    }},
    { SPRITE_UI_TOOLTIP_FRAME, (sprite_params_t) {
        .path = "sprite/ui_tooltip_frame.png",
        .hframes = 3,
        .vframes = 3,
        .recolor = false
    }},
    { SPRITE_UI_GOLD, (sprite_params_t) {
        .path = "sprite/ui_gold.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_UI_HOUSE, (sprite_params_t) {
        .path = "sprite/ui_house.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_UI_MOVE, (sprite_params_t) {
        .path = "sprite/ui_move.png",
        .hframes = 5,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING, (sprite_params_t) {
        .path = "sprite/select_ring.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_attack.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING_BUILDING_2, (sprite_params_t) {
        .path = "sprite/select_ring_building2x2.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING_BUILDING_2_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_building2x2_attack.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING_BUILDING_3, (sprite_params_t) {
        .path = "sprite/select_ring_building3x3.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING_BUILDING_3_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_building3x3_attack.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_SELECT_RING_GOLD, (sprite_params_t) {
        .path = "sprite/select_ring_gold.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_MINER_BUILDING, (sprite_params_t) {
        .path = "sprite/unit_miner_building.png",
        .hframes = 2,
        .vframes = 1,
        .recolor = true
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "sprite/unit_miner.png",
        .hframes = 15,
        .vframes = 6,
        .recolor = true
    }},
    { SPRITE_UNIT_COWBOY, (sprite_params_t) {
        .path = "sprite/unit_cowboy.png",
        .hframes = 15,
        .vframes = 3,
        .recolor = true
    }},
    { SPRITE_UNIT_WAGON, (sprite_params_t) {
        .path = "sprite/unit_wagon.png",
        .hframes = 15,
        .vframes = 3,
        .recolor = true
    }},
    { SPRITE_BUILDING_HOUSE, (sprite_params_t) {
        .path = "sprite/building_house.png",
        .hframes = 4,
        .vframes = 1,
        .recolor = true
    }},
    { SPRITE_BUILDING_CAMP, (sprite_params_t) {
        .path = "sprite/building_camp.png",
        .hframes = 4,
        .vframes = 1,
        .recolor = true
    }},
    { SPRITE_BUILDING_SALOON, (sprite_params_t) {
        .path = "sprite/building_saloon.png",
        .hframes = 4,
        .vframes = 1,
        .recolor = true
    }},
    { SPRITE_BUILDING_DESTROYED_2, (sprite_params_t) {
        .path = "sprite/building_destroyed2x2.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_BUILDING_DESTROYED_3, (sprite_params_t) {
        .path = "sprite/building_destroyed3x3.png",
        .hframes = 1,
        .vframes = 1,
        .recolor = false
    }},
    { SPRITE_FOG_OF_WAR, (sprite_params_t) {
        .path = "sprite/fog_of_war.png",
        .hframes = 8,
        .vframes = 6,
        .recolor = false
    }}  
};

const std::unordered_map<uint32_t, animation_data_t> ANIMATION_DATA = {
    { ANIMATION_UI_MOVE, (animation_data_t) {
        .vframe = 0,
        .hframe_start = 0, .hframe_end = 4,
        .frame_duration = 4,
        .loops = 1
    }},
    { ANIMATION_UI_MOVE_GOLD, (animation_data_t) {
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
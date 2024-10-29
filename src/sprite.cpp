#include "sprite.h"

const std::unordered_map<uint32_t, sprite_params_t> SPRITE_PARAMS = {
    { SPRITE_TILESET_ARIZONA, (sprite_params_t) {
        .path = "sprite/tileset_arizona.png",
        .hframes = -1,
        .vframes = -1,
        .options = SPRITE_OPTION_TILESET
    }},
    { SPRITE_TILE_DECORATION, (sprite_params_t) {
        .path = "sprite/tile_decorations.png",
        .hframes = 16,
        .vframes = 16,
        .options = SPRITE_OPTION_HFRAME_AS_TILE_SIZE
    }},
    { SPRITE_UI_FRAME, (sprite_params_t) {
        .path = "sprite/ui_frame.png",
        .hframes = 3,
        .vframes = 3,
        .options = 0
    }},
    { SPRITE_UI_FRAME_BOTTOM, (sprite_params_t) {
        .path = "sprite/ui_frame_bottom.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_FRAME_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_frame_buttons.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_MINIMAP, (sprite_params_t) {
        .path = "sprite/ui_minimap.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_BUTTON, (sprite_params_t) {
        .path = "sprite/ui_button.png",
        .hframes = 3,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_BUTTON_ICON, (sprite_params_t) {
        .path = "sprite/ui_button_icon.png",
        .hframes = UI_BUTTON_COUNT - 1,
        .vframes = 3,
        .options = 0
    }},
    { SPRITE_UI_TOOLTIP_FRAME, (sprite_params_t) {
        .path = "sprite/ui_tooltip_frame.png",
        .hframes = 3,
        .vframes = 3,
        .options = 0
    }},
    { SPRITE_UI_TEXT_FRAME, (sprite_params_t) {
        .path = "sprite/ui_text_frame.png",
        .hframes = 3,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_TABS, (sprite_params_t) {
        .path = "sprite/ui_frame_tabs.png",
        .hframes = 2,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_GOLD, (sprite_params_t) {
        .path = "sprite/ui_gold.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_HOUSE, (sprite_params_t) {
        .path = "sprite/ui_house.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_MOVE, (sprite_params_t) {
        .path = "sprite/ui_move.png",
        .hframes = 5,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_PARCHMENT_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_parchment_buttons.png",
        .hframes = 3,
        .vframes = 3,
        .options = 0
    }},
    { SPRITE_UI_OPTIONS_DROPDOWN, (sprite_params_t) {
        .path = "sprite/ui_options_dropdown.png",
        .hframes = 1,
        .vframes = 5,
        .options = 0
    }},
    { SPRITE_UI_CONTROL_GROUP_FRAME, (sprite_params_t) {
        .path = "sprite/ui_control_group_frame.png",
        .hframes = 3,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_MENU_BUTTON, (sprite_params_t) {
        .path = "sprite/ui_menu_button.png",
        .hframes = 2,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_UI_MENU_PARCHMENT_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_menu_parchment_buttons.png",
        .hframes = 2,
        .vframes = 2,
        .options = 0
    }},
    { SPRITE_SELECT_RING, (sprite_params_t) {
        .path = "sprite/select_ring.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_attack.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_WAGON, (sprite_params_t) {
        .path = "sprite/select_ring_wagon.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_WAGON_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_wagon_attack.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_BUILDING_2, (sprite_params_t) {
        .path = "sprite/select_ring_building2x2.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_BUILDING_2_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_building2x2_attack.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_BUILDING_3, (sprite_params_t) {
        .path = "sprite/select_ring_building3x3.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_BUILDING_3_ATTACK, (sprite_params_t) {
        .path = "sprite/select_ring_building3x3_attack.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_SELECT_RING_GOLD, (sprite_params_t) {
        .path = "sprite/select_ring_gold.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_MINER_BUILDING, (sprite_params_t) {
        .path = "sprite/unit_miner_building.png",
        .hframes = 2,
        .vframes = 3,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "sprite/unit_miner.png",
        .hframes = 15,
        .vframes = 6,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_UNIT_COWBOY, (sprite_params_t) {
        .path = "sprite/unit_cowboy.png",
        .hframes = 15,
        .vframes = 3,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_UNIT_WAGON, (sprite_params_t) {
        .path = "sprite/unit_wagon.png",
        .hframes = 15,
        .vframes = 3,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_UNIT_BANDIT, (sprite_params_t) {
        .path = "sprite/unit_bandit.png",
        .hframes = 15,
        .vframes = 3,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_BUILDING_HOUSE, (sprite_params_t) {
        .path = "sprite/building_house.png",
        .hframes = 4,
        .vframes = 1,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_BUILDING_CAMP, (sprite_params_t) {
        .path = "sprite/building_camp.png",
        .hframes = 5,
        .vframes = 1,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_BUILDING_SALOON, (sprite_params_t) {
        .path = "sprite/building_saloon.png",
        .hframes = 4,
        .vframes = 1,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_BUILDING_BUNKER, (sprite_params_t) {
        .path = "sprite/building_bunker.png",
        .hframes = 4,
        .vframes = 1,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_BUILDING_MINE, (sprite_params_t) {
        .path = "sprite/building_mine.png",
        .hframes = 3,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_BUILDING_DESTROYED_2, (sprite_params_t) {
        .path = "sprite/building_destroyed2x2.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_BUILDING_DESTROYED_3, (sprite_params_t) {
        .path = "sprite/building_destroyed3x3.png",
        .hframes = 1,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_FOG_OF_WAR, (sprite_params_t) {
        .path = "sprite/fog_of_war.png",
        .hframes = 47 * 2,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_RALLY_FLAG, (sprite_params_t) {
        .path = "sprite/rally_flag.png",
        .hframes = 6,
        .vframes = 1,
        .options = SPRITE_OPTION_RECOLOR
    }},
    { SPRITE_MENU_CLOUDS, (sprite_params_t) {
        .path = "sprite/menu_clouds.png",
        .hframes = 3,
        .vframes = 1,
        .options = 0
    }},
    { SPRITE_PARTICLE_SPARKS, (sprite_params_t) {
        .path = "sprite/particle_sparks.png",
        .hframes = 4,
        .vframes = 3,
        .options = 0
    }}  
};

std::unordered_map<uint32_t, tile_data_t> TILE_DATA = {
    { TILE_NULL, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 16,
        .index = 0
    }},
    { TILE_SAND, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 0,
        .source_y = 0,
        .index = 0
    }},
    { TILE_SAND2, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 16,
        .source_y = 0,
        .index = 0
    }},
    { TILE_SAND3, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WATER, (tile_data_t) {
        .type = TILE_TYPE_AUTO,
        .source_x = 0,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_NW_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_NE_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_SW_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_SE_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_NORTH_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_WEST_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_EAST_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_SOUTH_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_SW_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_SOUTH_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_SE_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_NW_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_NE_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_SW_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_SE_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_SOUTH_STAIR_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_SOUTH_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_SOUTH_STAIR_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 128,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_SOUTH_STAIR_FRONT_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_SOUTH_STAIR_FRONT_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_SOUTH_STAIR_FRONT_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 128,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_NORTH_STAIR_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_NORTH_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_NORTH_STAIR_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 176,
        .source_y = 48,
        .index = 0
    }},
    { TILE_WALL_WEST_STAIR_TOP, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_WEST_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_WEST_STAIR_BOTTOM, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 32,
        .index = 0
    }},
    { TILE_WALL_EAST_STAIR_TOP, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 0,
        .index = 0
    }},
    { TILE_WALL_EAST_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 16,
        .index = 0
    }},
    { TILE_WALL_EAST_STAIR_BOTTOM, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 32,
        .index = 0
    }},
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
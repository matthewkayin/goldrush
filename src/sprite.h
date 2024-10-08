#pragma once

#include "asserts.h"
#include "util.h"
#include <unordered_map>

enum UiButton {
    UI_BUTTON_NONE,
    UI_BUTTON_MOVE,
    UI_BUTTON_STOP,
    UI_BUTTON_ATTACK,
    UI_BUTTON_BUILD,
    UI_BUTTON_CANCEL,
    UI_BUTTON_UNLOAD,
    UI_BUTTON_BUILD_HOUSE,
    UI_BUTTON_BUILD_CAMP,
    UI_BUTTON_BUILD_SALOON,
    UI_BUTTON_MINE,
    UI_BUTTON_UNIT_MINER,
    UI_BUTTON_UNIT_COWBOY,
    UI_BUTTON_UNIT_WAGON,
    UI_BUTTON_COUNT
};

enum Sprite {
    SPRITE_TILESET_ARIZONA,
    SPRITE_TILE_DECORATION,
    SPRITE_UI_FRAME,
    SPRITE_UI_FRAME_BOTTOM,
    SPRITE_UI_FRAME_BUTTONS,
    SPRITE_UI_MINIMAP,
    SPRITE_UI_BUTTON,
    SPRITE_UI_BUTTON_ICON,
    SPRITE_UI_TOOLTIP_FRAME,
    SPRITE_UI_TEXT_FRAME,
    SPRITE_UI_TABS,
    SPRITE_UI_GOLD,
    SPRITE_UI_HOUSE,
    SPRITE_UI_MOVE,
    SPRITE_UI_PARCHMENT_BUTTONS,
    SPRITE_UI_OPTIONS_DROPDOWN,
    SPRITE_UI_CONTROL_GROUP_FRAME,
    SPRITE_SELECT_RING,
    SPRITE_SELECT_RING_ATTACK,
    SPRITE_SELECT_RING_WAGON,
    SPRITE_SELECT_RING_WAGON_ATTACK,
    SPRITE_SELECT_RING_BUILDING_2,
    SPRITE_SELECT_RING_BUILDING_2_ATTACK,
    SPRITE_SELECT_RING_BUILDING_3,
    SPRITE_SELECT_RING_BUILDING_3_ATTACK,
    SPRITE_SELECT_RING_GOLD,
    SPRITE_MINER_BUILDING,
    SPRITE_UNIT_MINER,
    SPRITE_UNIT_COWBOY,
    SPRITE_UNIT_WAGON,
    SPRITE_BUILDING_HOUSE,
    SPRITE_BUILDING_CAMP,
    SPRITE_BUILDING_SALOON,
    SPRITE_BUILDING_MINE,
    SPRITE_BUILDING_DESTROYED_2,
    SPRITE_BUILDING_DESTROYED_3,
    SPRITE_FOG_OF_WAR,
    SPRITE_RALLY_FLAG,
    SPRITE_MENU_CLOUDS,
    SPRITE_COUNT
};

const uint32_t SPRITE_OPTION_HFRAME_AS_TILE_SIZE = 1;
const uint32_t SPRITE_OPTION_RECOLOR = 1 << 1;
const uint32_t SPRITE_OPTION_TILESET = 1 << 2;

struct sprite_params_t {
    const char* path;
    int hframes;
    int vframes;
    uint32_t options;
};

extern const std::unordered_map<uint32_t, sprite_params_t> SPRITE_PARAMS;
extern uint16_t wall_index_offset;
extern std::unordered_map<uint32_t, uint32_t> neighbors_to_autotile_index;

enum Tile {
    TILE_SAND,
    TILE_WATER,
    TILE_WALL
};

enum TileType {
    TILE_TYPE_SINGLE,
    TILE_TYPE_AUTO
};

struct tile_data_t {
    TileType type;
    xy source_pos;
};

enum AnimationName {
    ANIMATION_UI_MOVE,
    ANIMATION_UI_MOVE_GOLD,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_MOVE_HALF_SPEED,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_UNIT_BUILD,
    ANIMATION_UNIT_DEATH,
    ANIMATION_UNIT_DEATH_FADE,
    ANIMATION_RALLY_FLAG
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
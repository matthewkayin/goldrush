#pragma once

#include "defines.h"
#include "util.h"
#include "id_array.h"
#include "engine.h"
#include <SDL2/SDL.h>
#include <queue>

#define UI_HEIGHT 88
#define PLAYER_NONE UINT8_MAX

// UI

const xy UI_FRAME_BOTTOM_POSITION = xy(136, SCREEN_HEIGHT - UI_HEIGHT);

enum UiMode {
    UI_MODE_MATCH_NOT_STARTED,
    UI_MODE_NONE,
    UI_MODE_SELECTING,
    UI_MODE_MINIMAP_DRAG,
    UI_MODE_BUILDING_PLACE,
    UI_MODE_TARGET_ATTACK,
    UI_MODE_TARGET_UNLOAD,
    UI_MODE_TARGET_REPAIR,
    UI_MODE_CHAT,
    UI_MODE_WAITING_FOR_PLAYERS,
    UI_MODE_MENU,
    UI_MODE_MENU_SURRENDER,
    UI_MODE_MATCH_OVER_PLAYERS_DISCONNECTED,
    UI_MODE_MATCH_OVER_SERVER_DISCONNECTED,
    UI_MODE_MATCH_OVER_PLAYER_WINS,
    UI_MODE_MATCH_OVER_PLAYER_LOST,
    UI_MODE_LEAVE_MATCH
};

// Map

struct tile_t {
    uint16_t index;
    uint16_t elevation;
};

// Units

enum UnitOrderType {
    UNIT_ORDER_NONE,
    UNIT_ORDER_MOVE_UNIT,
    UNIT_ORDER_MOVE_CELL,
    UNIT_ORDER_ATTACK_MOVE_CELL,
    UNIT_ORDER_BUILD,
    UNIT_ORDER_BUILD_ASSIST,
    UNIT_ORDER_REPAIR,
    UNIT_ORDER_UNLOAD
};

struct unit_order_t {
    UnitOrderType type;
    union {
        entity_id target_id;
        xy target_cell;
    };
};

struct unit_t {
    unit_order_t order;
};

// Entities

const entity_id CELL_EMPTY = ID_NULL;
const entity_id CELL_BLOCKED = ID_MAX + 2;

enum EntityType {
    UNIT_MINER
};

struct entity_t {
    EntityType type;
    uint8_t player_id;

    xy cell;
    xy_fixed position;

    union {
        unit_t unit;
    };
};

// Input

enum InputType: uint8_t {
    INPUT_NONE,
};

struct input_t {
    uint8_t type;
};

struct match_state_t {
    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    // UI
    UiMode ui_mode;
    xy camera_offset;

    // Map
    uint32_t map_width;
    uint32_t map_height;
    std::vector<tile_t> map_tiles;
    std::vector<entity_id> map_cells;

    // Entities
    id_array<entity_t> entities;
};

match_state_t match_init();
void match_handle_input(match_state_t& state, SDL_Event e);
void match_update(match_state_t& state);
void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);
void match_render(const match_state_t& state);

// Generic
void match_camera_clamp(match_state_t& state);
void match_camera_center_on_cell(match_state_t& state, xy cell);

// Map
void map_init(match_state_t& state, uint32_t width, uint32_t height);
void map_set_cell_rect(match_state_t& state, xy cell, int cell_size, entity_id value);

// Entities
entity_id entity_create_unit(match_state_t& state, EntityType type, uint8_t player_id, xy cell);
bool entity_is_unit(EntityType entity);
int entity_cell_size(EntityType entity);
Sprite entity_get_sprite(const entity_t entity);
uint16_t entity_get_elevation(const match_state_t& state, const entity_t& entity);
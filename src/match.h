#pragma once

#include "defines.h"
#include "util.h"
#include "id_array.h"
#include <SDL2/SDL.h>
#include <queue>

#define UI_HEIGHT 88

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

enum UnitTargetType {
    UNIT_TARGET_NONE,
    
};

struct unit_t {

};

// Entities

const entity_id CELL_EMPTY = ID_NULL;
const entity_id CELL_BLOCKED = ID_MAX + 2;

enum EntityType {
    UNIT_MINER
};

struct entity_t {
    EntityType type;
    xy cell;
    xy_fixed position;
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

// Entities
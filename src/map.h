#pragma once

#include "defines.h"
#include "util.h"
#include "types.h"
#include <vector>
#include <unordered_map>

const entity_id CELL_EMPTY = ID_NULL;
const entity_id CELL_BLOCKED = ID_MAX + 2;
const entity_id CELL_UNREACHABLE = ID_MAX + 3;
const entity_id CELL_DECORATION_1 = ID_MAX + 4;
const entity_id CELL_DECORATION_2 = ID_MAX + 5;
const entity_id CELL_DECORATION_3 = ID_MAX + 6;
const entity_id CELL_DECORATION_4 = ID_MAX + 7;
const entity_id CELL_DECORATION_5 = ID_MAX + 8;

const int FOG_HIDDEN = -1;
const int FOG_EXPLORED = 0;

struct tile_t {
    uint16_t index;
    uint16_t elevation;
};

struct remembered_entity_t {
    render_sprite_params_t sprite_params;
    xy cell;
    int cell_size;
};

struct map_reveal_t {
    int player_id;
    xy cell;
    int cell_size;
    int sight;
    int timer;
};

struct map_t {
    uint32_t width;
    uint32_t height;

    // Tiles
    std::vector<tile_t> tiles;

    // Cells
    std::vector<entity_id> cells;
    std::vector<entity_id> mine_cells;

    // Fog
    std::vector<int> fog[MAX_PLAYERS];
    std::vector<int> detection[MAX_PLAYERS];
    std::unordered_map<entity_id, remembered_entity_t> remembered_entities[MAX_PLAYERS];
    bool is_fog_dirty;

    std::vector<map_reveal_t> map_reveals;

    xy player_spawns[MAX_PLAYERS];
};

map_t map_init(const noise_t& noise);

void map_calculate_unreachable_cells(map_t& map);
void map_recalculate_unreachable_cells(map_t& map, xy cell);
bool map_is_cell_in_bounds(const map_t& map, xy cell);
bool map_is_cell_rect_in_bounds(const map_t& map, xy cell, int cell_size);
tile_t map_get_tile(const map_t& map, xy cell);
entity_id map_get_cell(const map_t& map, xy cell);
bool map_is_cell_rect_equal_to(const map_t& state, xy cell, int cell_size, entity_id value);
void map_set_cell_rect(map_t& state, xy cell, int cell_size, entity_id value);
bool map_is_cell_rect_occupied(const map_t& state, xy cell, int cell_size, xy origin = xy(-1, -1), bool ignore_miners = false);
bool map_is_cell_rect_occupied(const map_t& state, xy cell, xy cell_size, xy origin = xy(-1, -1), bool ignore_miners = false);
xy map_get_nearest_cell_around_rect(const map_t& state, xy start, int start_size, xy rect_position, int rect_size, bool allow_blocked_cells);
bool map_is_tile_ramp(const map_t& state, xy cell);
bool map_is_cell_rect_same_elevation(const map_t& state, xy cell, xy size);
void map_pathfind(const map_t& state, xy from, xy to, int cell_size, std::vector<xy>* path, bool gold_walk);
bool map_is_cell_rect_revealed(const map_t& state, uint8_t player_id, xy cell, int cell_size);
void map_fog_update(map_t& state, uint8_t player_id, xy cell, int cell_size, int sight, bool increment, bool has_detection);
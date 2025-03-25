#pragma once

#include "defines.h"
#include "noise.h"
#include "render/sprite.h"
#include "math/gmath.h"
#include <vector>

const EntityId CELL_EMPTY = ID_NULL;
const EntityId CELL_BLOCKED = ID_MAX + 2;
const EntityId CELL_UNREACHABLE = ID_MAX + 3;
const EntityId CELL_DECORATION_1 = ID_MAX + 4;
const EntityId CELL_DECORATION_2 = ID_MAX + 5;
const EntityId CELL_DECORATION_3 = ID_MAX + 6;
const EntityId CELL_DECORATION_4 = ID_MAX + 7;
const EntityId CELL_DECORATION_5 = ID_MAX + 8;

struct Tile {
    uint8_t sprite;
    uint8_t autotile_index;
    uint16_t elevation;
};

struct Map {
    uint32_t width;
    uint32_t height;
    std::vector<Tile> tiles;
    std::vector<EntityId> cells;
};

void map_init(Map& map, Noise& noise, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells);

uint32_t map_neighbors_to_autotile_index(uint32_t neighbors);
bool map_is_cell_in_bounds(const Map& map, ivec2 cell);
bool map_is_cell_rect_in_bounds(const Map& map, ivec2 cell, ivec2 size);
Tile map_get_tile(const Map& map, ivec2 cell);
EntityId map_get_cell(const Map& map, ivec2 cell);
bool map_is_tile_ramp(const Map& map, ivec2 cell);
bool map_is_cell_rect_same_elevation(const Map& map, ivec2 cell, ivec2 size);
bool map_is_cell_rect_occupied(const Map& map, ivec2 cell, ivec2 size);
void map_set_cell_rect(Map& map, ivec2 cell, int size, EntityId value);
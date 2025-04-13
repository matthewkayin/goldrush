#pragma once

#include "defines.h"
#include "noise.h"
#include "render/sprite.h"
#include "math/gmath.h"
#include <vector>

struct Tile {
    SpriteName sprite;
    ivec2 frame;
    uint16_t elevation;
};

enum CellLayer {
    CELL_LAYER_UNDERGROUND,
    CELL_LAYER_GROUND,
    CELL_LAYER_SKY,
    CELL_LAYER_COUNT
};

enum CellType {
    CELL_EMPTY,
    CELL_BLOCKED,
    CELL_UNREACHABLE,
    CELL_DECORATION_1,
    CELL_DECORATION_2,
    CELL_DECORATION_3,
    CELL_DECORATION_4,
    CELL_DECORATION_5,
    CELL_UNIT,
    CELL_BUILDING,
    CELL_MINER,
    CELL_GOLDMINE
};

struct Cell {
    CellType type;
    EntityId id;
};

struct Map {
    uint32_t width;
    uint32_t height;
    std::vector<Tile> tiles;
    std::vector<Cell> cells[CELL_LAYER_COUNT];
};

void map_init(Map& map, Noise& noise, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells);

uint32_t map_neighbors_to_autotile_index(uint32_t neighbors);

bool map_is_cell_in_bounds(const Map& map, ivec2 cell);
bool map_is_cell_rect_in_bounds(const Map& map, ivec2 cell, int size);

Tile map_get_tile(const Map& map, ivec2 cell);
bool map_is_tile_ramp(const Map& map, ivec2 cell);
bool map_is_cell_rect_same_elevation(const Map& map, ivec2 cell, int size);

Cell map_get_cell(const Map& map, CellLayer layer, ivec2 cell);
void map_set_cell(Map& map, CellLayer layer, ivec2 cell, Cell value);
void map_set_cell_rect(Map& map, CellLayer layer, ivec2 cell, int size, Cell value);
bool map_is_cell_rect_equal_to(const Map& map, CellLayer layer, ivec2 cell, int size, EntityId id);
bool map_is_cell_rect_occupied(const Map& map, CellLayer layer, ivec2 cell, int size, ivec2 origin = ivec2(-1, -1), bool gold_walk = false);

ivec2 map_get_player_town_hall_cell(const Map& map, ivec2 mine_cell);
ivec2 map_get_nearest_cell_around_rect(const Map& map, CellLayer layer, ivec2 start, int start_size, ivec2 rect_position, int rect_size, bool allow_blocked_cells = false, ivec2 ignore_cell = ivec2(-1, -1));

ivec2 map_get_exit_cell(const Map& map, CellLayer layer, ivec2 building_cell, int building_size, int unit_size, ivec2 rally_cell, bool allow_blocked_cells);

void map_pathfind(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, bool gold_walk, std::vector<ivec2>* ignore_cells = NULL);
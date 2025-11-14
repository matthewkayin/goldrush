#pragma once

#include "defines.h"
#include "noise.h"
#include "render/sprite.h"
#include "math/gmath.h"
#include <vector>
#include <unordered_map>

const uint32_t MAP_OPTION_IGNORE_UNITS = 1;
const uint32_t MAP_OPTION_IGNORE_MINERS = 2;
const uint32_t MAP_OPTION_AVOID_LANDMINES = 1 << 2;
const uint32_t MAP_OPTION_ALLOW_PATH_SQUIRRELING = 1 << 3;

struct Tile {
    SpriteName sprite;
    ivec2 frame;
    uint32_t elevation;
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
    uint8_t padding[2];
};

struct MapPathNode {
    fixed cost;
    fixed distance;
    // The parent is the previous node stepped in the path to reach this node
    // It should be an index in the explored list or -1 if it is the start node
    uint32_t parent;
    ivec2 cell;

    fixed score() const {
        return cost + distance;
    };
};

struct MapRegionPathNode {
    ivec2 cell;
    uint32_t connection;
    uint32_t parent;
    int cost;
    int score(const ivec2& target) {
        return cost + ivec2::manhattan_distance(cell, target);
    }
};

struct MapRegionConnection {
    std::vector<ivec2> left;
    std::vector<ivec2> right;
};

struct Map {
    uint32_t width;
    uint32_t height;
    std::vector<Tile> tiles;
    std::vector<Cell> cells[CELL_LAYER_COUNT];

    int pathing_region_count;
    std::vector<int> pathing_regions;
    std::vector<std::vector<int>> pathing_region_connection_indices;
    std::vector<MapRegionConnection> pathing_region_connections;
};

void map_init(Map& map, Noise& noise, uint32_t* lcg_seed, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells);
bool map_is_cell_blocked(Cell cell);
bool map_is_cell_rect_blocked(const Map& map, ivec2 cell, int cell_size);
void map_calculate_unreachable_cells(Map& map);

uint32_t map_neighbors_to_autotile_index(uint32_t neighbors);

bool map_is_cell_in_bounds(const Map& map, ivec2 cell);
bool map_is_cell_rect_in_bounds(const Map& map, ivec2 cell, int size);
ivec2 map_clamp_cell(const Map& map, ivec2 cell);

Tile map_get_tile(const Map& map, ivec2 cell);
bool map_is_tile_ramp(const Map& map, ivec2 cell);
bool map_is_cell_rect_same_elevation(const Map& map, ivec2 cell, int size);

Cell map_get_cell(const Map& map, CellLayer layer, ivec2 cell);
void map_set_cell(Map& map, CellLayer layer, ivec2 cell, Cell value);
void map_set_cell_rect(Map& map, CellLayer layer, ivec2 cell, int size, Cell value);
bool map_is_cell_rect_equal_to(const Map& map, CellLayer layer, ivec2 cell, int size, EntityId id);
bool map_is_cell_rect_empty(const Map& map, CellLayer layer, ivec2 cell, int size);
bool map_is_cell_rect_occupied(const Map& map, CellLayer layer, ivec2 cell, int size, ivec2 origin = ivec2(-1, -1), uint32_t ignore = 0);

ivec2 map_get_player_town_hall_cell(const Map& map, ivec2 mine_cell);
ivec2 map_get_nearest_cell_around_rect(const Map& map, CellLayer layer, ivec2 start, int start_size, ivec2 rect_position, int rect_size, uint32_t ignore = 0, ivec2 ignore_cell = ivec2(-1, -1));

ivec2 map_get_exit_cell(const Map& map, CellLayer layer, ivec2 building_cell, int building_size, int unit_size, ivec2 rally_cell, uint32_t ignore);

int map_get_pathing_region(const Map& map, ivec2 cell);

void map_pathfind_calculate_path(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, uint32_t options, std::vector<ivec2>* ignore_cells, bool limit_region);
void map_pathfind(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, uint32_t options, std::vector<ivec2>* ignore_cells = NULL);
void map_get_ideal_mine_exit_path(const Map& map, ivec2 mine_cell, ivec2 hall_cell, std::vector<ivec2>* path);
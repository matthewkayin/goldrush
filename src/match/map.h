#pragma once

#include "defines.h"
#include "noise.h"
#include "render/sprite.h"
#include "math/gmath.h"
#include "core/match_setting.h"
#include <vector>
#include <unordered_map>

#define MAP_REGION_MAX 128
#define MAP_REGION_CONNECTION_MAX 256
#define MAP_COST_TO_CONNECTION_MAX 16
#define MAP_REGION_CHUNK_SIZE 32

const uint32_t MAP_OPTION_IGNORE_UNITS = 1;
const uint32_t MAP_OPTION_IGNORE_MINERS = 2;
const uint32_t MAP_OPTION_AVOID_LANDMINES = 1 << 2;
const uint32_t MAP_OPTION_ALLOW_PATH_SQUIRRELING = 1 << 3;
const uint32_t MAP_OPTION_NO_REGION_PATH = 1 << 4;

struct Tile {
    SpriteName sprite;
    ivec2 frame;
    uint32_t elevation;
};
STATIC_ASSERT(sizeof(Tile) == 16);

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
    CELL_DECORATION,
    CELL_UNIT,
    CELL_BUILDING,
    CELL_MINER,
    CELL_GOLDMINE
};

struct Cell {
    CellType type;
    union {
        EntityId id;
        uint16_t decoration_hframe;
    };
    uint16_t padding = 0;
};
STATIC_ASSERT(sizeof(Cell) == 8);

struct MapRegionPathNode {
    int parent;
    ivec2 cell;
    int connection_index;
    int cost;
};

struct MapPathNode {
    // The parent is the previous node stepped in the path to reach this node
    // It should be an index in the explored list or -1 if it is the start node
    int parent;
    ivec2 cell;
    int cost;
    int score(ivec2 target) const {
        // Cost is divided by 2 because the cost scale is larger than the distance scale
        // (this is so that we can weight diagonal movement more heavily than adjacent movement.
        // diagonal movement costs 1.5 which is saved as a cost of 3 and adjacent movement costs
        // 1 which is saved as a cost of 2 (3/2 vs 2/2).
        return (cost / 2) + ivec2::manhattan_distance(cell, target);
    }
};

// The reason why the cells array has a size of MAP_REGION_CHUNK_SIZE
// is because each chunk is CHUNK_SIZE x CHUNK_SIZE big, which means
// the biggest possible connection is two totally flat regions side-by-side,
// their cells on one of the four sides of the region square are all lined up,
// so that's a connection that is MAP_REGION_CHUNK_SIZE big
struct MapRegionConnection {
    uint32_t cell_count;
    ivec2 cells[MAP_REGION_CHUNK_SIZE];
};

struct Map {
    MapType type;
    int width;
    int height;
    Tile tiles[MAX_MAP_SIZE * MAX_MAP_SIZE];
    Cell cells[CELL_LAYER_COUNT][MAX_MAP_SIZE * MAX_MAP_SIZE];

    uint32_t region_count;
    uint8_t regions[MAX_MAP_SIZE * MAX_MAP_SIZE];
    uint8_t region_connection_indices[MAP_REGION_MAX][MAP_REGION_MAX];
    uint32_t region_connection_count;
    MapRegionConnection region_connections[MAP_REGION_CONNECTION_MAX];
    uint8_t region_connection_to_connection_cost[MAP_REGION_CONNECTION_MAX][MAP_REGION_CONNECTION_MAX];
};

void map_init(Map& map, MapType map_type, int width, int height);
void map_init_generate(Map& map, MapType map_type, Noise* noise, int* lcg_seed, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells);
void map_init_regions(Map& map);
void map_cleanup_noise(const Map& map, Noise* noise);
void map_bake_tiles(Map& map, const Noise* noise, int* lcg_seed);
void map_bake_map_tiles_and_remove_artifacts(Map& map, Noise* noise, int* lcg_seed);
void map_bake_front_walls(Map& map);
Direction map_get_tile_stair_direction(const Tile& tile);
bool map_should_cell_be_blocked(const Map& map, ivec2 cell);
bool map_can_ramp_be_placed_on_tile(const Tile& tile);
Direction map_get_tile_stair_direction(const Tile& tile);
void map_bake_ramp(Map& map, Direction stair_direction, ivec2 stair_min, ivec2 stair_max);
void map_bake_ramps(Map& map, const Noise* noise);
SpriteName map_choose_ground_tile_sprite(MapType map_type, int index, int* lcg_seed);
SpriteName map_choose_water_tile_sprite(MapType map_type);
SpriteName map_get_plain_ground_tile_sprite(MapType map_type);
SpriteName map_get_decoration_sprite(MapType map_type);
bool map_is_cell_blocked(Cell cell);
bool map_is_cell_rect_blocked(const Map& map, ivec2 cell, int cell_size);
void map_calculate_unreachable_cells(Map& map);

uint32_t map_neighbors_to_autotile_index(uint32_t neighbors);
void map_generate_decorations(Map& map, Noise* noise, int* lcg_seed, const std::vector<ivec2>& goldmine_cells);
void map_create_decoration_at_cell(Map& map, int* lcg_seed, ivec2 cell);

bool map_is_cell_in_bounds(const Map& map, ivec2 cell);
bool map_is_cell_rect_in_bounds(const Map& map, ivec2 cell, int size);
ivec2 map_clamp_cell(const Map& map, ivec2 cell);

Tile map_get_tile(const Map& map, ivec2 cell);
bool map_is_tile_ground(const Map& map, ivec2 cell);
bool map_is_tile_ramp(const Map& map, ivec2 cell);
bool map_is_tile_water(const Map& map, ivec2 cell);
bool map_is_cell_rect_same_elevation(const Map& map, ivec2 cell, int size);

Cell map_get_cell(const Map& map, CellLayer layer, ivec2 cell);
void map_set_cell(Map& map, CellLayer layer, ivec2 cell, Cell value);
void map_set_cell_rect(Map& map, CellLayer layer, ivec2 cell, int size, Cell value);
bool map_is_cell_rect_equal_to(const Map& map, CellLayer layer, ivec2 cell, int size, EntityId id);
bool map_is_cell_rect_empty(const Map& map, CellLayer layer, ivec2 cell, int size);
bool map_is_cell_rect_occupied(const Map& map, CellLayer layer, ivec2 cell, int size, ivec2 origin = ivec2(-1, -1), uint32_t ignore = 0);

ivec2 map_get_player_town_hall_cell(const Map& map, ivec2 mine_cell);
ivec2 map_get_nearest_cell_around_rect(const Map& map, CellLayer layer, ivec2 start, int start_size, ivec2 rect_position, int rect_size, uint32_t ignore = 0, ivec2 ignore_cell = ivec2(-1, -1));

ivec2 map_get_exit_cell(const Map& map, CellLayer layer, ivec2 building_cell, int building_size, int unit_size, ivec2 rally_cell, uint32_t ignore, ivec2 ignore_cell = ivec2(-1, -1));

int map_get_region(const Map& map, ivec2 cell);
int map_get_region_count(const Map& map);
int map_are_regions_connected(const Map& map, int region_a, int region_b);

void map_pathfind(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, uint32_t options, std::vector<ivec2>* ignore_cells = NULL);
ivec2 map_get_ideal_mine_exit_path_rally_cell(const Map& map, ivec2 mine_cell, ivec2 hall_cell);
void map_get_ideal_mine_exit_path(const Map& map, ivec2 mine_cell, ivec2 hall_cell, std::vector<ivec2>* path);
ivec2 map_get_ideal_mine_entrance_cell(const Map& map, ivec2 mine_cell, ivec2 hall_cell);
void map_get_ideal_mine_entrance_path(const Map& map, ivec2 mine_cell, ivec2 hall_cell, std::vector<ivec2>* path);
#pragma once

#include "util.h"
#include <vector>

enum CellValue {
    CELL_EMPTY = -1,
    CELL_FILLED = -2
};

struct cell_value_t {
    ivec2 cell;
    int value;
};

struct gold_t {
    int face;
    int amount;
    ivec2 position;
};

struct map_t {
    std::vector<int> tiles;
    std::vector<int> cells;
    std::vector<cell_value_t> remembered_values;
    std::vector<gold_t> gold;
    int width;
    int height;
};

map_t map_init(const ivec2& size, ivec2* player_spawns);
bool map_cell_is_in_bounds(const map_t& map, const ivec2& cell);
bool map_cell_is_blocked(const map_t& map, const ivec2& cell);
bool map_cells_are_blocked(const map_t& map, const ivec2& cell, const ivec2& cell_size);
int map_cell_get_value(const map_t& map, const ivec2& cell);
bool map_cell_is_gold(const map_t& map, const ivec2& cell);
void map_cell_set_value(map_t& map, const ivec2& cell, int value);
void map_cells_set_value(map_t& map, const ivec2& cell, const ivec2& cell_size, int value);
void map_cell_set_temp_value(map_t& map, const ivec2& cell, int value);
void map_clear_temp_fills(map_t& map);
ivec2 map_get_first_free_cell_around_cells(const map_t& map, const ivec2& cell, const ivec2& cell_size);
ivec2 map_get_nearest_free_cell_around_cell(const map_t& map, const ivec2& from, const ivec2& cell);
std::vector<ivec2> map_pathfind(const map_t& map, const ivec2& from, const ivec2& to);

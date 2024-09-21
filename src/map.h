#pragma once

#include "util.h"
#include <vector>

struct tile_t {
    uint16_t base;
    uint16_t decoration;
};

enum CellType: uint16_t {
    CELL_EMPTY,
    CELL_BLOCKED,
    CELL_UNIT,
    CELL_BUILDING,
    CELL_MINE
};

struct cell_t {
    CellType type;
    uint16_t value;
};

enum FogType: uint16_t {
    FOG_HIDDEN,
    FOG_EXPLORED,
    FOG_REVEALED
};

struct map_t {
    uint32_t width;
    uint32_t height;
    std::vector<tile_t> tiles;
    std::vector<cell_t> cells;
    std::vector<FogType> fog;
    bool is_fog_dirty;
};

struct mine_t {
    xy cell;
    uint32_t gold_left;
    bool is_occupied;
};

bool map_is_cell_in_bounds(const map_t& map, xy cell);
bool map_is_cell_rect_in_bounds(const map_t& map, rect_t cell_rect);
bool map_is_cell_blocked(const map_t& map, xy cell);
bool map_is_cell_rect_blocked(const map_t& map, rect_t cell_rect);
cell_t map_get_cell(const map_t& map, xy cell);
void map_set_cell(map_t& map, xy cell, CellType type, uint16_t value = 0);
void map_set_cell_rect(map_t& map, rect_t cell_rect, CellType type, uint16_t id = 0);
FogType map_get_fog(const map_t& map, xy cell);
bool map_is_cell_rect_revealed(const map_t& map, rect_t rect);
void map_fog_reveal_at_cell(map_t& map, xy cell, xy size, int sight);
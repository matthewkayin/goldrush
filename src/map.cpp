#include "match.h"

#include "network.h"
#include "logger.h"

bool map_is_cell_in_bounds(const map_t& map, xy cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= map.width || cell.y >= map.height);
}

bool map_is_cell_rect_in_bounds(const map_t& map, rect_t cell_rect) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            if (x < 0 || y < 0 || x >= map.width || y >= map.height) {
                return false;
            }
        }
    }

    return true;
}

bool map_is_cell_blocked(const map_t& map, xy cell) {
    return map.cells[cell.x + (cell.y * map.width)].type != CELL_EMPTY;
}

bool map_is_cell_rect_blocked(const map_t& map, rect_t cell_rect) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            if (map.cells[x + (y * map.width)].type != CELL_EMPTY) {
                return true;
            }
        }
    }
    
    return false;
}

cell_t map_get_cell(const map_t& map, xy cell) {
    return map.cells[cell.x + (cell.y * map.height)];
}

void map_set_cell(map_t& map, xy cell, CellType type, uint16_t value) {
    map.cells[cell.x + (cell.y * map.height)] = (cell_t) {
        .type = type,
        .value = value
    };
    map.is_fog_dirty = true;
}

void map_set_cell_rect(map_t& map, rect_t cell_rect, CellType type, uint16_t value) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            map.cells[x + (y * map.height)] = (cell_t) {
                .type = type,
                .value = value
            };
        }
    }
    map.is_fog_dirty = true;
}


FogType map_get_fog(const map_t& map, uint8_t player_id, xy cell) {
    return map.player_fog[player_id][cell.x + (map.width * cell.y)];
}

bool map_is_cell_rect_revealed(const map_t& map, uint8_t player_id, rect_t rect) {
    for (int x = rect.position.x; x < rect.position.x + rect.size.x; x++) {
        for (int y = rect.position.y; y < rect.position.y + rect.size.y; y++) {
            if (map_get_fog(map, player_id, xy(x, y)) == FOG_REVEALED) {
                return true;
            }
        }
    }

    return false;
}

void map_fog_reveal_at_cell(map_t& map, uint8_t player_id, xy cell, xy size, int sight) {
    int xmin = std::max(0, cell.x - sight);
    int xmax = std::min((int)map.width, cell.x + size.x + sight);
    int ymin = std::max(0, cell.y - sight);
    int ymax = std::min((int)map.height, cell.y + size.y + sight);
    for (int x = xmin; x < xmax; x++) {
        for (int y = ymin; y < ymax; y++) {
            bool is_x_edge = x == cell.x - sight || x == (cell.x + size.x + sight) - 1;
            bool is_y_edge = y == cell.y - sight || y == (cell.y + size.y + sight) - 1;
            if (is_x_edge && is_y_edge) {
                continue;
            }

            map.player_fog[player_id][x + (map.width * y)] = FOG_REVEALED;
        }
    }
}

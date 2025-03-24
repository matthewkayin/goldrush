#include "map.h"

#include "core/logger.h"
#include "lcg.h"

SpriteName map_wall_autotile_lookup(uint32_t neighbors);

void map_init(Map& map, Noise& noise) {
    map.width = noise.width;
    map.height = noise.height;
    log_info("Generating map. Size: %ux%u", map.width, map.height);

    map.cells = std::vector<EntityId>(map.width * map.height, CELL_EMPTY);
    map.tiles = std::vector<Tile>(map.width * map.height, (Tile) {
        .sprite = SPRITE_TILE_SAND,
        .autotile_index = 0,
        .elevation = 0
    });

    // Clear out water that is too close to walls
    const int WATER_WALL_DIST = 4;
    for (int x = 0; x < noise.width; x++) {
        for (int y = 0; y < noise.height; y++) {
            if (noise.map[x + (y * noise.width)] != -1) {
                continue;
            }

            bool is_too_close_to_wall = false;
            for (int nx = x - WATER_WALL_DIST; nx < x + WATER_WALL_DIST + 1; nx++) {
                for (int ny = y - WATER_WALL_DIST; ny < y + WATER_WALL_DIST + 1; ny++) {
                    if (!map_is_cell_in_bounds(map, ivec2(nx, ny))) {
                        continue;
                    }
                    if (noise.map[nx + (ny * noise.width)] > 0 && ivec2::manhattan_distance(ivec2(x, y), ivec2(nx, ny)) <= WATER_WALL_DIST) {
                        is_too_close_to_wall = true;
                    }
                }
                if (is_too_close_to_wall) {
                    break;
                }
            }
            if (is_too_close_to_wall) {
                noise.map[x + (y * noise.width)] = 0;
            }
        }
    }

    // Bake map tiles
    std::vector<ivec2> artifacts;
    do {
        std::fill(map.tiles.begin(), map.tiles.end(), (Tile) {
            .sprite = SPRITE_TILE_SAND,
            .autotile_index = 0,
            .elevation = 0
        });
        for (ivec2 artifact : artifacts) {
            noise.map[artifact.x + (artifact.y * map.width)]--;
        }
        artifacts.clear();

        log_trace("Baking map tiles...");
        for (int y = 0; y < map.height; y++) {
            for (int x = 0; x < map.width; x++) {
                int index = x + (y * map.width);
                if (noise.map[index] >= 0) {
                    map.tiles[index].elevation = noise.map[index];
                    // First check if we need to place a regular wall here
                    uint32_t neighbors = 0;
                    if (noise.map[index] > 0) {
                        int8_t tile_elevation = noise.map[index];
                        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                            ivec2 neighbor_cell = ivec2(x, y) + DIRECTION_IVEC2[direction];
                            if (!map_is_cell_in_bounds(map, neighbor_cell)) {
                                continue;
                            }
                            int neighbor_index = neighbor_cell.x + (neighbor_cell.y * map.width);
                            int8_t neighbor_elevation = noise.map[neighbor_index];
                            if (tile_elevation > neighbor_elevation) {
                                neighbors += DIRECTION_MASK[direction];
                            }
                        }
                    }

                    // Regular sand tile 
                    if (neighbors == 0) {
                        int new_index = lcg_rand() % 7;
                        if (new_index < 4 && index % 3 == 0) {
                            map.tiles[index].sprite = new_index == 1 ? SPRITE_TILE_SAND3 : SPRITE_TILE_SAND2;
                        } else {
                            map.tiles[index].sprite = SPRITE_TILE_SAND;
                        }
                    // Wall tile 
                    } else {
                        map.tiles[index].sprite = map_wall_autotile_lookup(neighbors);
                        if (map.tiles[index].sprite == SPRITE_TILE_NULL) {
                            artifacts.push_back(ivec2(x, y));
                        }
                    }
                } else if (noise.map[index] == -1) {
                    uint32_t neighbors = 0;
                    // Check adjacent neighbors
                    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                        ivec2 neighbor_cell = ivec2(x, y) + DIRECTION_IVEC2[direction];
                        if (!map_is_cell_in_bounds(map, neighbor_cell) || 
                                noise.map[index] == noise.map[neighbor_cell.x + (neighbor_cell.y * map.width)]) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }
                    // Check diagonal neighbors
                    for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                        ivec2 neighbor_cell = ivec2(x, y) + DIRECTION_IVEC2[direction];
                        int prev_direction = direction - 1;
                        int next_direction = (direction + 1) % DIRECTION_COUNT;
                        if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                                (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                            continue;
                        }
                        if (!map_is_cell_in_bounds(map, neighbor_cell) || 
                                noise.map[index] == noise.map[neighbor_cell.x + (neighbor_cell.y * map.width)]) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }
                    // Set the map tile based on the neighbors
                    map.tiles[index] = (Tile) {
                        .sprite = SPRITE_TILE_WATER,
                        .autotile_index = (uint8_t)map_neighbors_to_autotile_index(neighbors),
                        .elevation = 0
                    };
                // End else if tile is water
                } 
            } // end for each x
        } // end for each y

        log_trace("Artifacts count: %u", artifacts.size());
    } while (!artifacts.empty());

    // Place front walls
    for (int index = 0; index < map.width * map.height; index++) {
        int previous = index - map.width;
        if (previous < 0) {
            continue;
        }
        if (map.tiles[previous].sprite == SPRITE_TILE_WALL_SOUTH_EDGE) {
            map.tiles[index].sprite = SPRITE_TILE_WALL_SOUTH_FRONT;
        } else if (map.tiles[previous].sprite == SPRITE_TILE_WALL_SW_CORNER) {
            map.tiles[index].sprite = SPRITE_TILE_WALL_SW_FRONT;
        } else if (map.tiles[previous].sprite == SPRITE_TILE_WALL_SE_CORNER) {
            map.tiles[index].sprite = SPRITE_TILE_WALL_SE_FRONT;
        }
    }

    // Generate ramps
    std::vector<ivec2> stair_cells;
    for (int pass = 0; pass < 2; pass++) {
        for (int x = 0; x < map.width; x++) {
            for (int y = 0; y < map.height; y++) {
                Tile tile = map.tiles[x + (y * map.width)];
                // Only generate ramps on straight edged walls
                if (!(tile.sprite == SPRITE_TILE_WALL_SOUTH_EDGE ||
                    tile.sprite == SPRITE_TILE_WALL_NORTH_EDGE ||
                    tile.sprite == SPRITE_TILE_WALL_WEST_EDGE ||
                    tile.sprite == SPRITE_TILE_WALL_EAST_EDGE)) {
                    continue;
                }
                // Determine whether this is a horizontal or vertical stair
                ivec2 step_direction = (tile.sprite == SPRITE_TILE_WALL_SOUTH_EDGE ||
                                    tile.sprite == SPRITE_TILE_WALL_NORTH_EDGE)
                                        ? ivec2(-1, 0)
                                        : ivec2(0, -1);
                ivec2 stair_min = ivec2(x, y);
                while (map_is_cell_in_bounds(map, stair_min) && map.tiles[stair_min.x + (stair_min.y * map.width)].sprite == tile.sprite) {
                    stair_min += step_direction;
                    if (tile.sprite == SPRITE_TILE_WALL_EAST_EDGE || tile.sprite == SPRITE_TILE_WALL_WEST_EDGE) {
                        ivec2 adjacent_cell = stair_min + ivec2(tile.sprite == SPRITE_TILE_WALL_EAST_EDGE ? 1 : -1, 0);
                        if (!map_is_cell_in_bounds(map, adjacent_cell) || map_get_cell(map, adjacent_cell) != CELL_EMPTY) {
                            break;
                        }
                    }
                }
                ivec2 stair_max = ivec2(x, y);
                stair_min -= step_direction;
                step_direction = ivec2(step_direction.x * -1, step_direction.y * -1);
                while (map_is_cell_in_bounds(map, stair_max) && map.tiles[stair_max.x + (stair_max.y * map.width)].sprite == tile.sprite) {
                    stair_max += step_direction;
                    if (tile.sprite == SPRITE_TILE_WALL_EAST_EDGE || tile.sprite == SPRITE_TILE_WALL_WEST_EDGE) {
                        ivec2 adjacent_cell = stair_min + ivec2(tile.sprite == SPRITE_TILE_WALL_EAST_EDGE ? 1 : -1, 0);
                        if (!map_is_cell_in_bounds(map, adjacent_cell) || map_get_cell(map, adjacent_cell) != CELL_EMPTY) {
                            break;
                        }
                    }
                }
                stair_max -= step_direction;

                int stair_length = ivec2::manhattan_distance(stair_max, stair_min);
                int min_stair_length = pass == 0 ? 3 : 2;
                if (stair_length < min_stair_length) {
                    continue;
                }

                bool chop_from_max = true;
                while (stair_length > 4)  {
                    if (chop_from_max) {
                        stair_max -= step_direction;
                    } else {
                        stair_min += step_direction;
                    }
                    chop_from_max = !chop_from_max;
                    stair_length--;
                }

                bool is_stair_too_close_to_other_stairs = false;
                for (ivec2 stair_cell : stair_cells) {
                    int dist = std::min(ivec2::manhattan_distance(stair_cell, stair_min), ivec2::manhattan_distance(stair_cell, stair_max));
                    if (dist < 16) {
                        is_stair_too_close_to_other_stairs = true;
                        break;
                    }
                }
                if (is_stair_too_close_to_other_stairs) {
                    continue;
                }

                stair_cells.push_back(stair_min);
                stair_cells.push_back(stair_max);
                for (ivec2 cell = stair_min; cell != stair_max + step_direction; cell += step_direction) {
                    SpriteName stair_tile = SPRITE_TILE_NULL;
                    if (tile.sprite == SPRITE_TILE_WALL_NORTH_EDGE) {
                        if (cell == stair_min) {
                            stair_tile = SPRITE_TILE_WALL_NORTH_STAIR_LEFT;
                        } else if (cell == stair_max) {
                            stair_tile = SPRITE_TILE_WALL_NORTH_STAIR_RIGHT;
                        } else {
                            stair_tile = SPRITE_TILE_WALL_NORTH_STAIR_CENTER;
                        }
                    } else if (tile.sprite == SPRITE_TILE_WALL_EAST_EDGE) {
                        if (cell == stair_min) {
                            stair_tile = SPRITE_TILE_WALL_EAST_STAIR_TOP;
                        } else if (cell == stair_max) {
                            stair_tile = SPRITE_TILE_WALL_EAST_STAIR_BOTTOM;
                        } else {
                            stair_tile = SPRITE_TILE_WALL_EAST_STAIR_CENTER;
                        }
                    } else if (tile.sprite == SPRITE_TILE_WALL_WEST_EDGE) {
                        if (cell == stair_min) {
                            stair_tile = SPRITE_TILE_WALL_WEST_STAIR_TOP;
                        } else if (cell == stair_max) {
                            stair_tile = SPRITE_TILE_WALL_WEST_STAIR_BOTTOM;
                        } else {
                            stair_tile = SPRITE_TILE_WALL_WEST_STAIR_CENTER;
                        }
                    } else if (tile.sprite == SPRITE_TILE_WALL_SOUTH_EDGE) {
                        if (cell == stair_min) {
                            stair_tile = SPRITE_TILE_WALL_SOUTH_STAIR_LEFT;
                        } else if (cell == stair_max) {
                            stair_tile = SPRITE_TILE_WALL_SOUTH_STAIR_RIGHT;
                        } else {
                            stair_tile = SPRITE_TILE_WALL_SOUTH_STAIR_CENTER;
                        }
                    }

                    map.tiles[cell.x + (cell.y * map.width)].sprite = stair_tile;
                    SpriteName south_front_tile = SPRITE_TILE_NULL;
                    if (stair_tile == SPRITE_TILE_WALL_SOUTH_STAIR_LEFT) {
                        south_front_tile = SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_LEFT;
                    } else if (stair_tile == SPRITE_TILE_WALL_SOUTH_STAIR_RIGHT) {
                        south_front_tile = SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_RIGHT;
                    } else if (stair_tile == SPRITE_TILE_WALL_SOUTH_STAIR_CENTER) {
                        south_front_tile = SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_CENTER;
                    }
                    if (south_front_tile != SPRITE_TILE_NULL) {
                        map.tiles[cell.x + ((cell.y + 1) * map.width)].sprite = south_front_tile;
                    }
                } // End for cell in ramp
            } // End for each y
        } // End for each x
    } // End for each pass
    // End create ramps

    // Block all walls and water
    for (int index = 0; index < map.width * map.height; index++) {
        if (!(map.tiles[index].sprite == SPRITE_TILE_SAND ||
                map.tiles[index].sprite == SPRITE_TILE_SAND2 ||
                map.tiles[index].sprite == SPRITE_TILE_SAND3 ||
                map_is_tile_ramp(map, ivec2(index % map.width, index / map.width)))) {
            map.cells[index] = CELL_BLOCKED;
        }
    }
}

SpriteName map_wall_autotile_lookup(uint32_t neighbors) {
    switch (neighbors) {
        case 1:
        case 3:
        case 129:
        case 131:
            return SPRITE_TILE_WALL_NORTH_EDGE;
        case 4:
        case 6:
        case 12:
        case 14:
            return SPRITE_TILE_WALL_EAST_EDGE;
        case 16:
        case 24:
        case 48:
        case 56:
            return SPRITE_TILE_WALL_SOUTH_EDGE;
        case 64:
        case 96:
        case 192:
        case 224:
            return SPRITE_TILE_WALL_WEST_EDGE;
        case 7:
        case 15:
        case 135:
        case 143:
        case 66:
            return SPRITE_TILE_WALL_NE_CORNER;
        case 193:
        case 195:
        case 225:
        case 227:
        case 132:
            return SPRITE_TILE_WALL_NW_CORNER;
        case 112:
        case 120:
        case 240:
        case 248:
        case 72:
            return SPRITE_TILE_WALL_SW_CORNER;
        case 28:
        case 30:
        case 60:
        case 62:
        case 36:
            return SPRITE_TILE_WALL_SE_CORNER;
        case 2:
            return SPRITE_TILE_WALL_NE_INNER_CORNER;
        case 8:
            return SPRITE_TILE_WALL_SE_INNER_CORNER;
        case 32:
            return SPRITE_TILE_WALL_SW_INNER_CORNER;
        case 128:
            return SPRITE_TILE_WALL_NW_INNER_CORNER;
        default:
            return SPRITE_TILE_NULL;
    }
}

uint32_t map_neighbors_to_autotile_index(uint32_t neighbors) {
    static uint32_t autotile_index[256];
    static bool initialized = false;
    if (!initialized) {
        uint32_t unique_index = 0;
        for (uint32_t neighbors = 0; neighbors < 256; neighbors++) {
            bool is_unique = true;
            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                if (direction % 2 == 1 && (DIRECTION_MASK[direction] & neighbors) == DIRECTION_MASK[direction]) {
                    int prev_direction = direction - 1;
                    int next_direction = (direction + 1) % DIRECTION_COUNT;
                    if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                        (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                        is_unique = false;
                        break;
                    }
                }
            }
            if (!is_unique) {
                continue;
            }
            autotile_index[neighbors] = unique_index;
            unique_index++;
        }

        initialized = true;
    }

    return autotile_index[neighbors];
}

bool map_is_cell_in_bounds(const Map& map, ivec2 cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= map.width || cell.y >= map.height);
}

EntityId map_get_cell(const Map& map, ivec2 cell) {
    return map.cells[cell.x + (cell.y * map.width)];
}

bool map_is_tile_ramp(const Map& map, ivec2 cell) {
    uint8_t tile_sprite = map.tiles[cell.x + (cell.y * map.width)].sprite;
    return tile_sprite >= SPRITE_TILE_WALL_SOUTH_STAIR_LEFT && tile_sprite <= SPRITE_TILE_WALL_WEST_STAIR_BOTTOM;
}
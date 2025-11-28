#include "map.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "render/render.h"
#include "lcg.h"
#include <tracy/tracy/Tracy.hpp>
#include <unordered_map>
#include <algorithm>

static const int MAP_PLAYER_SPAWN_SIZE = 13;
static const int MAP_PLAYER_SPAWN_MARGIN = 13;
static const int MAP_STAIR_SPACING = 16;
static const int PATHING_REGION_UNASSIGNED = -1;
static const int MAP_ISLAND_UNASSIGNED = -1;
static const uint32_t PATHFIND_ITERATION_MAX = 1999;

struct PoissonDiskParams {
    std::vector<int> avoid_values;
    int disk_radius;
    bool allow_unreachable_cells;
    ivec2 margin;
};

SpriteName map_wall_autotile_lookup(uint32_t neighbors);
bool map_is_poisson_point_valid(const Map& map, const PoissonDiskParams& params, ivec2 point);
std::vector<ivec2> map_poisson_disk(const Map& map, int* lcg_seed, PoissonDiskParams& params);

void map_init(Map& map, Noise& noise, int* lcg_seed, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells) {
    map.width = noise.width;
    map.height = noise.height;
    log_info("Generating map. Size: %ux%u", map.width, map.height);

    for (int layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        map.cells[layer] = std::vector<Cell>(map.width * map.height, (Cell) {
            .type = CELL_EMPTY,
            .id = ID_NULL
        });
    }
    map.tiles = std::vector<Tile>(map.width * map.height, (Tile) {
        .sprite = SPRITE_TILE_SAND1,
        .frame = ivec2(0, 0),
        .elevation = 0
    });

    // Clear out water that is too close to walls
    const int WATER_WALL_DIST = 4;
    for (int x = 0; x < noise.width; x++) {
        for (int y = 0; y < noise.height; y++) {
            if (noise.map[x + (y * noise.width)] != NOISE_VALUE_WATER) {
                continue;
            }

            bool is_too_close_to_wall = false;
            for (int nx = x - WATER_WALL_DIST; nx < x + WATER_WALL_DIST + 1; nx++) {
                for (int ny = y - WATER_WALL_DIST; ny < y + WATER_WALL_DIST + 1; ny++) {
                    if (!map_is_cell_in_bounds(map, ivec2(nx, ny))) {
                        continue;
                    }
                    if (noise.map[nx + (ny * noise.width)] == NOISE_VALUE_HIGHGROUND && ivec2::manhattan_distance(ivec2(x, y), ivec2(nx, ny)) <= WATER_WALL_DIST) {
                        is_too_close_to_wall = true;
                    }
                }
                if (is_too_close_to_wall) {
                    break;
                }
            }
            if (is_too_close_to_wall) {
                noise.map[x + (y * noise.width)] = NOISE_VALUE_LOWGROUND;
            }
        }
    }

    // Widen narrow gaps
    for (int y = 0; y < noise.height; y++) {
        for (int x = 0; x < noise.width; x++) {
            if (noise.map[x + (y * noise.width)] == NOISE_VALUE_WATER) {
                continue;
            }

            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                ivec2 wall = ivec2(x, y) + DIRECTION_IVEC2[direction];
                if (!map_is_cell_in_bounds(map, wall) || noise.map[wall.x + (wall.y * noise.width)] > noise.map[x + (y * noise.width)]) {
                    const int STEP_COUNT = direction % 2 == 0 ? 3 : 2;
                    for (int step = 0; step < STEP_COUNT; step++) {
                        ivec2 opposite = ivec2(x, y) - (DIRECTION_IVEC2[direction] * (step + 1));
                        if (map_is_cell_in_bounds(map, opposite) && noise.map[opposite.x + (opposite.y * noise.width)] > noise.map[x + (y * noise.width)]) {
                            noise.map[opposite.x + (opposite.y * noise.width)] = noise.map[x + (y * noise.width)];
                        }
                    }
                }
            }
        }
    }

    // Remove small lowground areas
    {
        std::vector<int> map_tile_islands(noise.width * noise.height, MAP_ISLAND_UNASSIGNED);
        std::vector<int> island_size;

        while (true) {
            // Set index equal to the first index of the first unassigned tile in the array
            int index;
            for (index = 0; index < noise.width * noise.height; index++) {
                if (map_tile_islands[index] == -1) {
                    break;
                }
            }
            if (index == noise.width * noise.height) {
                // Island mapping is complete
                break;
            }

            // Determine the next island index
            int island_index = island_size.size();
            island_size.push_back(0);
            int8_t island_noise_value = noise.map[index];

            // Flood fill this island index
            std::vector<ivec2> frontier;
            frontier.push_back(ivec2(index % noise.width, index / noise.height));

            while (!frontier.empty()) {
                ivec2 next = frontier.back();
                frontier.pop_back();

                if (next.x < 0 || next.y < 0 || next.x >= noise.width || next.y >= noise.height) {
                    continue;
                }
                if (noise.map[next.x + (next.y * noise.width)] != island_noise_value) {
                    continue;
                }

                // skip this because we've already explored it
                if (map_tile_islands[next.x + (next.y * noise.width)] != MAP_ISLAND_UNASSIGNED) {
                    continue;
                }

                map_tile_islands[next.x + (next.y * map.width)] = island_index;
                island_size[island_index]++;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    frontier.push_back(next + DIRECTION_IVEC2[direction]);
                }
            }
        } 
        // End assign noise tiles to islands

        for (int island_index = 0; island_index < (int)island_size.size(); island_index++) {
            // Big islands are fine as they are
            if (island_size[island_index] > 15) {
                continue;
            }

            // Find the first index which belongs to this island
            int first_index;
            for (first_index = 0; first_index < noise.width * noise.height; first_index++) {
                if (map_tile_islands[first_index] == island_index) {
                    break;
                }
            }
            ivec2 first_index_coord = ivec2(first_index % noise.width, first_index / noise.width);

            // And use that index to determine the island's noise value
            // If the noise value is non-zero, then skip it
            if (noise.map[first_index] != NOISE_VALUE_LOWGROUND) {
                continue;
            }

            // We now have a small 0-elevation section of the noise map that we would like to clean up
            // So replace the 0-elevation section with a different noise level
            for (int index = 0; index < noise.width * noise.height; index++) {
                if (map_tile_islands[index] != island_index) {
                    continue;
                }

                noise.map[index] = NOISE_VALUE_HIGHGROUND;
            }
        }
    }
    
    // Remove lowground nooks
    for (int y = 0; y < noise.height; y++) {
        for (int x = 0; x < noise.width; x++) {
            if (noise.map[x + (y * noise.width)] != NOISE_VALUE_LOWGROUND) {
                continue;
            }

            int highground_neighbor_count = 0;
            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                ivec2 neighbor = ivec2(x, y) + DIRECTION_IVEC2[direction];
                if (neighbor.x < 0 || neighbor.y < 0 || neighbor.x >= noise.width || neighbor.y >= noise.height) {
                    continue;
                }
                if (noise.map[neighbor.x + (neighbor.y * noise.width)] == NOISE_VALUE_HIGHGROUND) {
                    highground_neighbor_count++;
                }
            }
            if (highground_neighbor_count > 2) {
                noise.map[x + (y * noise.width)] = NOISE_VALUE_HIGHGROUND;
            }
        }
    }

    // Remove highground areas that are too small for wagons
    {
        // First mark all of the highground floor (i.e. non-ground) tiles
        std::vector<bool> is_highground_floor(noise.width * noise.height, false);
        for (int y = 0; y < noise.height; y++) {
            for (int x = 0; x < noise.width; x++) {
                if (noise.map[x + (y * noise.width)] != NOISE_VALUE_HIGHGROUND) {
                    continue;
                }

                is_highground_floor[x + (y * noise.width)] = true;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(map, adjacent) || noise.map[adjacent.x + (adjacent.y * noise.width)] == NOISE_VALUE_HIGHGROUND) {
                        continue;
                    }
                    is_highground_floor[x + (y * noise.width)] = false;
                }
            }
        }

        // Then mark all of the highground floors that can be occupied by a 2x2 unit
        std::vector<bool> is_2x2_highground_floor(noise.width * noise.height, false);
        for (int y = 0; y < noise.height; y++) {
            for (int x = 0; x < noise.width; x++) {
                if (!is_highground_floor[x + (y * noise.width)]) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    if (map_is_cell_in_bounds(map, adjacent) && is_highground_floor[adjacent.x + (adjacent.y * noise.width)]) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    int prev_direction = direction - 1;
                    int next_direction = direction + 1 == DIRECTION_COUNT ? 0 : direction + 1;
                    uint32_t adjacent_neighbors = DIRECTION_MASK[prev_direction] | DIRECTION_MASK[next_direction];
                    if (map_is_cell_in_bounds(map, adjacent) && 
                            is_highground_floor[adjacent.x + (adjacent.y * noise.width)] &&
                            (neighbors & adjacent_neighbors) == adjacent_neighbors) {
                        is_2x2_highground_floor[x + (y * noise.width)] = true;
                    }
                }
            }
        }

        // Finally, remove any walls that do not have a 2x2 occupiable floor surrounding them
        for (int y = 0; y < noise.height; y++) {
            for (int x = 0; x < noise.width; x++) {
                if (noise.map[x + (y * map.width)] != NOISE_VALUE_HIGHGROUND) {
                    continue;
                }

                bool has_adjacent_2x2_floor = false;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    if (map_is_cell_in_bounds(map, adjacent) && is_2x2_highground_floor[adjacent.x + (adjacent.y * noise.width)]) {
                        has_adjacent_2x2_floor = true;
                    }
                }

                if (!has_adjacent_2x2_floor) {
                    noise.map[x + (y * map.width)] = NOISE_VALUE_LOWGROUND;
                }
            }
        }
    }

    /*
    // Remove elevation artifacts
    uint32_t elevation_artifact_count;
    const int ELEVATION_NEAR_DIST = 4;
    do {
        elevation_artifact_count = 0;
        for (int x = 0; x < noise.width; x++) {
            for (int y = 0; y < noise.height; y++) {
                if (noise.map[x + (y * noise.width)] != 2) {
                    continue;
                }

                bool is_highground_too_close = false;
                for (int nx = x - ELEVATION_NEAR_DIST; nx < x + ELEVATION_NEAR_DIST + 1; nx++) {
                    for (int ny = y - ELEVATION_NEAR_DIST; ny < y + ELEVATION_NEAR_DIST + 1; ny++) {
                        if (!map_is_cell_in_bounds(map, ivec2(nx, ny))) {
                            continue;
                        }
                        if (noise.map[nx + (ny * noise.width)] != 1) {
                            is_highground_too_close = true;
                            break;
                        }
                    }
                    if (is_highground_too_close) {
                        break;
                    }
                }
                if (is_highground_too_close) {
                    noise.map[x + (y * noise.width)] = 1;
                    elevation_artifact_count++;
                }
            }
        }
    } while (elevation_artifact_count != 0);
    */

    // Bake map tiles
    std::vector<ivec2> artifacts;
    do {
        std::fill(map.tiles.begin(), map.tiles.end(), (Tile) {
            .sprite = SPRITE_TILE_SAND1,
            .frame = ivec2(0, 0),
            .elevation = 0
        });
        for (ivec2 artifact : artifacts) {
            noise.map[artifact.x + (artifact.y * map.width)]--;
        }
        artifacts.clear();

        log_debug("Baking map tiles...");
        for (int y = 0; y < map.height; y++) {
            for (int x = 0; x < map.width; x++) {
                int index = x + (y * map.width);
                if (noise.map[index] >= 0) {
                    map.tiles[index].elevation = (uint32_t)(noise.map[index] == NOISE_VALUE_HIGHGROUND);
                    // First check if we need to place a regular wall here
                    uint32_t neighbors = 0;
                    if (noise.map[index] == NOISE_VALUE_HIGHGROUND) {
                        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                            ivec2 neighbor_cell = ivec2(x, y) + DIRECTION_IVEC2[direction];
                            if (!map_is_cell_in_bounds(map, neighbor_cell)) {
                                continue;
                            }
                            int neighbor_index = neighbor_cell.x + (neighbor_cell.y * map.width);
                            uint32_t neighbor_elevation = (uint32_t)(noise.map[neighbor_index] == NOISE_VALUE_HIGHGROUND);
                            if (map.tiles[index].elevation > neighbor_elevation) {
                                neighbors += DIRECTION_MASK[direction];
                            }
                        }
                    }

                    // Regular sand tile 
                    if (neighbors == 0) {
                        int new_index = lcg_rand(lcg_seed) % 7;
                        if (new_index < 4 && index % 3 == 0) {
                            map.tiles[index].sprite = new_index == 1 ? SPRITE_TILE_SAND3 : SPRITE_TILE_SAND2;
                        } else {
                            map.tiles[index].sprite = SPRITE_TILE_SAND1;
                        }
                    // Wall tile 
                    } else {
                        map.tiles[index].sprite = map_wall_autotile_lookup(neighbors);
                        if (map.tiles[index].sprite == SPRITE_TILE_NULL) {
                            artifacts.push_back(ivec2(x, y));
                        }
                    }
                } else if (noise.map[index] == NOISE_VALUE_WATER) {
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
                    uint32_t autotile_index = map_neighbors_to_autotile_index(neighbors);
                    map.tiles[index] = (Tile) {
                        .sprite = SPRITE_TILE_WATER,
                        .frame = ivec2(autotile_index % AUTOTILE_HFRAMES, autotile_index / AUTOTILE_HFRAMES),
                        .elevation = 0
                    };
                // End else if tile is water
                } 
            } // end for each x
        } // end for each y

        log_debug("Artifacts count: %u", artifacts.size());
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
            for (int y = 0; y < (int)map.height; y++) {
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
                        if (!map_is_cell_in_bounds(map, adjacent_cell) || map_get_cell(map, CELL_LAYER_GROUND, adjacent_cell).type != CELL_EMPTY) {
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
                        if (!map_is_cell_in_bounds(map, adjacent_cell) || map_get_cell(map, CELL_LAYER_GROUND, adjacent_cell).type != CELL_EMPTY) {
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

                Direction stair_direction = DIRECTION_COUNT;
                if (tile.sprite == SPRITE_TILE_WALL_NORTH_EDGE) {
                    stair_direction = DIRECTION_NORTH;
                } else if (tile.sprite == SPRITE_TILE_WALL_EAST_EDGE) {
                    stair_direction = DIRECTION_EAST;
                } else if (tile.sprite == SPRITE_TILE_WALL_WEST_EDGE) {
                    stair_direction = DIRECTION_WEST;
                } else if (tile.sprite == SPRITE_TILE_WALL_SOUTH_EDGE) {
                    stair_direction = DIRECTION_SOUTH;
                }
                GOLD_ASSERT(stair_direction != DIRECTION_COUNT);

                int stair_mouth_cell_distance = stair_direction == DIRECTION_SOUTH ? 4 : 3;
                ivec2 stair_mouth_cell = stair_min + (DIRECTION_IVEC2[stair_direction] * stair_mouth_cell_distance);
                if (!map_is_cell_in_bounds(map, stair_mouth_cell)) {
                    continue;
                }

                bool is_stair_too_close_to_other_stairs = false;
                for (ivec2 stair_cell : stair_cells) {
                    int dist = std::min(ivec2::manhattan_distance(stair_cell, stair_min), ivec2::manhattan_distance(stair_cell, stair_max));
                    if (dist < MAP_STAIR_SPACING) {
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

                    ivec2 stair_front_cell = cell + DIRECTION_IVEC2[stair_direction];
                    if (stair_direction == DIRECTION_SOUTH) {
                        stair_front_cell += DIRECTION_IVEC2[stair_direction];
                    }
                    if (!map_is_cell_in_bounds(map, stair_front_cell)) {
                        continue;
                    }
                    SpriteName stair_front_cell_tile = map.tiles[stair_front_cell.x + (stair_front_cell.y * map.width)].sprite;
                    if (stair_front_cell_tile < SPRITE_TILE_SAND1 || stair_front_cell_tile > SPRITE_TILE_SAND3) {
                        continue;
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
    log_debug("Generated ramps.");

    // Block all walls and water
    for (int index = 0; index < map.width * map.height; index++) {
        if (!(map.tiles[index].sprite == SPRITE_TILE_SAND1 ||
                map.tiles[index].sprite == SPRITE_TILE_SAND2 ||
                map.tiles[index].sprite == SPRITE_TILE_SAND3 ||
                map_is_tile_ramp(map, ivec2(index % map.width, index / map.width)))) {
            map.cells[CELL_LAYER_GROUND][index].type = CELL_BLOCKED;
        }
    }

    // Calculate unreachable cells
    map_calculate_unreachable_cells(map);

    // Determine player spawns
    {
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            player_spawns.push_back(ivec2(-1, -1));
        }
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            // Chooses diagonal directions clockwise beginning with NE
            int spawn_direction = 1 + (player_id * 2);

            ivec2 start = ivec2(map.width / 2, map.height / 2) + ivec2(
                            DIRECTION_IVEC2[spawn_direction].x * ((map.width / 2) - MAP_PLAYER_SPAWN_MARGIN),
                            DIRECTION_IVEC2[spawn_direction].y * ((map.height / 2) - MAP_PLAYER_SPAWN_MARGIN));
            std::vector<ivec2> frontier;
            std::unordered_map<uint32_t, uint32_t> explored;
            frontier.push_back(start);
            ivec2 spawn_point = ivec2(-1, -1);

            while (!frontier.empty() && spawn_point.x == -1) {
                uint32_t next_index = 0;
                for (uint32_t index = 1; index < frontier.size(); index++) {
                    if (ivec2::manhattan_distance(frontier[index], start) < ivec2::manhattan_distance(frontier[next_index], start)) {
                        next_index = index;
                    }
                }
                ivec2 next = frontier[next_index];
                frontier.erase(frontier.begin() + next_index);

                if (map_is_cell_rect_same_elevation(map, next, MAP_PLAYER_SPAWN_SIZE) && 
                        !map_is_cell_rect_occupied(map, CELL_LAYER_GROUND, next, MAP_PLAYER_SPAWN_SIZE)) {
                    ivec2 candidate = next;
                    if (spawn_direction == DIRECTION_NORTHEAST || spawn_direction == DIRECTION_SOUTHEAST) {
                        candidate.x += 5;
                    } 
                    if (spawn_direction == DIRECTION_SOUTHEAST || spawn_direction == DIRECTION_SOUTHWEST) {
                        candidate.y += 5;
                    }
                    
                    // last check, check that candidate is not too close to stairs or walls
                    bool is_candidate_valid = true;
                    const int stair_radius = 2;
                    for (int x = candidate.x - stair_radius; x < candidate.x + 3 + stair_radius; x++) {
                        for (int y = candidate.y - stair_radius; y < candidate.y + 3 + stair_radius; y++) {
                            if (!map_is_cell_in_bounds(map, ivec2(x, y))) {
                                continue;
                            }
                            if (map_is_tile_ramp(map, ivec2(x, y)) || map_get_cell(map, CELL_LAYER_GROUND, ivec2(x, y)).type != CELL_EMPTY) {
                                is_candidate_valid = false;
                                break;
                            }
                        }
                        if (!is_candidate_valid) {
                            break;
                        }
                    }
                    if (is_candidate_valid) {
                        spawn_point = candidate;
                        break;
                    }
                }

                explored[next.x + (next.y * map.width)] = 1;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    ivec2 child = next + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_rect_in_bounds(map, child, MAP_PLAYER_SPAWN_SIZE)) {
                        continue;
                    }
                    if (explored.find(child.x + (child.y * map.width)) != explored.end()) {
                        continue;
                    }
                    bool is_in_frontier = false;
                    for (ivec2 cell : frontier) {
                        if (child == cell) {
                            is_in_frontier = true;
                        }
                    }
                    if (is_in_frontier) {
                        continue;
                    }
                    frontier.push_back(child);
                }
            }

            if (spawn_point.x == -1) {
                log_error("Unable to find valid spawn point for spawn direction %i", spawn_direction);
                spawn_point = start;
            }
            player_spawns[player_id] = spawn_point;
        }
    }
    // End determine player spawns
    log_debug("Determined player spawns.");

    // Generate gold mines
    PoissonDiskParams params = (PoissonDiskParams) {
        .avoid_values = std::vector<int>(map.width * map.height, 0),
        .disk_radius = 48,
        .allow_unreachable_cells = false,
        .margin = ivec2(5, 5)
    };
    {
        // Place a gold mine on each player's spawn
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            GOLD_ASSERT(player_spawns[player_id].x != -1);

            ivec2 mine_cell = player_spawns[player_id];
            goldmine_cells.push_back(mine_cell);
            params.avoid_values[mine_cell.x + (mine_cell.y * map.width)] = params.disk_radius;
        }

        // Generate the avoid values
        for (int index = 0; index < map.width * map.height; index++) {
            if (map.cells[CELL_LAYER_GROUND][index].type == CELL_BLOCKED || map_is_tile_ramp(map, ivec2(index % map.width, index / map.width))) {
                params.avoid_values[index] = 6;
            }
        }

        std::vector<ivec2> goldmine_results = map_poisson_disk(map, lcg_seed, params);
        goldmine_cells.insert(goldmine_cells.end(), goldmine_results.begin(), goldmine_results.end());
        for (ivec2 goldmine_cell : goldmine_cells) {
            params.avoid_values[goldmine_cell.x + (goldmine_cell.y * map.width)] = 4;
        }
    }
    // End generate gold mines
    log_debug("Generated gold mines.");

    // Generate decorations
    {
        // Place avoid values on each town hall cell
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            ivec2 town_hall_cell = map_get_player_town_hall_cell(map, player_spawns[player_id]);
            for (int y = town_hall_cell.y; y < town_hall_cell.y + 4; y++) {
                for (int x = town_hall_cell.x; x < town_hall_cell.x + 4; x++) {
                    params.avoid_values[x + (y * map.width)] = 4;
                }
            }
        }

        params.disk_radius = 16;
        params.allow_unreachable_cells = true;
        params.margin = ivec2(0, 0);
        std::vector<ivec2> decoration_cells = map_poisson_disk(map, lcg_seed, params);
        for (ivec2 cell : decoration_cells) {
            map.cells[CELL_LAYER_GROUND][cell.x + (cell.y * map.width)].type = (CellType)(CELL_DECORATION_1 + (lcg_rand(lcg_seed) % 5));
        }
    }

    // Calculate pathfinding regions
    map.pathing_region_count = 0;
    map.pathing_regions = std::vector(noise.width * noise.height, PATHING_REGION_UNASSIGNED);
    // Flood fill unassigned regions
    for (int y = 0; y < map.height; y++) {
        for (int x = 0; x < map.width; x++) {
            // Filter down to unblocked, unassigned cells
            Cell map_cell = map_get_cell(map, CELL_LAYER_GROUND, ivec2(x, y));
            if (map_is_cell_blocked(map_cell) || map_cell.type == CELL_UNREACHABLE ||
                    map.pathing_regions[x + (y * map.width)] != PATHING_REGION_UNASSIGNED) {
                continue;
            }

            std::vector<ivec2> frontier;
            frontier.push_back(ivec2(x, y));
            while (!frontier.empty()) {
                ivec2 next = frontier.back();
                frontier.pop_back();

                map.pathing_regions[next.x + (next.y * map.width)] = map.pathing_region_count;

                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 child = next + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(map, child)) {
                        continue;
                    }
                    Cell child_cell = map_get_cell(map, CELL_LAYER_GROUND, child);
                    if (map_is_cell_blocked(child_cell) || child_cell.type == CELL_UNREACHABLE ||
                            map.pathing_regions[child.x + (child.y * map.width)] != PATHING_REGION_UNASSIGNED) {
                        continue;
                    }
                    if (map_get_tile(map, child).elevation != map_get_tile(map, next).elevation) {
                        continue;
                    }
                    if (std::abs(x - child.x) > 64 || std::abs(y - child.y) > 64) {
                        continue;
                    }

                    frontier.push_back(child);
                }
            }
            
            map.pathing_region_count++;
        }
    }

    // Form connections between adjacent pathing regions
    map.pathing_region_connections.reserve(map.pathing_region_count);
    for (int index = 0; index < map.pathing_region_count; index++) {
        map.pathing_region_connection_indices.push_back(std::vector<int>());
    }
    std::vector<bool> is_connection_cell_explored(map.width * map.height, false);
    for (int y = 0; y < map.height; y++) {
        for (int x = 0; x < map.width; x++) {
            int pathing_region = map.pathing_regions[x + (y * map.width)]; 
            if (pathing_region == PATHING_REGION_UNASSIGNED) {
                continue;
            }
            if (is_connection_cell_explored[x + (y * map.width)]) {
                continue;
            }

            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                // Check that the neighbor is valid
                ivec2 cell = ivec2(x, y);
                ivec2 neighbor = cell + DIRECTION_IVEC2[direction];
                if (neighbor.x < 0 || neighbor.y < 0 || neighbor.x == map.width || neighbor.y == map.height) {
                    continue;
                }
                int neighbor_pathing_region = map.pathing_regions[neighbor.x + (neighbor.y * map.width)];
                if (neighbor_pathing_region == PATHING_REGION_UNASSIGNED || neighbor_pathing_region == pathing_region) {
                    continue;
                }

                /*
                 * Since the neighbor is valid, we can start creating a connection
                 *
                 * To do this, we will start with the left_cell (our original cell) and the right_cell (our neighbor cell)
                 * And step adjacent to the cells to find all the other cells that are a part of this connection
                 * 
                 * Since we are considering cells from y downward and x rightward, we can assume that
                 * for horizontal connections (direction WEST to EAST) the adjacent cells will be SOUTH and 
                 * for vertical connections (direction NORTH to SOUTH) the adjacent cells with be EAST
                 */
                MapRegionConnection connection;
                int adjacent_direction = direction == DIRECTION_WEST || direction == DIRECTION_EAST ? DIRECTION_SOUTH : DIRECTION_EAST;
                ivec2 left_cell = cell; 
                ivec2 right_cell = neighbor;
                while (map_is_cell_in_bounds(map, left_cell) && map_is_cell_in_bounds(map, right_cell) &&
                            map_get_pathing_region(map, left_cell) == pathing_region &&
                            map_get_pathing_region(map, right_cell) == neighbor_pathing_region) {
                    connection.left.push_back(left_cell);
                    connection.right.push_back(right_cell);
                    is_connection_cell_explored[left_cell.x + (left_cell.y * map.width)] = true;
                    is_connection_cell_explored[right_cell.x + (right_cell.y * map.width)] = true;

                    left_cell += DIRECTION_IVEC2[adjacent_direction];
                    right_cell += DIRECTION_IVEC2[adjacent_direction];
                }

                map.pathing_region_connections.push_back(connection);
                int connection_index = map.pathing_region_connections.size() - 1;
                map.pathing_region_connection_indices[pathing_region].push_back(connection_index);
                map.pathing_region_connection_indices[neighbor_pathing_region].push_back(connection_index);
            }
        }
    }
}

bool map_is_cell_blocked(Cell cell) {
    return cell.type == CELL_BLOCKED || 
                cell.type == CELL_BUILDING || 
                cell.type == CELL_GOLDMINE ||
                (cell.type >= CELL_DECORATION_1 && cell.type <= CELL_DECORATION_5);
}

bool map_is_cell_rect_blocked(const Map& map, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (map_is_cell_blocked(map_get_cell(map, CELL_LAYER_GROUND, ivec2(x, y)))) {
                return true;
            }
        }
    }

    return false;
}

void map_calculate_unreachable_cells(Map& map) {
    // Determine map "islands"
    std::vector<int> map_tile_islands = std::vector<int>(map.width * map.height, MAP_ISLAND_UNASSIGNED);
    std::vector<int> island_size;

    for (int index = 0; index < map.width * map.height; index++) {
        if (map.cells[CELL_LAYER_GROUND][index].type == CELL_UNREACHABLE) {
            map.cells[CELL_LAYER_GROUND][index].type = CELL_EMPTY;
        }
    }

    while (true) {
        // Set index equal to the index of the first unassigned tile in the array
        int index;
        for (index = 0; index < map.width * map.height; index++) {
            if (map_is_cell_blocked(map.cells[CELL_LAYER_GROUND][index])) {
                continue;
            }

            if (map_tile_islands[index] == MAP_ISLAND_UNASSIGNED) {
                break;
            }
        }
        if (index == map.width * map.height) {
            break; // island mapping is complete
        }

        // Determine the next island index
        int island_index = island_size.size();
        island_size.push_back(0);

        // Flood fill this island index so that every tile that is reachable from 
        // the start tile (the tile determined by index) has the same island index
        std::vector<ivec2> frontier;
        frontier.push_back(ivec2(index % map.width, index / map.width));

        while (!frontier.empty()) {
            ivec2 next = frontier[0];
            frontier.erase(frontier.begin());

            if (!map_is_cell_in_bounds(map, next)) {
                continue;
            }
            Cell cell_value = map.cells[CELL_LAYER_GROUND][next.x + (next.y * map.width)];
            if (map_is_cell_blocked(cell_value)) {
                continue;
            }

            // skip this because we've already explored it
            if (map_tile_islands[next.x + (next.y * map.width)] != MAP_ISLAND_UNASSIGNED) {
                continue;
            }

            map_tile_islands[next.x + (next.y * map.width)] = island_index;
            island_size[island_index]++;

            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                frontier.push_back(next + DIRECTION_IVEC2[direction]);
            }
        }
    }
    int biggest_island = 0;
    for (int island_index = 1; island_index < (int)island_size.size(); island_index++) {
        if (island_size[island_index] > island_size[biggest_island]) {
            biggest_island = island_index;
        }
    }
    // Everything that's not on the main "island" is considered blocked
    // This makes it so that we don't place any player spawns or gold at these locations
    for (int index = 0; index < map.width * map.height; index++) {
        if (map.cells[CELL_LAYER_GROUND][index].type == CELL_EMPTY && map_tile_islands[index] != biggest_island) {
            map.cells[CELL_LAYER_GROUND][index].type = CELL_UNREACHABLE;
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

uint32_t map_neighbors_to_autotile_index(uint32_t p_neighbors) {
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

    return autotile_index[p_neighbors];
}

bool map_is_poisson_point_valid(const Map& map, const PoissonDiskParams& params, ivec2 point) {
    if (point.x < params.margin.x || point.x >= map.width - params.margin.x || point.y < params.margin.y || point.y >= (int)map.height - params.margin.y) {
        return false;
    }

    Cell cell = map_get_cell(map, CELL_LAYER_GROUND, point);
    if (cell.type == CELL_UNREACHABLE && !params.allow_unreachable_cells) {
        return false;
    } else if (cell.type != CELL_EMPTY) {
        return false;
    }
    if (map_is_tile_ramp(map, point)) {
        return false;
    }

    for (int nx = point.x - params.disk_radius; nx < point.x + params.disk_radius + 1; nx++) {
        for (int ny = point.y - params.disk_radius; ny < point.y + params.disk_radius + 1; ny++) {
            ivec2 near_point = ivec2(nx, ny);
            if (!map_is_cell_in_bounds(map, near_point)) {
                continue;
            }
            int avoid_value = params.avoid_values[near_point.x + (near_point.y * map.width)];
            if (avoid_value != 0 && ivec2::manhattan_distance(point, near_point) <= avoid_value) {
                return false;
            }
        }
    }

    return true;
}

std::vector<ivec2> map_poisson_disk(const Map& map, int* lcg_seed, PoissonDiskParams& params) {
    std::vector<ivec2> sample;
    std::vector<ivec2> frontier;

    ivec2 first;
    do {
        first.x = 1 + (lcg_rand(lcg_seed) % (map.width - 2));
        first.y = 1 + (lcg_rand(lcg_seed) % (map.height - 2));
    } while (!map_is_poisson_point_valid(map, params, first));

    frontier.push_back(first);
    sample.push_back(first);
    params.avoid_values[first.x + (first.y * map.width)] = params.disk_radius;

    std::vector<ivec2> circle_offset_points;
    {
        int x = 0;
        int y = params.disk_radius;
        int d = 3 - 2 * params.disk_radius;
        circle_offset_points.push_back(ivec2(x, y));
        circle_offset_points.push_back(ivec2(-x, y));
        circle_offset_points.push_back(ivec2(x, -y));
        circle_offset_points.push_back(ivec2(-x, -y));
        circle_offset_points.push_back(ivec2(y, x));
        circle_offset_points.push_back(ivec2(-y, x));
        circle_offset_points.push_back(ivec2(y, -x));
        circle_offset_points.push_back(ivec2(-y, -x));
        while (y >= x) {
            if (d > 0) {
                y--;
                d += 4 * (x - y) + 10;
            } else {
                d += 4 * x + 6;
            }
            x++;
            circle_offset_points.push_back(ivec2(x, y));
            circle_offset_points.push_back(ivec2(-x, y));
            circle_offset_points.push_back(ivec2(x, -y));
            circle_offset_points.push_back(ivec2(-x, -y));
            circle_offset_points.push_back(ivec2(y, x));
            circle_offset_points.push_back(ivec2(-y, x));
            circle_offset_points.push_back(ivec2(y, -x));
            circle_offset_points.push_back(ivec2(-y, -x));
        }
    }

    while (!frontier.empty()) {
        int next_index = lcg_rand(lcg_seed) % frontier.size();
        ivec2 next = frontier[next_index];

        int child_attempts = 0;
        bool child_is_valid = false;
        ivec2 child;
        while (!child_is_valid && child_attempts < 30) {
            child_attempts++;
            child = next + circle_offset_points[lcg_rand(lcg_seed) % circle_offset_points.size()];
            child_is_valid = map_is_poisson_point_valid(map, params, child);
        }
        if (child_is_valid) {
            frontier.push_back(child);
            sample.push_back(child);
            params.avoid_values[child.x + (child.y * map.width)] = params.disk_radius;
        } else {
            frontier.erase(frontier.begin() + next_index);
        }
    }

    return sample;
}

bool map_is_cell_in_bounds(const Map& map, ivec2 cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= map.width || cell.y >= (int)map.height);
}

bool map_is_cell_rect_in_bounds(const Map& map, ivec2 cell, int size) {
    return !(cell.x < 0 || cell.y < 0 || cell.x + size > map.width || cell.y + size > (int)map.height);
}

ivec2 map_clamp_cell(const Map& map, ivec2 cell) {
    return ivec2(
        std::clamp(cell.x, 0, map.width),
        std::clamp(cell.y, 0, (int)map.height)
    );
}

Tile map_get_tile(const Map& map, ivec2 cell) {
    return map.tiles[cell.x + (cell.y * map.width)];
}

bool map_is_tile_ramp(const Map& map, ivec2 cell) {
    uint8_t tile_sprite = map.tiles[cell.x + (cell.y * map.width)].sprite;
    return tile_sprite >= SPRITE_TILE_WALL_SOUTH_STAIR_LEFT && tile_sprite <= SPRITE_TILE_WALL_WEST_STAIR_BOTTOM;
}

bool map_is_cell_rect_same_elevation(const Map& map, ivec2 cell, int size) {
    for (int y = cell.y; y < cell.y + size; y++) {
        for (int x = cell.x; x < cell.x + size; x++) {
            if (map_get_tile(map, ivec2(x, y)).elevation != map_get_tile(map, cell).elevation) {
                return false;
            }
        }
    }

    return true;
}

Cell map_get_cell(const Map& map, CellLayer layer, ivec2 cell) {
    return map.cells[layer][cell.x + (cell.y * map.width)];
}

void map_set_cell(Map& map, CellLayer layer, ivec2 cell, Cell value) {
    map.cells[layer][cell.x + (cell.y * map.width)] = value;
}

void map_set_cell_rect(Map& map, CellLayer layer, ivec2 cell, int size, Cell value) {
    for (int y = cell.y; y < cell.y + size; y++) {
        for (int x = cell.x; x < cell.x + size; x++) {
            map.cells[layer][x + (y * map.width)] = value;
        }
    }
}

bool map_is_cell_rect_equal_to(const Map& map, CellLayer layer, ivec2 cell, int size, EntityId id) {
    for (int y = cell.y; y < cell.y + size; y++) {
        for (int x = cell.x; x < cell.x + size; x++) {
            if (map.cells[layer][x + (y * map.width)].id != id) {
                return false;
            }
        }
    }

    return true;
}

bool map_is_cell_rect_empty(const Map& map, CellLayer layer, ivec2 cell, int size) {
    for (int y = cell.y; y < cell.y + size; y++) {
        for (int x = cell.x; x < cell.x + size; x++) {
            if (map.cells[layer][x + (y * map.width)].type != CELL_EMPTY) {
                return false;
            }
        }
    }

    return true;
}

bool map_is_cell_rect_occupied(const Map& map, CellLayer layer, ivec2 cell, int size, ivec2 origin, uint32_t ignore) {
    EntityId origin_id = origin.x == -1 ? ID_NULL : map_get_cell(map, layer, origin).id;
    Rect origin_rect = (Rect) { .x = origin.x, .y = origin.y, .w = size, .h = size };
    bool ignore_units = (ignore & MAP_OPTION_IGNORE_UNITS) == MAP_OPTION_IGNORE_UNITS;
    bool ignore_miners = (ignore & MAP_OPTION_IGNORE_MINERS) == MAP_OPTION_IGNORE_MINERS;

    for (int y = cell.y; y < cell.y + size; y++) {
        for (int x = cell.x; x < cell.x + size; x++) {
            Cell map_cell = map_get_cell(map, layer, ivec2(x, y));
            // If the cell is empty or the cells is not empty but it is within the origin's rect, then it's not occupied
            if (map_cell.type == CELL_EMPTY || origin_rect.has_point(ivec2(x, y))) {
                continue;
            }  
            // If cell is a miner and we are gold walking, then it is not blocked
            if (map_cell.type == CELL_MINER && ignore_miners) {
                continue;
            }
            // If cell is a unit and we are ignoring units, then it is not blocked
            if (map_cell.type == CELL_UNIT && ignore_units) {
                continue;
            }
            // If the cell is a unit that is far enough away from the origin, then it is not occupied
            // This allows units paths to ignore a unit ahead of them whose position may change
            if ((map_cell.type == CELL_UNIT || map_cell.type == CELL_MINER) && origin_id != ID_NULL
                    && ivec2::manhattan_distance(origin, ivec2(x, y)) > 5) {
                continue;
            }
            return true;
        }
    }

    return false;
}

bool map_is_player_town_hall_cell_valid(const Map& map, ivec2 mine_cell, ivec2 cell) {
    static const int HALL_SIZE = 4;
    if (!map_is_cell_rect_in_bounds(map, cell, HALL_SIZE)) {
        return false;
    }
    if (map_is_cell_rect_occupied(map, CELL_LAYER_GROUND, cell, HALL_SIZE)) {
        return false;
    }
    if (!map_is_cell_rect_same_elevation(map, cell, HALL_SIZE)) {
        return false;
    }
    if (map_get_tile(map, cell).elevation != map_get_tile(map, mine_cell).elevation) {
        return false;
    }

    // Check for surrounding stairs
    const int STAIR_RADIUS = 2;
    for (int x = cell.x - STAIR_RADIUS; x < cell.x + HALL_SIZE + STAIR_RADIUS + 1; x++) {
        for (int y = cell.y - STAIR_RADIUS; y < cell.y + HALL_SIZE + STAIR_RADIUS + 1; y++) {
            if (!map_is_cell_in_bounds(map, ivec2(x, y))) {
                continue;
            }
            if (map_is_tile_ramp(map, ivec2(x, y))) {
                return false;
            }
        }
    }

    // Check mine exit path
    std::vector<ivec2> mine_exit_path;
    map_get_ideal_mine_exit_path(map, mine_cell, cell, &mine_exit_path);

    if (mine_exit_path.empty()) {
        return false;
    }

    for (ivec2 path_cell : mine_exit_path) {
        for (int y = path_cell.y - 1; y < path_cell.y + 2; y++) {
            for (int x = path_cell.x - 1; x < path_cell.x + 2; x++) {
                if (!map_is_cell_in_bounds(map, ivec2(x, y))) {
                    continue;
                }
                Cell map_cell = map_get_cell(map, CELL_LAYER_GROUND, ivec2(x, y));
                if (map_cell.type == CELL_BLOCKED || 
                        (map_cell.type >= CELL_DECORATION_1 && map_cell.type <= CELL_DECORATION_5)) {
                    return false;
                }
            }
        }
    }

    return true;
}

ivec2 map_get_player_town_hall_cell(const Map& map, ivec2 mine_cell) {
    ivec2 nearest_cell;
    int nearest_cell_dist = -1;
    ivec2 rect_position = mine_cell - ivec2(4, 4);
    int rect_size = 11;
    int start_size = 4;
    ivec2 start = mine_cell;

    ivec2 cell_begin[4] = { 
        rect_position + ivec2(-start_size, -(start_size - 1)),
        rect_position + ivec2(-(start_size - 1), rect_size),
        rect_position + ivec2(rect_size, rect_size - 1),
        rect_position + ivec2(rect_size - 1, -start_size)
    };
    ivec2 cell_end[4] = { 
        ivec2(cell_begin[0].x, rect_position.y + rect_size - 1),
        ivec2(rect_position.x + rect_size - 1, cell_begin[1].y),
        ivec2(cell_begin[2].x, cell_begin[0].y),
        ivec2(cell_begin[0].x + 1, cell_begin[3].y)
    };

    ivec2 cell_step[4] = { ivec2(0, 1), ivec2(1, 0), ivec2(0, -1), ivec2(-1, 0) };
    uint32_t index = 0;
    ivec2 cell = cell_begin[index];
    while (index < 4) {
        if (map_is_player_town_hall_cell_valid(map, mine_cell, cell) &&
                (nearest_cell_dist == -1 || ivec2::manhattan_distance(start, cell) < nearest_cell_dist)) {
            nearest_cell = cell;
            nearest_cell_dist = ivec2::manhattan_distance(start, cell);
        }

        if (cell == cell_end[index]) {
            index++;
            if (index < 4) {
                cell = cell_begin[index];
            }
        } else {
            cell += cell_step[index];
        }
    }

    // return the cell if we found one, otherwise default back to the non-stair sensitive version of this function
    return nearest_cell_dist != -1 ? nearest_cell : map_get_nearest_cell_around_rect(map, CELL_LAYER_GROUND, start, start_size, rect_position, rect_size);
}

// Returns the nearest cell around the rect relative to start_cell
// If there are no free cells around the rect in a radius of 1, then this returns the start cell
ivec2 map_get_nearest_cell_around_rect(const Map& map, CellLayer layer, ivec2 start, int start_size, ivec2 rect_position, int rect_size, uint32_t ignore, ivec2 ignore_cell) {
    ivec2 nearest_cell;
    int nearest_cell_dist = -1;

    ivec2 cell_begin[4] = { 
        rect_position + ivec2(-start_size, -(start_size - 1)),
        rect_position + ivec2(-(start_size - 1), rect_size),
        rect_position + ivec2(rect_size, rect_size - 1),
        rect_position + ivec2(rect_size - 1, -start_size)
    };
    ivec2 cell_end[4] = { 
        ivec2(cell_begin[0].x, rect_position.y + rect_size - 1),
        ivec2(rect_position.x + rect_size - 1, cell_begin[1].y),
        ivec2(cell_begin[2].x, cell_begin[0].y),
        ivec2(cell_begin[0].x + 1, cell_begin[3].y)
    };
    ivec2 cell_step[4] = { ivec2(0, 1), ivec2(1, 0), ivec2(0, -1), ivec2(-1, 0) };
    uint32_t index = 0;
    ivec2 cell = cell_begin[index];
    while (index < 4) {
        if (map_is_cell_rect_in_bounds(map, cell, start_size) && cell != ignore_cell) {
            if (!map_is_cell_rect_occupied(map, layer, cell, start_size, ivec2(-1, -1), ignore) && (nearest_cell_dist == -1 || ivec2::manhattan_distance(start, cell) < nearest_cell_dist)) {
                nearest_cell = cell;
                nearest_cell_dist = ivec2::manhattan_distance(start, cell);
            }
        } 

        if (cell == cell_end[index]) {
            index++;
            if (index < 4) {
                cell = cell_begin[index];
            }
        } else {
            cell += cell_step[index];
        }
    }

    return nearest_cell_dist != -1 ? nearest_cell : start;
}

ivec2 map_get_exit_cell(const Map& map, CellLayer layer, ivec2 building_cell, int building_size, int unit_size, ivec2 rally_cell, uint32_t ignore) {
    ivec2 exit_cell = ivec2(-1, -1);
    int exit_cell_dist = -1;
    for (int x = building_cell.x - unit_size; x < building_cell.x + building_size + unit_size; x++) {
        ivec2 cell = ivec2(x, building_cell.y - unit_size);
        int cell_dist = ivec2::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(map, cell, unit_size) && 
                !map_is_cell_rect_occupied(map, layer, cell, unit_size, ivec2(-1, -1), ignore) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = ivec2(x, building_cell.y + building_size);
        cell_dist = ivec2::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(map, cell, unit_size) && 
                !map_is_cell_rect_occupied(map, layer, cell, unit_size, ivec2(-1, -1), ignore) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }
    for (int y = building_cell.y - unit_size; y < building_cell.y + building_size + unit_size; y++) {
        ivec2 cell = ivec2(building_cell.x - unit_size, y);
        int cell_dist = ivec2::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(map, cell, unit_size) && 
                !map_is_cell_rect_occupied(map, layer, cell, unit_size, ivec2(-1, -1), ignore) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = ivec2(building_cell.x + building_size, y);
        cell_dist = ivec2::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(map, cell, unit_size) && 
                !map_is_cell_rect_occupied(map, layer, cell, unit_size, ivec2(-1, -1), ignore) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }

    return exit_cell;
}

int map_get_pathing_region(const Map& map, ivec2 cell) {
    return map.pathing_regions[cell.x + (cell.y * map.width)];
}

ivec2 map_pathfind_correct_target(const Map& map, CellLayer layer, ivec2 from, ivec2 to, uint32_t ignore, std::vector<ivec2>* ignore_cells) {
    if (from == to) {
        return to;
    }

    std::vector<MapPathNode> frontier;
    std::vector<MapPathNode> explored;
    std::vector<int> explored_indices = std::vector<int>(map.width * map.height, -1);

    if (ignore_cells != NULL) {
        for (ivec2 cell : *ignore_cells) {
            explored_indices[cell.x + (cell.y * map.width)] = 1;
        }
    }

    Cell cell = map_get_cell(map, layer, to);
    if (!(map_is_cell_blocked(cell) || cell.type == CELL_UNREACHABLE || cell.type == CELL_UNIT)) {
        // Target is reachable, so don't reverse pathfind
        return to;
    }

    // Reverse pathfind to find the nearest reachable cell
    frontier.push_back((MapPathNode) {
        .cost = fixed::from_int(0),
        .distance = fixed::from_int(0),
        .parent = -1,
        .cell = to
    });

    while (!frontier.empty()) {
        // Find the smallest path
        uint32_t smallest_index = 0;
        for (uint32_t i = 1; i < frontier.size(); i++) {
            if (frontier[i].score() < frontier[smallest_index].score()) {
                smallest_index = i;
            }
        }

        // Pop the smallest path
        MapPathNode smallest = frontier[smallest_index];
        frontier[smallest_index] = frontier.back();
        frontier.pop_back();

        // If it's the solution, return it
        if (smallest.cell == from) {
            return smallest.cell;
        }
        if (!map_is_cell_rect_occupied(map, layer, smallest.cell, 1, from, ignore)) {
            return smallest.cell;
        }

        // Otherwise mark this cell as explored
        explored_indices[smallest.cell.x + (smallest.cell.y * map.width)] = 1;

        // Consider all children
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            MapPathNode child = (MapPathNode) {
                .cost = smallest.cost + (direction % 2 == 1 ? (fixed::from_int(3) / 2) : fixed::from_int(1)),
                .distance = fixed::from_int(ivec2::manhattan_distance(smallest.cell + DIRECTION_IVEC2[direction], from)),
                .parent = -1,
                .cell = smallest.cell + DIRECTION_IVEC2[direction]
            };

            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(map, child.cell, 1)) {
                continue;
            }

            // Don't consider explored indices
            if (explored_indices[child.cell.x + (child.cell.y * map.width)] != -1) {
                continue;
            }

            // Check if it's in the frontier
            uint32_t frontier_index;
            for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                MapPathNode& frontier_node = frontier[frontier_index];
                if (frontier_node.cell == child.cell) {
                    break;
                }
            }
            // If it's in the frontier...
            if (frontier_index < frontier.size()) {
                // ...and the child represents a shorter version of the frontier path, then replace the frontier version with the shorter child
                if (child.score() < frontier[frontier_index].score()) {
                    frontier[frontier_index] = child;
                }
                continue;
            }
            // If it's not in the frontier, then add it
            frontier.push_back(child);
        } // End for each child / direction
    } // End while frontier not empty

    frontier.clear();
    memset(&explored_indices[0], -1, map.width * map.height * sizeof(int));

    return to;
}

ivec2 map_pathfind_get_region_path_target(const Map& map, ivec2 from, ivec2 to, int cell_size) {
    ZoneScoped;

    if (map.pathing_regions[from.x + (from.y * map.width)] == map.pathing_regions[to.x + (to.y * map.width)] ||
            map.pathing_regions[to.x + (to.y * map.width)] == PATHING_REGION_UNASSIGNED) {
        return to;
    }

    // Region-pathing
#ifdef GOLD_DEBUG
    bool found_region_path = false;
#endif
    MapRegionPathNode region_path_end;
    std::vector<MapRegionPathNode> frontier;
    std::vector<MapRegionPathNode> explored;
    std::vector<bool> is_connection_explored = std::vector(map.pathing_region_connections.size(), false);

    frontier.push_back((MapRegionPathNode) {
        .cell = from,
        .connection = -1,
        .parent = -1,
        .cost = 0,
    });

    while (!frontier.empty()) {
        int smallest_index = 0;
        for (int index = 1; index < (int)frontier.size(); index++) {
            if (frontier[index].score(to) < frontier[smallest_index].score(to)) {
                smallest_index = index;
            }
        }

        MapRegionPathNode next = frontier[smallest_index];
        frontier[smallest_index] = frontier.back();
        frontier.pop_back();

        if (next.cell == to) {
            region_path_end = next;
        #ifdef GOLD_DEBUG
            found_region_path = true;
        #endif
            break;
        }

        if (next.connection != -1 && is_connection_explored[next.connection]) {
            continue;
        }

        explored.push_back(next);
        if (next.connection != -1) {
            is_connection_explored[next.connection] = true;
        }

        int next_region = map_get_pathing_region(map, next.cell);
        if (next_region == map_get_pathing_region(map, to)) {
            MapRegionPathNode child = (MapRegionPathNode) {
                .cell = to,
                .connection = -1,
                .parent = (int)explored.size() - 1,
                .cost = next.cost + ivec2::manhattan_distance(next.cell, to),
            };

            int frontier_index;
            for (frontier_index = 0; frontier_index < (int)frontier.size(); frontier_index++) {
                if (child.cell == frontier[frontier_index].cell) {
                    break;
                }
            }
            if (frontier_index < (int)frontier.size()) {
                if (child.score(to) < frontier[frontier_index].score(to)) {
                    frontier[frontier_index] = child;
                }
                continue;
            }

            frontier.push_back(child);
        }
        for (int connection_index : map.pathing_region_connection_indices[next_region]) {
            if (is_connection_explored[connection_index]) {
                continue;
            }

            // Find the nearest connection cell
            ivec2 nearest_connection_cell = ivec2(-1, -1);
            // Choose from among the connection cells that are not in the region we're currently in
            const MapRegionConnection& connection = map.pathing_region_connections[connection_index];
            const std::vector<ivec2>& connection_cells = map_get_pathing_region(map, connection.left[0]) == next_region ? connection.right : connection.left;
            for (ivec2 cell : connection_cells) {
                if (cell_size == 2 && (!map_is_cell_rect_in_bounds(map, cell, 2) || map_is_cell_rect_blocked(map, cell, 2))) {
                    continue;
                }
                if (nearest_connection_cell.x == -1 || ivec2::manhattan_distance(next.cell, cell) < ivec2::manhattan_distance(next.cell, nearest_connection_cell)) {
                    nearest_connection_cell = cell;
                }
            }

            // If the nearest cell is -1, -1, it means that connected_region is connected to next_region,
            // but we could not find a connected cell, which probably means that the available connection
            // cells are too small for the unit that is currently pathing
            if (nearest_connection_cell.x == -1) {
                continue;
            }

            MapRegionPathNode child = (MapRegionPathNode) {
                .cell = nearest_connection_cell,
                .connection = connection_index,
                .parent = (int)explored.size() - 1,
                .cost = next.cost + ivec2::manhattan_distance(next.cell, nearest_connection_cell)
            };

            int frontier_index;
            for (frontier_index = 0; frontier_index < (int)frontier.size(); frontier_index++) {
                if (connection_index == frontier[frontier_index].connection) {
                    break;
                }
            }
            if (frontier_index < (int)frontier.size()) {
                if (child.score(to) < frontier[frontier_index].score(to)) {
                    frontier[frontier_index] = child;
                }
                continue;
            }

            frontier.push_back(child);
        } // End for each child / connected region
    } // End while frontier not empty

#ifdef GOLD_DEBUG
    GOLD_ASSERT(found_region_path);
#endif
    MapRegionPathNode region_current = region_path_end;
    ivec2 target = to;
    while (region_current.parent != -1) {
        target = region_current.cell;
        region_current = explored[region_current.parent];
    }

    return target;
}

void map_pathfind_calculate_path(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, uint32_t options, std::vector<ivec2>* ignore_cells, bool limit_region) {
    ZoneScoped;

    static const int EXPLORED_INDEX_NOT_EXPLORED = -1;
    static const int EXPLORED_INDEX_IGNORE_CELL = -2;

    // Don't bother pathing to the unit's cell
    if (from == to) {
        return;
    }

    // Find an alternate cell for large units
    if (cell_size > 1 && map_is_cell_rect_occupied(map, layer, to, cell_size, from, options)) {
        ivec2 nearest_alternative;
        int nearest_alternative_distance = -1;
        for (int x = 0; x < cell_size; x++) {
            for (int y = 0; y < cell_size; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }

                ivec2 alternative = to - ivec2(x, y);
                if (map_is_cell_rect_in_bounds(map, alternative, cell_size) &&
                    !map_is_cell_rect_occupied(map, layer, alternative, cell_size, from, options)) {
                    if (nearest_alternative_distance == -1 || ivec2::manhattan_distance(from, alternative) < nearest_alternative_distance) {
                        nearest_alternative = alternative;
                        nearest_alternative_distance = ivec2::manhattan_distance(from, alternative);
                    }
                }
            }
        }

        if (nearest_alternative_distance != -1) {
            to = nearest_alternative;
        }
    }

    std::vector<MapPathNode> frontier;
    std::vector<MapPathNode> explored;
    std::vector<int> explored_indices = std::vector<int>(map.width * map.height, EXPLORED_INDEX_NOT_EXPLORED);
    uint32_t closest_explored = 0;
    bool found_path = false;
    bool avoid_landmines = (options & MAP_OPTION_AVOID_LANDMINES) == MAP_OPTION_AVOID_LANDMINES;
    MapPathNode path_end;

    // Fill the ignore cells into explored indices array so that we can do a constant time lookup to see if a cell should be ignored
    if (ignore_cells != NULL) {
        for (ivec2 ignore_cell : *ignore_cells) {
            explored_indices[ignore_cell.x + (ignore_cell.y * map.width)] = EXPLORED_INDEX_IGNORE_CELL;
        }
    }

    frontier.push_back((MapPathNode) {
        .cost = fixed::from_int(0),
        .distance = fixed::from_int(ivec2::manhattan_distance(from, to)),
        .parent = -1,
        .cell = from
    });

    while (!frontier.empty()) {
        // Find the smallest path
        uint32_t smallest_index = 0;
        for (uint32_t i = 1; i < frontier.size(); i++) {
            if (frontier[i].score() < frontier[smallest_index].score()) {
                smallest_index = i;
            }
        }

        // Pop the smallest path
        MapPathNode smallest = frontier[smallest_index];
        frontier.erase(frontier.begin() + smallest_index);

        // If it's the solution, return it
        if (smallest.cell == to) {
            found_path = true;
            path_end = smallest;
            break;
        }

        // Otherwise, add this tile to the explored list
        explored.push_back(smallest);
        explored_indices[smallest.cell.x + (smallest.cell.y * map.width)] = explored.size() - 1;
        if (explored[explored.size() - 1].distance < explored[closest_explored].distance) {
            closest_explored = explored.size() - 1;
        }

        if (explored.size() > PATHFIND_ITERATION_MAX) {
            break;
        }

        // Consider all children
        // Adjacent cells are considered first so that we can use information about them to inform whether or not a diagonal movement is allowed
        bool is_adjacent_direction_blocked[4] = { true, true, true, true };
        const int CHILD_DIRECTIONS[DIRECTION_COUNT] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST, 
                                                        DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST, DIRECTION_NORTHWEST };
        for (int direction_index = 0; direction_index < DIRECTION_COUNT; direction_index++) {
            int direction = CHILD_DIRECTIONS[direction_index];
            MapPathNode child = (MapPathNode) {
                .cost = smallest.cost + (direction % 2 == 1 ? (fixed::from_int(3) / 2) : fixed::from_int(1)),
                .distance = fixed::from_int(ivec2::manhattan_distance(smallest.cell + DIRECTION_IVEC2[direction], to)),
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_IVEC2[direction]
            };

            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(map, child.cell, cell_size)) {
                continue;
            }

            // Don't consider different-region children, unless they are the goal
            if (limit_region && layer == CELL_LAYER_GROUND && child.cell != to && 
                    map.pathing_regions[child.cell.x + (child.cell.y * map.width)] != 
                    map.pathing_regions[from.x + (from.y * map.width)]) {
                continue;
            }

            // Skip occupied cells (unless the child is the goal. this avoids worst-case pathing)
            if (map_is_cell_rect_occupied(map, layer, child.cell, cell_size, from, options) &&
                !(child.cell == to && ivec2::manhattan_distance(smallest.cell, child.cell) == 1)) {
                continue;
            }

            // If set in options, skip cells that are too close to landmines
            if (avoid_landmines) {
                bool is_cell_too_close_to_landmine = false;
                for (int y = child.cell.y - 1; y < child.cell.y + 2; y++) {
                    for (int x = child.cell.x - 1; x < child.cell.x + 2; x++) {
                        ivec2 cell = ivec2(x, y);
                        if (!map_is_cell_in_bounds(map, cell)) {
                            continue;
                        }

                        Cell map_underground_cell = map_get_cell(map, CELL_LAYER_UNDERGROUND, cell);
                        if (map_underground_cell.type == CELL_BUILDING) {
                            is_cell_too_close_to_landmine = true;
                        }
                    }
                }
                if (is_cell_too_close_to_landmine) {
                    continue;
                }
            }

            // Ignore ignore_cells
            if (explored_indices[child.cell.x + (child.cell.y * map.width)] == EXPLORED_INDEX_IGNORE_CELL) {
                continue;
            }

            // Don't allow diagonal movement through cracks
            if (direction % 2 == 0) {
                is_adjacent_direction_blocked[direction / 2] = false;
            } else {
                int next_direction = direction + 1 == DIRECTION_COUNT ? 0 : direction + 1;
                int prev_direction = direction - 1;
                if (is_adjacent_direction_blocked[next_direction / 2] && is_adjacent_direction_blocked[prev_direction / 2]) {
                    continue;
                }
            }

            // Don't consider already explored children
            if (explored_indices[child.cell.x + (child.cell.y * map.width)] != EXPLORED_INDEX_NOT_EXPLORED) {
                continue;
            }

            uint32_t frontier_index;
            for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                MapPathNode& frontier_node = frontier[frontier_index];
                if (frontier_node.cell == child.cell) {
                    break;
                }
            }
            // If it is in the frontier...
            if (frontier_index < frontier.size()) {
                // ...and the child represents a shorter version of the frontier path, then replace the frontier version with the shorter child
                if (child.score() < frontier[frontier_index].score()) {
                    frontier[frontier_index] = child;
                }
                continue;
            }
            // If it's not in the frontier, then add it to the frontier
            frontier.push_back(child);
        } // End for each child
    } // End while not frontier empty

    // Backtrack to build the path
    MapPathNode current = found_path ? path_end : explored[closest_explored];
    path->reserve(path->size() + current.cost.integer_part() + 1);
    while (current.parent != -1) {
        path->push_back(current.cell);
        current = explored[current.parent];
    }
    std::reverse(path->begin(), path->end());
}

void map_pathfind(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, uint32_t ignore, std::vector<ivec2>* ignore_cells) {
    ZoneScoped;

    path->clear();

    // Don't bother pathing to the unit's cell
    if (from == to) {
        return;
    }

    ivec2 original_to = to;
    to = map_pathfind_correct_target(map, layer, from, to, ignore, ignore_cells);
    bool allow_squirreling = (ignore & MAP_OPTION_ALLOW_PATH_SQUIRRELING) == MAP_OPTION_ALLOW_PATH_SQUIRRELING;
    if (to != original_to && ivec2::manhattan_distance(from, to) < 3 &&
            map_get_cell(map, layer, original_to).type == CELL_UNIT && !allow_squirreling) {
        return;
    }

    if (layer == CELL_LAYER_GROUND) {
        ivec2 region_path_target = map_pathfind_get_region_path_target(map, from, to, cell_size);
        if (ivec2::manhattan_distance(region_path_target, to) > 3) {
            to = region_path_target;
        }
    }
    map_pathfind_calculate_path(map, layer, from, to, cell_size, path, ignore, ignore_cells, to != original_to);
}

void map_get_ideal_mine_exit_path(const Map& map, ivec2 mine_cell, ivec2 hall_cell, std::vector<ivec2>* path) {
    ivec2 rally_cell = map_get_nearest_cell_around_rect(map, CELL_LAYER_GROUND, mine_cell + ivec2(1, 1), 1, hall_cell, 4, MAP_OPTION_IGNORE_MINERS);
    ivec2 mine_exit_cell = map_get_exit_cell(map, CELL_LAYER_GROUND, mine_cell, 3, 1, rally_cell, MAP_OPTION_IGNORE_MINERS);
    GOLD_ASSERT(mine_exit_cell.x != -1);

    path->clear();
    map_pathfind_calculate_path(map, CELL_LAYER_GROUND, mine_exit_cell, rally_cell, 1, path, MAP_OPTION_IGNORE_MINERS, NULL, false);
    path->push_back(mine_exit_cell);
}
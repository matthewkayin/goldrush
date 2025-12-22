#include "map.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "render/render.h"
#include "lcg.h"
#include <unordered_map>
#include <algorithm>
#include <tracy/tracy/Tracy.hpp>

static const int MAP_PLAYER_SPAWN_SIZE = 13;
static const int MAP_PLAYER_SPAWN_MARGIN = 3;
static const int MAP_STAIR_SPACING = 16;
static const int MAP_ISLAND_UNASSIGNED = -1;
static const int REGION_UNASSIGNED = -1;
static const int REGION_CHUNK_SIZE = 32;
static const uint32_t PATHFIND_ITERATION_MAX = 1999;

struct PoissonAvoidValue {
    ivec2 cell;
    int distance;
};

struct PoissonDiskParams {
    std::vector<PoissonAvoidValue> avoid_values;
    int disk_radius;
    bool allow_unreachable_cells;
    ivec2 margin;
};

SpriteName map_wall_autotile_lookup(uint32_t neighbors);
bool map_is_poisson_point_valid(const Map& map, const PoissonDiskParams& params, ivec2 point);
std::vector<ivec2> map_poisson_disk(const Map& map, int* lcg_seed, PoissonDiskParams& params);
bool map_is_tree_cell_valid(const Map& map, ivec2 cell);

void map_init(Map& map, MapType map_type, Noise* noise, int* lcg_seed, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells) {
    map.type = map_type;
    map.width = noise->width;
    map.height = noise->height;
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
    const int WATER_WALL_DIST = 6;
    for (int x = 0; x < noise->width; x++) {
        for (int y = 0; y < noise->height; y++) {
            if (noise->map[x + (y * noise->width)] != NOISE_VALUE_WATER) {
                continue;
            }

            bool is_too_close_to_wall = false;
            for (int nx = x - WATER_WALL_DIST; nx < x + WATER_WALL_DIST + 1; nx++) {
                for (int ny = y - WATER_WALL_DIST; ny < y + WATER_WALL_DIST + 1; ny++) {
                    if (!map_is_cell_in_bounds(map, ivec2(nx, ny))) {
                        continue;
                    }
                    if (noise->map[nx + (ny * noise->width)] == NOISE_VALUE_HIGHGROUND && ivec2::manhattan_distance(ivec2(x, y), ivec2(nx, ny)) <= WATER_WALL_DIST) {
                        is_too_close_to_wall = true;
                    }
                }
                if (is_too_close_to_wall) {
                    break;
                }
            }
            if (is_too_close_to_wall) {
                noise->map[x + (y * noise->width)] = NOISE_VALUE_LOWGROUND;
            }
        }
    }

    // Remove tiny lakes
    {
        std::vector<bool> is_cell_explored(noise->width * noise->height, false);
        for (int y = 0; y < noise->height; y++) {
            for (int x = 0; x < noise->width; x++) {
                if (noise->map[x + (y * noise->width)] != NOISE_VALUE_WATER ||
                        is_cell_explored[x + (y * noise->width)]) {
                    continue;
                }

                std::vector<ivec2> water_cells;
                std::vector<ivec2> frontier;

                frontier.push_back(ivec2(x, y));
                is_cell_explored[x + (y * noise->width)] = true;
                while (!frontier.empty()) {
                    ivec2 next = frontier.back();
                    frontier.pop_back();
                    water_cells.push_back(next);

                    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                        ivec2 child = next + DIRECTION_IVEC2[direction];
                        if (!map_is_cell_in_bounds(map, child) || 
                                is_cell_explored[child.x + (child.y * map.width)] ||
                                noise->map[child.x + (child.y * noise->width)] != NOISE_VALUE_WATER) {
                            continue;
                        }

                        frontier.push_back(child);
                        is_cell_explored[child.x + (child.y * noise->width)] = true;
                    }
                }

                if (water_cells.size() < 8) {
                    for (ivec2 cell : water_cells) {
                        noise->map[cell.x + (cell.y * noise->width)] = NOISE_VALUE_LOWGROUND;
                    }
                }
            }
        }
    }

    // Widen narrow gaps
    for (int y = 0; y < noise->height; y++) {
        for (int x = 0; x < noise->width; x++) {
            if (noise->map[x + (y * noise->width)] == NOISE_VALUE_WATER) {
                continue;
            }

            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                ivec2 wall = ivec2(x, y) + DIRECTION_IVEC2[direction];
                if (!map_is_cell_in_bounds(map, wall) || noise->map[wall.x + (wall.y * noise->width)] > noise->map[x + (y * noise->width)]) {
                    const int STEP_COUNT = direction % 2 == 0 ? 3 : 2;
                    for (int step = 0; step < STEP_COUNT; step++) {
                        ivec2 opposite = ivec2(x, y) - (DIRECTION_IVEC2[direction] * (step + 1));
                        if (map_is_cell_in_bounds(map, opposite) && noise->map[opposite.x + (opposite.y * noise->width)] > noise->map[x + (y * noise->width)]) {
                            noise->map[opposite.x + (opposite.y * noise->width)] = noise->map[x + (y * noise->width)];
                        }
                    }
                }
            }
        }
    }

    // Remove small lowground areas
    {
        std::vector<int> map_tile_islands(noise->width * noise->height, MAP_ISLAND_UNASSIGNED);
        std::vector<int> island_size;

        while (true) {
            // Set index equal to the first index of the first unassigned tile in the array
            int index;
            for (index = 0; index < noise->width * noise->height; index++) {
                if (map_tile_islands[index] == -1) {
                    break;
                }
            }
            if (index == noise->width * noise->height) {
                // Island mapping is complete
                break;
            }

            // Determine the next island index
            int island_index = island_size.size();
            island_size.push_back(0);
            int8_t island_noise_value = noise->map[index];

            // Flood fill this island index
            std::vector<ivec2> frontier;
            frontier.push_back(ivec2(index % noise->width, index / noise->height));

            while (!frontier.empty()) {
                ivec2 next = frontier.back();
                frontier.pop_back();

                if (next.x < 0 || next.y < 0 || next.x >= noise->width || next.y >= noise->height) {
                    continue;
                }
                if (noise->map[next.x + (next.y * noise->width)] != island_noise_value) {
                    continue;
                }

                // skip this because we've already explored it
                if (map_tile_islands[next.x + (next.y * noise->width)] != MAP_ISLAND_UNASSIGNED) {
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
            for (first_index = 0; first_index < noise->width * noise->height; first_index++) {
                if (map_tile_islands[first_index] == island_index) {
                    break;
                }
            }
            ivec2 first_index_coord = ivec2(first_index % noise->width, first_index / noise->width);

            // And use that index to determine the island's noise value
            // If the noise value is non-zero, then skip it
            if (noise->map[first_index] != NOISE_VALUE_LOWGROUND) {
                continue;
            }

            // We now have a small 0-elevation section of the noise map that we would like to clean up
            // So replace the 0-elevation section with a different noise level
            for (int index = 0; index < noise->width * noise->height; index++) {
                if (map_tile_islands[index] != island_index) {
                    continue;
                }

                noise->map[index] = NOISE_VALUE_HIGHGROUND;
            }
        }
    }
    
    // Remove lowground nooks
    for (int y = 0; y < noise->height; y++) {
        for (int x = 0; x < noise->width; x++) {
            if (noise->map[x + (y * noise->width)] != NOISE_VALUE_LOWGROUND) {
                continue;
            }

            int highground_neighbor_count = 0;
            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                ivec2 neighbor = ivec2(x, y) + DIRECTION_IVEC2[direction];
                if (neighbor.x < 0 || neighbor.y < 0 || neighbor.x >= noise->width || neighbor.y >= noise->height) {
                    continue;
                }
                if (noise->map[neighbor.x + (neighbor.y * noise->width)] == NOISE_VALUE_HIGHGROUND) {
                    highground_neighbor_count++;
                }
            }
            if (highground_neighbor_count > 2) {
                noise->map[x + (y * noise->width)] = NOISE_VALUE_HIGHGROUND;
            }
        }
    }

    // Remove highground areas that are too small for wagons
    {
        // First mark all of the highground floor (i.e. non-ground) tiles
        std::vector<bool> is_highground_floor(noise->width * noise->height, false);
        for (int y = 0; y < noise->height; y++) {
            for (int x = 0; x < noise->width; x++) {
                if (noise->map[x + (y * noise->width)] != NOISE_VALUE_HIGHGROUND) {
                    continue;
                }

                is_highground_floor[x + (y * noise->width)] = true;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(map, adjacent) || noise->map[adjacent.x + (adjacent.y * noise->width)] == NOISE_VALUE_HIGHGROUND) {
                        continue;
                    }
                    is_highground_floor[x + (y * noise->width)] = false;
                }
            }
        }

        // Then mark all of the highground floors that can be occupied by a 2x2 unit
        std::vector<bool> is_2x2_highground_floor(noise->width * noise->height, false);
        for (int y = 0; y < noise->height; y++) {
            for (int x = 0; x < noise->width; x++) {
                if (!is_highground_floor[x + (y * noise->width)]) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    if (map_is_cell_in_bounds(map, adjacent) && is_highground_floor[adjacent.x + (adjacent.y * noise->width)]) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    int prev_direction = direction - 1;
                    int next_direction = direction + 1 == DIRECTION_COUNT ? 0 : direction + 1;
                    uint32_t adjacent_neighbors = DIRECTION_MASK[prev_direction] | DIRECTION_MASK[next_direction];
                    if (map_is_cell_in_bounds(map, adjacent) && 
                            is_highground_floor[adjacent.x + (adjacent.y * noise->width)] &&
                            (neighbors & adjacent_neighbors) == adjacent_neighbors) {
                        is_2x2_highground_floor[x + (y * noise->width)] = true;
                    }
                }
            }
        }

        // Finally, remove any walls that do not have a 2x2 occupiable floor surrounding them
        for (int y = 0; y < noise->height; y++) {
            for (int x = 0; x < noise->width; x++) {
                if (noise->map[x + (y * map.width)] != NOISE_VALUE_HIGHGROUND) {
                    continue;
                }

                bool has_adjacent_2x2_floor = false;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    ivec2 adjacent = ivec2(x, y) + DIRECTION_IVEC2[direction];
                    if (map_is_cell_in_bounds(map, adjacent) && is_2x2_highground_floor[adjacent.x + (adjacent.y * noise->width)]) {
                        has_adjacent_2x2_floor = true;
                    }
                }

                if (!has_adjacent_2x2_floor) {
                    noise->map[x + (y * map.width)] = NOISE_VALUE_LOWGROUND;
                }
            }
        }
    }

    // Bake map tiles
    std::vector<ivec2> artifacts;
    do {
        std::fill(map.tiles.begin(), map.tiles.end(), (Tile) {
            .sprite = map_get_plain_ground_tile_sprite(map_type),
            .frame = ivec2(0, 0),
            .elevation = 0
        });
        for (ivec2 artifact : artifacts) {
            noise->map[artifact.x + (artifact.y * map.width)] = NOISE_VALUE_LOWGROUND;
        }
        artifacts.clear();

        log_debug("Baking map tiles...");
        for (int y = 0; y < map.height; y++) {
            for (int x = 0; x < map.width; x++) {
                int index = x + (y * map.width);
                if (noise->map[index] == NOISE_VALUE_LOWGROUND || noise->map[index] == NOISE_VALUE_HIGHGROUND) {
                    map.tiles[index].elevation = (uint32_t)(noise->map[index] == NOISE_VALUE_HIGHGROUND);
                    // First check if we need to place a regular wall here
                    uint32_t neighbors = 0;
                    if (noise->map[index] == NOISE_VALUE_HIGHGROUND) {
                        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                            ivec2 neighbor_cell = ivec2(x, y) + DIRECTION_IVEC2[direction];
                            if (!map_is_cell_in_bounds(map, neighbor_cell)) {
                                continue;
                            }
                            int neighbor_index = neighbor_cell.x + (neighbor_cell.y * map.width);
                            uint32_t neighbor_elevation = (uint32_t)(noise->map[neighbor_index] == NOISE_VALUE_HIGHGROUND);
                            if (map.tiles[index].elevation > neighbor_elevation) {
                                neighbors += DIRECTION_MASK[direction];
                            }
                        }
                    }

                    // Regular sand tile 
                    if (neighbors == 0) {
                        map.tiles[index].sprite = map_choose_ground_tile_sprite(map_type, index, lcg_seed);
                    // Wall tile 
                    } else {
                        map.tiles[index].sprite = map_wall_autotile_lookup(neighbors);
                        if (map.tiles[index].sprite == SPRITE_TILE_NULL) {
                            artifacts.push_back(ivec2(x, y));
                        }
                    }
                } else if (noise->map[index] == NOISE_VALUE_WATER) {
                    uint32_t neighbors = 0;
                    // Check adjacent neighbors
                    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                        ivec2 neighbor_cell = ivec2(x, y) + DIRECTION_IVEC2[direction];
                        if (!map_is_cell_in_bounds(map, neighbor_cell) || 
                                noise->map[index] == noise->map[neighbor_cell.x + (neighbor_cell.y * map.width)]) {
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
                                noise->map[index] == noise->map[neighbor_cell.x + (neighbor_cell.y * map.width)]) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }
                    // Set the map tile based on the neighbors
                    uint32_t autotile_index = map_neighbors_to_autotile_index(neighbors);
                    map.tiles[index] = (Tile) {
                        .sprite = map_choose_water_tile_sprite(map_type),
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
    std::vector<ivec2> stair_centers;
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
                stair_centers.push_back((stair_min + stair_max) / 2);
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
                    if (!map_is_tile_ground(map, stair_front_cell)) {
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
        ivec2 cell = ivec2(index % map.width, index / map.width);
        if (!map_is_tile_ground(map, cell) && !map_is_tile_ramp(map, cell)) {
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
                            DIRECTION_IVEC2[spawn_direction].x * ((map.width / 2) - (MAP_PLAYER_SPAWN_SIZE + MAP_PLAYER_SPAWN_MARGIN)),
                            DIRECTION_IVEC2[spawn_direction].y * ((map.height / 2) - (MAP_PLAYER_SPAWN_SIZE + MAP_PLAYER_SPAWN_MARGIN)));
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
                    if (!map_is_cell_rect_in_bounds(map, child, MAP_PLAYER_SPAWN_SIZE + MAP_PLAYER_SPAWN_MARGIN)) {
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
    {
        PoissonDiskParams params = (PoissonDiskParams) {
            .avoid_values = std::vector<PoissonAvoidValue>(),
            .disk_radius = 48,
            .allow_unreachable_cells = false,
            .margin = ivec2(5, 5)
        };

        // Place a gold mine on each player's spawn
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            GOLD_ASSERT(player_spawns[player_id].x != -1);

            ivec2 mine_cell = player_spawns[player_id];
            goldmine_cells.push_back(mine_cell);
            params.avoid_values.push_back((PoissonAvoidValue) {
                .cell = mine_cell,
                .distance = 48
            });
        }

        // Generate the avoid values
        for (int index = 0; index < map.width * map.height; index++) {
            if (map.cells[CELL_LAYER_GROUND][index].type == CELL_BLOCKED || map_is_tile_ramp(map, ivec2(index % map.width, index / map.width))) {
                params.avoid_values.push_back((PoissonAvoidValue) {
                    .cell = ivec2(index % map.width, index / map.width),
                    .distance = 6
                }); 
            }
        }

        std::vector<ivec2> goldmine_results = map_poisson_disk(map, lcg_seed, params);
        goldmine_cells.insert(goldmine_cells.end(), goldmine_results.begin(), goldmine_results.end());
    }
    // End generate gold mines
    log_debug("Generated gold mines.");

    // Generate decorations
    {
        // Generate avoid values for decorations
        std::vector<PoissonAvoidValue> avoid_values;

        // Ramps
        for (int y = 0; y < map.height; y++) {
            for (int x = 0; x < map.width; x++) {
                if (!map_is_tile_ramp(map, ivec2(x, y))) {
                    continue;
                }
                avoid_values.push_back((PoissonAvoidValue) {
                    .cell = ivec2(x, y),
                    .distance = 6
                });
            }
        }

        // Goldmines
        for (ivec2 goldmine_cell : goldmine_cells) {
            avoid_values.push_back((PoissonAvoidValue) {
                .cell = goldmine_cell,
                .distance = 16
            });
        }

        PoissonDiskParams params = (PoissonDiskParams) {
            .avoid_values = avoid_values,
            .disk_radius = 16,
            .allow_unreachable_cells = true,
            .margin = ivec2(0, 0)
        };

        std::vector<ivec2> decoration_cells = map_poisson_disk(map, lcg_seed, params);

        for (ivec2 cell : decoration_cells) {
            map_create_decoration_at_cell(map, lcg_seed, cell);
        }
    }
    log_debug("Generated decorations.");

    map_calculate_unreachable_cells(map);

    // Make sure that the map size is equally divisible by the region size
    // At the time of writing this, map sizes are 96, 128, and 160 and region size is 32
    GOLD_ASSERT(map.width % REGION_CHUNK_SIZE == 0);

    // Create map regions
    int region_count = 0;
    map.regions = std::vector(map.width * map.height, REGION_UNASSIGNED);
    for (int chunk_y = 0; chunk_y < map.height / REGION_CHUNK_SIZE; chunk_y++) {
        for (int chunk_x = 0; chunk_x < map.width / REGION_CHUNK_SIZE; chunk_x++) {
            for (int y = chunk_y * REGION_CHUNK_SIZE; y < (chunk_y + 1) * REGION_CHUNK_SIZE; y++) {
                for (int x = chunk_x * REGION_CHUNK_SIZE; x < (chunk_x + 1) * REGION_CHUNK_SIZE; x++) {
                    // Filter down to unblocked, unassigned cells
                    Cell map_cell = map_get_cell(map, CELL_LAYER_GROUND, ivec2(x, y));
                    if (map_cell.type == CELL_BLOCKED || map_cell.type == CELL_UNREACHABLE || map_cell.type == CELL_DECORATION ||
                            map_get_region(map, ivec2(x, y)) != REGION_UNASSIGNED) {
                        continue;
                    }

                    // Now we know that this cell should be in a region
                    // Flood fill to find all cells connected to this cell
                    // that are within the same chunk

                    std::vector<ivec2> frontier;
                    frontier.push_back(ivec2(x, y));
                    while (!frontier.empty()) {
                        ivec2 next = frontier.back();
                        frontier.pop_back();

                        map.regions[next.x + (next.y * map.width)] = region_count;

                        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                            ivec2 child = next + DIRECTION_IVEC2[direction];
                            if (child.x < chunk_x || child.y < chunk_y ||
                                    child.x >= (chunk_x + 1) * REGION_CHUNK_SIZE || child.y >= (chunk_y + 1) * REGION_CHUNK_SIZE) {
                                continue;
                            }

                            Cell child_cell = map_get_cell(map, CELL_LAYER_GROUND, child);
                            if (child_cell.type == CELL_BLOCKED || child_cell.type == CELL_UNREACHABLE || child_cell.type == CELL_DECORATION ||
                                    map_get_region(map, child) != REGION_UNASSIGNED) {
                                continue;
                            }
                            if (map_get_tile(map, child).elevation != map_get_tile(map, next).elevation) {
                                continue;
                            }

                            frontier.push_back(child);
                        }
                    } // end while not frontier empty

                    region_count++;
                }
            }
        }
    }
    
    // Create region connections
    for (int region = 0; region < region_count; region++) {
        map.region_connections.push_back(std::vector<std::vector<ivec2>>());
        for (int other_region = 0; other_region < region_count; other_region++) {
            map.region_connections[region].push_back(std::vector<ivec2>());
        }
    }
    for (int y = 0; y < map.height; y++) {
        for (int x = 0; x < map.width; x++) {
            int region = map_get_region(map, ivec2(x, y));
            if (region == REGION_UNASSIGNED) {
                continue;
            }

            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                ivec2 neighbor = ivec2(x, y) + DIRECTION_IVEC2[direction];
                if (!map_is_cell_in_bounds(map, neighbor)) {
                    continue;
                }
                int neighbor_region = map_get_region(map, neighbor);
                if (neighbor_region == REGION_UNASSIGNED || neighbor_region == region) {
                    continue;
                }

                map.region_connections[region][neighbor_region].push_back(neighbor);
            }
        }
    }
}

SpriteName map_choose_ground_tile_sprite(MapType map_type, int index, int* lcg_seed) {
    switch (map_type) {
        case MAP_TYPE_ARIZONA: {
            int new_index = lcg_rand(lcg_seed) % 7;
            if (new_index == 1 && index % 3 == 0) {
                return SPRITE_TILE_SAND3;
            } else if (new_index < 4 && index % 3 == 0) {
                return SPRITE_TILE_SAND2;
            } else {
                return SPRITE_TILE_SAND1;
            }
        }
        case MAP_TYPE_KLONDIKE: {
            int new_index = lcg_rand(lcg_seed) % 20;
            if (new_index == 1 && index % 7 == 0) {
                return SPRITE_TILE_SNOW3;
            } else if (new_index == 0 && index % 7 == 0) {
                return SPRITE_TILE_SNOW2;
            } else {
                return SPRITE_TILE_SNOW1;
            }
        }
        case MAP_TYPE_COUNT: {
            GOLD_ASSERT(false);
            return SPRITE_TILE_NULL;
        }
    }
}

SpriteName map_choose_water_tile_sprite(MapType map_type) {
    switch (map_type) {
        case MAP_TYPE_ARIZONA:
            return SPRITE_TILE_SAND_WATER;
        case MAP_TYPE_KLONDIKE:
            return SPRITE_TILE_SNOW_WATER;
        case MAP_TYPE_COUNT: {
            GOLD_ASSERT(false);
            return SPRITE_TILE_NULL;
        }
    }
}

SpriteName map_get_plain_ground_tile_sprite(MapType map_type) {
    switch (map_type) {
        case MAP_TYPE_ARIZONA:
            return SPRITE_TILE_SAND1;
        case MAP_TYPE_KLONDIKE:
            return SPRITE_TILE_SNOW1;
        case MAP_TYPE_COUNT: {
            GOLD_ASSERT(false);
            return SPRITE_TILE_NULL;
        }
    }
}

SpriteName map_get_decoration_sprite(MapType map_type) {
    switch (map_type) {
        case MAP_TYPE_ARIZONA: 
            return SPRITE_DECORATION_ARIZONA;
        case MAP_TYPE_KLONDIKE: 
            return SPRITE_DECORATION_KLONDIKE;
        case MAP_TYPE_COUNT: {
            GOLD_ASSERT(false);
            return SPRITE_TILE_NULL;
        }
    }
}

void map_create_decoration_at_cell(Map& map, int* lcg_seed, ivec2 cell) {
    SpriteName decoration_sprite = map_get_decoration_sprite(map.type);
    const SpriteInfo& decoration_sprite_info = render_get_sprite_info(decoration_sprite);

    map.cells[CELL_LAYER_GROUND][cell.x + (cell.y * map.width)] = (Cell) {
        .type = CELL_DECORATION,
        .decoration_hframe = (uint16_t)(lcg_rand(lcg_seed) % decoration_sprite_info.hframes)
    };
}

bool map_is_cell_blocked(Cell cell) {
    return cell.type == CELL_BLOCKED || 
                cell.type == CELL_BUILDING || 
                cell.type == CELL_GOLDMINE ||
                cell.type == CELL_DECORATION;
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
    // Don't allow trees on the very top cell
    if (map.type == MAP_TYPE_KLONDIKE && point.y < 1) {
        return false;
    }
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

    for (const PoissonAvoidValue& avoid_value : params.avoid_values) {
        if (ivec2::manhattan_distance(point, avoid_value.cell) <= avoid_value.distance) {
            return false;
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
    params.avoid_values.push_back((PoissonAvoidValue) {
        .cell = first,
        .distance = params.disk_radius
    });

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
            params.avoid_values.push_back((PoissonAvoidValue) {
                .cell = child,
                .distance = params.disk_radius
            });
        } else {
            frontier.erase(frontier.begin() + next_index);
        }
    }

    return sample;
}

bool map_is_tree_cell_valid(const Map& map, ivec2 cell) {
    if (!map_is_cell_in_bounds(map, cell)) {
        return false;
    }

    if (map_get_cell(map, CELL_LAYER_GROUND, cell).type != CELL_EMPTY) {
        return false;
    }
    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
        ivec2 neighbor_cell = cell + DIRECTION_IVEC2[direction];
        if (!map_is_cell_in_bounds(map, neighbor_cell)) {
            continue;
        }
        if (map_get_cell(map, CELL_LAYER_GROUND, neighbor_cell).type != CELL_EMPTY) {
            return false;
        }
    }

    return true;
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

bool map_is_tile_ground(const Map& map, ivec2 cell) {
    uint8_t tile_sprite = map.tiles[cell.x + (cell.y * map.width)].sprite;
    return tile_sprite == SPRITE_TILE_SAND1 ||
        tile_sprite == SPRITE_TILE_SAND2 ||
        tile_sprite == SPRITE_TILE_SAND3 ||
        tile_sprite == SPRITE_TILE_SNOW1 ||
        tile_sprite == SPRITE_TILE_SNOW2 ||
        tile_sprite == SPRITE_TILE_SNOW3;
}

bool map_is_tile_ramp(const Map& map, ivec2 cell) {
    uint8_t tile_sprite = map.tiles[cell.x + (cell.y * map.width)].sprite;
    return tile_sprite >= SPRITE_TILE_WALL_SOUTH_STAIR_LEFT && tile_sprite <= SPRITE_TILE_WALL_WEST_STAIR_BOTTOM;
}

bool map_is_tile_water(const Map& map, ivec2 cell) {
    uint8_t tile_sprite = map.tiles[cell.x + (cell.y * map.width)].sprite;
    return tile_sprite == SPRITE_TILE_SAND_WATER ||
        tile_sprite == SPRITE_TILE_SNOW_WATER;
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
                if (map_cell.type == CELL_BLOCKED || map_cell.type == CELL_DECORATION) {
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

int map_get_region(const Map& map, ivec2 cell) {
    return map.regions[cell.x + (cell.y * map.width)];
}

int map_get_region_count(const Map& map) {
    return map.region_connections.size();
}

int map_are_regions_connected(const Map& map, int region_a, int region_b) {
    return !map.region_connections[region_a][region_b].empty();
}

ivec2 map_pathfind_correct_target(const Map& map, CellLayer layer, ivec2 from, ivec2 to, uint32_t ignore, std::vector<ivec2>* ignore_cells) {
    ZoneScoped;

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
        .parent = -1,
        .cell = to,
        .cost = 0
    });

    while (!frontier.empty()) {
        // Find the smallest path
        uint32_t smallest_index = 0;
        for (uint32_t i = 1; i < frontier.size(); i++) {
            if (ivec2::manhattan_distance(frontier[i].cell, from) < 
                    ivec2::manhattan_distance(frontier[smallest_index].cell, from)) {
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
                .parent = -1,
                .cell = smallest.cell + DIRECTION_IVEC2[direction],
                .cost = 0
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
                if (ivec2::manhattan_distance(child.cell, from) <
                        ivec2::manhattan_distance(frontier[frontier_index].cell, from)) {
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

std::vector<int> map_get_region_path(const Map& map, int from_region, int to_region) {
    ZoneScoped;

    std::vector<int> path;

    if (from_region == to_region) {
        path.push_back(to_region);
        return path;
    }

    std::vector<MapRegionPathNode> frontier;
    std::vector<MapRegionPathNode> explored;
    std::vector<bool> is_region_explored(map_get_region_count(map), false);
    
    frontier.push_back((MapRegionPathNode) {
        .parent = -1,
        .region = from_region,
        .cost = 0
    });

    MapRegionPathNode end_node;
    end_node.cost = -1;
    while (!frontier.empty()) {
        uint32_t smallest_index = 0;
        for (uint32_t index = 1; index < frontier.size(); index++) {
            if (frontier[index].cost < frontier[smallest_index].cost) {
                smallest_index = index;
            }
        }
        MapRegionPathNode smallest = frontier[smallest_index];
        frontier[smallest_index] = frontier.back();
        frontier.pop_back();

        if (smallest.region == to_region) {
            end_node = smallest;
            break;
        }

        explored.push_back(smallest);
        is_region_explored[smallest.region] = true;

        for (int connected_region = 0; connected_region < map_get_region_count(map); connected_region++) {
            if (!map_are_regions_connected(map, smallest.region, connected_region)) {
                continue;
            }
            if (is_region_explored[connected_region]) {
                continue;
            }

            MapRegionPathNode child = (MapRegionPathNode) {
                .parent = (int)explored.size() - 1,
                .region = connected_region,
                .cost = smallest.cost + 1
            };

            uint32_t frontier_index;
            for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                if (child.region == frontier[frontier_index].region) {
                    if (child.cost < frontier[frontier_index].cost) {
                        frontier[frontier_index] = child;
                    }
                    break;
                }
            }
            if (frontier_index < frontier.size()) {
                continue;
            }

            frontier.push_back(child);
        }
    } // End while not frontier empty

    // Assert that a path was found
    GOLD_ASSERT(end_node.cost != -1);

    // Build region path
    MapRegionPathNode current = end_node;
    while (current.parent != -1) {
        path.push_back(current.region);
        current = explored[current.parent];
    }

    return path;
}

ivec2 map_get_region_connection_cell_closest_to_cell(const Map& map, ivec2 cell, int to_region) {
    int from_region = map_get_region(map, cell);

    GOLD_ASSERT(!map.region_connections[from_region][to_region].empty());
    ivec2 nearest_connection_cell = ivec2(-1, -1);
    for (ivec2 connection_cell : map.region_connections[from_region][to_region]) {
        if (nearest_connection_cell.x == -1 ||
                ivec2::manhattan_distance(connection_cell, cell) <
                ivec2::manhattan_distance(nearest_connection_cell, cell)) {
            nearest_connection_cell = connection_cell;
        }
    }

    return nearest_connection_cell;
}

void map_pathfind(const Map& map, CellLayer layer, ivec2 from, ivec2 to, int cell_size, std::vector<ivec2>* path, uint32_t ignore, std::vector<ivec2>* ignore_cells) {
    ZoneScoped;

    static const int EXPLORED_INDEX_NOT_EXPLORED = -1;
    static const int EXPLORED_INDEX_IGNORE_CELL = -2;

    path->clear();

    // Don't bother pathing to the unit's cell
    if (from == to) {
        return;
    }

    // If pathing into unwalkable territory, correct target
    ivec2 original_to = to;
    to = map_pathfind_correct_target(map, layer, from, to, ignore, ignore_cells);
    bool allow_squirreling = (ignore & MAP_OPTION_ALLOW_PATH_SQUIRRELING) == MAP_OPTION_ALLOW_PATH_SQUIRRELING;
    if (to != original_to && ivec2::manhattan_distance(from, to) < 3 &&
            map_get_cell(map, layer, original_to).type == CELL_UNIT && !allow_squirreling) {
        return;
    }

    // Find an alternate cell for large units
    if (cell_size > 1 && map_is_cell_rect_occupied(map, layer, to, cell_size, from, ignore)) {
        ivec2 nearest_alternative;
        int nearest_alternative_distance = -1;
        for (int x = 0; x < cell_size; x++) {
            for (int y = 0; y < cell_size; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }

                ivec2 alternative = to - ivec2(x, y);
                if (map_is_cell_rect_in_bounds(map, alternative, cell_size) &&
                    !map_is_cell_rect_occupied(map, layer, alternative, cell_size, from, ignore)) {
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
    bool avoid_landmines = (ignore & MAP_OPTION_AVOID_LANDMINES) == MAP_OPTION_AVOID_LANDMINES;
    MapPathNode path_end;

    // Fill the ignore cells into explored indices array so that we can do a constant time lookup to see if a cell should be ignored
    if (ignore_cells != NULL) {
        for (ivec2 ignore_cell : *ignore_cells) {
            explored_indices[ignore_cell.x + (ignore_cell.y * map.width)] = EXPLORED_INDEX_IGNORE_CELL;
        }
    }

    std::vector<int> region_path; 
    ivec2 heuristic_cell; 
    bool no_region_path = (ignore & MAP_OPTION_NO_REGION_PATH) == MAP_OPTION_NO_REGION_PATH;
    if (no_region_path || layer == CELL_LAYER_SKY || map_get_region(map, from) == map_get_region(map, to)) {
        heuristic_cell = to;
    } else {
        region_path = map_get_region_path(map, map_get_region(map, from), map_get_region(map, to));
        heuristic_cell = map_get_region_connection_cell_closest_to_cell(map, from, region_path.back());
    }

    frontier.push_back((MapPathNode) {
        .parent = -1,
        .cell = from,
        .cost = 0
    });

    while (!frontier.empty()) {
        // Find the smallest path
        uint32_t smallest_index = 0;
        for (uint32_t i = 1; i < frontier.size(); i++) {
            if (frontier[i].score(heuristic_cell) < frontier[smallest_index].score(heuristic_cell)) {
                smallest_index = i;
            }
        }

        // Pop the smallest path
        MapPathNode smallest = frontier[smallest_index];
        frontier[smallest_index] = frontier.back();
        frontier.pop_back();

        // If it's the solution, return it
        if (smallest.cell == to) {
            found_path = true;
            path_end = smallest;
            break;
        }

        // Check if we hit the next region target
        if (region_path.size() > 1 && map_get_region(map, smallest.cell) == region_path.back()) {
            region_path.pop_back();
            heuristic_cell = region_path.back() == map_get_region(map, to) 
                ? to 
                : map_get_region_connection_cell_closest_to_cell(map, smallest.cell, region_path.back());

            // Clear the frontier once we get to a new region.
            // This helps ensure path completion on long, roundabout paths
            // by making it so that we prioritize new cells in the new region
            // instead of old cells from earlier in the pathing process.
            // It also means we can get away with less frontier comparisons.
            frontier.clear();
        // If we've reached the region of the to cell, then clear the region path and just path directly to our final target.
        // This helps handle special cases where sometimes we pathfind into our target region on the way to an intermediate region.
        // In these cases, the pathfinding was taking the unit all the way to the intermediate region and then doubling back to the
        // target region. This logic prevents the doubling back and lets us path straight to our goal.
        } else if (region_path.size() > 1 && map_get_region(map, smallest.cell) == map_get_region(map, to)) {
            region_path.clear();
            heuristic_cell = to;
            frontier.clear();
        }

        // Otherwise, add this tile to the explored list
        explored.push_back(smallest);
        explored_indices[smallest.cell.x + (smallest.cell.y * map.width)] = (int)explored.size() - 1;
        if (ivec2::manhattan_distance(explored.back().cell, heuristic_cell) < ivec2::manhattan_distance(explored[closest_explored].cell, heuristic_cell)) {
            closest_explored = (int)explored.size() - 1;
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
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_IVEC2[direction],
                .cost = smallest.cost + (direction % 2 == 1 ? 3 : 2)
            };

            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(map, child.cell, cell_size)) {
                continue;
            }

            // Skip occupied cells (unless the child is the goal. this avoids worst-case pathing)
            if (map_is_cell_rect_occupied(map, layer, child.cell, cell_size, from, ignore) &&
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
                if (child.score(heuristic_cell) < frontier[frontier_index].score(heuristic_cell)) {
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
    while (current.parent != -1) {
        path->push_back(current.cell);
        current = explored[current.parent];
    }
    std::reverse(path->begin(), path->end());
}

void map_get_ideal_mine_exit_path(const Map& map, ivec2 mine_cell, ivec2 hall_cell, std::vector<ivec2>* path) {
    ivec2 rally_cell = map_get_nearest_cell_around_rect(map, CELL_LAYER_GROUND, mine_cell + ivec2(1, 1), 1, hall_cell, 4, MAP_OPTION_IGNORE_MINERS);
    ivec2 mine_exit_cell = map_get_exit_cell(map, CELL_LAYER_GROUND, mine_cell, 3, 1, rally_cell, MAP_OPTION_IGNORE_MINERS);
    GOLD_ASSERT(mine_exit_cell.x != -1);

    path->clear();
    map_pathfind(map, CELL_LAYER_GROUND, mine_exit_cell, rally_cell, 1, path, MAP_OPTION_IGNORE_MINERS | MAP_OPTION_NO_REGION_PATH, NULL);
    path->push_back(mine_exit_cell);
}
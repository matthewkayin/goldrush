#include "map.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "lcg.h"
#include <unordered_map>

static const ivec2 MAP_PLAYER_SPAWN_SIZE = ivec2(11, 11);
static const int MAP_PLAYER_SPAWN_MARGIN = 11;

struct PoissonDiskParams {
    std::vector<int> avoid_values;
    int disk_radius;
    bool allow_unreachable_cells;
    ivec2 margin;
};

SpriteName map_wall_autotile_lookup(uint32_t neighbors);
bool map_is_poisson_point_valid(const Map& map, const PoissonDiskParams& params, ivec2 point);
std::vector<ivec2> map_poisson_disk(const Map& map, PoissonDiskParams& params);

void map_init(Map& map, Noise& noise, std::vector<ivec2>& player_spawns, std::vector<ivec2>& goldmine_cells) {
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

    // Widen gaps that are too narrow
    for (int y = 0; y < noise.height; y++) {
        for (int x = 0; x < noise.width; x++) {
            if (noise.map[x + (y * noise.width)] == -1 || noise.map[x + (y * noise.width)] == 2) {
                continue;
            }

            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                ivec2 wall = ivec2(x, y) + DIRECTION_IVEC2[direction];
                if (!map_is_cell_in_bounds(map, wall)) {
                    continue;
                }
                if (noise.map[wall.x + (wall.y * noise.width)] > noise.map[x + (y * noise.width)]) {
                    for (int step = 0; step < 3; step++) {
                        ivec2 opposite = ivec2(x, y) - (DIRECTION_IVEC2[direction] * (step + 1));
                        if (map_is_cell_in_bounds(map, opposite) && noise.map[opposite.x + (opposite.y * noise.width)] > noise.map[x + (y * noise.width)]) {
                            noise.map[opposite.x + (opposite.y * noise.width)] = noise.map[x + (y * noise.width)];
                        }
                    }
                }
            }
        }
    }

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

    // Calculate unreachable cells
    {
        // Assume any previously unreachable cells are now empty / reachable
        for (int index = 0; index < map.width * map.height; index++) {
            if (map.cells[index] == CELL_UNREACHABLE) {
                map.cells[index] = CELL_EMPTY;
            }
        }

        // Determine map "islands"
        std::vector<int> map_tile_islands = std::vector<int>(map.width * map.height, -1);
        std::vector<int> island_size;

        while (true) {
            // Set index equal to the index of the first unassigned tile in the array
            int index;
            for (index = 0; index < map.width * map.height; index++) {
                if (map.cells[index] == CELL_BLOCKED) {
                    continue;
                }

                if (map_tile_islands[index] == -1) {
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
                EntityId cell_value = map.cells[next.x + (next.y * map.width)];
                if (cell_value == CELL_BLOCKED) {
                    continue;
                }

                // skip this because we've already explored it
                if (map_tile_islands[next.x + (next.y * map.width)] != -1) {
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
        for (int island_index = 1; island_index < island_size.size(); island_index++) {
            if (island_size[island_index] > island_size[biggest_island]) {
                biggest_island = island_index;
            }
        }
        // Everything that's not on the main "island" is considered blocked
        // This makes it so that we don't place any player spawns or gold at these locations
        for (int index = 0; index < map.width * map.height; index++) {
            if (map.cells[index] == CELL_EMPTY && map_tile_islands[index] != biggest_island) {
                map.cells[index] = CELL_UNREACHABLE;
            }
        }
    }
    // End calculate unreachable cells

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
                        !map_is_cell_rect_occupied(map, next, MAP_PLAYER_SPAWN_SIZE)) {
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
                            if (map_is_tile_ramp(map, ivec2(x, y)) || map_get_cell(map, ivec2(x, y)) != CELL_EMPTY) {
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
            int spawn_index = lcg_rand() % MAX_PLAYERS;
            while (player_spawns[spawn_index].x != -1) {
                spawn_index = (spawn_index + 1) % MAX_PLAYERS;
            }
            player_spawns[spawn_index] = spawn_point;
        }
    }
    // End determine player spawns

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
            ivec2 mine_cell = player_spawns[player_id];
            // TODO: create gold mine
            params.avoid_values[mine_cell.x + (mine_cell.y * map.width)] = params.disk_radius;

            // TODO: alter player spawn based on gold mine location
            GOLD_ASSERT(player_spawns[player_id].x != -1);
        }

        // Generate the avoid values
        for (int index = 0; index < map.width * map.height; index++) {
            if (map.cells[index] == CELL_BLOCKED || map_is_tile_ramp(map, ivec2(index % map.width, index / map.width))) {
                params.avoid_values[index] = 4;
            }
        }

        goldmine_cells = map_poisson_disk(map, params);
        for (ivec2 goldmine_cell : goldmine_cells) {
            params.avoid_values[goldmine_cell.x + (goldmine_cell.y * map.width)] = 4;
        }
    }
    // End generate gold mines

    // Generate decorations
    {
        params.disk_radius = 16;
        params.allow_unreachable_cells = true;
        params.margin = ivec2(0, 0);
        std::vector<ivec2> decoration_cells = map_poisson_disk(map, params);
        for (ivec2 cell : decoration_cells) {
            map.cells[cell.x + (cell.y * map.width)] = CELL_DECORATION_1 + (lcg_rand() % 5);
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

bool map_is_poisson_point_valid(const Map& map, const PoissonDiskParams& params, ivec2 point) {
    if (point.x < params.margin.x || point.x >= map.width - params.margin.x || point.y < params.margin.y || point.y >= map.height - params.margin.y) {
        return false;
    }

    EntityId cell_value = map_get_cell(map, point);
    if (cell_value == CELL_UNREACHABLE && !params.allow_unreachable_cells) {
        return false;
    } else if (cell_value != CELL_EMPTY) {
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

std::vector<ivec2> map_poisson_disk(const Map& map, PoissonDiskParams& params) {
    std::vector<ivec2> sample;
    std::vector<ivec2> frontier;

    ivec2 first;
    do {
        first.x = 1 + (lcg_rand() % (map.width - 2));
        first.y = 1 + (lcg_rand() % (map.height - 2));
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
        int next_index = lcg_rand() % frontier.size();
        ivec2 next = frontier[next_index];

        int child_attempts = 0;
        bool child_is_valid = false;
        ivec2 child;
        while (!child_is_valid && child_attempts < 30) {
            child_attempts++;
            child = next + circle_offset_points[lcg_rand() % circle_offset_points.size()];
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
    return !(cell.x < 0 || cell.y < 0 || cell.x >= map.width || cell.y >= map.height);
}

bool map_is_cell_rect_in_bounds(const Map& map, ivec2 cell, ivec2 size) {
    return !(cell.x < 0 || cell.y < 0 || cell.x + size.x >= map.width || cell.y + size.y >= map.height);
}

Tile map_get_tile(const Map& map, ivec2 cell) {
    return map.tiles[cell.x + (cell.y * map.width)];
}

EntityId map_get_cell(const Map& map, ivec2 cell) {
    return map.cells[cell.x + (cell.y * map.width)];
}

bool map_is_tile_ramp(const Map& map, ivec2 cell) {
    uint8_t tile_sprite = map.tiles[cell.x + (cell.y * map.width)].sprite;
    return tile_sprite >= SPRITE_TILE_WALL_SOUTH_STAIR_LEFT && tile_sprite <= SPRITE_TILE_WALL_WEST_STAIR_BOTTOM;
}

bool map_is_cell_rect_same_elevation(const Map& map, ivec2 cell, ivec2 size) {
    for (int y = cell.y; y < cell.y + size.y; y++) {
        for (int x = cell.x; x < cell.x + size.x; x++) {
            if (map_get_tile(map, ivec2(x, y)).elevation != map_get_tile(map, cell).elevation) {
                return false;
            }
        }
    }

    return true;
}

bool map_is_cell_rect_occupied(const Map& map, ivec2 cell, ivec2 size) {
    for (int y = cell.y; y < cell.y + size.y; y++) {
        for (int x = cell.x; x < cell.x + size.x; x++) {
            if (map_get_cell(map, ivec2(x, y)) != CELL_EMPTY) {
                return false;
            }
        }
    }

    return true;
}

void map_set_cell_rect(Map& map, ivec2 cell, int size, EntityId value) {
    for (int y = cell.y; y < cell.y + size; y++) {
        for (int x = cell.x; x < cell.x + size; x++) {
            map.cells[x + (y * map.width)] = value;
        }
    }
}
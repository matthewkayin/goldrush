#include "match.h"

#include "engine.h"
#include "logger.h"
#include "noise.h"
#include "network.h"
#include "lcg.h"

const std::unordered_map<MapSize, map_size_t> MAP_SIZE_DATA = {
    { MAP_SIZE_SMALL, (map_size_t) {
        .tile_size = 96,
        .gold_disk_radius = 42
    }},
    { MAP_SIZE_MEDIUM, (map_size_t) {
        .tile_size = 128,
        .gold_disk_radius = 42
    }},
    { MAP_SIZE_LARGE, (map_size_t) {
        .tile_size = 164,
        .gold_disk_radius = 48
    }}
};

Tile wall_autotile_lookup(uint32_t neighbors) {
    switch (neighbors) {
        case 1:
        case 3:
        case 129:
        case 131:
            return TILE_WALL_NORTH_EDGE;
        case 4:
        case 6:
        case 12:
        case 14:
            return TILE_WALL_EAST_EDGE;
        case 16:
        case 24:
        case 48:
        case 56:
            return TILE_WALL_SOUTH_EDGE;
        case 64:
        case 96:
        case 192:
        case 224:
            return TILE_WALL_WEST_EDGE;
        case 7:
        case 15:
        case 135:
        case 143:
        case 66:
            return TILE_WALL_NE_CORNER;
        case 193:
        case 195:
        case 225:
        case 227:
        case 132:
            return TILE_WALL_NW_CORNER;
        case 112:
        case 120:
        case 240:
        case 248:
        case 72:
            return TILE_WALL_SW_CORNER;
        case 28:
        case 30:
        case 60:
        case 62:
        case 36:
            return TILE_WALL_SE_CORNER;
        case 2:
            return TILE_WALL_NE_INNER_CORNER;
        case 8:
            return TILE_WALL_SE_INNER_CORNER;
        case 32:
            return TILE_WALL_SW_INNER_CORNER;
        case 128:
            return TILE_WALL_NW_INNER_CORNER;
        default:
            return TILE_NULL;
    }
}

struct poisson_disk_params_t {
    std::vector<int> avoid_values;
    int disk_radius;
    bool allow_unreachable_cells;
    xy margin;
};

bool poisson_is_point_valid(const match_state_t& state, const poisson_disk_params_t& params, xy point) {
    if (point.x < params.margin.x || point.x >= state.map_width - params.margin.x || point.y < params.margin.y || point.y >= state.map_height - params.margin.y) {
        return false;
    }

    entity_id cell = map_get_cell(state, point);
    if (cell == CELL_UNREACHABLE && !params.allow_unreachable_cells) {
        return false;
    } else if (cell != CELL_EMPTY) {
        return false;
    }
    if (map_is_tile_ramp(state, point)) {
        return false;
    }

    for (int nx = point.x - params.disk_radius; nx < point.x + params.disk_radius + 1; nx++) {
        for (int ny = point.y - params.disk_radius; ny < point.y + params.disk_radius + 1; ny++) {
            xy near_point = xy(nx, ny);
            if (!map_is_cell_in_bounds(state, near_point)) {
                continue;
            }
            int avoid_value = params.avoid_values[near_point.x + (near_point.y * state.map_width)];
            if (avoid_value != 0 && xy::manhattan_distance(point, near_point) <= avoid_value) {
                return false;
            }
        }
    }

    return true;
}

std::vector<xy> poisson_disk(const match_state_t& state, poisson_disk_params_t params) {
    std::vector<xy> sample;
    std::vector<xy> frontier;

    xy first;
    do {
        first.x = 1 + (lcg_rand() % (state.map_width - 2));
        first.y = 1 + (lcg_rand() % (state.map_height - 2));
    } while (!poisson_is_point_valid(state, params, first));

    frontier.push_back(first);
    sample.push_back(first);
    params.avoid_values[first.x + (first.y * state.map_width)] = params.disk_radius;

    std::vector<xy> circle_offset_points;
    {
        int x = 0;
        int y = params.disk_radius;
        int d = 3 - 2 * params.disk_radius;
        circle_offset_points.push_back(xy(x, y));
        circle_offset_points.push_back(xy(-x, y));
        circle_offset_points.push_back(xy(x, -y));
        circle_offset_points.push_back(xy(-x, -y));
        circle_offset_points.push_back(xy(y, x));
        circle_offset_points.push_back(xy(-y, x));
        circle_offset_points.push_back(xy(y, -x));
        circle_offset_points.push_back(xy(-y, -x));
        while (y >= x) {
            if (d > 0) {
                y--;
                d += 4 * (x - y) + 10;
            } else {
                d += 4 * x + 6;
            }
            x++;
            circle_offset_points.push_back(xy(x, y));
            circle_offset_points.push_back(xy(-x, y));
            circle_offset_points.push_back(xy(x, -y));
            circle_offset_points.push_back(xy(-x, -y));
            circle_offset_points.push_back(xy(y, x));
            circle_offset_points.push_back(xy(-y, x));
            circle_offset_points.push_back(xy(y, -x));
            circle_offset_points.push_back(xy(-y, -x));
        }
    }

    while (!frontier.empty()) {
        int next_index = lcg_rand() % frontier.size();
        xy next = frontier[next_index];

        int child_attempts = 0;
        bool child_is_valid = false;
        xy child;
        while (!child_is_valid && child_attempts < 30) {
            child_attempts++;
            child = next + circle_offset_points[lcg_rand() % circle_offset_points.size()];
            child_is_valid = poisson_is_point_valid(state, params, child);
        }
        if (child_is_valid) {
            frontier.push_back(child);
            sample.push_back(child);
            params.avoid_values[child.x + (child.y * state.map_width)] = params.disk_radius;
        } else {
            frontier.erase(frontier.begin() + next_index);
        }
    }

    return sample;
}

void map_init(match_state_t& state, const noise_t& noise) {
    state.map_width = noise.width;
    state.map_height = noise.height;
    log_trace("Generating map. Size: %ux%u", state.map_width, state.map_height);

    state.map_cells = std::vector<entity_id>(state.map_width * state.map_height, CELL_EMPTY);
    state.map_mine_cells = std::vector<entity_id>(state.map_width * state.map_height, ID_NULL);
    state.map_tiles = std::vector<tile_t>(state.map_width * state.map_height, (tile_t) {
        .index = engine.tile_index[TILE_SAND],
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
                    if (!map_is_cell_in_bounds(state, xy(nx, ny))) {
                        continue;
                    }
                    if (noise.map[nx + (ny * noise.width)] > 0 && xy::manhattan_distance(xy(x, y), xy(nx, ny)) <= WATER_WALL_DIST) {
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
                xy wall = xy(x, y) + DIRECTION_XY[direction];
                if (!map_is_cell_in_bounds(state, wall)) {
                    continue;
                }
                if (noise.map[wall.x + (wall.y * noise.width)] > noise.map[x + (y * noise.width)]) {
                    for (int step = 0; step < 3; step++) {
                        xy opposite = xy(x, y) - (DIRECTION_XY[direction] * (step + 1));
                        if (map_is_cell_in_bounds(state, opposite) && noise.map[opposite.x + (opposite.y * noise.width)] > noise.map[x + (y * noise.width)]) {
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
                        if (!map_is_cell_in_bounds(state, xy(nx, ny))) {
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
    std::vector<xy> artifacts;
    do {
        std::fill(state.map_tiles.begin(), state.map_tiles.end(), (tile_t) {
            .index = 0,
            .elevation = 0
        });
        for (xy artifact : artifacts) {
            noise.map[artifact.x + (artifact.y * state.map_width)]--;
        }
        artifacts.clear();

        log_trace("Baking map tiles...");
        for (int y = 0; y < state.map_height; y++) {
            for (int x = 0; x < state.map_width; x++) {
                int index = x + (y * state.map_width);
                if (noise.map[index] >= 0) {
                    state.map_tiles[index].elevation = noise.map[index];
                    // First check if we need to place a regular wall here
                    uint32_t neighbors = 0;
                    if (noise.map[index] > 0) {
                        int8_t tile_elevation = noise.map[index];
                        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                            xy neighbor_cell = xy(x, y) + DIRECTION_XY[direction];
                            if (!map_is_cell_in_bounds(state, neighbor_cell)) {
                                continue;
                            }
                            int neighbor_index = neighbor_cell.x + (neighbor_cell.y * state.map_width);
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
                            state.map_tiles[index].index = new_index == 1 ? engine.tile_index[TILE_SAND3] : engine.tile_index[TILE_SAND2];
                        } else {
                            state.map_tiles[index].index = engine.tile_index[TILE_SAND];
                        }
                    // Wall tile 
                    } else {
                        state.map_tiles[index].index = engine.tile_index[wall_autotile_lookup(neighbors)];
                        if (state.map_tiles[index].index == engine.tile_index[TILE_NULL]) {
                            artifacts.push_back(xy(x, y));
                        }
                    }
                } else if (noise.map[index] == -1) {
                    uint32_t neighbors = 0;
                    // Check adjacent neighbors
                    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                        xy neighbor_cell = xy(x, y) + DIRECTION_XY[direction];
                        if (!map_is_cell_in_bounds(state, neighbor_cell) || 
                                noise.map[index] == noise.map[neighbor_cell.x + (neighbor_cell.y * state.map_width)]) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }
                    // Check diagonal neighbors
                    for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                        xy neighbor_cell = xy(x, y) + DIRECTION_XY[direction];
                        int prev_direction = direction - 1;
                        int next_direction = (direction + 1) % DIRECTION_COUNT;
                        if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                                (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                            continue;
                        }
                        if (!map_is_cell_in_bounds(state, neighbor_cell) || 
                                noise.map[index] == noise.map[neighbor_cell.x + (neighbor_cell.y * state.map_width)]) {
                            neighbors += DIRECTION_MASK[direction];
                        }
                    }
                    // Set the map tile based on the neighbors
                    state.map_tiles[index] = (tile_t) {
                        .index = (uint16_t)(engine.tile_index[TILE_WATER] + engine.neighbors_to_autotile_index[neighbors]),
                        .elevation = 0
                    };
                // End else if tile is water
                } 
            } // end for each x
        } // end for each y

        log_trace("Artifacts count: %u", artifacts.size());
    } while (!artifacts.empty());

    // Place front walls
    for (int index = 0; index < state.map_width * state.map_height; index++) {
        int previous = index - state.map_width;
        if (previous < 0) {
            continue;
        }
        if (state.map_tiles[previous].index == engine.tile_index[TILE_WALL_SOUTH_EDGE]) {
            state.map_tiles[index].index = engine.tile_index[TILE_WALL_SOUTH_FRONT];
        } else if (state.map_tiles[previous].index == engine.tile_index[TILE_WALL_SW_CORNER]) {
            state.map_tiles[index].index = engine.tile_index[TILE_WALL_SW_FRONT];
        } else if (state.map_tiles[previous].index == engine.tile_index[TILE_WALL_SE_CORNER]) {
            state.map_tiles[index].index = engine.tile_index[TILE_WALL_SE_FRONT];
        }
    }

    // Generate ramps
    std::vector<xy> stair_cells;
    for (int pass = 0; pass < 2; pass++) {
        for (int x = 0; x < state.map_width; x++) {
            for (int y = 0; y < state.map_height; y++) {
                tile_t tile = state.map_tiles[x + (y * state.map_width)];
                // Only generate ramps on straight edged walls
                if (!(tile.index == engine.tile_index[TILE_WALL_SOUTH_EDGE] ||
                    tile.index == engine.tile_index[TILE_WALL_NORTH_EDGE] ||
                    tile.index == engine.tile_index[TILE_WALL_WEST_EDGE] ||
                    tile.index == engine.tile_index[TILE_WALL_EAST_EDGE])) {
                    continue;
                }
                // Determine whether this is a horizontal or vertical stair
                xy step_direction = (tile.index == engine.tile_index[TILE_WALL_SOUTH_EDGE] ||
                                    tile.index == engine.tile_index[TILE_WALL_NORTH_EDGE])
                                        ? xy(-1, 0)
                                        : xy(0, -1);
                xy stair_min = xy(x, y);
                while (map_is_cell_in_bounds(state, stair_min) && state.map_tiles[stair_min.x + (stair_min.y * state.map_width)].index == tile.index) {
                    stair_min += step_direction;
                    if (tile.index == engine.tile_index[TILE_WALL_EAST_EDGE] || tile.index == engine.tile_index[TILE_WALL_WEST_EDGE]) {
                        xy adjacent_cell = stair_min + xy(tile.index == engine.tile_index[TILE_WALL_EAST_EDGE] ? 1 : -1, 0);
                        if (!map_is_cell_in_bounds(state, adjacent_cell) || map_get_cell(state, adjacent_cell) != CELL_EMPTY) {
                            break;
                        }
                    }
                }
                xy stair_max = xy(x, y);
                stair_min -= step_direction;
                step_direction = xy(step_direction.x * -1, step_direction.y * -1);
                while (map_is_cell_in_bounds(state, stair_max) && state.map_tiles[stair_max.x + (stair_max.y * state.map_width)].index == tile.index) {
                    stair_max += step_direction;
                    if (tile.index == engine.tile_index[TILE_WALL_EAST_EDGE] || tile.index == engine.tile_index[TILE_WALL_WEST_EDGE]) {
                        xy adjacent_cell = stair_min + xy(tile.index == engine.tile_index[TILE_WALL_EAST_EDGE] ? 1 : -1, 0);
                        if (!map_is_cell_in_bounds(state, adjacent_cell) || map_get_cell(state, adjacent_cell) != CELL_EMPTY) {
                            break;
                        }
                    }
                }
                stair_max -= step_direction;

                int stair_length = xy::manhattan_distance(stair_max, stair_min);
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
                for (xy stair_cell : stair_cells) {
                    int dist = std::min(xy::manhattan_distance(stair_cell, stair_min), xy::manhattan_distance(stair_cell, stair_max));
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
                for (xy cell = stair_min; cell != stair_max + step_direction; cell += step_direction) {
                    Tile stair_tile = TILE_NULL;
                    if (tile.index == engine.tile_index[TILE_WALL_NORTH_EDGE]) {
                        if (cell == stair_min) {
                            stair_tile = TILE_WALL_NORTH_STAIR_LEFT;
                        } else if (cell == stair_max) {
                            stair_tile = TILE_WALL_NORTH_STAIR_RIGHT;
                        } else {
                            stair_tile = TILE_WALL_NORTH_STAIR_CENTER;
                        }
                    } else if (tile.index == engine.tile_index[TILE_WALL_EAST_EDGE]) {
                        if (cell == stair_min) {
                            stair_tile = TILE_WALL_EAST_STAIR_TOP;
                        } else if (cell == stair_max) {
                            stair_tile = TILE_WALL_EAST_STAIR_BOTTOM;
                        } else {
                            stair_tile = TILE_WALL_EAST_STAIR_CENTER;
                        }
                    } else if (tile.index == engine.tile_index[TILE_WALL_WEST_EDGE]) {
                        if (cell == stair_min) {
                            stair_tile = TILE_WALL_WEST_STAIR_TOP;
                        } else if (cell == stair_max) {
                            stair_tile = TILE_WALL_WEST_STAIR_BOTTOM;
                        } else {
                            stair_tile = TILE_WALL_WEST_STAIR_CENTER;
                        }
                    } else if (tile.index == engine.tile_index[TILE_WALL_SOUTH_EDGE]) {
                        if (cell == stair_min) {
                            stair_tile = TILE_WALL_SOUTH_STAIR_LEFT;
                        } else if (cell == stair_max) {
                            stair_tile = TILE_WALL_SOUTH_STAIR_RIGHT;
                        } else {
                            stair_tile = TILE_WALL_SOUTH_STAIR_CENTER;
                        }
                    }

                    state.map_tiles[cell.x + (cell.y * state.map_width)].index = engine.tile_index[stair_tile];
                    Tile south_front_tile = TILE_NULL;
                    if (stair_tile == TILE_WALL_SOUTH_STAIR_LEFT) {
                        south_front_tile = TILE_WALL_SOUTH_STAIR_FRONT_LEFT;
                    } else if (stair_tile == TILE_WALL_SOUTH_STAIR_RIGHT) {
                        south_front_tile = TILE_WALL_SOUTH_STAIR_FRONT_RIGHT;
                    } else if (stair_tile == TILE_WALL_SOUTH_STAIR_CENTER) {
                        south_front_tile = TILE_WALL_SOUTH_STAIR_FRONT_CENTER;
                    }
                    if (south_front_tile != TILE_NULL) {
                        state.map_tiles[cell.x + ((cell.y + 1) * state.map_width)].index = engine.tile_index[south_front_tile];
                    }
                } // End for cell in ramp
            } // End for each y
        } // End for each x
    } // End for each pass
    // End create ramps

    // Block all walls and water
    for (int index = 0; index < state.map_width * state.map_height; index++) {
        if (!(state.map_tiles[index].index == engine.tile_index[TILE_SAND] ||
                state.map_tiles[index].index == engine.tile_index[TILE_SAND2] ||
                state.map_tiles[index].index == engine.tile_index[TILE_SAND3] ||
                map_is_tile_ramp(state, xy(index % state.map_width, index / state.map_width)))) {
            state.map_cells[index] = CELL_BLOCKED;
        }
    }

    map_calculate_unreachable_cells(state);

    // Determine player spawns
    const xy player_spawn_size = xy(11, 11);
    const int player_spawn_margin = 11;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state.map_player_spawns[player_id] = xy(-1, -1);
    }
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        // Chooses diagonal directions clockwise beginning with NE
        int spawn_direction = 1 + (player_id * 2);

        xy start = xy(state.map_width / 2, state.map_height / 2) + xy(
                        DIRECTION_XY[spawn_direction].x * ((state.map_width / 2) - player_spawn_margin),
                        DIRECTION_XY[spawn_direction].y * ((state.map_height / 2) - player_spawn_margin));
        std::vector<xy> frontier;
        std::unordered_map<uint32_t, uint32_t> explored;
        frontier.push_back(start);
        xy spawn_point = xy(-1, -1);

        while (!frontier.empty() && spawn_point.x == -1) {
            uint32_t next_index = 0;
            for (uint32_t index = 1; index < frontier.size(); index++) {
                if (xy::manhattan_distance(frontier[index], start) < xy::manhattan_distance(frontier[next_index], start)) {
                    next_index = index;
                }
            }
            xy next = frontier[next_index];
            frontier.erase(frontier.begin() + next_index);

            if (map_is_cell_rect_same_elevation(state, next, player_spawn_size) && 
                    !map_is_cell_rect_occupied(state, next, player_spawn_size)) {
                xy candidate = next;
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
                        if (!map_is_cell_in_bounds(state, xy(x, y))) {
                            continue;
                        }
                        if (map_is_tile_ramp(state, xy(x, y)) || map_get_cell(state, xy(x, y)) != CELL_EMPTY) {
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

            explored[next.x + (next.y * state.map_width)] = 1;
            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                xy child = next + DIRECTION_XY[direction];
                if (!map_is_cell_rect_in_bounds(state, child, player_spawn_size.x)) {
                    continue;
                }
                if (explored.find(child.x + (child.y * state.map_width)) != explored.end()) {
                    continue;
                }
                bool is_in_frontier = false;
                for (xy cell : frontier) {
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
        while (state.map_player_spawns[spawn_index].x != -1) {
            spawn_index = (spawn_index + 1) % MAX_PLAYERS;
        }
        state.map_player_spawns[spawn_index] = spawn_point;
    }

    log_trace("Generating gold mines...");

    poisson_disk_params_t params = (poisson_disk_params_t) {
        .avoid_values = std::vector<int>(state.map_width * state.map_height, 0),
        .disk_radius = MAP_SIZE_DATA.at((MapSize)network_get_match_setting(MATCH_SETTING_MAP_SIZE)).gold_disk_radius,
        .allow_unreachable_cells = false,
        .margin = xy(5, 5)
    };

    // Place a gold mine on each player's spawn
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        xy mine_cell = state.map_player_spawns[player_id];
        entity_create_gold_mine(state, mine_cell, 5000);
        params.avoid_values[mine_cell.x + (mine_cell.y * state.map_width)] = params.disk_radius;

        state.map_player_spawns[player_id] = map_get_player_spawn_town_hall_cell(state, mine_cell);
        GOLD_ASSERT(state.map_player_spawns[player_id].x != -1);
    }

    // Generate the avoid values
    for (int index = 0; index < state.map_width * state.map_height; index++) {
        if (state.map_cells[index] == CELL_BLOCKED || map_is_tile_ramp(state, xy(index % state.map_width, index / state.map_width))) {
            params.avoid_values[index] = 4;
        }
    }

    std::vector<xy> gold_sample = poisson_disk(state, params);
    for (uint32_t patch_id = 0; patch_id < gold_sample.size(); patch_id++) {
        entity_create_gold_mine(state, gold_sample[patch_id], 5000);
    }
    // Recalculate unreachables in case the gold cells blocked anything
    map_calculate_unreachable_cells(state);

    // Fill in the gold values in the poisson avoid table so that decorations are not placed too close to gold
    for (const entity_t& entity : state.entities) {
        // All entities should be gold at this point anyways but it doesn't hurt to check
        if (entity.type == ENTITY_GOLD_MINE) {
            params.avoid_values[entity.cell.x + (entity.cell.y * state.map_width)] = 4;
        }
    }
    // Add player spawns to the avoid values
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        params.avoid_values[state.map_player_spawns[player_id].x + (state.map_player_spawns[player_id].y * state.map_width)] = 16;
    }

    // Generate decorations
    // The poisson avoid_values array is copied by value so we can re-use it for the next poisson disk
    log_trace("Generating decorations...");
    params.disk_radius = 16;
    params.allow_unreachable_cells = true;
    params.margin = xy(0, 0);
    std::vector<xy> decoration_cells = poisson_disk(state, params);
    for (xy cell : decoration_cells) {
        state.map_cells[cell.x + (cell.y * state.map_width)] = CELL_DECORATION_1 + (lcg_rand() % 5);
    }
}

void map_calculate_unreachable_cells(match_state_t& state) {
    // Assume any previously unreachable cells are now empty / reachable
    for (int index = 0; index < state.map_width * state.map_height; index++) {
        if (state.map_cells[index] == CELL_UNREACHABLE) {
            state.map_cells[index] = CELL_EMPTY;
        }
    }

    // Determine map "islands"
    std::vector<int> map_tile_islands = std::vector<int>(state.map_width * state.map_height, -1);
    std::vector<int> island_size;

    while (true) {
        // Set index equal to the index of the first unassigned tile in the array
        int index;
        for (index = 0; index < state.map_width * state.map_height; index++) {
            if (state.map_cells[index] == CELL_BLOCKED) {
                continue;
            }
            if (state.map_cells[index] < CELL_EMPTY && !entity_is_unit(state.entities.get_by_id(state.map_cells[index]).type)) {
                continue;
            }

            if (map_tile_islands[index] == -1) {
                break;
            }
        }
        if (index == state.map_width * state.map_height) {
            break; // island mapping is complete
        }

        // Determine the next island index
        int island_index = island_size.size();
        island_size.push_back(0);

        // Flood fill this island index so that every tile that is reachable from 
        // the start tile (the tile determined by index) has the same island index
        std::vector<xy> frontier;
        frontier.push_back(xy(index % state.map_width, index / state.map_width));

        while (!frontier.empty()) {
            xy next = frontier[0];
            frontier.erase(frontier.begin());

            if (!map_is_cell_in_bounds(state, next)) {
                continue;
            }
            entity_id cell_value = state.map_cells[next.x + (next.y * state.map_width)];
            if (cell_value == CELL_BLOCKED) {
                continue;
            }
            // If cell is a building or gold, consider it blocked (but ignore units)
            if (cell_value < CELL_EMPTY && !entity_is_unit(state.entities.get_by_id(cell_value).type)) {
                continue;
            }

            // skip this because we've already explored it
            if (map_tile_islands[next.x + (next.y * state.map_width)] != -1) {
                continue;
            }

            map_tile_islands[next.x + (next.y * state.map_width)] = island_index;
            island_size[island_index]++;

            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                frontier.push_back(next + DIRECTION_XY[direction]);
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
    for (int index = 0; index < state.map_width * state.map_height; index++) {
        if (state.map_cells[index] == CELL_EMPTY && map_tile_islands[index] != biggest_island) {
            state.map_cells[index] = CELL_UNREACHABLE;
        }
    }
}

void map_recalculate_unreachable_cells(match_state_t& state, xy cell) {
    std::vector<xy> frontier;

    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
        xy child = cell + DIRECTION_XY[direction];
        if (map_is_cell_in_bounds(state, child) && map_get_cell(state, child) == CELL_UNREACHABLE) {
            frontier.push_back(child);
        }
    }

    while (!frontier.empty()) {
        xy next = frontier[0];
        frontier.erase(frontier.begin());

        if (!map_is_cell_in_bounds(state, next) || map_get_cell(state, next) != CELL_UNREACHABLE) {
            continue;
        }

        state.map_cells[next.x + (next.y * state.map_width)] = CELL_EMPTY;

        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            frontier.push_back(next + DIRECTION_XY[direction]);
        }
    }
}

bool map_is_cell_in_bounds(const match_state_t& state, xy cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= state.map_width || cell.y >= state.map_height);
}

bool map_is_cell_rect_in_bounds(const match_state_t& state, xy cell, int cell_size) {
    return !(cell.x < 0 || cell.y < 0 || cell.x + cell_size - 1 >= state.map_width || cell.y + cell_size - 1 >= state.map_height);
}

tile_t map_get_tile(const match_state_t& state, xy cell) {
    return state.map_tiles[cell.x + (cell.y * state.map_width)];
}

entity_id map_get_cell(const match_state_t& state, xy cell) {
    return state.map_cells[cell.x + (cell.y * state.map_width)];
}

bool map_is_cell_rect_equal_to(const match_state_t& state, xy cell, int cell_size, entity_id value) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state.map_cells[x + (y * state.map_width)] != value) {
                return false;
            }
        }
    }

    return true;
}

void map_set_cell_rect(match_state_t& state, xy cell, int cell_size, entity_id value) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            state.map_cells[x + (y * state.map_width)] = value;
        }
    }
}

bool map_is_cell_rect_occupied(const match_state_t& state, xy cell, int cell_size, xy origin, bool gold_walk) {
    return map_is_cell_rect_occupied(state, cell, xy(cell_size, cell_size), origin, gold_walk);
}

bool map_is_cell_rect_occupied(const match_state_t& state, xy cell, xy size, xy origin, bool gold_walk) {
    entity_id origin_id = origin.x == -1 ? ID_NULL : map_get_cell(state, origin);
    SDL_Rect origin_rect = (SDL_Rect) { .x = origin.x, .y = origin.y, .w = size.x, .h = size.y };

    for (int y = cell.y; y < cell.y + size.y; y++) {
        for (int x = cell.x; x < cell.x + size.x; x++) {
            entity_id cell_id = map_get_cell(state, xy(x, y));
            if (cell_id > CELL_EMPTY) {
                return true;
            }
            if (cell_id == CELL_EMPTY || sdl_rect_has_point(origin_rect, xy(x, y))) {
                continue;
            }  
            const entity_t& entity = state.entities.get_by_id(cell_id);
            if (!entity_is_unit(entity.type)) {
                return true;
            }
            if (origin_id != ID_NULL && xy::manhattan_distance(origin, xy(x, y)) > 5) {
                continue;
            }
            if (gold_walk && (entity.mode == MODE_UNIT_OUT_MINE || entity_is_mining(state, entity))) {
                continue;
            }

            return true;
        }
    }

    return false;
}

xy map_get_player_spawn_town_hall_cell(const match_state_t& state, xy mine_cell) {
    xy nearest_cell;
    int nearest_cell_dist = -1;
    xy rect_position = mine_cell - xy(4, 4);
    int rect_size = 11;
    int start_size = entity_cell_size(ENTITY_HALL);
    xy start = mine_cell;

    xy cell_begin[4] = { 
        rect_position + xy(-start_size, -(start_size - 1)),
        rect_position + xy(-(start_size - 1), rect_size),
        rect_position + xy(rect_size, rect_size - 1),
        rect_position + xy(rect_size - 1, -start_size)
    };
    xy cell_end[4] = { 
        xy(cell_begin[0].x, rect_position.y + rect_size - 1),
        xy(rect_position.x + rect_size - 1, cell_begin[1].y),
        xy(cell_begin[2].x, cell_begin[0].y),
        xy(cell_begin[0].x + 1, cell_begin[3].y)
    };

    xy cell_step[4] = { xy(0, 1), xy(1, 0), xy(0, -1), xy(-1, 0) };
    uint32_t index = 0;
    xy cell = cell_begin[index];
    while (index < 4) {
        // check if cell is valid
        bool cell_is_valid = map_is_cell_rect_in_bounds(state, cell, start_size);
        if (cell_is_valid) {
            cell_is_valid = !map_is_cell_rect_occupied(state, cell, start_size, xy(-1, -1)) && 
                    map_is_cell_rect_same_elevation(state, cell, xy(start_size, start_size)) && 
                    map_get_tile(state, cell).elevation == map_get_tile(state, mine_cell).elevation &&
                    (nearest_cell_dist == -1 || xy::manhattan_distance(start, cell) < nearest_cell_dist);
        }
        // after the initial check, check the surrounding cells
        if (cell_is_valid) {
            const int STAIR_RADIUS = 2;
            for (int x = cell.x - STAIR_RADIUS; x < cell.x + start_size + STAIR_RADIUS + 1; x++) {
                for (int y = cell.y - STAIR_RADIUS; y < cell.y + start_size + STAIR_RADIUS + 1; y++) {
                    if (!map_is_cell_in_bounds(state, xy(x, y))) {
                        continue;
                    }
                    if (map_is_tile_ramp(state, xy(x, y)) || map_get_cell(state, xy(x, y)) != CELL_EMPTY) {
                        cell_is_valid = false;
                        break;
                    }
                }
                if (!cell_is_valid) {
                    break;
                }
            }
        }

        if (cell_is_valid) {
            nearest_cell = cell;
            nearest_cell_dist = xy::manhattan_distance(start, cell);
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
    return nearest_cell_dist != -1 ? nearest_cell : map_get_nearest_cell_around_rect(state, start, start_size, rect_position, rect_size, false);
}

// Returns the nearest cell around the rect relative to start_cell
// If there are no free cells around the rect in a radius of 1, then this returns the start cell
xy map_get_nearest_cell_around_rect(const match_state_t& state, xy start, int start_size, xy rect_position, int rect_size, bool allow_blocked_cells, xy ignore_cell) {
    xy nearest_cell;
    int nearest_cell_dist = -1;

    xy cell_begin[4] = { 
        rect_position + xy(-start_size, -(start_size - 1)),
        rect_position + xy(-(start_size - 1), rect_size),
        rect_position + xy(rect_size, rect_size - 1),
        rect_position + xy(rect_size - 1, -start_size)
    };
    xy cell_end[4] = { 
        xy(cell_begin[0].x, rect_position.y + rect_size - 1),
        xy(rect_position.x + rect_size - 1, cell_begin[1].y),
        xy(cell_begin[2].x, cell_begin[0].y),
        xy(cell_begin[0].x + 1, cell_begin[3].y)
    };
    xy cell_step[4] = { xy(0, 1), xy(1, 0), xy(0, -1), xy(-1, 0) };
    uint32_t index = 0;
    xy cell = cell_begin[index];
    while (index < 4) {
        if (map_is_cell_rect_in_bounds(state, cell, start_size) && cell != ignore_cell) {
            if (!map_is_cell_rect_occupied(state, cell, start_size, xy(-1, -1), allow_blocked_cells) && (nearest_cell_dist == -1 || xy::manhattan_distance(start, cell) < nearest_cell_dist)) {
                nearest_cell = cell;
                nearest_cell_dist = xy::manhattan_distance(start, cell);
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

bool map_is_tile_ramp(const match_state_t& state, xy cell) {
    uint16_t tile_index = state.map_tiles[cell.x + (cell.y * state.map_width)].index;
    return tile_index >= engine.tile_index[TILE_WALL_SOUTH_STAIR_LEFT] && tile_index <= engine.tile_index[TILE_WALL_WEST_STAIR_BOTTOM];
}

bool map_is_cell_rect_same_elevation(const match_state_t& state, xy cell, xy size) {
    for (int x = cell.x; x < cell.x + size.x; x++) {
        for (int y = cell.y; y < cell.y + size.y; y++) {
            if (state.map_tiles[x + (y * state.map_width)].elevation != state.map_tiles[cell.x + (cell.y * state.map_width)].elevation || 
                    map_is_tile_ramp(state, xy(x, y))) {
                return false;
            }
        }
    }

    return true;
}

void map_pathfind(const match_state_t& state, xy from, xy to, int cell_size, std::vector<xy>* path, bool gold_walk, std::vector<xy>* ignore_cells) {
    struct node_t {
        fixed cost;
        fixed distance;
        // The parent is the previous node stepped in the path to reach this node
        // It should be an index in the explored list or -1 if it is the start node
        int parent;
        xy cell;

        fixed score() const {
            return cost + distance;
        };
    };

    // Don't bother pathing to the unit's cell
    if (from == to) {
        path->clear();
        return;
    }

    // Find an alternate cell for large units
    if (cell_size > 1 && map_is_cell_rect_occupied(state, to, cell_size, from, gold_walk)) {
        xy nearest_alternative;
        int nearest_alternative_distance = -1;
        for (int x = 0; x < cell_size; x++) {
            for (int y = 0; y < cell_size; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }

                xy alternative = to - xy(x, y);
                if (map_is_cell_rect_in_bounds(state, alternative, cell_size) &&
                    !map_is_cell_rect_occupied(state, alternative, cell_size, from, gold_walk)) {
                    if (nearest_alternative_distance == -1 || xy::manhattan_distance(from, alternative) < nearest_alternative_distance) {
                        nearest_alternative = alternative;
                        nearest_alternative_distance = xy::manhattan_distance(from, alternative);
                    }
                }
            }
        }

        if (nearest_alternative_distance != -1) {
            to = nearest_alternative;
        }
    }

    std::vector<node_t> frontier;
    std::vector<node_t> explored;
    std::vector<int> explored_indices = std::vector<int>(state.map_width * state.map_height, -1);

    if (ignore_cells != NULL) {
        for (xy cell : *ignore_cells) {
            explored_indices[cell.x + (cell.y * state.map_width)] = 1;
        }
    }

    bool is_target_unreachable = false;
    for (int y = to.y; y < to.y + cell_size; y++) {
        for (int x = to.x; x < to.x + cell_size; x++) {
            entity_id cell_value = map_get_cell(state, xy(x, y));
            if (cell_value == CELL_BLOCKED || cell_value == CELL_UNREACHABLE) {
                is_target_unreachable = true;
                // Cause break on both inner and outer loops
                x = to.x + cell_size;
                y = to.y + cell_size;
            }
        }
    }
    if (is_target_unreachable) {
        // Reverse pathfind to find the nearest reachable cell
        frontier.push_back((node_t) {
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
            node_t smallest = frontier[smallest_index];
            frontier.erase(frontier.begin() + smallest_index);

            // If it's the solution, return it
            if (!map_is_cell_rect_occupied(state, smallest.cell, cell_size, from, gold_walk)) {
                to = smallest.cell;
                break; // breaks while frontier not empty
            }

            // Otherwise mark this cell as explored
            explored_indices[smallest.cell.x + (smallest.cell.y * state.map_width)] = 1;

            // Consider all children
            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                node_t child = (node_t) {
                    .cost = smallest.cost + (direction % 2 == 1 ? (fixed::from_int(3) / 2) : fixed::from_int(1)),
                    .distance = fixed::from_int(xy::manhattan_distance(smallest.cell + DIRECTION_XY[direction], to)),
                    .parent = -1,
                    .cell = smallest.cell + DIRECTION_XY[direction]
                };

                // Don't consider out of bounds children
                if (!map_is_cell_rect_in_bounds(state, child.cell, cell_size)) {
                    continue;
                }

                // Don't consider explored indices
                if (explored_indices[child.cell.x + (child.cell.y * state.map_width)] != -1) {
                    continue;
                }

                // Check if it's in the frontier
                uint32_t frontier_index;
                for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                    node_t& frontier_node = frontier[frontier_index];
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
        memset(&explored_indices[0], -1, state.map_width * state.map_height * sizeof(int));
    } // End if target_is_unreachable

    uint32_t closest_explored = 0;
    bool found_path = false;
    node_t path_end;

    frontier.push_back((node_t) {
        .cost = fixed::from_int(0),
        .distance = fixed::from_int(xy::manhattan_distance(from, to)),
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
        node_t smallest = frontier[smallest_index];
        frontier.erase(frontier.begin() + smallest_index);

        // If it's the solution, return it
        if (smallest.cell == to) {
            found_path = true;
            path_end = smallest;
            break;
        }

        // Otherwise, add this tile to the explored list
        explored.push_back(smallest);
        explored_indices[smallest.cell.x + (smallest.cell.y * state.map_width)] = explored.size() - 1;
        if (explored[explored.size() - 1].distance < explored[closest_explored].distance) {
            closest_explored = explored.size() - 1;
        }

        if (explored.size() > 1999) {
            log_trace("Pathfinding too long, going with closest explored...");
            break;
        }

        // Consider all children
        // Adjacent cells are considered first so that we can use information about them to inform whether or not a diagonal movement is allowed
        bool is_adjacent_direction_blocked[4] = { true, true, true, true };
        const int CHILD_DIRECTIONS[DIRECTION_COUNT] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST, 
                                                        DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST, DIRECTION_NORTHWEST };
        for (int direction_index = 0; direction_index < DIRECTION_COUNT; direction_index++) {
            int direction = CHILD_DIRECTIONS[direction_index];
            node_t child = (node_t) {
                .cost = smallest.cost + (direction % 2 == 1 ? (fixed::from_int(3) / 2) : fixed::from_int(1)),
                .distance = fixed::from_int(xy::manhattan_distance(smallest.cell + DIRECTION_XY[direction], to)),
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_XY[direction]
            };

            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(state, child.cell, cell_size)) {
                continue;
            }

            // Skip occupied cells (unless the child is the goal. this avoids worst-case pathing)
            if (map_is_cell_rect_occupied(state, child.cell, cell_size, from, gold_walk) &&
                !(child.cell == to && xy::manhattan_distance(smallest.cell, child.cell) == 1)) {
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
            if (explored_indices[child.cell.x + (child.cell.y * state.map_width)] != -1) {
                continue;
            }

            uint32_t frontier_index;
            for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                node_t& frontier_node = frontier[frontier_index];
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
    node_t current = found_path ? path_end : explored[closest_explored];
    path->clear();
    path->reserve(current.cost.integer_part() + 1);
    while (current.parent != -1) {
        path->insert(path->begin(), current.cell);
        current = explored[current.parent];
    }

    if (!path->empty()) {
        // Previously we allowed the algorithm to consider the target_cell even if it was blocked. This was done for efficiency's sake,
        // but if the target_cell really is blocked, we need to remove it from the path. The unit will path as close as they can.
        if ((*path)[path->size() - 1] == to && map_is_cell_rect_occupied(state, to, cell_size, from, gold_walk)) {
            path->pop_back();
        }
    }
}

bool map_is_cell_rect_revealed(const match_state_t& state, uint8_t player_id, xy cell, int cell_size) {
    uint8_t team = network_get_player(player_id).team;
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state.map_fog[team][x + (y * state.map_width)] > 0) {
                return true;
            }
        }
    }

    return false;
}

int map_get_fog(const match_state_t& state, uint8_t player_id, xy cell) {
    return state.map_fog[network_get_player(player_id).team][cell.x + (cell.y * state.map_width)];
}

bool map_is_cell_detected(const match_state_t& state, uint8_t player_id, xy cell) {
    return state.map_detection[network_get_player(player_id).team][cell.x + (cell.y * state.map_width)] > 0;
}

void map_fog_update(match_state_t& state, uint8_t player_id, xy cell, int cell_size, int sight, bool increment, bool has_detection) { 
    /*
    * This function does a raytrace from the cell center outwards to determine what this unit can see
    * Raytracing is done using Bresenham's Line Generation Algorithm (https://www.geeksforgeeks.org/bresenhams-line-generation-algorithm/)
    */

    uint8_t player_team = network_get_player(player_id).team;

    xy search_corners[4] = {
        cell - xy(sight, sight),
        cell + xy((cell_size - 1) + sight, -sight),
        cell + xy((cell_size - 1) + sight, (cell_size - 1) + sight),
        cell + xy(-sight, (cell_size - 1) + sight)
    };
    for (int search_index = 0; search_index < 4; search_index++) {
        xy search_goal = search_corners[search_index + 1 == 4 ? 0 : search_index + 1];
        xy search_step = DIRECTION_XY[(search_index * 2) + 2 == DIRECTION_COUNT 
                                            ? DIRECTION_NORTH 
                                            : (search_index * 2) + 2];
        for (xy line_end = search_corners[search_index]; line_end != search_goal; line_end += search_step) {
            xy line_start;
            switch (cell_size) {
                case 1:
                    line_start = cell;
                    break;
                case 3:
                    line_start = cell + xy(1, 1);
                    break;
                case 2:
                case 4: {
                    xy center_cell = cell_size == 2 ? cell : cell + xy(1, 1);
                    if (line_end.x < center_cell.x) {
                        line_start.x = center_cell.x;
                    } else if (line_end.x > center_cell.x + 1) {
                        line_start.x = center_cell.x + 1;
                    } else {
                        line_start.x = line_end.x;
                    }
                    if (line_end.y < center_cell.y) {
                        line_start.y = center_cell.y;
                    } else if (line_end.y > center_cell.y + 1) {
                        line_start.y = center_cell.y + 1;
                    } else {
                        line_start.y = line_end.y;
                    }
                    break;
                }
                default:
                    log_warn("cell size of %i not handled in map_fog_update", cell_size);
                    line_start = cell;
                    break;
            }

            // we want slope to be between 0 and 1
            // if "run" is greater than "rise" then m is naturally between 0 and 1, we will step with x in increments of 1 and handle y increments that are less than 1
            // if "rise" is greater than "run" (use_x_step is false) then we will swap x and y so that we can step with y in increments of 1 and handle x increments that are less than 1
            bool use_x_step = std::abs(line_end.x - line_start.x) >= std::abs(line_end.y - line_start.y);
            int slope = std::abs(2 * (use_x_step ? (line_end.y - line_start.y) : (line_end.x - line_start.x)));
            int slope_error = slope - std::abs((use_x_step ? (line_end.x - line_start.x) : (line_end.y - line_start.y)));
            xy line_step;
            xy line_opposite_step;
            if (use_x_step) {
                line_step = xy(1, 0) * (line_end.x >= line_start.x ? 1 : -1);
                line_opposite_step = xy(0, 1) * (line_end.y >= line_start.y ? 1 : -1);
            } else {
                line_step = xy(0, 1) * (line_end.y >= line_start.y ? 1 : -1);
                line_opposite_step = xy(1, 0) * (line_end.x >= line_start.x ? 1 : -1);
            }
            for (xy line_cell = line_start; line_cell != line_end; line_cell += line_step) {
                if (!map_is_cell_in_bounds(state, line_cell) || xy::euclidean_distance_squared(line_start, line_cell) > sight * sight) {
                    break;
                }

                if (increment) {
                    if (state.map_fog[player_team][line_cell.x + (line_cell.y * state.map_width)] == FOG_HIDDEN) {
                        state.map_fog[player_team][line_cell.x + (line_cell.y * state.map_width)] = 1;
                    } else {
                        state.map_fog[player_team][line_cell.x + (line_cell.y * state.map_width)]++;
                    }
                    if (has_detection) {
                        state.map_detection[player_team][line_cell.x + (line_cell.y * state.map_width)]++;
                    }
                } else {
                    state.map_fog[player_team][line_cell.x + (line_cell.y * state.map_width)]--;
                    if (has_detection) {
                        state.map_detection[player_team][line_cell.x + (line_cell.y * state.map_width)]--;
                    }

                    // Remember revealed entities
                    // First check for a mine
                    entity_id cell_value = state.map_mine_cells[line_cell.x + (line_cell.y * state.map_width)];
                    // If there's no mine, check the regular cell grid
                    if (cell_value == ID_NULL) {
                        cell_value = map_get_cell(state, line_cell);
                    }
                    if (cell_value < CELL_EMPTY) {
                        entity_t& entity = state.entities.get_by_id(cell_value);
                        if (!entity_is_unit(entity.type) && entity_is_selectable(entity) && entity.type != ENTITY_LAND_MINE) {
                            state.remembered_entities[player_team][cell_value] = (remembered_entity_t) {
                                .sprite_params = (render_sprite_params_t) {
                                    .sprite = entity_get_sprite(entity),
                                    .frame = entity_get_animation_frame(entity),
                                    .position = entity.position.to_xy(),
                                    .options = 0,
                                    .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLD_MINE ? (uint8_t)RECOLOR_NONE : network_get_player(entity.player_id).recolor_id
                                },
                                .cell = entity.cell,
                                .cell_size = entity_cell_size(entity.type)
                            };
                        }
                    } // End if cell value < cell empty
                } // End if !increment

                if (map_get_tile(state, line_cell).elevation > map_get_tile(state, line_start).elevation) {
                    break;
                }

                slope_error += slope;
                if (slope_error >= 0) {
                    line_cell += line_opposite_step;
                    slope_error -= 2 * std::abs((use_x_step ? (line_end.x - line_start.x) : (line_end.y - line_start.y)));
                }
            } // End for each line cell in line
        } // End for each line end from corner to corner
    } // End for each search index

    state.map_is_fog_dirty = true;
}
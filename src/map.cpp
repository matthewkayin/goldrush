#include "match.h"

#include "network.h"
#include "logger.h"
#include "lcg.h"

static const uint32_t MINE_GOLD_AMOUNT_LOW = 1000;
static const uint32_t MINE_GOLD_AMOUNT_HIGH = 3000;

Tile wall_autotile_lookup(uint32_t neighbors) {
    switch (neighbors) {
        case 1:
        case 3:
        case 129:
        case 131:
            return TILE_WALL_SOUTH_EDGE;
        case 4:
        case 6:
        case 12:
        case 14:
            return TILE_WALL_WEST_EDGE;
        case 16:
        case 24:
        case 48:
        case 56:
            return TILE_WALL_NORTH_EDGE;
        case 64:
        case 96:
        case 192:
        case 224:
            return TILE_WALL_EAST_EDGE;
        case 2:
            return TILE_WALL_SW_CORNER;
        case 8:
            return TILE_WALL_NW_CORNER;
        case 32:
            return TILE_WALL_NE_CORNER;
        case 128:
            return TILE_WALL_SE_CORNER;
        case 7:
        case 15:
        case 135:
        case 143:
            return TILE_WALL_SW_INNER_CORNER;
        case 28:
        case 30:
        case 60:
        case 62:
            return TILE_WALL_NW_INNER_CORNER;
        case 112:
        case 120:
        case 240:
        case 248:
            return TILE_WALL_NE_INNER_CORNER;
        case 193:
        case 195:
        case 225:
        case 227:
            return TILE_WALL_SE_INNER_CORNER;
        default:
            return TILE_NULL;
    }
}

void map_init(match_state_t& state, std::vector<xy>& player_spawns, MapName map_name, uint32_t width, uint32_t height) {
    log_trace("Generating map. Map name: %u. Map size: %ux%u", map_name, width, height);
    state.map_width = width;
    state.map_height = height;
    state.map_tiles = std::vector<tile_t>(state.map_width * state.map_height, (tile_t) {
        .index = 0,
        .elevation = 0,
        .is_ramp = 0
    });
    state.map_cells = std::vector<cell_t>(state.map_width * state.map_height, (cell_t) {
        .type = CELL_EMPTY,
        .value = 0
    });

    std::vector<tile_t> map_tiles_prebaked(state.map_width * state.map_height, (tile_t) {
        .index = TILE_SAND,
        .elevation = 0,
        .is_ramp = 0
    });
    switch (map_name) {
        case MAP_OASIS:
            map_gen_oasis(state, player_spawns, map_tiles_prebaked);
            break;
        case MAP_GOLD_TEST:
            map_gen_gold_test(state, player_spawns, map_tiles_prebaked);
            break;
        default:
            log_error("Map name of %i not handled", map_name);
            GOLD_ASSERT_MESSAGE(false, "Map name not handled");
            return;
    }

    // Bake map tiles
    log_trace("Baking map tiles...");
    for (int y = 0; y < state.map_height; y++) {
        for (int x = 0; x < state.map_width; x++) {
            int index = x + (y * state.map_width);
            state.map_tiles[index] = map_tiles_prebaked[index];
            if (map_tiles_prebaked[index].index == TILE_SAND) {
                // First check if we need to place a regular wall here
                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                    xy neighbor_cell = xy(x, y) + DIRECTION_XY[direction];
                    if (!map_is_cell_in_bounds(state, neighbor_cell)) {
                        continue;
                    }
                    if (map_tiles_prebaked[x + (y * state.map_width)].elevation < map_tiles_prebaked[neighbor_cell.x + (neighbor_cell.y * state.map_width)].elevation &&
                        map_tiles_prebaked[neighbor_cell.x + (neighbor_cell.y * state.map_width)].is_ramp == 0) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }

                // Regular sand tile 
                if (neighbors == 0) {
                    int new_index = lcg_rand() % 7;
                    if (new_index < 4 && index % 3 == 0) {
                        state.map_tiles[index].index = new_index == 1 ? TILE_DATA[TILE_SAND3].index : TILE_DATA[TILE_SAND2].index;
                    } else {
                        state.map_tiles[index].index = TILE_DATA[TILE_SAND].index;
                    }
                // Wall tile 
                } else {
                    state.map_tiles[index].index = TILE_DATA[wall_autotile_lookup(neighbors)].index;
                }
            } else if (map_tiles_prebaked[index].index == TILE_WATER) {
                uint32_t neighbors = 0;
                // Check adjacent neighbors
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    xy neighbor_cell = xy(x, y) + DIRECTION_XY[direction];
                    if (!map_is_cell_in_bounds(state, neighbor_cell) || 
                        map_tiles_prebaked[index].index == map_tiles_prebaked[neighbor_cell.x + (neighbor_cell.y * state.map_width)].index) {
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
                        map_tiles_prebaked[index].index == map_tiles_prebaked[neighbor_cell.x + (neighbor_cell.y * state.map_width)].index) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                // Set the map tile based on the neighbors
                state.map_tiles[index].index = TILE_DATA[TILE_WATER].index + neighbors_to_autotile_index[neighbors];
            // End else if tile is water
            } else {
                state.map_tiles[index].index = TILE_DATA[map_tiles_prebaked[index].index].index;
            }

            if (y != 0) {
                // Add the extended walls
                if (state.map_tiles[index - state.map_width].index == TILE_DATA[TILE_WALL_SW_CORNER].index) {
                    state.map_tiles[index].index = TILE_DATA[TILE_WALL_SW_FRONT].index;
                } else if (state.map_tiles[index - state.map_width].index == TILE_DATA[TILE_WALL_SOUTH_EDGE].index) {
                    state.map_tiles[index].index = TILE_DATA[TILE_WALL_SOUTH_FRONT].index;
                } else if (state.map_tiles[index - state.map_width].index == TILE_DATA[TILE_WALL_SE_CORNER].index) {
                    state.map_tiles[index].index = TILE_DATA[TILE_WALL_SE_FRONT].index;
                }
            }

            // Increase the elevation of every wall
            // Walls are created on lower elevation than the highground because of how the algorithm works, 
            // but we want them on higher elevation so that they y-sort with units properly
            if (state.map_tiles[index].index >= TILE_DATA[TILE_WALL_NW_CORNER].index) {
                state.map_tiles[index].elevation++;
            }

            // Block every wall on the cell grid
            if (state.map_tiles[index].index >= TILE_DATA[TILE_WALL_SOUTH_STAIR_LEFT].index && state.map_tiles[index].index <= TILE_DATA[TILE_WALL_WEST_STAIR_BOTTOM].index) {
                state.map_tiles[index].is_ramp = 1;
            }
            if ((state.map_tiles[index].index >= TILE_DATA[TILE_WALL_NW_CORNER].index && state.map_tiles[index].is_ramp != 1) ||
                (state.map_tiles[index].index >= TILE_DATA[TILE_WATER].index && state.map_tiles[index].index < TILE_DATA[TILE_WATER].index + 47)) {
                state.map_cells[index].type = CELL_BLOCKED;
            }
        } // end for each x
    } // end for each y
    // End bake tiles

    // Generate decorations
    log_trace("Generating decorations...");
    for (int i = 0; i < state.map_width * state.map_height; i++) {
        if (lcg_rand() % 40 == 0 && i % 5 == 0 && state.map_cells[i].type == CELL_EMPTY) {
            xy decoration_cell = xy(i % state.map_width, i / state.map_width);
            if (map_is_cell_blocked(state, decoration_cell)) {
                continue;
            }
            bool is_adjacent_to_ramp = false;
            for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                xy neighbor_cell = decoration_cell + DIRECTION_XY[direction];
                if (!map_is_cell_in_bounds(state, neighbor_cell)) {
                    continue;
                }
                if (state.map_tiles[neighbor_cell.x + (neighbor_cell.y * state.map_width)].is_ramp == 1) {
                    is_adjacent_to_ramp = true;
                    break;
                }
            }
            if (is_adjacent_to_ramp) {
                continue;
            }
            bool is_gold_nearby = false;
            for (mine_t& mine : state.mines) {
                if (mine_get_block_building_rect(mine.cell).has_point(decoration_cell)) {
                    is_gold_nearby = true;
                    break;
                }
            }
            if (is_gold_nearby) {
                continue;
            }
            bool is_too_close_to_player = false;
            for (xy cell : player_spawns) {
                if (xy::manhattan_distance(cell, decoration_cell) < 4) {
                    is_too_close_to_player = true;
                    break;
                }
            }
            if (is_too_close_to_player) {
                continue;
            }
            state.map_decorations.push_back((decoration_t) {
                .index = (uint16_t)(lcg_rand() % 5),
                .cell = decoration_cell
            });
            map_set_cell(state, decoration_cell, CELL_BLOCKED);
        }
    }

    // Set map elevation
    state.map_lowest_elevation = 0;
    state.map_highest_elevation = 0;
    for (uint32_t index = 0; index < state.map_width * state.map_height; index++) {
        state.map_lowest_elevation = std::min(state.map_lowest_elevation, state.map_tiles[index].elevation);
        state.map_highest_elevation = std::max(state.map_highest_elevation, state.map_tiles[index].elevation);
    }
    log_trace("Elevation low: %i / high: %i", state.map_lowest_elevation, state.map_highest_elevation);
    log_trace("Map generation complete.");
}

void map_gen_oasis(match_state_t& state, std::vector<xy>& player_spawns, std::vector<tile_t>& map_tiles_prebaked) {
    log_trace("Generating map oasis...");

    // Generate spawn points
    log_trace("Generating spawn points...");
    int spawn_margin = 8;
    xy map_center = xy(state.map_width / 2, state.map_height / 2);
    int player_count = 0;
    for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
        if (network_get_player(i).status == PLAYER_STATUS_NONE) {
            continue;
        }
        player_count++;
    }
    for (int i = 0; i < DIRECTION_COUNT; i++) {
        // For player count < 5, generate corner spawns only
        if (player_count < 5 && i % 2 == 0) {
            continue;
        }
        player_spawns.push_back(map_center + xy(
            DIRECTION_XY[i].x * ((state.map_width / 2) - spawn_margin), 
            DIRECTION_XY[i].y * ((state.map_height / 2) - spawn_margin)));
    }

    log_trace("Generating crater...");
    int lake_radius = 16;
    int crater_radius = 42;
    std::vector<xy> crater_frontier;
    std::unordered_map<int, int> crater_explored;
    crater_frontier.push_back(xy(state.map_width / 2, state.map_height / 2));
    while (!crater_frontier.empty()) {
        xy next = crater_frontier[0];
        crater_frontier.erase(crater_frontier.begin());
        int index = next.x + (next.y * state.map_width);
        if (xy::euclidean_distance_squared(next, xy(state.map_width / 2, state.map_height / 2)) > crater_radius * crater_radius ||
            std::abs((next - xy(state.map_width / 2, state.map_height / 2)).x) == crater_radius || 
            std::abs((next - xy(state.map_width / 2, state.map_height / 2)).y) == crater_radius) {
            continue;
        }
        if (crater_explored.find(index) != crater_explored.end()) {
            continue;
        }
        crater_explored[index] = 1;
        map_tiles_prebaked[index].elevation = -1;
        if (xy::euclidean_distance_squared(next, xy(state.map_width / 2, state.map_height / 2)) <= lake_radius * lake_radius &&
            std::abs((next - xy(state.map_width / 2, state.map_height / 2)).x) != lake_radius &&
            std::abs((next - xy(state.map_width / 2, state.map_height / 2)).y) != lake_radius) {
            map_tiles_prebaked[index].index = TILE_WATER;
        } 
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            xy child = next + DIRECTION_XY[direction];
            crater_frontier.push_back(child);
        }
    }
    for (int y = (state.map_height / 2) - 2; y < (state.map_height / 2) + 3; y++) {
        uint16_t left_tile;
        uint16_t right_tile;
        uint16_t top_tile;
        uint16_t top_tile_front;
        uint16_t bottom_tile;
        if (y == (state.map_height / 2) - 2) {
            left_tile = TILE_WALL_EAST_STAIR_TOP;
            right_tile = TILE_WALL_WEST_STAIR_TOP;
            bottom_tile = TILE_WALL_NORTH_STAIR_LEFT;
            top_tile = TILE_WALL_SOUTH_STAIR_LEFT;
            top_tile_front = TILE_WALL_SOUTH_STAIR_FRONT_LEFT;
        } else if (y == (state.map_height / 2) + 2) {
            left_tile = TILE_WALL_EAST_STAIR_BOTTOM;
            right_tile = TILE_WALL_WEST_STAIR_BOTTOM;
            bottom_tile = TILE_WALL_NORTH_STAIR_RIGHT;
            top_tile = TILE_WALL_SOUTH_STAIR_RIGHT;
            top_tile_front = TILE_WALL_SOUTH_STAIR_FRONT_RIGHT;
        } else {
            left_tile = TILE_WALL_EAST_STAIR_CENTER;
            right_tile = TILE_WALL_WEST_STAIR_CENTER;
            bottom_tile = TILE_WALL_NORTH_STAIR_CENTER;
            top_tile = TILE_WALL_SOUTH_STAIR_CENTER;
            top_tile_front = TILE_WALL_SOUTH_STAIR_FRONT_CENTER;
        }
        int x = (state.map_width / 2) - (crater_radius - 1);
        map_tiles_prebaked[x + (y * state.map_width)].index = left_tile;
        map_tiles_prebaked[y + (x * state.map_width)].index = top_tile;
        map_tiles_prebaked[y + ((x + 1) * state.map_width)].index = top_tile_front;
        x = (state.map_width / 2) + (crater_radius - 1);
        map_tiles_prebaked[x + (y * state.map_width)].index = right_tile;
        map_tiles_prebaked[y + (x * state.map_width)].index = bottom_tile;
    }

    log_trace("Generating high value mines...");
    std::vector<xy> mine_cells = {
        xy((state.map_width / 2) + lake_radius + 1, (state.map_height / 2) + lake_radius + 1),
        xy((state.map_width / 2) - (lake_radius + 1 + MINE_SIZE), (state.map_height / 2) + lake_radius + 1),
        xy((state.map_width / 2) + lake_radius + 1, (state.map_height / 2) - (lake_radius + 1 + MINE_SIZE)),
        xy((state.map_width / 2) - (lake_radius + 1 + MINE_SIZE), (state.map_height / 2) - (lake_radius + 1 + MINE_SIZE)),
    };
    for (xy cell : mine_cells) {
        entity_id mine_id = state.mines.push_back((mine_t) {
            .cell = cell,
            .gold_left = MINE_GOLD_AMOUNT_HIGH,
            .occupancy = OCCUPANCY_EMPTY
        });
        map_set_cell_rect(state, rect_t(cell, xy(MINE_SIZE, MINE_SIZE)), CELL_MINE, mine_id);
    }

    log_trace("Generating low value mines...");
    mine_cells.clear();
    int mine_cell_target = player_count + 2;
    int iterations = 0;
    while (mine_cells.size() < mine_cell_target) {
        xy mine_cell;
        mine_cell.x = lcg_rand() % (state.map_width - MINE_SIZE);
        mine_cell.y = lcg_rand() % (state.map_height - MINE_SIZE);
        if (xy::euclidean_distance_squared(mine_cell, xy(state.map_width / 2, state.map_height / 2)) <= (crater_radius + 1) * (crater_radius + 1)) {
            continue;
        }
        bool mine_cell_too_close = false;
        for (xy cell : mine_cells) {
            if (xy::manhattan_distance(cell, mine_cell) < 24) {
                mine_cell_too_close = true;
                break;
            }
        }
        if (mine_cell_too_close) {
            continue;
        }
        for (xy cell : player_spawns) {
            if (xy::manhattan_distance(cell, mine_cell) < 24) {
                mine_cell_too_close = true;
                break;
            }
            continue;
        }
        if (mine_cell_too_close) {
            continue;
        }
        mine_cells.push_back(mine_cell);
        iterations++;
        if (iterations > 20) {
            log_trace("Reached max iterations. Restarting...");
            mine_cells.clear();
            iterations = 0;
        }
    }
    for (xy cell : mine_cells) {
        entity_id mine_id = state.mines.push_back((mine_t) {
            .cell = cell,
            .gold_left = MINE_GOLD_AMOUNT_LOW,
            .occupancy = OCCUPANCY_EMPTY
        });
        map_set_cell_rect(state, rect_t(cell, xy(MINE_SIZE, MINE_SIZE)), CELL_MINE, mine_id);
    }

    log_trace("Map Oasis complete.");
}

void map_gen_gold_test(match_state_t& state, std::vector<xy>& player_spawns, std::vector<tile_t>& map_tiles_prebaked) {
    player_spawns.push_back(xy(12, 12));
    for (int x = 6; x < state.map_width - 6; x += 18) {
        for (int y = 6; y < state.map_height - 6; y += 18) {
            map_create_mine(state, xy(x, y), 1000);
        }
    }
}

void map_create_mine(match_state_t& state, xy cell, uint32_t gold_amount) {
    entity_id mine_id = state.mines.push_back((mine_t) {
        .cell = cell,
        .gold_left = gold_amount,
        .occupancy = OCCUPANCY_EMPTY
    });
    map_set_cell_rect(state, rect_t(cell, xy(MINE_SIZE, MINE_SIZE)), CELL_MINE, mine_id);
}

bool map_is_cell_in_bounds(const match_state_t& state, xy cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= state.map_width || cell.y >= state.map_height);
}

bool map_is_cell_rect_in_bounds(const match_state_t& state, rect_t cell_rect) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            if (x < 0 || y < 0 || x >= state.map_width || y >= state.map_height) {
                return false;
            }
        }
    }

    return true;
}

bool map_is_cell_blocked(const match_state_t& state, xy cell) {
    return state.map_cells[cell.x + (cell.y * state.map_width)].type != CELL_EMPTY;
}

bool map_is_cell_rect_occupied(const match_state_t& state, rect_t cell_rect, xy origin, bool ignore_miners) {
    entity_id origin_id = origin.x == -1 ? ID_NULL : map_get_cell(state, origin).value;
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            cell_t cell = map_get_cell(state, xy(x, y));
            if (cell.type == CELL_EMPTY) {
                continue;
            }
            if (cell.type == CELL_UNIT) {
                const unit_t& unit = state.units.get_by_id(cell.value);
                if (cell.value == origin_id) {
                    GOLD_ASSERT(cell.value != ID_NULL);
                    continue;
                }

                if (origin_id != ID_NULL && xy::manhattan_distance(origin, xy(x, y)) > 3) {
                    continue;
                }

                if (ignore_miners) {
                    if (unit.target.type == UNIT_TARGET_MINE || unit.target.type == UNIT_TARGET_CAMP) {
                        continue;
                    }
                }
            }
            return true;
        }
    }

    return false;
}

bool map_is_cell_rect_same_elevation(const match_state_t& state, rect_t cell_rect) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            if (map_get_elevation(state, xy(x, y)) != map_get_elevation(state, cell_rect.position)) {
                return true;
            }
        }
    }
    
    return false;
}

cell_t map_get_cell(const match_state_t& state, xy cell) {
    return state.map_cells[cell.x + (cell.y * state.map_height)];
}

void map_set_cell(match_state_t& state, xy cell, CellType type, uint16_t value) {
    state.map_cells[cell.x + (cell.y * state.map_height)] = (cell_t) {
        .type = type,
        .value = value
    };
    state.is_fog_dirty = true;
}

void map_set_cell_rect(match_state_t& state, rect_t cell_rect, CellType type, uint16_t value) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            state.map_cells[x + (y * state.map_height)] = (cell_t) {
                .type = type,
                .value = value
            };
        }
    }
    state.is_fog_dirty = true;
}


FogType map_get_fog(const match_state_t& state, uint8_t player_id, xy cell) {
    return state.player_fog[player_id][cell.x + (state.map_width * cell.y)];
}

tile_t map_get_tile(const match_state_t& state, xy cell) {
    return state.map_tiles[cell.x + (cell.y * state.map_width)];
}

int8_t map_get_elevation(const match_state_t& state, xy cell) {
    return state.map_tiles[cell.x + (cell.y * state.map_width)].elevation;
}

void map_pathfind(const match_state_t& state, xy from, xy to, xy cell_size, std::vector<xy>* path, bool should_ignore_miners) {
    struct node_t {
        fixed cost;
        fixed distance;
        // The parent is the previous node stepped in the path to reach this node
        // It should be an index in the explored list, or -1 if it is the start node
        int parent; 
        xy cell;

        fixed score() const {
            return cost + distance;
        }
    };

    // Don't bother pathing to unit's cell
    if (from == to) {
        log_trace("from == to, end pathfind");
        path->clear();
        return;
    }

    // Find an alternate cell for large units
    if (cell_size.x + cell_size.y > 2 && 
        map_is_cell_rect_occupied(state, rect_t(to, cell_size), from, should_ignore_miners)) {
        xy nearest_alternate;
        int nearest_alternate_distance = -1;
        for (int x = 0; x < cell_size.x; x++) {
            for (int y = 0; y < cell_size.y; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }
                xy alternate = to - xy(x, y);
                if (map_is_cell_rect_in_bounds(state, rect_t(alternate, cell_size)) && 
                   !map_is_cell_rect_occupied(state, rect_t(alternate, cell_size), from, should_ignore_miners)) {
                    if (nearest_alternate_distance == -1 || xy::manhattan_distance(from, alternate) < nearest_alternate_distance) {
                        nearest_alternate = alternate;
                        nearest_alternate_distance = xy::manhattan_distance(from, alternate);
                    }
                }
            }
        }

        if (nearest_alternate_distance != -1) {
            to = nearest_alternate;
        }
    }

    std::vector<node_t> frontier;
    std::vector<node_t> explored;
    int explored_indices[state.map_width * state.map_height];
    memset(explored_indices, -1, state.map_width * state.map_height * sizeof(int));
    uint32_t closest_explored = 0;
    bool found_path = false;
    node_t path_end;

    frontier.push_back((node_t) {
        .cost = fixed::from_raw(0),
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
            log_warn("Pathfinding too long, going with closest explored...");
            break;
        }

        // Consider all children
        // Adjacent cells are considered first so that we can use information about them to inform whether or not a diagonal movement is allowed
        bool is_adjacent_direction_blocked[4] = { true, true, true, true };
        const int CHILD_DIRECTIONS[DIRECTION_COUNT] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST, 
                                                        DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST, DIRECTION_NORTHWEST };
        for (int i = 0; i < DIRECTION_COUNT; i++) {
            int direction = CHILD_DIRECTIONS[i];
            fixed cost_increase = fixed::from_int(1);
            if (direction % 2 == 1) {
                cost_increase = should_ignore_miners ? fixed::from_int(2) : (fixed::from_int(3) / 2);
            }
            node_t child = (node_t) {
                .cost = smallest.cost + cost_increase,
                .distance = fixed::from_int(xy::manhattan_distance(smallest.cell + DIRECTION_XY[direction], to)),
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_XY[direction]
            };
            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(state, rect_t(child.cell, cell_size))) {
                continue;
            }
            // If the cell is occupied skip it, unless the cell is one step away from our goal in which case we keep going because this avoids worst-case pathfinding
            if (map_is_cell_rect_occupied(state, rect_t(child.cell, cell_size), from, should_ignore_miners) && (child.cell != to || 
                    xy::manhattan_distance(smallest.cell, child.cell) != 1)) {
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
            // Check if it's in the frontier
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
    } // End while !frontier.empty()

    // Backtrack to build the path
    node_t current = found_path ? path_end : explored[closest_explored];
    path->clear();
    path->reserve(current.cost.integer_part());
    while (current.parent != -1) {
        path->insert(path->begin(), current.cell);
        current = explored[current.parent];
    }

    if (!path->empty()) {
        // Previously we allowed the algorithm to consider the target_cell even if it was blocked. This was done for efficiency's sake,
        // but if the target_cell really is blocked, we need to remove it from the path. The unit will path as close as they can.
        if ((*path)[path->size() - 1] == to && map_is_cell_blocked(state, to)) {
            path->pop_back();
        }
    }
}

bool map_is_cell_rect_revealed(const match_state_t& state, uint8_t player_id, rect_t rect) {
    for (int x = rect.position.x; x < rect.position.x + rect.size.x; x++) {
        for (int y = rect.position.y; y < rect.position.y + rect.size.y; y++) {
            if (map_get_fog(state, player_id, xy(x, y)) == FOG_REVEALED) {
                return true;
            }
        }
    }

    return false;
}

void map_fog_reveal_at_cell(match_state_t& state, uint8_t player_id, xy cell, xy size, int sight) {
    std::queue<xy> frontier;
    std::unordered_map<int, int> explored;
    frontier.push(cell);
    while (!frontier.empty()) {
        xy next = frontier.front();
        frontier.pop();
        auto explored_it = explored.find(next.x + (state.map_width * next.y));
        if (explored_it != explored.end()) {
            continue;
        }
        explored[next.x + (state.map_width * next.y)] = 1;
        if (((next.x == cell.x - sight || next.x == cell.x + sight) && 
            (next.y == cell.y - sight || next.y == cell.y + sight)) ||
            std::abs(next.x - cell.x) > sight || std::abs(next.y - cell.y) > sight) {
            continue;
        }
        if (!map_is_cell_in_bounds(state, next)) {
            continue;
        }
        state.player_fog[player_id][next.x + (state.map_width * next.y)] = FOG_REVEALED;
        if (map_get_elevation(state, cell) < map_get_elevation(state, next)) {
            continue;
        }
        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
            frontier.push(next + DIRECTION_XY[direction]);
        }
    }
}

void map_update_fog(match_state_t& state, uint8_t player_id) {
    // First remember any revealed buildings
    for (uint32_t building_index = 0; building_index < state.buildings.size(); building_index++) {
        building_t& building = state.buildings[building_index];
        if (!map_is_cell_rect_revealed(state, player_id, rect_t(building.cell, building_cell_size(building.type)))) {
            continue;
        }
        state.remembered_buildings[player_id][state.buildings.get_id_of(building_index)] = (remembered_building_t) {
            .player_id = building.player_id,
            .type = building.type,
            .health = building.health,
            .cell = building.cell,
            .mode = building.mode,
            .occupancy = building.occupancy
        };
    }

    // Then remember any revealed mines
    for (uint32_t mine_index = 0; mine_index < state.mines.size(); mine_index++) {
        mine_t& mine = state.mines[mine_index];
        if (!map_is_cell_rect_revealed(state, player_id, rect_t(mine.cell, xy(MINE_SIZE, MINE_SIZE)))) {
            continue;
        }
        state.remembered_mines[player_id][state.mines.get_id_of(mine_index)] = (mine_t) {
            .cell = mine.cell,
            .gold_left = mine.gold_left,
            .occupancy = mine.occupancy
        };
    }

    // Now dim anything that is revealed
    for (uint32_t i = 0; i < state.map_width * state.map_height; i++) {
        if (state.player_fog[player_id][i] == FOG_REVEALED) {
            state.player_fog[player_id][i] = FOG_EXPLORED;
        }
    }

    // Reveal based on unit vision
    for (const unit_t& unit : state.units) {
        if (unit.player_id != player_id || unit.mode == UNIT_MODE_FERRY) {
            continue;
        }

        map_fog_reveal_at_cell(state, player_id, unit.cell, unit_cell_size(unit.type), UNIT_DATA.at(unit.type).sight);
    }

    for (const building_t& building : state.buildings) {
        if (building.player_id != player_id) {
            continue;
        }

        map_fog_reveal_at_cell(state, player_id, building.cell, building_cell_size(building.type), 4);
    }

    // Finally remove from the remembered buildings map for any buildings that no longer exist
    // Note the buildings are only removed from the list if the player can see where the building once was
    auto it = state.remembered_buildings[player_id].begin();
    while (it != state.remembered_buildings[player_id].end()) {
        if (state.buildings.get_index_of(it->first) == INDEX_INVALID && map_is_cell_rect_revealed(state, player_id, rect_t(it->second.cell, building_cell_size(it->second.type)))) {
            log_trace("deleting remembered building");
            it = state.remembered_buildings[player_id].erase(it);
        } else {
            it++;
        }
    }

    state.is_fog_dirty = false;
}

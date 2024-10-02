#include "match.h"

#include "network.h"
#include "logger.h"

void map_init(match_state_t& state, uint32_t width, uint32_t height) {
    state.map_width = width;
    state.map_height = height;
    state.map_tiles = std::vector<tile_t>(state.map_width * state.map_height, (tile_t) {
        .index = TILE_ARIZONA_SAND1,
        .elevation = 0,
        .blocked = BLOCKED_NONE
    });
    state.map_cells = std::vector<cell_t>(state.map_width * state.map_height, (cell_t) {
        .type = CELL_EMPTY,
        .value = 0
    });

    /*
    for (uint32_t i = 0; i < state.map.width * state.map.height; i++) {
        int new_base = lcg_rand() % 7;
        if (new_base < 4 && i % 3 == 0) {
            state.map.tiles[i] = new_base == 1 ? 2 : 1; 
        }
    }
    */
   
    rect_t highground_rect = rect_t(xy(1, 3), xy(6, 4));
    for (int x = highground_rect.position.x; x < highground_rect.position.x + highground_rect.size.x; x++) {
        for (int y = highground_rect.position.y; y < highground_rect.position.y + highground_rect.size.y; y++) {
            if ((x == highground_rect.position.x || x == highground_rect.position.x + highground_rect.size.x - 1) &&
                (y == highground_rect.position.y)) {
                continue;
            }
            if (x == highground_rect.position.x + 2 && y == highground_rect.position.y + highground_rect.size.y - 2) {
                state.map_tiles[x + (y * state.map_width)] = (tile_t) {
                    .index = TILE_ARIZONA_SAND1,
                    .elevation = 1,
                    .blocked = (uint8_t)(DIRECTION_MASK[DIRECTION_SOUTHEAST] | DIRECTION_MASK[DIRECTION_SOUTHWEST])
                };
            } else if (x == highground_rect.position.x + 2 && y == highground_rect.position.y + highground_rect.size.y - 1) {
                state.map_tiles[x + (y * state.map_width)] = (tile_t) {
                    .index = TILE_ARIZONA_SAND1,
                    .elevation = 0,
                    .blocked = (uint8_t)(~(DIRECTION_MASK[DIRECTION_NORTH] | DIRECTION_MASK[DIRECTION_SOUTH]))
                };
            } else {
                state.map_tiles[x + (y * state.map_width)] = (tile_t) {
                    .index = TILE_ARIZONA_SAND1,
                    .elevation = 1,
                    .blocked = BLOCKED_NONE
                };
            }
        }
    }
    for (int x = highground_rect.position.x; x < highground_rect.position.x + highground_rect.size.x; x++) {
        int y = highground_rect.position.y + highground_rect.size.y;
        if (x == highground_rect.position.x + 2) {
            state.map_tiles[x + (y * state.map_width)] = (tile_t) {
                .index = TILE_ARIZONA_SAND1,
                .elevation = 0,
                .blocked = (uint8_t)(~(DIRECTION_MASK[DIRECTION_NORTH] | DIRECTION_MASK[DIRECTION_SOUTH]))
            };
            continue;
        }
        uint16_t tile = TILE_ARIZONA_WALL_FRONT_CENTER;
        if (x == highground_rect.position.x || x == highground_rect.position.x + 3) {
            tile = TILE_ARIZONA_WALL_FRONT_LEFT;
        } else if (x == highground_rect.position.x + highground_rect.size.x - 1 || x == highground_rect.position.x + 1) {
            tile = TILE_ARIZONA_WALL_FRONT_RIGHT;
        }
        state.map_tiles[x + (y * state.map_width)] = (tile_t) {
            .index = tile,
            .elevation = 0,
            .blocked = BLOCKED_COMPLETELY
        };
    }

    state.map_lowest_elevation = 0;
    state.map_highest_elevation = 0;
    for (uint32_t index = 0; index < state.map_width * state.map_height; index++) {
        state.map_lowest_elevation = std::min(state.map_lowest_elevation, state.map_tiles[index].elevation);
        state.map_highest_elevation = std::max(state.map_highest_elevation, state.map_tiles[index].elevation);
    }
    GOLD_ASSERT(std::abs(state.map_highest_elevation - state.map_lowest_elevation) < 6);
    
    for (int x = 0; x < state.map_width; x++) {
        for (int y = 0; y < state.map_height; y++) {
            if (state.map_tiles[x + (y * state.map_width)].blocked != BLOCKED_NONE) {
                continue;
            }
            uint8_t lower_neighbors = 0;
            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                xy neighbor_cell = xy(x, y) + DIRECTION_XY[direction];
                if (!map_is_cell_in_bounds(state, neighbor_cell)) {
                    continue;
                }
                if (map_get_elevation(state, neighbor_cell) < map_get_elevation(state, xy(x, y))) {
                    lower_neighbors += DIRECTION_MASK[direction];
                }
            }
            state.map_tiles[x + (y * state.map_width)].blocked = lower_neighbors;
            if (lower_neighbors != 0) {
                xy cell = xy(x, y);
                log_trace("cell %xi blocked %u", &cell, lower_neighbors);
            }
        }
    }

    for (int index = 0; index < state.map_width * state.map_height; index++) {
        if (state.map_tiles[index].blocked == BLOCKED_COMPLETELY) {
            state.map_cells[index].type = CELL_BLOCKED;
        }
    }
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
                if (cell.value == origin_id) {
                    GOLD_ASSERT(cell.value != ID_NULL);
                    continue;
                }

                if (origin_id != ID_NULL && xy::manhattan_distance(origin, xy(x, y)) > 3) {
                    continue;
                }

                if (ignore_miners) {
                    const unit_t& unit = state.units.get_by_id(cell.value);
                    if ((unit.target.type == UNIT_TARGET_MINE || unit.target.type == UNIT_TARGET_CAMP) && 
                        (origin_id == ID_NULL || xy::manhattan_distance(origin, xy(x, y)) > 1)) {
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

bool map_can_cell_rect_step_in_direction(const match_state_t& state, rect_t from, int direction) {
    int inverse_direction = (direction + 4) % DIRECTION_COUNT;
    for (int x = from.position.x; x < from.position.x + from.size.x; x++) {
        for (int y = from.position.y; y < from.position.y + from.size.y; y++) {
            if (map_is_cell_blocked_in_direction(state, xy(x, y), direction)) {
                return false;
            }
            if (map_is_cell_blocked_in_direction(state, xy(x, y) + DIRECTION_XY[direction], inverse_direction)) {
                return false;
            }
        }
    }

    return true;
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
    int xmin = std::max(0, cell.x - sight);
    int xmax = std::min((int)state.map_width, cell.x + size.x + sight);
    int ymin = std::max(0, cell.y - sight);
    int ymax = std::min((int)state.map_height, cell.y + size.y + sight);
    for (int x = xmin; x < xmax; x++) {
        for (int y = ymin; y < ymax; y++) {
            bool is_x_edge = x == cell.x - sight || x == (cell.x + size.x + sight) - 1;
            bool is_y_edge = y == cell.y - sight || y == (cell.y + size.y + sight) - 1;
            if (is_x_edge && is_y_edge) {
                continue;
            }

            state.player_fog[player_id][x + (state.map_width * y)] = FOG_REVEALED;
        }
    }
}

tile_t map_get_tile(const match_state_t& state, xy cell) {
    return state.map_tiles[cell.x + (cell.y * state.map_width)];
}

int8_t map_get_elevation(const match_state_t& state, xy cell) {
    return state.map_tiles[cell.x + (cell.y * state.map_width)].elevation;
}

bool map_is_cell_blocked_in_direction(const match_state_t& state, xy cell, int direction) {
    return (state.map_tiles[cell.x + (cell.y * state.map_width)].blocked & DIRECTION_MASK[direction]) == DIRECTION_MASK[direction];
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
            log_warn("Pathfinding too long, aborting...");
            path->clear();
            return;
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
            // Skip this child if the cell is directionally blocked
            if (!map_can_cell_rect_step_in_direction(state, rect_t(smallest.cell, cell_size), direction)) {
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
    // Previously we allowed the algorithm to consider the target_cell even if it was blocked. This was done for efficiency's sake,
    // but if the target_cell really is blocked, we need to remove it from the path. The unit will path as close as they can.
    if ((*path)[path->size() - 1] == to && map_is_cell_blocked(state, to)) {
        path->pop_back();
    }
}

void map_fog_reveal(match_state_t& state, uint8_t player_id) {
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
}

void map_update_fog(match_state_t& state, uint8_t player_id) {
    // First remember any revealed buildings
    for (uint32_t building_index = 0; building_index < state.buildings.size(); building_index++) {
        building_t& building = state.buildings[building_index];
        if (!map_is_cell_rect_revealed(state, player_id, rect_t(building.cell, building_cell_size(building.type)))) {
            continue;
        }
        state.remembered_buildings[state.buildings.get_id_of(building_index)] = (remembered_building_t) {
            .player_id = building.player_id,
            .type = building.type,
            .health = building.health,
            .cell = building.cell,
            .mode = building.mode
        };
    }

    // Then remember any revealed mines
    for (uint32_t mine_index = 0; mine_index < state.mines.size(); mine_index++) {
        mine_t& mine = state.mines[mine_index];
        if (!map_is_cell_rect_revealed(state, player_id, rect_t(mine.cell, xy(MINE_SIZE, MINE_SIZE)))) {
            continue;
        }
        state.remembered_mines[state.mines.get_id_of(mine_index)] = (mine_t) {
            .cell = mine.cell,
            .gold_left = mine.gold_left,
            .is_occupied = mine.is_occupied
        };
    }

    // Now dim anything that is revealed
    for (uint32_t i = 0; i < state.map_width * state.map_height; i++) {
        if (state.player_fog[player_id][i] == FOG_REVEALED) {
            state.player_fog[player_id][i] = FOG_EXPLORED;
        }
    }

    map_fog_reveal(state, player_id);

    // Note the buildings are only removed from the list if the player can see where the building once was
    auto it = state.remembered_buildings.begin();
    while (it != state.remembered_buildings.end()) {
        if (state.buildings.get_index_of(it->first) == INDEX_INVALID && map_is_cell_rect_revealed(state, player_id, rect_t(it->second.cell, building_cell_size(it->second.type)))) {
            log_trace("deleting remembered building");
            it = state.remembered_buildings.erase(it);
        } else {
            it++;
        }
    }
}

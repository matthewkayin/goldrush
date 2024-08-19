#include "match.h"

#include "network.h"

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

bool map_is_cell_rect_blocked(const match_state_t& state, rect_t cell_rect) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            if (state.map_cells[x + (y * state.map_width)].type != CELL_EMPTY) {
                return true;
            }
        }
    }
    
    return false;
}

bool map_is_cell_rect_blocked_pathfind(const match_state_t& state, xy origin, rect_t cell_rect) {
    entity_id origin_id = state.map_cells[origin.x + (origin.y * state.map_width)].value;
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            cell_t cell = state.map_cells[x + (y * state.map_width)];
            if (cell.type == CELL_EMPTY) {
                continue;
            }
            if (cell.type == CELL_UNIT) {
                if (cell.value == origin_id) {
                    continue;
                }
                if (xy::manhattan_distance(origin, xy(x, y)) > 3) {
                    continue;
                }
            }
            return true;
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

bool map_is_cell_gold(const match_state_t& state, xy cell) {
    uint32_t cell_type = map_get_cell(state, cell).type;
    return cell_type >= CELL_GOLD1 && cell_type <= CELL_GOLD3;
}

void map_decrement_gold(match_state_t& state, xy cell) {
    state.map_cells[cell.x + (cell.y * state.map_width)].value--;
    uint16_t gold_left = state.map_cells[cell.x + (cell.y * state.map_width)].value;
    if (gold_left <= 0) {
        state.map_cells[cell.x + (cell.y * state.map_width)] = (cell_t) {
            .type = CELL_EMPTY,
            .value = 0
        };
    }
    state.is_fog_dirty = true;
}

#include "logger.h"
void map_pathfind(const match_state_t& state, xy from, xy to, xy cell_size, std::vector<xy>* path) {
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
        path->clear();
        return;
    }

    if (map_is_cell_rect_blocked_pathfind(state, from, rect_t(to, cell_size))) {
        xy nearest_alternate;
        int nearest_alternate_distance = -1;
        for (int x = 0; x < cell_size.x; x++) {
            for (int y = 0; y < cell_size.y; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }
                xy alternate = to - xy(x, y);
                if (map_is_cell_rect_in_bounds(state, rect_t(alternate, cell_size)) && !map_is_cell_rect_blocked_pathfind(state, from, rect_t(alternate, cell_size))) {
                    if (nearest_alternate_distance == -1 || xy::manhattan_distance(from, alternate) < nearest_alternate_distance) {
                        nearest_alternate = alternate;
                        nearest_alternate_distance = xy::manhattan_distance(from, alternate);
                    }
                }
            }
        }

        if (nearest_alternate_distance == -1) {
            path->clear();
            return;
        }

        to = nearest_alternate;
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

    // TODO prevent unit from pathing through diagonal gaps
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

        // Consider all children
        // Adjacent cells are considered first so that we can use information about them to inform whether or not a diagonal movement is allowed
        bool is_adjacent_direction_blocked[4] = { true, true, true, true };
        const int CHILD_DIRECTIONS[DIRECTION_COUNT] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST, 
                                                        DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST, DIRECTION_NORTHWEST };
        for (int i = 0; i < DIRECTION_COUNT; i++) {
            int direction = CHILD_DIRECTIONS[i];
            fixed cost_increase = direction % 2 == 0 ? fixed::from_int(1) : (fixed::from_int(3) / 2);
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
            // Don't consider blocked spaces, unless:
            // 1. the blocked space is a unit that is very far away. we pretend such spaces are blocked since the unit will probably move by the time we get there
            // 2. the blocked space is the target_cell. by allowing the path to go here we can avoid worst-case pathfinding even when the target_cell is blocked
            if (!(child.cell == to && xy::manhattan_distance(smallest.cell, child.cell) == 1)) {
                if (map_is_cell_rect_blocked_pathfind(state, from, rect_t(child.cell, cell_size))) {
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

fog_t map_get_fog(const match_state_t& state, xy cell) {
    return state.map_fog[cell.x + (state.map_width * cell.y)];
}

void map_fog_reveal(match_state_t& state, xy cell, xy size, int sight) {
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

            state.map_fog[x + (state.map_width * y)] = (fog_t) {
                .type = FOG_REVEALED,
                .value = FOG_VALUE_NONE
            };
        }
    }
}

void map_update_fog(match_state_t& state) {
    // First dim anything that is revealed
    for (uint32_t i = 0; i < state.map_width * state.map_height; i++) {
        if (state.map_fog[i].type == FOG_REVEALED) {
            if (state.map_cells[i].type == CELL_BUILDING) {
                uint32_t cell_value_index = state.buildings.get_index_of(state.map_cells[i].value);
                GOLD_ASSERT(cell_value_index != INDEX_INVALID);
                uint8_t cell_value_player_id = state.buildings[cell_value_index].player_id;
                xy cell_value_size = building_cell_size(state.buildings[cell_value_index].type);
                xy cell_value_origin = xy(i % state.map_width, i / state.map_width);
                for (uint32_t x = cell_value_origin.x; x < cell_value_origin.x + cell_value_size.x; x++) {
                    for (uint32_t y = cell_value_origin.y; y < cell_value_origin.y + cell_value_size.y; y++) {
                        if (map_get_fog(state, xy(x, y)).type == FOG_HIDDEN) {
                            continue;
                        }
                        state.map_fog[x + (y * state.map_width)] = (fog_t) {
                            .type = FOG_EXPLORED,
                            .value = (uint16_t)cell_value_player_id
                        };
                    }
                }
            } else {
                state.map_fog[i] = (fog_t) {
                    .type = FOG_EXPLORED,
                    .value = FOG_VALUE_NONE
                };
            }
        }
    }

    // Reveal based on unit vision
    for (const unit_t& unit : state.units) {
        if (unit.player_id != network_get_player_id()) {
            continue;
        }

        map_fog_reveal(state, unit.cell, unit_cell_size(unit.type), UNIT_DATA.at(unit.type).sight);
    }

    for (const building_t& building : state.buildings) {
        if (building.player_id != network_get_player_id()) {
            continue;
        }

        map_fog_reveal(state, building.cell, building_cell_size(building.type), 4);
    }

    state.is_fog_dirty = false;
}
#include "match.h"

#include "engine.h"
#include "logger.h"

void map_init(match_state_t& state, uint32_t width, uint32_t height) {
    state.map_width = width;
    state.map_height = height;

    state.map_cells = std::vector<entity_id>(state.map_width * state.map_height, CELL_EMPTY);

    state.map_tiles = std::vector<tile_t>(state.map_width * state.map_height, (tile_t) {
        .index = engine.tile_index[TILE_SAND],
        .elevation = 0
    });
    for (int y = 0; y < state.map_height; y++) {
        for (int x = 0; x < state.map_width; x++) {
            if (x == 0 || y == 0 || x == state.map_width - 1 || y == state.map_height - 1) {
                state.map_tiles[x + (y * state.map_width)].index = TILE_SAND3;
            } else if ((x + (y * state.map_width)) % 3 == 0) {
                state.map_tiles[x + (y * state.map_width)].index = TILE_SAND2;
            }
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

void map_set_cell_rect(match_state_t& state, xy cell, int cell_size, entity_id value) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            state.map_cells[x + (y * state.map_width)] = value;
        }
    }
}

bool map_is_cell_rect_occupied(const match_state_t& state, xy cell, int cell_size, xy origin, bool ignore_miners) {
    entity_id origin_id = origin.x == -1 ? ID_NULL : map_get_cell(state, origin);

    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            entity_id cell_id = map_get_cell(state, xy(x, y));
            if (cell_id == origin_id || cell_id == CELL_EMPTY || cell_id == CELL_BLOCKED) {
                continue;
            }  
            if (origin_id != ID_NULL && xy::manhattan_distance(origin, xy(x, y)) > 3) {
                continue;
            }
            if (ignore_miners) {
                // TODO
            }

            return true;
        }
    }

    return false;
}

// Returns the nearest cell around the rect relative to start_cell
// If there are no free cells around the rect in a radius of 1, then this returns the start cell
xy map_get_nearest_cell_around_rect(const match_state_t& state, xy start, int start_size, xy rect_position, int rect_size, bool allow_blocked_cells) {
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
        if (map_is_cell_rect_in_bounds(state, cell, start_size)) {
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

void map_pathfind(const match_state_t& state, xy from, xy to, int cell_size, std::vector<xy>* path, bool ignore_miners) {
    struct node_t {
        int cost;
        int distance;
        // The parent is the previous node stepped in the path to reach this node
        // It should be an index in the explored list or -1 if it is the start node
        int parent;
        xy cell;

        int score() const {
            return cost + distance;
        };
    };

    // Don't bother pathing to the unit's cell
    if (from == to) {
        path->clear();
        return;
    }

    // Find an alternate cell for large units
    if (cell_size > 1 && map_is_cell_rect_occupied(state, to, cell_size, from, ignore_miners)) {
        xy nearest_alternative;
        int nearest_alternative_distance = -1;
        for (int x = 0; x < cell_size; x++) {
            for (int y = 0; y < cell_size; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }

                xy alternative = to - xy(x, y);
                if (map_is_cell_rect_in_bounds(state, alternative, cell_size) &&
                    !map_is_cell_rect_occupied(state, alternative, cell_size, from, ignore_miners)) {
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
    int explored_indices[state.map_width * state.map_height];
    memset(explored_indices, -1, state.map_width * state.map_height * sizeof(int));
    uint32_t closest_explored = 0;
    bool found_path = false;
    node_t path_end;

    frontier.push_back((node_t) {
        .cost = 0,
        .distance = xy::manhattan_distance(from, to),
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
                .cost = smallest.cost + 1,
                .distance = xy::manhattan_distance(smallest.cell + DIRECTION_XY[direction], to),
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_XY[direction]
            };

            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(state, child.cell, cell_size)) {
                continue;
            }

            // Skip occupied cells (unless the child is the goal. this avoids worst-case pathing)
            if (map_is_cell_rect_occupied(state, child.cell, cell_size, from, ignore_miners) &&
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
    } // End while not frontier empty

    // Backtrack to build the path
    node_t current = found_path ? path_end : explored[closest_explored];
    path->clear();
    path->reserve(current.cost);
    while (current.parent != -1) {
        path->insert(path->begin(), current.cell);
        current = explored[current.parent];
    }

    if (!path->empty()) {
        // Previously we allowed the algorithm to consider the target_cell even if it was blocked. This was done for efficiency's sake,
        // but if the target_cell really is blocked, we need to remove it from the path. The unit will path as close as they can.
        if ((*path)[path->size() - 1] == to && map_is_cell_rect_occupied(state, to, cell_size, from, ignore_miners)) {
            path->pop_back();
        }
    }
}
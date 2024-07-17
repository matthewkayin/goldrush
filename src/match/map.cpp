#include "map.h"

map_t map_init(const ivec2& size) {
    map_t map;

    map.width = size.x;
    map.height = size.y;

    map.tiles = std::vector<int>(map.width * map.height);
    map.cells = std::vector<int>(map.width * map.height, CELL_EMPTY);
    for (int i = 0; i < map.width * map.height; i += 3) {
        map.tiles[i] = 1;
    }
    for (int y = 0; y < map.height; y++) {
        for (int x = 0; x < map.width; x++) {
            if (y == 0 || y == map.height - 1 || x == 0 || x == map.width - 1) {
                map.tiles[x + (y * map.width)] = 2;
            }
        }
    }

    // Place gold on the map
    int gold_remaining = 32;
    while (gold_remaining != 0) {
        ivec2 gold_location = ivec2(rand() % map.width, rand() % map.height);
        if (map_cell_get_value(map, gold_location) == CELL_EMPTY) {
            map_cell_set_value(map, gold_location, CELL_GOLD);
            gold_remaining--;
        }
    }

    return map;
}

bool map_cell_is_in_bounds(const map_t& map, const ivec2& cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= map.width || cell.y >= map.height);
}

bool map_cell_is_blocked(const map_t& map, const ivec2& cell) {
    return map.cells[cell.x + (cell.y * map.width)] != CELL_EMPTY;
}

bool map_cells_are_blocked(const map_t& map, const ivec2& cell, const ivec2& cell_size) {
    for (int y = cell.y; y < cell.y + cell_size.y; y++) {
        for (int x = cell.x; x < cell.x + cell_size.x; x++) {
            if (map.cells[x + (y * map.width)] != CELL_EMPTY) {
                return true;
            }
        }
    }

    return false;
}

int map_cell_get_value(const map_t& map, const ivec2& cell) {
    return map.cells[cell.x + (cell.y * map.width)];
}

void map_cell_set_value(map_t& map, const ivec2& cell, int value) {
    map.cells[cell.x + (cell.y * map.width)] = value;
}

void map_cells_set_value(map_t& map, const ivec2& cell, const ivec2& cell_size, int value) {
    for (int y = cell.y; y < cell.y + cell_size.y; y++) {
        for (int x = cell.x; x < cell.x + cell_size.x; x++) {
            map.cells[x + (y * map.width)] = value;
        }
    }
}

ivec2 map_get_first_free_cell_around_cells(const map_t& map, const ivec2& cell, const ivec2& cell_size) {
    ivec2 _cell = cell + ivec2(-1, 0);
    bool cell_is_valid = map_cell_is_in_bounds(map, _cell) && !map_cell_is_blocked(map, _cell);

    int step_count = 0;
    int x_step_amount = cell_size.x + 1;
    int y_step_amount = cell_size.y;
    Direction step_direction = DIRECTION_SOUTH;

    while (!cell_is_valid) {
        _cell += DIRECTION_IVEC2[step_direction];
        step_count++;

        bool is_y_stepping = step_direction == DIRECTION_SOUTH || step_direction == DIRECTION_NORTH;
        int step_amount = is_y_stepping ? y_step_amount : x_step_amount;
        if (step_count == step_amount) {
            if (is_y_stepping) {
                y_step_amount++;
            } else {
                x_step_amount++;
            }
            step_count = 0;

            switch (step_direction) {
                case DIRECTION_SOUTH:
                    step_direction = DIRECTION_EAST;
                    break;
                case DIRECTION_EAST:
                    step_direction = DIRECTION_NORTH;
                    break;
                case DIRECTION_NORTH:
                    step_direction = DIRECTION_WEST;
                    break;
                case DIRECTION_WEST:
                    step_direction = DIRECTION_SOUTH;
                    break;
                default:
                    break;
            }
        }

        cell_is_valid = map_cell_is_in_bounds(map, _cell) && !map_cell_is_blocked(map, _cell);
    }

    return _cell;
}

std::vector<ivec2> map_pathfind(const map_t& map, const ivec2& from, const ivec2& to) {
    struct path_t {
        int score;
        std::vector<ivec2> points;
    };

    std::vector<path_t> frontier;
    std::vector<ivec2> explored;

    frontier.push_back((path_t) { 
        .score = abs(to.x - from.x) + abs(to.y - from.y), 
        .points = { from } 
    });

    while (!frontier.empty()) {
        // Find the smallest path
        uint32_t smallest_index = 0;
        for (uint32_t i = 1; i < frontier.size(); i++) {
            if (frontier[i].score < frontier[smallest_index].score) {
                smallest_index = i;
            }
        }

        // Pop the smallest path
        path_t smallest = frontier[smallest_index];
        frontier.erase(frontier.begin() + smallest_index);

        // If it's the solution, return it
        ivec2 smallest_head = smallest.points[smallest.points.size() - 1];
        if (smallest_head == to) {
            smallest.points.erase(smallest.points.begin());
            return smallest.points;
        }

        // Otherwise, add this tile to the explored list
        explored.push_back(smallest_head);

        // Consider all children
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = smallest_head + DIRECTION_IVEC2[direction];
            int child_score = smallest.points.size() + 1 + abs(child.x - to.x) + abs(child.y - to.y);
            // Don't consider out of bounds children
            if (child.x < 0 || child.x >= map.width || child.y < 0 || child.y >= map.height) {
                continue;
            }
            // Don't consider blocked spaces
            if (map_cell_is_blocked(map, child)) {
                continue;
            }
            // Don't consider already explored children
            bool is_in_explored = false;
            for (const ivec2& explored_cell : explored) {
                if (explored_cell == child) {
                    is_in_explored = true;
                    break;
                }
            }
            if (is_in_explored) {
                continue;
            }
            // Check if it's in the frontier
            uint32_t frontier_index;
            for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                std::vector<ivec2>& frontier_path = frontier[frontier_index].points;
                if (frontier_path[frontier_path.size() - 1] == child) {
                    break;
                }
            }
            // If it is in the frontier...
            if (frontier_index < frontier.size()) {
                // ...and the child represents a shorter version of the frontier path, then replace the frontier version with the shorter child
                if (child_score < frontier[frontier_index].score) {
                    frontier[frontier_index].points = smallest.points;
                    frontier[frontier_index].points.push_back(child);
                    frontier[frontier_index].score = child_score;
                }
                continue;
            }
            // If it's not in the frontier, then add it to the frontier
            path_t new_path = (path_t) {
                .score = child_score,
                .points = smallest.points
            };
            new_path.points.push_back(child);
            frontier.push_back(new_path);
        } // End for each child
    } // End while !frontier.empty()

    return std::vector<ivec2>();
}

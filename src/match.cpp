#include "match.h"

#include "defines.h"
#include "asserts.h"
#include "engine.h"
#include "logger.h"
#include "util.h"
#include "network.h"
#include <vector>
#include <cstdlib>
#include <algorithm>

static const uint32_t TICK_DURATION = 4;
static const uint32_t MAX_UNITS = 200;

static const int CAMERA_DRAG_MARGIN = 16;
static const int CAMERA_DRAG_SPEED = 8;

// UNIT

struct unit_t {
    bool is_selected;

    bool is_moving;
    int direction;
    vec2 position;
    vec2 target_position;
    ivec2 cell;
    std::vector<ivec2> path;
    uint32_t path_timer;
    static const uint32_t path_pause_duration = 60;

    ivec2 animation_frame;
    uint32_t animation_timer;
    static const uint32_t animation_frame_duration = 8;

    rect_t get_rect() {
        ivec2 size = sprite_get_frame_size(SPRITE_UNIT_MINER);
        return rect_t(ivec2(position.x.integer_part(), position.y.integer_part()) - (size / 2), size);
    }
};

// INPUT

enum InputType {
    INPUT_NONE,
    INPUT_MOVE
};

struct input_move_t {
    ivec2 target_cell;
    uint8_t unit_count;
    uint8_t unit_ids[MAX_UNITS];
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
    };
};

// STATE

enum MatchMode {
    MATCH_MODE_NOT_STARTED,
    MATCH_MODE_RUNNING
};

struct match_state_t {
    MatchMode mode;

    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    ivec2 camera_offset;

    bool is_selecting;
    ivec2 select_origin;
    rect_t select_rect;

    std::vector<int> map_tiles;
    int map_width;
    int map_height;
    std::vector<bool> cell_is_blocked;

    std::vector<unit_t> units[MAX_PLAYERS];
};
static match_state_t state;

void input_flush();
void input_deserialize(uint8_t* in_buffer, size_t in_buffer_length);

vec2 cell_center_position(ivec2 cell);
void unit_spawn(uint8_t player_id, ivec2 cell);
void unit_try_move(unit_t& unit);
void unit_update(unit_t& unit);
std::vector<ivec2> pathfind(ivec2 from, ivec2 to);

void match_init() {
    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        input_t empty_input;
        empty_input.type = INPUT_NONE;
        std::vector<input_t> empty_input_list = { empty_input };
        state.inputs[player_id].push_back(empty_input_list);
        state.inputs[player_id].push_back(empty_input_list);
        state.inputs[player_id].push_back(empty_input_list);
    }
    state.tick_timer = 0;

    state.camera_offset = ivec2(0, 0);
    state.is_selecting = false;

    // Init map
    state.map_width = 40;
    state.map_height = 24;
    state.map_tiles = std::vector<int>(state.map_width * state.map_height);
    for (int i = 0; i < state.map_width * state.map_height; i += 3) {
        state.map_tiles[i] = 1;
    }
    for (int y = 0; y < state.map_height; y++) {
        for (int x = 0; x < state.map_width; x++) {
            if (y == 0 || y == state.map_height - 1 || x == 0 || x == state.map_width - 1) {
                state.map_tiles[x + (y * state.map_width)] = 2;
            }
        }
    }

    // Init cell grid
    state.cell_is_blocked = std::vector<bool>(state.map_width * state.map_height, false);

    // Init units
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NOT_READY) {
            continue;
        }
        state.units[player_id].reserve(MAX_UNITS);
    }

    unit_spawn(0, ivec2(1, 1));
    unit_spawn(0, ivec2(2, 2));
    unit_spawn(0, ivec2(3, 1));

    state.mode = MATCH_MODE_NOT_STARTED;
    if (!network_is_server()) {
        network_client_toggle_ready();
    }
}

void match_handle_input(uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_MOVE: {
            // Calculate unit group info
            ivec2 first_unit_cell = state.units[player_id][input.move.unit_ids[0]].cell;
            ivec2 group_center = first_unit_cell;
            ivec2 group_min = first_unit_cell;
            ivec2 group_max = first_unit_cell;
            for (uint32_t i = 1; i < input.move.unit_count; i++) {
                unit_t& unit = state.units[player_id][input.move.unit_ids[i]];
                group_center += unit.cell;
                group_min.x = std::min(group_min.x, unit.cell.x);
                group_min.y = std::min(group_min.y, unit.cell.y);
                group_max.x = std::max(group_max.x, unit.cell.x);
                group_max.y = std::max(group_max.y, unit.cell.y);
            }
            group_center = group_center / input.move.unit_count;
            bool is_target_inside_group = rect_t(group_min, group_max - group_min).has_point(input.move.target_cell);

            for (uint32_t i = 0; i < input.move.unit_count; i++) {
                unit_t& unit = state.units[player_id][input.move.unit_ids[i]];

                // Determine the unit's target
                ivec2 offset = unit.cell - group_center;
                ivec2 unit_target = input.move.target_cell + offset;
                bool is_target_invalid = is_target_inside_group || unit_target.x < 0 || unit_target.x >= state.map_width || unit_target.y < 0 || unit_target.y >= state.map_height ||
                                         ivec2::manhattan_distance(unit_target, input.move.target_cell) > 3 || state.cell_is_blocked[unit_target.x + (unit_target.y * state.map_width)];
                if (is_target_invalid) {
                    unit_target = input.move.target_cell;
                }

                // Don't bother pathfinding to the same cell
                if (unit_target == unit.cell) {
                    continue;
                }
                // Path to the target
                unit.path = pathfind(unit.cell, unit_target);
            }
            break;
        }
        default:
            break;
    }
}

void match_update() {
    // NETWORK EVENTS
    network_service();

    if (state.mode == MATCH_MODE_NOT_STARTED) {
        network_event_t network_event;
        while (network_poll_events(&network_event)) {
            if (network_is_server() && network_event.type == NETWORK_EVENT_CLIENT_READY) {
                bool all_players_ready = true;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    const player_t& player = network_get_player(player_id);
                    if (player.status == PLAYER_STATUS_NOT_READY) {
                        all_players_ready = false;
                    }
                }

                if (all_players_ready) {
                    network_server_start_match();
                    state.mode = MATCH_MODE_RUNNING;
                    log_info("match start");
                }
            } else if (!network_is_server() && network_event.type == NETWORK_EVENT_MATCH_START) {
                state.mode = MATCH_MODE_RUNNING;
                log_info("match start");
            }
        }
        
        // We make the check again here in case the mode changed
        if (state.mode == MATCH_MODE_NOT_STARTED) {
            return;
        }
    }

    network_event_t network_event;
    while (network_poll_events(&network_event)) {
        switch (network_event.type) {
            case NETWORK_EVENT_INPUT: {
                input_deserialize(network_event.input.data, network_event.input.data_length);
                break;
            }
            default:
                break;
        }
    }

    // NETWORK
    if (state.tick_timer == 0) {
        bool has_all_inputs = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            if (state.inputs[player_id].empty()) {
                has_all_inputs = false;
                break;
            }
        }

        if (!has_all_inputs) {
            log_info("missing inputs for this frame. waiting...");
            return;
        }

        // Begin next tick
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            for (const input_t& input : state.inputs[player_id][0]) {
                match_handle_input(player_id, input);
            }
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        }

        state.tick_timer = TICK_DURATION;
        input_flush();
    }
    state.tick_timer--;

    ivec2 mouse_pos = input_get_mouse_position();
    uint8_t current_player_id = network_get_player_id();

    // CAMERA DRAG
    ivec2 camera_drag_direction = ivec2(0, 0);
    if (mouse_pos.x < CAMERA_DRAG_MARGIN) {
        camera_drag_direction.x = -1;
    } else if (mouse_pos.x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
        camera_drag_direction.x = 1;
    }
    if (mouse_pos.y < CAMERA_DRAG_MARGIN) {
        camera_drag_direction.y = -1;
    } else if (mouse_pos.y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
        camera_drag_direction.y = 1;
    }
    state.camera_offset += camera_drag_direction * CAMERA_DRAG_SPEED;
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, (state.map_width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, (state.map_height * TILE_SIZE) - SCREEN_HEIGHT);

    // SELECT RECT
    ivec2 mouse_world_pos = mouse_pos + state.camera_offset;
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        // On begin selecting
        state.is_selecting = true;
        state.select_origin = mouse_world_pos;
    }
    if (state.is_selecting) {
        // Update select rect
        state.select_rect.position = ivec2(std::min(state.select_origin.x, mouse_world_pos.x), std::min(state.select_origin.y, mouse_world_pos.y));
        state.select_rect.size = ivec2(std::max(1, std::abs(state.select_origin.x - mouse_world_pos.x)), std::max(1, std::abs(state.select_origin.y - mouse_world_pos.y)));

        // Update unit selection
        for (unit_t& unit : state.units[current_player_id]) {
            unit.is_selected = unit.get_rect().intersects(state.select_rect);
        }
    }
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        state.is_selecting = false;
    } 

    // Command
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT)) {
        input_t input;
        input.type = INPUT_MOVE;
        input.move.target_cell = mouse_world_pos / TILE_SIZE;
        input.move.unit_count = 0;
        for (uint8_t unit_id = 0; unit_id < state.units[current_player_id].size(); unit_id++) {
            if (!state.units[current_player_id][unit_id].is_selected) {
                continue;
            }
            input.move.unit_ids[input.move.unit_count] = unit_id;
            input.move.unit_count++;
        }
        if (input.move.unit_count != 0) {
            state.input_queue.push_back(input);
        }
    }

    // Unit update
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (unit_t& unit : state.units[player_id]) {
            unit_update(unit);
        }
    }
}

void match_render() {
    uint8_t current_player_id = network_get_player_id();

    render_map(state.camera_offset, &state.map_tiles[0], state.map_width, state.map_height);

    // Select rings
    for (const unit_t& unit : state.units[current_player_id]) {
        if (unit.is_selected) {
            render_sprite(state.camera_offset, SPRITE_SELECT_RING, ivec2(0, 0), unit.position, true);
        }
    }

    // Units
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (const unit_t& unit : state.units[player_id]) {
            render_sprite(state.camera_offset, SPRITE_UNIT_MINER, unit.animation_frame, unit.position, true);
        }
    }

    // Select rect
    if (state.is_selecting) {
        render_rect(rect_t(state.select_rect.position - state.camera_offset, state.select_rect.size), COLOR_WHITE);
    }
}

void input_flush() {
    static const uint32_t INPUT_MAX_SIZE = 201;

    static uint8_t out_buffer[INPUT_BUFFER_SIZE];
    static size_t out_buffer_length;

    // Always send at least 1 input per tick
    if (state.input_queue.empty()) {
        input_t empty_input;
        empty_input.type = INPUT_NONE;
        state.input_queue.push_back(empty_input);
    }

    // Assert that the out buffer is big enough
    GOLD_ASSERT((INPUT_BUFFER_SIZE - 3) >= (INPUT_MAX_SIZE * state.input_queue.size()));

    // Leave a space in the buffer for the network message type
    uint8_t current_player_id = network_get_player_id();
    out_buffer[1] = current_player_id;
    out_buffer_length = 2;

    // Serialize the inputs
    for (const input_t& input : state.input_queue) {
        out_buffer[out_buffer_length] = input.type;
        out_buffer_length++;
        switch (input.type) {
            case INPUT_MOVE: {
                memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(ivec2));
                out_buffer_length += sizeof(ivec2);

                out_buffer[out_buffer_length] = input.move.unit_count;
                out_buffer_length += 1;

                memcpy(out_buffer + out_buffer_length, input.move.unit_ids, input.move.unit_count * sizeof(uint8_t));
                out_buffer_length += input.move.unit_count * sizeof(uint8_t);
                break;
            }
            default:
                break;
        }
    }
    state.inputs[current_player_id].push_back(state.input_queue);
    state.input_queue.clear();

    // Send them to other players
    if (network_is_server()) {
        network_server_send_input(out_buffer, out_buffer_length);
    } else {
        network_client_send_input(out_buffer, out_buffer_length);
    }
}

void input_deserialize(uint8_t* in_buffer, size_t in_buffer_length) {
    std::vector<input_t> tick_inputs;

    uint8_t in_player_id = in_buffer[1];
    size_t in_buffer_head = 2;

    while (in_buffer_head < in_buffer_length) {
        input_t input;
        input.type = in_buffer[in_buffer_head];
        in_buffer_head++;

        switch (input.type) {
            case INPUT_MOVE: {
                memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(ivec2));
                in_buffer_head += sizeof(ivec2);

                input.move.unit_count = in_buffer[in_buffer_head];
                memcpy(input.move.unit_ids, in_buffer + in_buffer_head + 1, input.move.unit_count * sizeof(uint8_t));
                in_buffer_head += 1 + input.move.unit_count;
            }
            default:
                break;
        }

        tick_inputs.push_back(input);
    }

    state.inputs[in_player_id].push_back(tick_inputs);
}

vec2 cell_center_position(ivec2 cell) {
    return vec2(fixed::from_int((cell.x * TILE_SIZE) + (TILE_SIZE / 2)), fixed::from_int((cell.y * TILE_SIZE) + (TILE_SIZE / 2)));
}

void unit_spawn(uint8_t player_id, ivec2 cell) {
    unit_t unit;
    unit.is_selected = false;
    unit.is_moving = false;
    unit.cell = cell;
    unit.position = cell_center_position(cell);
    unit.direction = DIRECTION_SOUTH;
    state.units[player_id].push_back(unit);

    state.cell_is_blocked[cell.x + (cell.y * state.map_width)] = true;
}

void unit_try_move(unit_t& unit) {
    ivec2 next_point = unit.path[0];

    // Set the unit's direction, even if it doesn't begin movement
    ivec2 next_direction = next_point - unit.cell;
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        if (DIRECTION_IVEC2[direction] == next_direction) {
            unit.direction = direction;
            break;
        }
    }

    // Don't move if the tile is blocked
    if (state.cell_is_blocked[next_point.x + (next_point.y * state.map_width)]) {
        return;
    }

    // Set state to begin movement
    state.cell_is_blocked[unit.cell.x + (unit.cell.y * state.map_width)] = false;
    unit.cell = next_point;
    state.cell_is_blocked[unit.cell.x + (unit.cell.y * state.map_width)] = true;
    unit.target_position = cell_center_position(unit.cell);
    unit.path.erase(unit.path.begin());
    unit.is_moving = true;
    unit.path_timer = 0;
}

void unit_update(unit_t& unit) {
    if (!unit.is_moving && !unit.path.empty()) {
        unit_try_move(unit);
        if (unit.is_moving) {
            unit.animation_frame.x = 1;
            unit.animation_timer = unit.animation_frame_duration;
        } else if (unit.path_timer == 0) {
            unit.path_timer = unit.path_pause_duration;
        }
    }
    if (unit.is_moving) {
        fixed movement_left = fixed::from_int(1);
        while (movement_left.raw_value > 0) {
            fixed distance_to_target = unit.position.distance_to(unit.target_position);
            if (distance_to_target > movement_left) {
                unit.position += DIRECTION_VEC2[unit.direction] * movement_left;
                movement_left = 0;
            } else {
                unit.position = unit.target_position;
                unit.is_moving = false;
                movement_left -= distance_to_target;
                if (!unit.path.empty()) {
                    unit_try_move(unit);
                }
                if (!unit.is_moving) {
                    movement_left.raw_value = 0;
                }
            }
        }
    } else if (!unit.path.empty()) {
        unit.path_timer--;
        if (unit.path_timer == 0) {
            unit.path.clear();
        }
    }

    if (!unit.is_moving) {
        unit.animation_frame.x = 0;
    } else {
        unit.animation_timer--;
        if (unit.animation_timer == 0) {
            unit.animation_frame.x++;
            if (unit.animation_frame.x == 5) {
                unit.animation_frame.x = 1;
            }
            unit.animation_timer = unit.animation_frame_duration;
        }
    }
    if (unit.direction == DIRECTION_NORTH) {
        unit.animation_frame.y = 1;
    } else if (unit.direction == DIRECTION_SOUTH) {
        unit.animation_frame.y = 0;
    } else if (unit.direction > DIRECTION_SOUTH) {
        unit.animation_frame.y = 3;
    } else {
        unit.animation_frame.y = 2;
    }
}

std::vector<ivec2> pathfind(ivec2 from, ivec2 to) {
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
            if (child.x < 0 || child.x >= state.map_width || child.y < 0 || child.y >= state.map_height) {
                continue;
            }
            // Don't consider blocked spaces
            if (state.cell_is_blocked[child.x + (state.map_width * child.y)]) {
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

    std::vector<ivec2> empty_path;
    return empty_path;
}
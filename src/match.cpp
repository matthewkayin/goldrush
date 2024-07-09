#include "match.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include "input.h"
#include "network.h"
#include <cstdlib>
#include <algorithm>

static const uint32_t TICK_DURATION = 4;

static const int CAMERA_DRAG_MARGIN = 8;
static const int CAMERA_DRAG_SPEED = 8;

// ANIMATION

struct animation_data_t {
    int v_frame;
    int h_frame_start;
    int h_frame_end;
    uint32_t frame_duration;
    bool is_looping;
};

static const std::unordered_map<uint32_t, animation_data_t> animation_data = {
    { ANIMATION_UI_MOVE, (animation_data_t) {
        .v_frame = 0,
        .h_frame_start = 0, .h_frame_end = 4,
        .frame_duration = 4,
        .is_looping = false
    }},
    { ANIMATION_UNIT_IDLE, (animation_data_t) {
        .v_frame = -1,
        .h_frame_start = 0, .h_frame_end = 0,
        .frame_duration = 0,
        .is_looping = false
    }},
    { ANIMATION_UNIT_MOVE, (animation_data_t) {
        .v_frame = -1,
        .h_frame_start = 1, .h_frame_end = 4,
        .frame_duration = 8,
        .is_looping = true
    }},
    { ANIMATION_UNIT_ATTACK, (animation_data_t) {
        .v_frame = -1,
        .h_frame_start = 5, .h_frame_end = 7,
        .frame_duration = 8,
        .is_looping = false
    }}
};

void animation_t::play(Animation animation) {
    if (this->animation == animation && is_playing) {
        return;
    }
    this->animation = animation;

    auto it = animation_data.find(animation);
    GOLD_ASSERT(it != animation_data.end());

    timer = 0;
    frame.x = it->second.h_frame_start;
    if (it->second.v_frame != -1) {
        frame.y = it->second.v_frame;
    }
    
    // If h_frame_start == h_frame_end, then this is a single-frame "animation" so we shouldn't play it
    if (it->second.h_frame_start == it->second.h_frame_end) {
        is_playing = false;
    } else {
        is_playing = true;
    }
}

void animation_t::update() {
    if (!is_playing) {
        return;
    }
    
    auto it = animation_data.find(animation);

    timer++;
    if (timer == it->second.frame_duration) {
        timer = 0;
        int direction = it->second.h_frame_start < it->second.h_frame_end ? 1 : -1;
        frame.x += direction;
        if (frame.x == it->second.h_frame_end + 1) {
            if (it->second.is_looping) {
                frame.x = it->second.h_frame_start;
            } else {
                frame.x--;
                is_playing = false;
            }
        }
    }
}

void animation_t::stop() {
    is_playing = false;
}

vec2 cell_center_position(ivec2 cell) {
    return vec2(fixed::from_int((cell.x * TILE_SIZE) + (TILE_SIZE / 2)), fixed::from_int((cell.y * TILE_SIZE) + (TILE_SIZE / 2)));
}

void match_t::init() {
    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        input_t empty_input;
        empty_input.type = INPUT_NONE;
        std::vector<input_t> empty_input_list = { empty_input };
        inputs[player_id].push_back(empty_input_list);
        inputs[player_id].push_back(empty_input_list);
        inputs[player_id].push_back(empty_input_list);
    }
    tick_timer = 0;

    camera_offset = ivec2(0, 0);
    is_selecting = false;
    is_minimap_dragging = false;

    // Init map
    map_width = 256;
    map_height = 256;
    map_tiles = std::vector<int>(map_width * map_height);
    map_cells = std::vector<int>(map_width * map_height, CELL_EMPTY);
    for (int i = 0; i < map_width * map_height; i += 3) {
        map_tiles[i] = 1;
    }
    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            if (y == 0 || y == map_height - 1 || x == 0 || x == map_width - 1) {
                map_tiles[x + (y * map_width)] = 2;
            }
        }
    }

    // Init units
    ivec2 player_spawns[MAX_PLAYERS];
    uint8_t current_player_id = network_get_player_id();
    uint32_t player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }
        player_count++;

        ivec2 spawn_point;
        bool is_spawn_point_valid = false;
        int spawn_margin = 6;
        while (!is_spawn_point_valid) {
            int spawn_edge = rand() % 4; 
            // north edge
            if (spawn_edge == 0) {
                spawn_point = ivec2(spawn_margin + (rand() % (map_width - (2 * spawn_margin))), spawn_margin + (rand() % 4));
            // south edge
            } else if (spawn_edge == 1) {
                spawn_point = ivec2(spawn_margin + (rand() % (map_width - (2 * spawn_margin))), map_height - (spawn_margin + (rand() % 4)));
            // west edge
            } else if (spawn_edge == 2) {
                spawn_point = ivec2(spawn_margin + (rand() % 4), spawn_margin + (rand() % (map_height - (2 * spawn_margin))));
            // east edge
            } else {
                spawn_point = ivec2(map_width - (spawn_margin + (rand() % 4)), spawn_margin + (rand() % (map_height - (2 * spawn_margin))));
            }
            // Assert that the spawn point is even in the map
            GOLD_ASSERT(rect_t(ivec2(0, 0), ivec2(map_width, map_height)).has_point(spawn_point));

            is_spawn_point_valid = true;
            for (uint8_t other_player_id = 0; other_player_id < player_id; other_player_id++) {
                if (ivec2::manhattan_distance(spawn_point, player_spawns[other_player_id]) < 8) {
                    is_spawn_point_valid = false;
                    break;
                }
            }

            player_spawns[player_id] = spawn_point;
        }

        if (player_id == current_player_id) {
            camera_move_to_cell(spawn_point);
        }
        units[player_id].reserve(MAX_UNITS);
        unit_spawn(player_id, spawn_point + ivec2(-1, -1));
        unit_spawn(player_id, spawn_point + ivec2(1, -1));
        unit_spawn(player_id, spawn_point + ivec2(0, 1));

        log_info("player %u spawn %vi", player_id, &spawn_point);
    }
    log_info("players initialized");

    // Init ui buttons
    ivec2 ui_button_padding = ivec2(4, 6);
    ivec2 ui_button_top_left = ivec2(SCREEN_WIDTH - 132 + 14, SCREEN_HEIGHT - UI_HEIGHT + 10);
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 3; x++) {
            int index = x + (y * 3);
            ui_buttons[index].enabled = false;
            ui_buttons[index].icon = (ButtonIcon)x;
            ui_buttons[index].rect = rect_t(ui_button_top_left + ivec2((32 + ui_button_padding.x) * x, (32 + ui_button_padding.y) * y), ivec2(32, 32));
        }
    }
    ui_button_hovered = -1;

    mode = MATCH_MODE_NOT_STARTED;
    if (!network_is_server()) {
        network_client_toggle_ready();
    }
    if (network_is_server() && player_count == 1) {
        mode = MATCH_MODE_RUNNING;
    }
}

void match_t::update() {
    // NETWORK EVENTS
    network_service();

    if (mode == MATCH_MODE_NOT_STARTED) {
        network_event_t network_event;
        while (network_poll_events(&network_event)) {
            if (network_is_server() && (network_event.type == NETWORK_EVENT_CLIENT_READY || network_event.type == NETWORK_EVENT_CLIENT_DISCONNECTED)) {
                bool all_players_ready = true;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    const player_t& player = network_get_player(player_id);
                    if (player.status == PLAYER_STATUS_NOT_READY) {
                        all_players_ready = false;
                    }
                }

                if (all_players_ready) {
                    network_server_start_match();
                    mode = MATCH_MODE_RUNNING;
                }
            } else if (!network_is_server() && network_event.type == NETWORK_EVENT_MATCH_START) {
                mode = MATCH_MODE_RUNNING;
            }
        }
        
        // We make the check again here in case the mode changed
        if (mode == MATCH_MODE_NOT_STARTED) {
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
    if (tick_timer == 0) {
        bool has_all_inputs = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            if (inputs[player_id].empty()) {
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

            for (const input_t& input : inputs[player_id][0]) {
                handle_input(player_id, input);
            }
            inputs[player_id].erase(inputs[player_id].begin());
        }

        tick_timer = TICK_DURATION;
        input_flush();
    }
    tick_timer--;

    ivec2 mouse_pos = input_get_mouse_position();
    uint8_t current_player_id = network_get_player_id();

    // CAMERA DRAG
    if (!is_selecting && !is_minimap_dragging) {
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
        camera_offset += camera_drag_direction * CAMERA_DRAG_SPEED;
        camera_clamp();
    }

    // UI Buttons
    if (!input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT)) {
        ui_button_hovered = -1;
        for (int i = 0; i < 6; i++) {
            if (!ui_buttons[i].enabled) {
                continue;
            }

            if (ui_buttons[i].rect.has_point(mouse_pos)) {
                ui_button_hovered = i;
                break;
            }
        }
    }

    // LEFT MOUSE CLICK
    ivec2 mouse_world_pos = mouse_pos + camera_offset;
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        if (ui_button_hovered != -1) {
            // handle ui button press
        } else if (MINIMAP_RECT.has_point(mouse_pos)) {
            is_minimap_dragging = true;
        } else if (mouse_pos.y < SCREEN_HEIGHT - UI_HEIGHT) {
            // On begin selecting
            is_selecting = true;
            select_origin = mouse_world_pos;
        } 
    }
    // SELECT RECT
    if (is_selecting) {
        // Update select rect
        select_rect.position = ivec2(std::min(select_origin.x, mouse_world_pos.x), std::min(select_origin.y, mouse_world_pos.y));
        select_rect.size = ivec2(std::max(1, std::abs(select_origin.x - mouse_world_pos.x)), std::max(1, std::abs(select_origin.y - mouse_world_pos.y)));

        // Update unit selection
        for (unit_t& unit : units[current_player_id]) {
            unit.is_selected = unit.get_rect().intersects(select_rect);
        }
    } else if (is_minimap_dragging) {
        ivec2 minimap_pos = mouse_pos - MINIMAP_RECT.position;
        minimap_pos.x = std::clamp(minimap_pos.x, 0, MINIMAP_RECT.size.x);
        minimap_pos.y = std::clamp(minimap_pos.y, 0, MINIMAP_RECT.size.y);
        ivec2 map_pos = ivec2((map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        camera_move_to_cell(map_pos / TILE_SIZE);
    }
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        is_selecting = false;
        is_minimap_dragging = false;
    } 

    // Command
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT)) {
        bool should_command_move = false;
        ivec2 move_target;
        if (MINIMAP_RECT.has_point(mouse_pos)) {
            ivec2 minimap_pos = mouse_pos - MINIMAP_RECT.position;
            move_target = ivec2((map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
            should_command_move = true;
        } else if (mouse_pos.y < SCREEN_HEIGHT - UI_HEIGHT) {
            move_target = mouse_world_pos;
            should_command_move = true;
        }

        if (should_command_move) {
            input_t input;
            input.type = INPUT_MOVE;
            input.move.target_cell = move_target / TILE_SIZE;
            input.move.unit_count = 0;
            for (uint8_t unit_id = 0; unit_id < units[current_player_id].size(); unit_id++) {
                if (!units[current_player_id][unit_id].is_selected) {
                    continue;
                }
                input.move.unit_ids[input.move.unit_count] = unit_id;
                input.move.unit_count++;
            }
            if (input.move.unit_count != 0) {
                input_queue.push_back(input);

                ui_move_position = move_target;
                ui_move_animation.stop();
                ui_move_animation.play(ANIMATION_UI_MOVE);
            }
        } 
    }

    // Unit update
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (unit_t& unit : units[player_id]) {
            unit_update(unit);
        }
    }

    // UI update
    ui_move_animation.update();
}

void match_t::handle_input(uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_MOVE: {
            // Calculate unit group info
            ivec2 first_unit_cell = units[player_id][input.move.unit_ids[0]].cell;
            ivec2 group_center = first_unit_cell;
            ivec2 group_min = first_unit_cell;
            ivec2 group_max = first_unit_cell;
            for (uint32_t i = 1; i < input.move.unit_count; i++) {
                unit_t& unit = units[player_id][input.move.unit_ids[i]];
                group_center += unit.cell;
                group_min.x = std::min(group_min.x, unit.cell.x);
                group_min.y = std::min(group_min.y, unit.cell.y);
                group_max.x = std::max(group_max.x, unit.cell.x);
                group_max.y = std::max(group_max.y, unit.cell.y);
            }
            group_center = group_center / input.move.unit_count;
            bool is_target_inside_group = rect_t(group_min, group_max - group_min).has_point(input.move.target_cell);

            for (uint32_t i = 0; i < input.move.unit_count; i++) {
                unit_t& unit = units[player_id][input.move.unit_ids[i]];

                // Determine the unit's target
                ivec2 offset = unit.cell - group_center;
                ivec2 unit_target = input.move.target_cell + offset;
                bool is_target_invalid = is_target_inside_group || unit_target.x < 0 || unit_target.x >= map_width || unit_target.y < 0 || unit_target.y >= map_height ||
                                         ivec2::manhattan_distance(unit_target, input.move.target_cell) > 3 || cell_is_blocked(unit_target);
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
        } // End case UNIT_MOVE
        default:
            break;
    }
}

void match_t::input_flush() {
    static const uint32_t INPUT_MAX_SIZE = 201;

    static uint8_t out_buffer[INPUT_BUFFER_SIZE];
    static size_t out_buffer_length;

    // Always send at least 1 input per tick
    if (input_queue.empty()) {
        input_t empty_input;
        empty_input.type = INPUT_NONE;
        input_queue.push_back(empty_input);
    }

    // Assert that the out buffer is big enough
    GOLD_ASSERT((INPUT_BUFFER_SIZE - 3) >= (INPUT_MAX_SIZE * input_queue.size()));

    // Leave a space in the buffer for the network message type
    uint8_t current_player_id = network_get_player_id();
    out_buffer[1] = current_player_id;
    out_buffer_length = 2;

    // Serialize the inputs
    for (const input_t& input : input_queue) {
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
    inputs[current_player_id].push_back(input_queue);
    input_queue.clear();

    // Send them to other players
    if (network_is_server()) {
        network_server_send_input(out_buffer, out_buffer_length);
    } else {
        network_client_send_input(out_buffer, out_buffer_length);
    }
}

void match_t::input_deserialize(uint8_t* in_buffer, size_t in_buffer_length) {
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

    inputs[in_player_id].push_back(tick_inputs);
}

void match_t::camera_clamp() {
    camera_offset.x = std::clamp(camera_offset.x, 0, (map_width * TILE_SIZE) - SCREEN_WIDTH);
    camera_offset.y = std::clamp(camera_offset.y, 0, (map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT);
}

void match_t::camera_move_to_cell(ivec2 cell) {
    camera_offset = ivec2((cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2), (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2));
    camera_clamp();
}

bool match_t::cell_is_blocked(ivec2 cell) {
    return map_cells[cell.x + (cell.y * map_width)] != CELL_EMPTY;
}

void match_t::cell_set_value(ivec2 cell, int value) {
    map_cells[cell.x + (cell.y * map_width)] = value;
}

void match_t::unit_spawn(uint8_t player_id, ivec2 cell) {
    unit_t unit;
    unit.is_selected = false;
    unit.is_moving = false;
    unit.cell = cell;
    unit.position = cell_center_position(cell);
    unit.direction = DIRECTION_SOUTH;
    units[player_id].push_back(unit);

    cell_set_value(cell, CELL_FILLED);
}

void match_t::unit_try_move(unit_t& unit) {
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
    if (cell_is_blocked(next_point)) {
        return;
    }

    // Set state to begin movement
    cell_set_value(unit.cell, CELL_EMPTY);
    unit.cell = next_point;
    cell_set_value(unit.cell, CELL_FILLED);
    unit.target_position = cell_center_position(unit.cell);
    unit.path.erase(unit.path.begin());
    unit.is_moving = true;
    unit.path_timer.stop();
}

void match_t::unit_update(unit_t& unit) {
    if (!unit.is_moving && !unit.path.empty()) {
        unit_try_move(unit);
        if (!unit.is_moving && unit.path_timer.is_stopped) {
            unit.path_timer.start(unit_t::PATH_PAUSE_DURATION);
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
    } else if (!unit.path_timer.is_stopped) {
        unit.path_timer.update();
        if (unit.path_timer.is_finished) {
            unit.path_timer.stop();
            ivec2 unit_target = unit.path[unit.path.size() - 1];
            unit.path.clear();
            if (!cell_is_blocked(unit_target)) {
                log_info("repathing");
                unit.path = pathfind(unit.cell, unit_target);
            }
        }
    }

    unit.animation.update();
    if (unit.is_moving) {
        unit.animation.play(ANIMATION_UNIT_MOVE);
    } else {
        unit.animation.play(ANIMATION_UNIT_IDLE);
    }
    if (unit.direction == DIRECTION_NORTH) {
        unit.animation.frame.y = 1;
    } else if (unit.direction == DIRECTION_SOUTH) {
        unit.animation.frame.y = 0;
    } else if (unit.direction > DIRECTION_SOUTH) {
        unit.animation.frame.y = 3;
    } else {
        unit.animation.frame.y = 2;
    }
}

std::vector<ivec2> match_t::pathfind(ivec2 from, ivec2 to) {
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
            if (child.x < 0 || child.x >= map_width || child.y < 0 || child.y >= map_height) {
                continue;
            }
            // Don't consider blocked spaces
            if (cell_is_blocked(child)) {
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

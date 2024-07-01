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
static const int CELL_SIZE = 8;

// UNIT

struct unit_t {
    bool is_selected;
    vec2 position;
    vec2 target_position;
    animation_t animation;

    unit_t() {
        animation = animation_t(SPRITE_UNIT_MINER);
        is_selected = false;
        target_position = vec2(fixed::from_int(-1), fixed::from_int(-1));
    }

    rect_t get_rect() {
        return rect_t(ivec2(position.x.integer_part(), position.y.integer_part()), sprite_get_frame_size(SPRITE_UNIT_MINER));
    }
};

// INPUT

enum InputType {
    INPUT_NONE,
    INPUT_MOVE
};

struct input_move_t {
    vec2 target_position;
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

struct match_state_t {
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

    /*
    std::vector<bool> cell_is_blocked;
    int cell_width;
    int cell_height;
    */

    std::vector<unit_t> units[MAX_PLAYERS];
};
static match_state_t state;

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
                memcpy(out_buffer + out_buffer_length, &input.move.target_position, sizeof(vec2));
                out_buffer_length += sizeof(vec2);

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
                memcpy(&input.move.target_position, in_buffer + in_buffer_head, sizeof(vec2));
                in_buffer_head += sizeof(vec2);

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

    // Init units
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NOT_READY) {
            continue;
        }
        state.units[player_id].reserve(MAX_UNITS);
    }

    state.units[0].push_back(unit_t());
    state.units[0][0].position = vec2(fixed::from_int(16), fixed::from_int(32));
    state.units[0].push_back(unit_t());
    state.units[0][1].position = vec2(fixed::from_int(32), fixed::from_int(48));
    state.units[0].push_back(unit_t());
    state.units[0][2].position = vec2(fixed::from_int(64), fixed::from_int(16));

    state.units[1].push_back(unit_t());
    state.units[1][0].position = vec2(fixed::from_int(128), fixed::from_int(32));
    state.units[1].push_back(unit_t());
    state.units[1][1].position = vec2(fixed::from_int(128 + 32), fixed::from_int(48));
    state.units[1].push_back(unit_t());
    state.units[1][2].position = vec2(fixed::from_int(128 + 64), fixed::from_int(16));

    // Init cell grid
    /*
    state.cell_width = state.map_width * (TILE_SIZE / CELL_SIZE);
    state.cell_height = state.map_height * (TILE_SIZE / CELL_SIZE);
    state.cell_is_blocked = std::vector<bool>(state.cell_width * state.cell_height, false);
    for (unit_t& unit : state.units[0]) {
        ivec2 start_cell = ivec2(unit.position.x.integer_part(), unit.position.y.integer_part()) / CELL_SIZE;
        ivec2 end_cell = ivec2(std::min(start_cell.x + unit.cell_size.x, state.cell_width - 1), std::min(start_cell.y + unit.cell_size.y, state.cell_height - 1));

        for (int cell_y = start_cell.y; cell_y < end_cell.y; cell_y++) {
            for (int cell_x = start_cell.x; cell_x < end_cell.x; cell_x++) {
                state.cell_is_blocked[cell_x + (cell_y * state.cell_width)] = true;
            }
        }
    }
    */
}

void match_handle_input(uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_MOVE: {
            for (uint32_t i = 0; i < input.move.unit_count; i++) {
                state.units[player_id][input.move.unit_ids[i]].target_position = input.move.target_position;
                state.units[player_id][input.move.unit_ids[i]].animation.start();
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
        input.move.target_position = vec2(fixed::from_int(mouse_world_pos.x), fixed::from_int(mouse_world_pos.y));
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
            if (unit.target_position.x.integer_part() == -1 && unit.target_position.y.integer_part() == -1) {
                continue;
            }
            // unit.animation.update();
            fixed step = fixed::from_int(2);
            fixed distance = unit.position.distance_to(unit.target_position);
            vec2 old_position = unit.position;
            if (distance < step) {
                unit.position = unit.target_position;
                unit.target_position = vec2(fixed::from_int(-1), fixed::from_int(-1));
                unit.animation.stop();
                unit.animation.frame.x = 0;
            } else {
                unit.position += unit.position.direction_to(unit.target_position) * step;
            }
            vec2 actual_step = unit.position - old_position;
            log_info("step: %d,%d position: %d,%d", actual_step.x, actual_step.y, unit.position.x, unit.position.y);
        }
    }
}

// PATHFINDING

/*
void cell_set_blocked(ivec2 cell_start, ivec2 cell_size, bool value) {
    ivec2 cell_end = ivec2(std::min(cell_start.x + cell_size.x, state.cell_width - 1), std::min(cell_start.y + cell_size.y, state.cell_height - 1));

    for (int cell_y = cell_start.y; cell_y < cell_end.y; cell_y++) {
        for (int cell_x = cell_start.x; cell_x < cell_end.x; cell_x++) {
            state.cell_is_blocked[cell_x + (cell_y * state.cell_width)] = value;
        }
    }
}
*/

void match_render() {
    uint8_t current_player_id = network_get_player_id();

    render_map(state.camera_offset, &state.map_tiles[0], state.map_width, state.map_height);

    // Select rings
    for (const unit_t& unit : state.units[current_player_id]) {
        if (unit.is_selected) {
            render_sprite(state.camera_offset, SPRITE_SELECT_RING, ivec2(0, 0), unit.position + vec2(fixed::from_int(0), fixed::from_int(17)));
        }
    }

    // Units
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (const unit_t& unit : state.units[player_id]) {
            render_sprite_animation(state.camera_offset, unit.animation, unit.position);
        }
    }

    // Select rect
    if (state.is_selecting) {
        render_rect(rect_t(state.select_rect.position - state.camera_offset, state.select_rect.size), COLOR_WHITE);
    }
}
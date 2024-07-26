#include "match.h"

#include "asserts.h"
#include "network.h"
#include "logger.h"
#include "input.h"
#include <cstring>
#include <unordered_map>
#include <array>
#include <algorithm>

static const uint32_t TICK_DURATION = 4;
static const uint32_t INPUT_MAX_SIZE = 256;
static const uint32_t UI_STATUS_DURATION = 60;
const std::unordered_map<UiButtonset, std::array<UiButton, 6>> UI_BUTTONS = {
    { UI_BUTTONSET_NONE, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_UNIT, { UI_BUTTON_MOVE, UI_BUTTON_STOP, UI_BUTTON_ATTACK,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_MINER, { UI_BUTTON_MOVE, UI_BUTTON_STOP, UI_BUTTON_ATTACK,
                      UI_BUTTON_BUILD, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_BUILD, { UI_BUTTON_BUILD_HOUSE, UI_BUTTON_BUILD_CAMP, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_CANCEL, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }}
};
static const xy UI_BUTTON_SIZE = xy(32, 32);
static const xy UI_BUTTON_PADDING = xy(4, 6);
static const xy UI_BUTTON_TOP_LEFT = xy(SCREEN_WIDTH - 132 + 14, SCREEN_HEIGHT - UI_HEIGHT + 10);
static const xy UI_BUTTON_POSITIONS[6] = { UI_BUTTON_TOP_LEFT, UI_BUTTON_TOP_LEFT + xy(UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x, 0), UI_BUTTON_TOP_LEFT + xy(2 * (UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x), 0),
                                              UI_BUTTON_TOP_LEFT + xy(0, UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y), UI_BUTTON_TOP_LEFT + xy(UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x, UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y), UI_BUTTON_TOP_LEFT + xy(2 * (UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x), UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y) };
static const rect_t UI_BUTTON_RECT[6] = {
    rect_t(UI_BUTTON_POSITIONS[0], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[1], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[2], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[3], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[4], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[5], UI_BUTTON_SIZE),
};

static const int CAMERA_DRAG_MARGIN = 8;
static const int CAMERA_DRAG_SPEED = 8;

static const uint32_t PATH_PAUSE_DURATION = 20;
static const uint32_t BUILD_TICK_DURATION = 8;

static const char* UI_STATUS_CANT_BUILD = "You can't build there.";
static const char* UI_STATUS_NOT_ENOUGH_GOLD = "Not enough gold.";

const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA = {
    { UNIT_MINER, (unit_data_t) {
        .sprite = SPRITE_UNIT_MINER,
        .max_health = 20,
        .speed = fixed::from_int(1)
    }}
};

const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA = {
    { BUILDING_HOUSE, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 100,
        .max_health = 100,
        .builder_positions_x = { 3, 16, -4 },
        .builder_positions_y = { 15, 15, 3 },
        .builder_flip_h = { false, true, false }
    }},
    { BUILDING_CAMP, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 100,
        .max_health = 100,
        .builder_positions_x = { 1, 15, 14 },
        .builder_positions_y = { 13, 13, 2 },
        .builder_flip_h = { false, true, true }
    }}
};

match_state_t match_init() {
    match_state_t state;

    // Init UI
    state.ui_mode = UI_MODE_NOT_STARTED;
    state.ui_buttonset = UI_BUTTONSET_NONE;
    state.camera_offset = xy(0, 0);
    state.ui_status_timer = 0;

    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        // Since all inputs will be handled 4 ticks in the future, 
        // we pad the input list with 3 ticks-worth of empty inputs
        input_t empty_input;
        empty_input.type = INPUT_NONE;
        std::vector<input_t> empty_input_list = { empty_input };
        state.inputs[player_id].push_back(empty_input_list);
        state.inputs[player_id].push_back(empty_input_list);
        state.inputs[player_id].push_back(empty_input_list);
    }
    state.tick_timer = 0;

    // Init map
    state.map_width = 64;
    state.map_height = 64;
    state.map_tiles = std::vector<uint32_t>(state.map_width * state.map_height);
    state.map_cells = std::vector<uint32_t>(state.map_width * state.map_height, CELL_EMPTY);

    for (uint32_t i = 0; i < state.map_width * state.map_height; i += 3) {
        state.map_tiles[i] = 1;
    }
    for (uint32_t y = 0; y < state.map_height; y++) {
        for (uint32_t x = 0; x < state.map_width; x++) {
            if (y == 0 || y == state.map_height - 1 || x == 0 || x == state.map_width - 1) {
                state.map_tiles[x + (y * state.map_width)] = 2;
            }
        }
    }

    // Init players
    xy player_spawns[MAX_PLAYERS];
    bool is_spawn_direction_used[DIRECTION_COUNT];
    uint8_t player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }
        player_count++;

        state.player_gold[player_id] = 150;

        // Find an unused spawn direction
        int spawn_direction;
        do {
            spawn_direction = rand() % DIRECTION_COUNT;
        } while (is_spawn_direction_used[spawn_direction]);
        is_spawn_direction_used[spawn_direction] = true;

        // Determine player spawn point
        player_spawns[player_id] = xy(state.map_width / 2, state.map_height / 2) + 
                                   xy(DIRECTION_XY[spawn_direction].x * ((state.map_width * 6) / 16), 
                                      DIRECTION_XY[spawn_direction].y * ((state.map_height * 6) / 16));
        log_info("player %u spawn %vi", player_id, &player_spawns[player_id]);

        // Place player starting units
        match_unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(-1, -1));
        match_unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(1, -1));
        match_unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(0, 1));
    }
    state.camera_offset = match_camera_clamp(match_camera_centered_on_cell(player_spawns[network_get_player_id()]), state.map_width, state.map_height);

    // Place gold on the map
    int gold_target = 128;
    int gold_count = 0;
    while (gold_count < gold_target) {
        // Randomly find the start of a gold cluster
        xy cluster_cell = xy(rand() % state.map_width, rand() % state.map_height);
        if (match_map_is_cell_blocked(state, cluster_cell)) {
            continue;
        }

        // Check that it's not too close to the player spawns
        bool is_cluster_too_close_to_player = false;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status != PLAYER_STATUS_NONE && 
                xy::manhattan_distance(cluster_cell, player_spawns[player_id]) < 6) {
                    is_cluster_too_close_to_player = true;
                    break;
            }
        }
        if (is_cluster_too_close_to_player) {
            continue;
        }

        // Place gold on the map around the cluster
        int cluster_size = std::min(3 + (rand() % 7), gold_target - gold_count);
        for (int i = 0; i < cluster_size; i++) {
            match_map_set_cell_value(state, cluster_cell, CELL_GOLD1 + (rand() % 3), 100);
            gold_count++;
        }

        // Determine the position of the next gold cell
        int guess_direction = rand() % DIRECTION_COUNT;
        int direction = guess_direction;
        do {
            direction = (direction + 1) % DIRECTION_COUNT;
        } while (direction != guess_direction && 
                !match_map_is_cell_in_bounds(state, cluster_cell) && 
                 match_map_is_cell_blocked(state, cluster_cell + DIRECTION_XY[direction]));
        if (direction == guess_direction) {
            // There are no free spaces to place gold in, so exit the loop for this cluster
            break;
        }
        cluster_cell += DIRECTION_XY[direction];
    }

    if (!network_is_server()) {
        network_client_toggle_ready();
    }
    if (network_is_server() && player_count == 1) {
        state.ui_mode = UI_MODE_NONE;
    }

    return state;
}

void match_update(match_state_t& state) {
    // NETWORK EVENTS
    network_service();

    // Wait for match to start
    if (state.ui_mode == UI_MODE_NOT_STARTED) {
        network_event_t network_event;
        while (network_poll_events(&network_event)) {
            if (network_is_server() && (network_event.type == NETWORK_EVENT_CLIENT_READY || network_event.type == NETWORK_EVENT_CLIENT_DISCONNECTED)) {
                if (network_are_all_players_ready()) {
                    network_server_start_match();
                    state.ui_mode = UI_MODE_NONE;
                }
            } else if (!network_is_server() && network_event.type == NETWORK_EVENT_MATCH_START) {
                state.ui_mode = UI_MODE_NONE;
            }
        }
        
        // We make the check again here in case the mode changed
        if (state.ui_mode == UI_MODE_NOT_STARTED) {
            return;
        }
    }

    // Poll network events
    network_event_t network_event;
    while (network_poll_events(&network_event)) {
        switch (network_event.type) {
            case NETWORK_EVENT_INPUT: {
                // Deserialize input
                std::vector<input_t> tick_inputs;

                uint8_t* in_buffer = network_event.input.data;
                size_t in_buffer_length = network_event.input.data_length;
                uint8_t in_player_id = in_buffer[1];
                size_t in_buffer_head = 2;

                while (in_buffer_head < in_buffer_length) {
                    tick_inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
                }

                state.inputs[in_player_id].push_back(tick_inputs);
                break;
            }
            default:
                break;
        }
    }

    // Tick loop
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

        // HANDLE INPUT
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            for (const input_t& input : state.inputs[player_id][0]) {
                match_input_handle(state, player_id, input);
            } // End for each input in player queue
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        } // End for each player
        // End handle input

        state.tick_timer = TICK_DURATION;

        // Flush input
        uint8_t out_buffer[INPUT_BUFFER_SIZE];
        size_t out_buffer_length;

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
            match_input_serialize(out_buffer, out_buffer_length, input);
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
    state.tick_timer--;

    xy mouse_pos = input_get_mouse_position();
    xy mouse_world_pos = mouse_pos + state.camera_offset;

    // LEFT MOUSE CLICK
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        if (match_get_ui_button_hovered(state) != -1) {
            match_ui_handle_button_pressed(state, match_get_ui_button(state, match_get_ui_button_hovered(state)));
        } else if (state.ui_mode == UI_MODE_BUILDING_PLACE && !match_is_mouse_in_ui()) {
            if (match_building_can_be_placed(state, state.ui_building_type, match_ui_building_cell(state))) {
                input_t input;
                input.type = INPUT_BUILD;
                input.build.building_type = state.ui_building_type;
                input.build.target_cell = match_ui_building_cell(state);
                input.build.unit_id = state.selection.ids[0];
                state.input_queue.push_back(input);

                state.ui_buttonset = UI_BUTTONSET_MINER;
                state.ui_mode = UI_MODE_NONE;
            } else {
                match_ui_show_status(state, UI_STATUS_CANT_BUILD);
            }
        } else if (state.ui_mode == UI_MODE_NONE && MINIMAP_RECT.has_point(mouse_pos)) {
            // On begin minimap drag
            state.ui_mode = UI_MODE_MINIMAP_DRAG;
        } else if (state.ui_mode == UI_MODE_NONE && !match_is_mouse_in_ui()) {
            // On begin selecting
            state.ui_mode = UI_MODE_SELECTING;
            state.select_origin = mouse_world_pos;
        } 
    } 

    // SELECT RECT
    if (state.ui_mode == UI_MODE_SELECTING) {
        // Update select rect
        state.select_rect.position = xy(std::min(state.select_origin.x, mouse_world_pos.x), std::min(state.select_origin.y, mouse_world_pos.y));
        state.select_rect.size = xy(std::max(1, std::abs(state.select_origin.x - mouse_world_pos.x)), std::max(1, std::abs(state.select_origin.y - mouse_world_pos.y)));
    } else if (state.ui_mode == UI_MODE_MINIMAP_DRAG) {
        xy minimap_pos = mouse_pos - MINIMAP_RECT.position;
        minimap_pos.x = std::clamp(minimap_pos.x, 0, MINIMAP_RECT.size.x);
        minimap_pos.y = std::clamp(minimap_pos.y, 0, MINIMAP_RECT.size.y);
        xy map_pos = xy((state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        state.camera_offset = match_camera_clamp(match_camera_centered_on_cell(map_pos / TILE_SIZE), state.map_width, state.map_height);
    }

    // LEFT MOUSE RELEASE
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        if (state.ui_mode == UI_MODE_SELECTING) {
            selection_t selection = match_ui_create_selection_from_rect(state);
            match_ui_set_selection(state, selection);
            state.ui_mode = UI_MODE_NONE;
        } else if (state.ui_mode == UI_MODE_MINIMAP_DRAG) {
            state.ui_mode = UI_MODE_NONE;
        }
    }

    // RIGHT MOUSE CLICK
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT)) {
        if (state.ui_mode == UI_MODE_NONE && state.selection.type == SELECTION_TYPE_UNITS && 
            (MINIMAP_RECT.has_point(mouse_pos) || !match_is_mouse_in_ui())) {
            xy move_target;
            if (match_is_mouse_in_ui()) {
                xy minimap_pos = mouse_pos - MINIMAP_RECT.position;
                move_target = xy((state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, 
                                 (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
            } else {
                move_target = mouse_world_pos;
            }

            // Create the move event
            input_t input;
            input.type = INPUT_MOVE;
            input.move.target_cell = move_target / TILE_SIZE;
            input.move.unit_count = 0;
            memcpy(input.move.unit_ids, &state.selection.ids[0], state.selection.ids.size() * sizeof(uint16_t));
            input.move.unit_count = state.selection.ids.size();
            // The unit count should be greater than 0 because the selection type is SELECTION_TYPE_UNITS
            GOLD_ASSERT(input.move.unit_count != 0);
            state.input_queue.push_back(input);

            // Provide instant user feedback
            state.ui_move_value = match_map_get_cell_value(state, input.move.target_cell);
            if ((state.ui_move_value & CELL_TYPE_MASK) == CELL_EMPTY) {
                state.ui_move_animation = animation_create(ANIMATION_UI_MOVE);
                state.ui_move_position = mouse_world_pos;
            } else {
                state.ui_move_animation = animation_create(ANIMATION_UI_MOVE_GOLD);
                state.ui_move_position = match_cell_center(input.move.target_cell).to_xy();
            }
        }
    }

    // CAMERA DRAG
    if (state.ui_mode != UI_MODE_SELECTING && state.ui_mode != UI_MODE_MINIMAP_DRAG) {
        xy camera_drag_direction = xy(0, 0);
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
        state.camera_offset = match_camera_clamp(state.camera_offset, state.map_width, state.map_height);
    }

    // Update UI status timer
    if (state.ui_status_timer != 0) {
        state.ui_status_timer--;
    }

    // Update particles
    if (animation_is_playing(state.ui_move_animation)) {
        animation_update(state.ui_move_animation);
    }

    match_unit_update(state);
}

// INPUT

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_MOVE: {
            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);

            out_buffer[out_buffer_length] = input.move.unit_count;
            out_buffer_length += 1;

            memcpy(out_buffer + out_buffer_length, input.move.unit_ids, input.move.unit_count * sizeof(entity_id));
            out_buffer_length += input.move.unit_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP: {
            out_buffer[out_buffer_length] = input.stop.unit_count;
            out_buffer_length += 1;

            memcpy(out_buffer + out_buffer_length, input.stop.unit_ids, input.stop.unit_count * sizeof(entity_id));
            out_buffer_length += input.stop.unit_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD: {
            memcpy(out_buffer + out_buffer_length, &input.build, sizeof(input_build_t));
            out_buffer_length += sizeof(input_build_t);
            break;
        }
        case INPUT_BUILD_CANCEL: {
            memcpy(out_buffer + out_buffer_length, &input.build_cancel, sizeof(input_build_cancel_t));
            out_buffer_length += sizeof(input_build_cancel_t);
            break;
        }
        default:
            break;
    }
}

input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head) {
    input_t input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        case INPUT_MOVE: {
            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            input.move.unit_count = in_buffer[in_buffer_head];
            in_buffer_head++;
            memcpy(input.move.unit_ids, in_buffer + in_buffer_head, input.move.unit_count * sizeof(entity_id));
            in_buffer_head += input.move.unit_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP: {
            input.stop.unit_count = in_buffer[in_buffer_head];
            in_buffer_head++;
            memcpy(input.stop.unit_ids, in_buffer + in_buffer_head, input.stop.unit_count * sizeof(entity_id));
            in_buffer_head += input.stop.unit_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD: {
            memcpy(&input.build, in_buffer + in_buffer_head, sizeof(input_build_t));
            in_buffer_head += sizeof(input_build_t);
            break;
        }
        case INPUT_BUILD_CANCEL: {
            memcpy(&input.build_cancel, in_buffer + in_buffer_head, sizeof(input_build_cancel_t));
            in_buffer_head += sizeof(input_build_cancel_t);
        }
        default:
            break;
    }

    return input;
}

void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_MOVE: {
            bool should_move_as_group = input.move.unit_count > 1 && match_map_get_cell_value(state, input.move.target_cell) == CELL_EMPTY;
            xy group_center;

            if (should_move_as_group) {
                // Collect a list of all the unit cells
                std::vector<xy> unit_cells;
                unit_cells.reserve(input.move.unit_count);
                for (uint32_t i = 0; i < input.move.unit_count; i++) {
                    uint32_t unit_index = state.units.get_index_of(input.move.unit_ids[i]);
                    if (unit_index == INDEX_INVALID) {
                        continue;
                    }

                    unit_cells.push_back(state.units[unit_index].cell);
                }

                // Create a bounding rect around the cells
                rect_t group_rect = create_bounding_rect_for_points(&unit_cells[0], unit_cells.size());

                // Use the rect to determine the group center
                group_center = group_rect.position + (group_rect.size / 2);
                
                // If the input target is inside the bounding rect, then don't move as a group.
                // This allows the units to converge in on the target cell (otherwise they would just stay roughly where they are in this situation)
                if (group_rect.has_point(input.move.target_cell)) {
                    should_move_as_group = false;
                }
            }

            // Give each unit the move command
            for (uint32_t i = 0; i < input.move.unit_count; i++) {
                uint32_t unit_index = state.units.get_index_of(input.move.unit_ids[i]);
                if (unit_index == INDEX_INVALID) {
                    continue;
                }
                unit_t& unit = state.units[unit_index];

                // Determine the unit's target
                xy unit_target = input.move.target_cell;
                if (should_move_as_group) {
                    xy group_move_target = input.move.target_cell + (unit.cell - group_center);
                    if (match_map_is_cell_in_bounds(state, group_move_target) && xy::manhattan_distance(group_move_target, input.move.target_cell) <= 3) {
                        unit_target = group_move_target;
                    }
                }

                // Set the units target
                switch (match_map_get_cell_type(state, unit_target)) {
                    case CELL_EMPTY: {
                        unit.target = UNIT_TARGET_CELL;
                        unit.target_cell = unit_target;
                        break;
                    }
                    case CELL_UNIT: {
                        unit.target = UNIT_TARGET_UNIT;
                        unit.target_entity_id = match_map_get_cell_id(state, unit_target);
                        break;
                    }
                    case CELL_BUILDING: {
                        unit.target = UNIT_TARGET_BUILDING;
                        unit.target_entity_id = match_map_get_cell_id(state, unit_target);
                        break;
                    }
                    case CELL_GOLD1:
                    case CELL_GOLD2:
                    case CELL_GOLD3: {
                        unit.target = UNIT_TARGET_GOLD;
                        unit.target_cell = unit_target;
                        break;
                    }
                } // End switch unit target cell type
                match_unit_path_to_target(state, unit);
                if (unit.path.empty()) {
                    unit.target = UNIT_TARGET_NONE;
                }
            }
            break;
        }
        case INPUT_STOP: {
            for (uint32_t i = 0; i < input.stop.unit_count; i++) {
                uint32_t unit_index = state.units.get_index_of(input.stop.unit_ids[i]);
                if (unit_index == INDEX_INVALID) {
                    continue;
                }
                unit_t& unit = state.units[unit_index];

                unit.path.clear();
                unit.target = UNIT_TARGET_NONE;
                unit.building_type = BUILDING_NONE;
            }
            break;
        }
        case INPUT_BUILD: {
            uint32_t unit_index = state.units.get_index_of(input.build.unit_id);
            if (unit_index == INDEX_INVALID) {
                return;
            }
            unit_t& unit = state.units[unit_index];

            // Determine the unit's target
            xy unit_build_target = match_get_nearest_free_cell_within_rect(unit.cell, rect_t(input.build.target_cell, match_building_cell_size((BuildingType)input.build.building_type)));
            unit.target = UNIT_TARGET_BUILD;
            unit.target_cell = unit_build_target;
            match_unit_path_to_target(state, unit);
            if (unit.path.empty()) {
                unit.target = UNIT_TARGET_NONE;
                match_ui_show_status(state, UI_STATUS_CANT_BUILD);
                return;
            }

            unit.building_type = (BuildingType)input.build.building_type;
            unit.building_cell = input.build.target_cell;
            unit.building_id = ID_NULL;
            break;
        }
        case INPUT_BUILD_CANCEL: {
            uint32_t building_index = state.buildings.get_index_of(input.build_cancel.building_id);
            if (building_index == INDEX_INVALID) {
                return;
            }
            building_t& building = state.buildings[building_index];

            // Refund the player
            state.player_gold[player_id] += BUILDING_DATA.at(building.type).cost;
            // Tell the unit to stop building this building
            for (unit_t& unit : state.units) {
                if (unit.building_id == input.build_cancel.building_id) {
                    match_unit_stop_building(state, unit, building);
                }
            }
            // Destroy the building
            match_building_destroy(state, input.build_cancel.building_id);
            break;
        }
        default:
            break;
    }
}

// UI

void match_ui_show_status(match_state_t& state, const char* message) {
    state.ui_status_message = std::string(message);
    state.ui_status_timer = UI_STATUS_DURATION;
}

UiButton match_get_ui_button(const match_state_t& state, int index) {
    return UI_BUTTONS.at(state.ui_buttonset)[index];
}

int match_get_ui_button_hovered(const match_state_t& state) {
    xy mouse_pos = input_get_mouse_position();
    for (int i = 0; i < 6; i++) {
        if (match_get_ui_button(state, i) == UI_BUTTON_NONE) {
            continue;
        }

        if (UI_BUTTON_RECT[i].has_point(mouse_pos)) {
            return i;
        }
    }

    return -1;
}

const rect_t& match_get_ui_button_rect(int index) {
    return UI_BUTTON_RECT[index];
}

void match_ui_handle_button_pressed(match_state_t& state, UiButton button) {
    switch (button) {
        case UI_BUTTON_STOP: {
            if (state.selection.type == SELECTION_TYPE_UNITS) {
                input_t input;
                input.type = INPUT_STOP;
                memcpy(input.stop.unit_ids, &state.selection.ids[0], state.selection.ids.size() * sizeof(entity_id));
                input.stop.unit_count = state.selection.ids.size();
                state.input_queue.push_back(input);
            }
            break;
        }
        case UI_BUTTON_BUILD: {
            state.ui_buttonset = UI_BUTTONSET_BUILD;
            break;
        }
        case UI_BUTTON_CANCEL: {
            if (state.ui_buttonset == UI_BUTTONSET_BUILD) {
                state.ui_buttonset = UI_BUTTONSET_MINER;
            } else if (state.ui_mode == UI_MODE_BUILDING_PLACE) {
                state.ui_mode = UI_MODE_NONE;
                state.ui_buttonset = UI_BUTTONSET_BUILD;
            } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
                input_t input;
                input.type = INPUT_BUILD_CANCEL;
                input.build_cancel.building_id = state.selection.ids[0];
                state.input_queue.push_back(input);

                // Clear the player's selection
                selection_t empty_selection;
                empty_selection.type = SELECTION_TYPE_NONE;
                match_ui_set_selection(state, empty_selection);
            }
            break;
        }
        case UI_BUTTON_BUILD_HOUSE:
        case UI_BUTTON_BUILD_CAMP: {
            // Begin building placement
            BuildingType building_type = (BuildingType)(BUILDING_HOUSE + (button - UI_BUTTON_BUILD_HOUSE));

            if (state.player_gold[network_get_player_id()] < BUILDING_DATA.at(building_type).cost) {
                match_ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
            } else {
                state.ui_mode = UI_MODE_BUILDING_PLACE;
                state.ui_building_type = building_type;
            }

            break;
        }
        case UI_BUTTON_NONE:
        default:
            break;
    }
}

bool match_is_mouse_in_ui() {
    xy mouse_pos = input_get_mouse_position();
    return (mouse_pos.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
           (mouse_pos.x <= 136 && mouse_pos.y >= SCREEN_HEIGHT - 136) ||
           (mouse_pos.x >= SCREEN_WIDTH - 132 && mouse_pos.y >= SCREEN_HEIGHT - 106);
}

xy match_ui_building_cell(const match_state_t& state) {
    return (input_get_mouse_position() + state.camera_offset) / TILE_SIZE;
}

selection_t match_ui_create_selection_from_rect(const match_state_t& state) {
    selection_t selection;
    selection.type = SELECTION_TYPE_NONE;

    // Check all the current player's units
    for (uint32_t index = 0; index < state.units.size(); index++) {
        const unit_t& unit = state.units[index];

        // Don't select other player's units
        if (unit.player_id != network_get_player_id()) {
            continue;
        }

        // Don't select units which are building
        if (match_unit_is_building(unit)) {
            continue;
        }

        if (match_unit_get_rect(unit).intersects(state.select_rect)) {
            selection.ids.push_back(state.units.get_id_of(index));
        }
    }
    // If we're selecting units, then return the unit selection
    if (!selection.ids.empty()) {
        selection.type = SELECTION_TYPE_UNITS;
        return selection;
    }

    // Otherwise, check the player's buildings
    for (uint32_t index = 0; index < state.buildings.size(); index++) {
        const building_t& building = state.buildings[index];

        // Don't select other player's buildings
        // TODO: remove this to allow enemy building selection
        if (building.player_id != network_get_player_id()) {
            continue;
        }

        if (match_building_get_rect(building).intersects(state.select_rect)) {
            // Return here so that only one building can be selected at once
            selection.ids.push_back(state.buildings.get_id_of(index));
            selection.type = SELECTION_TYPE_BUILDINGS;
            return selection;
        }
    }

    return selection;
}

void match_ui_set_selection(match_state_t& state, selection_t& selection) {
    state.selection = selection;

    if (selection.type == SELECTION_TYPE_NONE) {
        state.ui_buttonset = UI_BUTTONSET_NONE;
    } else if (selection.type == SELECTION_TYPE_UNITS) {
        if (selection.ids.size() == 1 && state.units[state.units.get_index_of(selection.ids[0])].type == UNIT_MINER) {
            state.ui_buttonset = UI_BUTTONSET_MINER;
        } else {
            state.ui_buttonset = UI_BUTTONSET_UNIT;
        }
    } else if (selection.type == SELECTION_TYPE_BUILDINGS) {
        building_t& building = state.buildings[state.buildings.get_index_of(selection.ids[0])];
        if (!building.is_finished) {
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
        } else {
            state.ui_buttonset = UI_BUTTONSET_NONE;
        }
    }
}

xy match_camera_clamp(xy camera_offset, int map_width, int map_height) {
    return xy(std::clamp(camera_offset.x, 0, (map_width * TILE_SIZE) - SCREEN_WIDTH),
                 std::clamp(camera_offset.y, 0, (map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT));
}

xy match_camera_centered_on_cell(xy cell) {
    return xy((cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2), (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2));
}

// Map

xy_fixed match_cell_center(xy cell) {
    return xy_fixed((cell * TILE_SIZE) + xy(TILE_SIZE / 2, TILE_SIZE / 2));
}

xy match_get_nearest_free_cell_within_rect(xy start_cell, rect_t rect) {
    xy nearest_cell = rect.position;
    uint32_t nearest_cell_dist = xy::manhattan_distance(start_cell, nearest_cell);

    for (int y = rect.position.y; y < rect.position.y + rect.size.y; y++) {
        for (int x = rect.position.x; x < rect.position.x + rect.size.x; x++) {
            uint32_t cell_dist = xy::manhattan_distance(start_cell, xy(x, y));
            if (cell_dist < nearest_cell_dist) {
                nearest_cell = xy(x, y);
                nearest_cell_dist = cell_dist;
            }
        }
    }

    return nearest_cell;
}

xy match_get_first_empty_cell_around_rect(const match_state_t& state, rect_t rect) {
    xy cell = rect.position + xy(-1, 0);
    int step_direction = DIRECTION_SOUTH;
    int step_count = 0;
    int x_step_amount = rect.size.x + 1;
    int y_step_amount = rect.size.y;

    while (match_map_is_cell_blocked(state, cell) || !match_map_is_cell_in_bounds(state, cell)) {
        cell += DIRECTION_XY[step_direction];
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

            // change directions counterclockwise
            step_direction = step_direction == DIRECTION_NORTH ? DIRECTION_WEST : step_direction - 2;
        }
    }

    return cell;
}

bool match_map_is_cell_in_bounds(const match_state_t& state, xy cell) {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= state.map_width || cell.y >= state.map_height);
}

bool match_map_is_cell_blocked(const match_state_t& state, xy cell) {
    return state.map_cells[cell.x + (cell.y * state.map_width)] != CELL_EMPTY;
}

bool match_map_is_cell_rect_blocked(const match_state_t& state, rect_t cell_rect) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            if (state.map_cells[x + (y * state.map_width)] != CELL_EMPTY) {
                return true;
            }
        }
    }
    
    return false;
}

uint32_t match_map_get_cell_value(const match_state_t& state, xy cell) {
    return state.map_cells[cell.x + (cell.y * state.map_height)];
}

uint32_t match_map_get_cell_type(const match_state_t& state, xy cell) {
    return state.map_cells[cell.x + (cell.y * (state.map_height))] & CELL_TYPE_MASK;
}

entity_id match_map_get_cell_id(const match_state_t& state, xy cell) {
    // explicit typecast to truncate the 32bit value and return only the 16 bits which represent the id
    return (entity_id)state.map_cells[cell.x + (cell.y * (state.map_height))];
}

void match_map_set_cell_value(match_state_t& state, xy cell, uint32_t type, uint32_t id) {
    state.map_cells[cell.x + (cell.y * state.map_height)] = type | id;
}

void match_map_set_cell_rect_value(match_state_t& state, rect_t cell_rect, uint32_t type, uint32_t id) {
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            state.map_cells[x + (y * state.map_height)] = type | id;
        }
    }
}

// Unit

void match_unit_create(match_state_t& state, uint8_t player_id, UnitType type, const xy& cell) {
    auto it = UNIT_DATA.find(type);
    GOLD_ASSERT(it != UNIT_DATA.end());

    unit_t unit;
    unit.type = type;
    unit.player_id = player_id;
    unit.animation = animation_create(ANIMATION_UNIT_IDLE);
    unit.health = it->second.max_health;

    unit.direction = DIRECTION_SOUTH;
    unit.cell = cell;
    unit.position = match_cell_center(unit.cell);
    unit.target = UNIT_TARGET_NONE;

    unit.building_id = ID_NULL;
    unit.building_type = BUILDING_NONE;

    unit.timer = 0;

    entity_id unit_id = state.units.push_back(unit);
    match_map_set_cell_value(state, unit.cell, CELL_UNIT, unit_id);
}

rect_t match_unit_get_rect(const unit_t& unit) {
    return rect_t(unit.position.to_xy() - xy(TILE_SIZE / 2, TILE_SIZE / 2), xy(TILE_SIZE, TILE_SIZE));
}

bool match_unit_is_moving(const unit_t& unit) {
    return unit.position != match_cell_center(unit.cell) || !unit.path.empty();
}

bool match_unit_is_building(const unit_t& unit) {
    return unit.building_id != ID_NULL;
}

void match_unit_update(match_state_t& state) {
    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        unit_t& unit = state.units[unit_index];
        entity_id unit_id = state.units.get_id_of(unit_index);
        const unit_data_t& unit_data = UNIT_DATA.at(unit.type);

        // BEHAVIOR TIMER
        if (unit.timer != 0) {
            unit.timer--;
            // On Behavior timer finished
            if (unit.timer == 0) {
                if (match_unit_is_moving(unit)) {
                    // Re-pathfind
                    match_unit_path_to_target(state, unit);
                } else if (match_unit_is_building(unit)) {
                    // Building tick
                    building_t& building = state.buildings[state.buildings.get_index_of(unit.building_id)];

                    building.health++;
                    if (building.health == BUILDING_DATA.at(building.type).max_health) {
                        // On building finished
                        building.is_finished = true;

                        // If selecting the building
                        if (state.selection.type == SELECTION_TYPE_BUILDINGS && state.selection.ids[0] == unit.building_id) {
                            // Trigger a re-select so that UI buttons are updated correctly
                            match_ui_set_selection(state, state.selection);
                        }

                        match_unit_stop_building(state, unit, building);
                    } else {
                        unit.timer = BUILD_TICK_DURATION;
                    }
                }
            }
        }

        // MOVEMENT
        if (match_unit_is_moving(unit) && unit.timer == 0) {
            fixed movement_left = unit_data.speed;
            while (movement_left.raw_value > 0) {
                // If the unit is not moving between tiles, then pop the next cell off the path
                if (unit.position == match_cell_center(unit.cell)) {
                    unit.direction = get_enum_direction_from_xy_direction(unit.path[0] - unit.cell);
                    if (match_map_is_cell_blocked(state, unit.path[0])) {
                        // Path is blocked, stop movement
                        unit.timer = PATH_PAUSE_DURATION;
                        break;
                    }

                    match_map_set_cell_value(state, unit.cell, CELL_EMPTY);
                    unit.cell = unit.path[0];
                    match_map_set_cell_value(state, unit.cell, CELL_UNIT, unit_id);
                    unit.path.erase(unit.path.begin());
                }

                // Step unit along movement
                if (unit.position.distance_to(match_cell_center(unit.cell)) > movement_left) {
                    unit.position += DIRECTION_XY_FIXED[unit.direction] * movement_left;
                    movement_left = fixed::from_raw(0);
                } else {
                    movement_left -= unit.position.distance_to(match_cell_center(unit.cell));
                    unit.position = match_cell_center(unit.cell);
                    if (unit.path.empty()) {
                        movement_left = fixed::from_raw(0);
                    }
                }
            } // End while movement_left > 0

            // On movement finished
            if (!match_unit_is_moving(unit)) {
                if (unit.target == UNIT_TARGET_BUILD) {
                    // Temporarily set cell to empty so that we can check if the space is clear for building
                    match_map_set_cell_value(state, unit.cell, CELL_EMPTY);
                    bool can_build = unit.cell == unit.target_cell && 
                                                  !match_map_is_cell_rect_blocked(state, rect_t(unit.building_cell, match_building_cell_size(unit.building_type)));
                    if (!can_build) {
                        // Set the cell back to filled
                        match_map_set_cell_value(state, unit.cell, CELL_EMPTY);
                        match_ui_show_status(state, UI_STATUS_CANT_BUILD);
                        unit.target = UNIT_TARGET_NONE;
                    } else {
                        state.player_gold[unit.player_id] -= BUILDING_DATA.at(unit.building_type).cost;
                        unit.building_id = match_building_create(state, unit.player_id, unit.building_type, unit.building_cell);
                        unit.timer = BUILD_TICK_DURATION;

                        // De-select the unit if it is selected
                        auto it = std::find(state.selection.ids.begin(), state.selection.ids.end(), unit_id);
                        if (it != state.selection.ids.end()) {
                            // De-select the unit
                            state.selection.ids.erase(it);

                            // If the selection is now empty, select the building instead
                            if (state.selection.ids.empty()) {
                                state.selection.type = SELECTION_TYPE_BUILDINGS;
                                state.selection.ids.push_back(unit.building_id);
                            }

                            match_ui_set_selection(state, state.selection);
                        }
                    }
                }
            }
        } // End if unit is moving

        // ANIMATION
        AnimationName expected_animation = match_unit_get_expected_animation(unit);
        if (unit.animation.name != expected_animation || !animation_is_playing(unit.animation)) {
            unit.animation = animation_create(expected_animation);
        }
        animation_update(unit.animation);
        unit.animation.frame.y = match_unit_get_animation_vframe(unit);
    } // End for each unit
}

void match_unit_path_to_target(const match_state_t& state, unit_t& unit) {
    xy target_cell;

    switch (unit.target) {
        case UNIT_TARGET_NONE:
            target_cell = unit.cell;
            break;
        case UNIT_TARGET_BUILD:
        case UNIT_TARGET_CELL:
        case UNIT_TARGET_GOLD:
            // TODO: if unit target gold, but the gold does not exist anymore, find nearest gold
            target_cell = unit.target_cell;
            break;
        case UNIT_TARGET_UNIT: {
            uint32_t unit_index = state.units.get_index_of(unit.target_entity_id);
            target_cell = unit_index == INDEX_INVALID ? unit.cell : state.units[unit_index].cell;
            break;
        }
        case UNIT_TARGET_BUILDING: {
            // TODO: if unit targets building, path to the nearest cell around the building
            // possibly lump the pathing up into one function so that initial pathing and repathing can work off the same code?
            // as well as unit finished but target invalid pathing?
            // match_unit_path_to_target() ?
            // also we can make the pathing more performant by temporarily unblocking the destination whenever it is blocked and thus allowing the unit to path while skipping the worst case
            uint32_t building_index = state.buildings.get_index_of(unit.target_entity_id);
            target_cell = building_index == INDEX_INVALID ? unit.cell : state.buildings[building_index].cell;
            break;
        }
    }

    // Don't bother pathing to unit's cell
    if (target_cell == unit.cell) {
        unit.path.clear();
        return;
    }

    struct node_t {
        uint32_t cost;
        uint32_t distance;
        // The parent is the previous node stepped in the path to reach this node
        // It should be an index in the explored list, or -1 if it is the start node
        int parent; 
        xy cell;

        uint32_t score() const {
            return cost + distance;
        }
    };

    std::vector<node_t> frontier;
    std::vector<node_t> explored;
    int explored_indices[state.map_width * state.map_height];
    memset(explored_indices, -1, state.map_width * state.map_height * sizeof(int));
    uint32_t closest_explored = 0;
    bool found_path = false;
    node_t path_end;

    frontier.push_back((node_t) {
        .cost = 0,
        .distance = xy::manhattan_distance(unit.cell, target_cell),
        .parent = -1,
        .cell = unit.cell
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
        if (smallest.cell == target_cell) {
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
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            node_t child = (node_t) {
                .cost = smallest.cost + 1,
                .distance = xy::manhattan_distance(smallest.cell + DIRECTION_XY[direction], target_cell),
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_XY[direction]
            };
            // Don't consider out of bounds children
            if (!match_map_is_cell_in_bounds(state, child.cell)) {
                continue;
            }
            // Don't consider blocked spaces
            // ...except for the target cell. If the target is blocked we pretend that it isn't. 
            // This allows us to fully path to the target and ignore worst-case pathfinding
            if (match_map_is_cell_blocked(state, child.cell) && child.cell != target_cell) {
                continue;
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
    unit.path.clear();
    unit.path.reserve(current.cost);
    while (current.parent != -1) {
        unit.path.insert(unit.path.begin(), current.cell);
        current = explored[current.parent];
    }
    // Since earlier in the algorithm we treated the target_cell as if it was not blocked,
    // now we have to consider whether or not it really is free, and if it's not, remove it
    // from the path
    if (match_map_is_cell_blocked(state, target_cell)) {
        unit.path.pop_back();
    }
}

AnimationName match_unit_get_expected_animation(const unit_t& unit) {
    if (match_unit_is_moving(unit) && unit.timer == 0) {
        return ANIMATION_UNIT_MOVE;
    } else if (match_unit_is_building(unit)) {
        return ANIMATION_UNIT_BUILD;
    }

    return ANIMATION_UNIT_IDLE;
}

int match_unit_get_animation_vframe(const unit_t& unit) {
    if (unit.animation.name == ANIMATION_UNIT_BUILD) {
        return 0;
    }
    if (unit.direction == DIRECTION_NORTH) {
        return 1;
    } else if (unit.direction == DIRECTION_SOUTH) {
        return 0;
    } else if (unit.direction > DIRECTION_SOUTH) {
        return 3;
    } else {
        return 2;
    }
}

void match_unit_stop_building(match_state_t& state, unit_t& unit, const building_t& building) {
    unit.cell = match_get_first_empty_cell_around_rect(state, rect_t(building.cell, match_building_cell_size(building.type)));
    unit.position = match_cell_center(unit.cell);
    unit.target = UNIT_TARGET_NONE;
    unit.building_id = ID_NULL;
}

// Building

entity_id match_building_create(match_state_t& state, uint8_t player_id, BuildingType type, xy cell) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;

    building_t building;
    building.player_id = player_id;
    building.type = type;
    building.health = 3;

    building.cell = cell;

    building.is_finished = false;

    entity_id building_id = state.buildings.push_back(building);
    match_map_set_cell_rect_value(state, rect_t(cell, xy(building_data.cell_width, building_data.cell_height)), CELL_BUILDING, building_id);
    return building_id;
}

void match_building_destroy(match_state_t& state, entity_id building_id) {
    uint32_t building_index = state.buildings.get_index_of(building_id);
    building_t& building = state.buildings[building_index];
    const building_data_t& building_data = BUILDING_DATA.find(building.type)->second;

    match_map_set_cell_rect_value(state, rect_t(building.cell, xy(building_data.cell_width, building_data.cell_height)), CELL_EMPTY);
    state.buildings.remove_at(building_index);
}

xy match_building_cell_size(BuildingType type) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;
    return xy(building_data.cell_width, building_data.cell_height);
}

rect_t match_building_get_rect(const building_t& building) {
    return rect_t(building.cell * TILE_SIZE, match_building_cell_size(building.type) * TILE_SIZE);
}

bool match_building_can_be_placed(const match_state_t& state, BuildingType type, xy cell) {
    rect_t building_rect = rect_t(cell, match_building_cell_size(type));
    return match_map_is_cell_in_bounds(state, building_rect.position + building_rect.size) && !match_map_is_cell_rect_blocked(state, building_rect);
}
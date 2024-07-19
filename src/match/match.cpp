#include "match.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include "input.h"
#include "network.h"
#include <cstdlib>
#include <algorithm>

static const uint32_t TICK_DURATION = 4;

static const uint32_t UI_STATUS_DURATION = 60;
const std::unordered_map<UiButtonset, std::array<Button, 6>> UI_BUTTONS = {
    { UI_BUTTONSET_NONE, { BUTTON_NONE, BUTTON_NONE, BUTTON_NONE,
                      BUTTON_NONE, BUTTON_NONE, BUTTON_NONE }},
    { UI_BUTTONSET_UNIT, { BUTTON_MOVE, BUTTON_STOP, BUTTON_ATTACK,
                      BUTTON_NONE, BUTTON_NONE, BUTTON_NONE }},
    { UI_BUTTONSET_MINER, { BUTTON_MOVE, BUTTON_STOP, BUTTON_ATTACK,
                      BUTTON_BUILD, BUTTON_NONE, BUTTON_NONE }},
    { UI_BUTTONSET_BUILD, { BUTTON_BUILD_HOUSE, BUTTON_BUILD_CAMP, BUTTON_NONE,
                      BUTTON_BUILD, BUTTON_NONE, BUTTON_CANCEL }},
    { UI_BUTTONSET_CANCEL, { BUTTON_NONE, BUTTON_NONE, BUTTON_NONE,
                      BUTTON_NONE, BUTTON_NONE, BUTTON_CANCEL }}
};
static const ivec2 UI_BUTTON_SIZE = ivec2(32, 32);
static const ivec2 UI_BUTTON_PADDING = ivec2(4, 6);
static const ivec2 UI_BUTTON_TOP_LEFT = ivec2(SCREEN_WIDTH - 132 + 14, SCREEN_HEIGHT - UI_HEIGHT + 10);
static const ivec2 UI_BUTTON_POSITIONS[6] = { UI_BUTTON_TOP_LEFT, UI_BUTTON_TOP_LEFT + ivec2(UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x, 0), UI_BUTTON_TOP_LEFT + ivec2(2 * (UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x), 0),
                                              UI_BUTTON_TOP_LEFT + ivec2(0, UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y), UI_BUTTON_TOP_LEFT + ivec2(UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x, UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y), UI_BUTTON_TOP_LEFT + ivec2(2 * (UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x), UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y) };
const rect_t UI_BUTTON_RECT[6] = {
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

const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA = {
    { UNIT_MINER, (unit_data_t) {
        .cost = 50,
        .max_health = 20
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

    // Init UI
    state.ui_mode = UI_MODE_NONE;
    state.ui_buttonset = UI_BUTTONSET_NONE;
    state.camera_offset = ivec2(0, 0);
    state.selection.type = SELECTION_TYPE_NONE;
    state.ui_button_hovered = -1;
    state.ui_status_timer = 0;

    // Init map
    state.map = map_init(ivec2(64, 64));

    // Spawn players and Init units
    ivec2 player_spawns[MAX_PLAYERS];
    uint8_t current_player_id = network_get_player_id();
    bool is_spawn_direction_used[DIRECTION_COUNT];
    memset(is_spawn_direction_used, 0, sizeof(is_spawn_direction_used));
    uint32_t player_count = 0;
    ivec2 map_center = ivec2(state.map.width / 2, state.map.height / 2);
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }
        player_count++;

        int spawn_direction = rand() % DIRECTION_COUNT;
        while (is_spawn_direction_used[spawn_direction]) {
            spawn_direction = rand() % DIRECTION_COUNT;
        }
        is_spawn_direction_used[spawn_direction] = true;

        player_spawns[player_id] = map_center + ivec2(DIRECTION_IVEC2[spawn_direction].x * ((state.map.width * 6) / 16), DIRECTION_IVEC2[spawn_direction].y * ((state.map.height * 6) / 16));

        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + ivec2(-1, -1));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + ivec2(1, -1));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + ivec2(0, 1));

        log_info("player %u spawn %vi", player_id, &player_spawns[player_id]);
    }
    state.camera_offset = camera_clamp(camera_centered_on_cell(player_spawns[current_player_id]), state.map.width, state.map.height);
    log_info("players initialized");

    // Init resources
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state.player_gold[player_id] = 150;
    }

    state.mode = MATCH_MODE_NOT_STARTED;
    if (!network_is_server()) {
        network_client_toggle_ready();
    }
    if (network_is_server() && player_count == 1) {
        state.mode = MATCH_MODE_RUNNING;
    }

    return state;
}

void match_update(match_state_t& state) {
    // NETWORK EVENTS
    network_service();

    // Wait for match to start
    if (state.mode == MATCH_MODE_NOT_STARTED) {
        network_event_t network_event;
        while (network_poll_events(&network_event)) {
            if (network_is_server() && (network_event.type == NETWORK_EVENT_CLIENT_READY || network_event.type == NETWORK_EVENT_CLIENT_DISCONNECTED)) {
                if (network_are_all_players_ready()) {
                    network_server_start_match();
                    state.mode = MATCH_MODE_RUNNING;
                }
            } else if (!network_is_server() && network_event.type == NETWORK_EVENT_MATCH_START) {
                state.mode = MATCH_MODE_RUNNING;
            }
        }
        
        // We make the check again here in case the mode changed
        if (state.mode == MATCH_MODE_NOT_STARTED) {
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
                    tick_inputs.push_back(input_deserialize(in_buffer, in_buffer_head));
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
                switch (input.type) {
                    case INPUT_MOVE: {
                        // Calculate unit group info
                        bool should_use_offset_of_target = !map_cell_is_blocked(state.map, input.move.target_cell);
                        ivec2 group_center;

                        if (should_use_offset_of_target) {
                            log_info("calculating group center");
                            bool has_found_first_unit = false;
                            ivec2 group_min;
                            ivec2 group_max;
                            uint32_t unit_count = 0;
                            for (uint32_t i = 0; i < input.move.unit_count; i++) {
                                uint32_t unit_index = state.units.get_index_of(input.move.unit_ids[i]);
                                if (unit_index == id_array<unit_t>::INDEX_INVALID) {
                                    continue;
                                }

                                unit_t& unit = state.units[unit_index];
                                if (!has_found_first_unit) {
                                    group_center = unit.cell;
                                    group_min = unit.cell;
                                    group_max = unit.cell;
                                    has_found_first_unit = true;
                                } else {
                                    group_center += unit.cell;
                                    group_min.x = std::min(group_min.x, unit.cell.x);
                                    group_min.y = std::min(group_min.y, unit.cell.y);
                                    group_max.x = std::max(group_max.x, unit.cell.x);
                                    group_max.y = std::max(group_max.y, unit.cell.y);
                                }

                                unit_count++;
                            }
                            group_max.x++;
                            group_max.y++;
                            group_center = group_center / unit_count;
                            bool is_target_inside_group = rect_t(group_min, group_max - group_min).has_point(input.move.target_cell);
                            if (is_target_inside_group) {
                                should_use_offset_of_target = false;
                            }
                        }

                        for (uint32_t i = 0; i < input.move.unit_count; i++) {
                            uint32_t unit_index = state.units.get_index_of(input.move.unit_ids[i]);
                            if (unit_index == id_array<unit_t>::INDEX_INVALID) {
                                continue;
                            }
                            unit_t& unit = state.units[unit_index];

                            // Determine the unit's target
                            ivec2 unit_target = input.move.target_cell;
                            if (should_use_offset_of_target) {
                                ivec2 offset = unit.cell - group_center;
                                unit_target = input.move.target_cell + offset;
                                bool is_target_invalid = unit_target.x < 0 || unit_target.x >= state.map.width || unit_target.y < 0 || unit_target.y >= state.map.height ||
                                                        ivec2::manhattan_distance(unit_target, input.move.target_cell) > 3;
                                log_info("target invalid? %i", (int)is_target_invalid);
                                if (is_target_invalid) {
                                    unit_target = input.move.target_cell;
                                }
                            }

                            // Don't bother moving to the same cell
                            if (unit_target == unit.cell) {
                                continue;
                            }
                            // Don't bother moving to a blocked cell
                            if (map_cell_is_blocked(state.map, unit_target)) {
                                unit_target = map_get_nearest_free_cell_around_cell(state.map, unit.cell, input.move.target_cell);
                            }

                            unit.order = ORDER_MOVE;
                            unit.target_cell = unit_target;
                            map_cell_set_temp_value(state.map, unit.target_cell, CELL_FILLED);
                            map_cell_set_temp_value(state.map, unit.cell, CELL_EMPTY);
                        }
                        map_clear_temp_fills(state.map);
                        break;
                    } // End case INPUT_MOVE
                    case INPUT_STOP: {
                        for (uint32_t i = 0; i < input.stop.unit_count; i++) {
                            uint32_t unit_index = state.units.get_index_of(input.stop.unit_ids[i]);
                            if (unit_index == id_array<unit_t>::INDEX_INVALID) {
                                continue;
                            }
                            unit_t& unit = state.units[unit_index];

                            unit.order = ORDER_NONE;
                            unit.target_cell = unit.cell;
                            unit.path.clear();
                        }
                        break;
                    }
                    case INPUT_BUILD: {
                        uint32_t unit_index = state.units.get_index_of(input.build.unit_id);
                        if (unit_index == id_array<unit_t>::INDEX_INVALID) {
                            break;
                        }
                        unit_t& unit = state.units[unit_index];

                        // Determine the unit's target
                        ivec2 nearest_cell = input.build.target_cell;
                        int nearest_cell_dist = ivec2::manhattan_distance(unit.cell, nearest_cell);
                        ivec2 building_cell_size = BUILDING_DATA.at(input.build.building_type).cell_size();
                        for (int y = input.build.target_cell.y; y < input.build.target_cell.y + building_cell_size.y; y++) {
                            for (int x = input.build.target_cell.x; x < input.build.target_cell.x + building_cell_size.x; x++) {
                                if (x == 0 && y == 0) {
                                    continue;
                                }

                                int cell_dist = ivec2::manhattan_distance(unit.cell, ivec2(x, y));
                                if (cell_dist < nearest_cell_dist) {
                                    nearest_cell = ivec2(x, y);
                                    nearest_cell_dist = cell_dist;
                                }
                            }
                        }

                        unit.building_type = (BuildingType)input.build.building_type;
                        unit.building_cell = input.build.target_cell;
                        unit.target_cell = nearest_cell;
                        unit.order = ORDER_BUILD;

                        break;
                    }
                    case INPUT_BUILD_CANCEL: {
                        uint32_t index = state.buildings.get_index_of(input.build_cancel.building_id);
                        if (index == id_array<building_t>::INDEX_INVALID) {
                            continue;
                        }
                        building_t& building = state.buildings[index];

                        // Refund the player
                        state.player_gold[player_id] += BUILDING_DATA.at(building.type).cost;
                        // Tell the unit to stop building this building
                        for (unit_t& unit : state.units) {
                            if (unit.mode == UNIT_MODE_BUILD && unit.building_id == input.build_cancel.building_id) {
                                unit_eject_from_building(unit, state.map);
                            }
                        }
                        // Destroy the building
                        building_destroy(state, input.build_cancel.building_id);
                        break;
                    }
                    default:
                        break;
                } // End switch input.type
            } // End for each input in player queue
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        } // End for each player
        // End handle input

        state.tick_timer = TICK_DURATION;

        // Flush input
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
            input_serialize(out_buffer, out_buffer_length, input);
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

    ivec2 mouse_pos = input_get_mouse_position();
    uint8_t current_player_id = network_get_player_id();
    ivec2 mouse_world_pos = mouse_pos + state.camera_offset;
    bool is_mouse_in_ui = (mouse_pos.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
                            (mouse_pos.x <= 136 && mouse_pos.y >= SCREEN_HEIGHT - 136) ||
                            (mouse_pos.x >= SCREEN_WIDTH - 132 && mouse_pos.y >= SCREEN_HEIGHT - 106);
    Button ui_button_pressed = BUTTON_NONE;

    // UI Buttons
    if (!input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT)) {
        state.ui_button_hovered = -1;
        for (int i = 0; i < 6; i++) {
            if (UI_BUTTONS.at(state.ui_buttonset)[i] == BUTTON_NONE) {
                continue;
            }

            if (UI_BUTTON_RECT[i].has_point(mouse_pos)) {
                state.ui_button_hovered = i;
                break;
            }
        }
    }

    // Building placement
    if (state.ui_mode == UI_MODE_BUILDING_PLACE) {
        if (is_mouse_in_ui) {
            state.ui_building_cell = ivec2(-1, -1);
        } else {
            state.ui_building_cell = mouse_world_pos / TILE_SIZE;
        }
    }

    // LEFT MOUSE CLICK
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        if (state.ui_button_hovered != -1) {
            // On UI button pressed
            ui_button_pressed = UI_BUTTONS.at(state.ui_buttonset)[state.ui_button_hovered];
        } else if (state.ui_mode == UI_MODE_BUILDING_PLACE && !is_mouse_in_ui) {
            // On building place
            ivec2 building_cell_size = BUILDING_DATA.at(state.ui_building_type).cell_size();
            bool building_can_be_placed = state.ui_building_cell.x + building_cell_size.x < state.map.width + 1 && 
                                            state.ui_building_cell.y + building_cell_size.y < state.map.height + 1 &&
                                            !map_cells_are_blocked(state.map, state.ui_building_cell, building_cell_size);
            if (building_can_be_placed) {
                input_t input;
                input.type = INPUT_BUILD;
                input.build.building_type = state.ui_building_type;
                input.build.target_cell = state.ui_building_cell;
                input.build.unit_id = state.selection.ids[0];
                state.input_queue.push_back(input);

                state.ui_buttonset = UI_BUTTONSET_MINER;
                state.ui_mode = UI_MODE_NONE;
            } else {
                ui_show_status(state, "You can't build there.");
            }
        } else if (state.ui_mode == UI_MODE_NONE && MINIMAP_RECT.has_point(mouse_pos)) {
            // On begin minimap drag
            state.ui_mode = UI_MODE_MINIMAP_DRAG;
        } else if (state.ui_mode == UI_MODE_NONE && !is_mouse_in_ui) {
            // On begin selecting
            state.ui_mode = UI_MODE_SELECTING;
            state.select_origin = mouse_world_pos;
        } 
    } 

    // UI BUTTON PRESSED
    switch (ui_button_pressed) {
        case BUTTON_STOP: {
            if (state.selection.type == SELECTION_TYPE_UNITS) {
                input_t input;
                input.type = INPUT_STOP;
                memcpy(input.stop.unit_ids, &state.selection.ids[0], state.selection.ids.size() * sizeof(uint16_t));
                input.stop.unit_count = state.selection.ids.size();
                state.input_queue.push_back(input);
            }
            break;
        }
        case BUTTON_BUILD: {
            state.ui_buttonset = UI_BUTTONSET_BUILD;
            break;
        }
        case BUTTON_CANCEL: {
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
                ui_set_selection(state, empty_selection);
            }
            break;
        }
        case BUTTON_BUILD_HOUSE:
        case BUTTON_BUILD_CAMP: {
            // Begin building placement
            BuildingType building_type = (BuildingType)(ui_button_pressed - BUTTON_BUILD_HOUSE);
            uint8_t player_id = network_get_player_id();

            if (state.player_gold[player_id] < BUILDING_DATA.at(building_type).cost) {
                ui_show_status(state, "Not enough gold.");
            } else {
                state.ui_mode = UI_MODE_BUILDING_PLACE;
                state.ui_building_type = building_type;
                state.ui_building_cell = ivec2(-1, -1);
            }

            break;
        }
        case BUTTON_NONE:
        default:
            break;
    }

    // SELECT RECT
    if (state.ui_mode == UI_MODE_SELECTING) {
        // Update select rect
        state.select_rect.position = ivec2(std::min(state.select_origin.x, mouse_world_pos.x), std::min(state.select_origin.y, mouse_world_pos.y));
        state.select_rect.size = ivec2(std::max(1, std::abs(state.select_origin.x - mouse_world_pos.x)), std::max(1, std::abs(state.select_origin.y - mouse_world_pos.y)));
    } else if (state.ui_mode == UI_MODE_MINIMAP_DRAG) {
        ivec2 minimap_pos = mouse_pos - MINIMAP_RECT.position;
        minimap_pos.x = std::clamp(minimap_pos.x, 0, MINIMAP_RECT.size.x);
        minimap_pos.y = std::clamp(minimap_pos.y, 0, MINIMAP_RECT.size.y);
        ivec2 map_pos = ivec2((state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        state.camera_offset = camera_clamp(camera_centered_on_cell(map_pos / TILE_SIZE), state.map.width, state.map.height);
    }

    // LEFT MOUSE RELEASE
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        if (state.ui_mode == UI_MODE_SELECTING) {
            selection_t selection = selection_create(state.units, state.buildings, current_player_id, state.select_rect);
            ui_set_selection(state, selection);
            state.ui_mode = UI_MODE_NONE;
        } else if (state.ui_mode == UI_MODE_MINIMAP_DRAG) {
            state.ui_mode = UI_MODE_NONE;
        }
    }

    // CAMERA DRAG
    if (state.ui_mode != UI_MODE_SELECTING && state.ui_mode != UI_MODE_MINIMAP_DRAG) {
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
        state.camera_offset = camera_clamp(state.camera_offset, state.map.width, state.map.height);
    }

    // Right-click move
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT) && state.ui_mode == UI_MODE_NONE && state.selection.type == SELECTION_TYPE_UNITS) {
        bool should_command_move = !is_mouse_in_ui || MINIMAP_RECT.has_point(mouse_pos);

        if (should_command_move) {
            // Determine move target
            ivec2 move_target;
            if (is_mouse_in_ui) {
                ivec2 minimap_pos = mouse_pos - MINIMAP_RECT.position;
                move_target = ivec2((state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
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

            // Send the move event
            if (input.move.unit_count != 0) {
                state.input_queue.push_back(input);

                // Play the move animation to provide instant user feedback
                state.ui_move_position = move_target;
                state.ui_move_animation = animation_start(ANIMATION_UI_MOVE);
            }
        } 
    }

    // Unit update
    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        unit_t& unit = state.units[unit_index];
        uint16_t unit_id = state.units.ids[unit_index];

        // Path timer
        if (unit.mode == UNIT_MODE_MOVE_BLOCKED) {
            // Increment path timer
            unit.path_timer--;
            if (unit.path_timer == 0) {
                unit.mode = UNIT_MODE_IDLE;
                unit.path.clear();
            }
        }

        // Pathfind
        bool unit_needs_to_pathfind = unit.cell != unit.target_cell && (unit.path.empty() || unit.path[unit.path.size() - 1] != unit.target_cell);
        if (unit_needs_to_pathfind) {
            if (map_cell_is_blocked(state.map, unit.target_cell)) {
                unit.target_cell = map_get_nearest_free_cell_around_cell(state.map, unit.cell, unit.target_cell);
            }
            unit.path = map_pathfind(state.map, unit.cell, unit.target_cell);
            if (unit.path.empty()) {
                unit.order = ORDER_NONE;
                unit.target_cell = unit.cell;
            } else {
                unit.mode = UNIT_MODE_MOVE;
            }
        }

        if (unit.mode == UNIT_MODE_MOVE) {
            // Movement
            fixed movement_left = fixed::from_int(1);
            while (movement_left.raw_value > 0) {
                // Try to start moving to the next tile
                if (unit.position == unit.target_position) {
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
                    if (map_cell_is_blocked(state.map, next_point)) {
                        if (unit.path_timer == 0) {
                            unit.path_timer = PATH_PAUSE_DURATION;
                            unit.mode = UNIT_MODE_MOVE_BLOCKED;
                        }
                        break;
                    }

                    // Set state to begin movement step
                    map_cell_set_value(state.map, unit.cell, CELL_EMPTY);
                    unit.cell = next_point;
                    map_cell_set_value(state.map, unit.cell, CELL_FILLED);
                    unit.target_position = cell_center_position(unit.cell);
                    unit.path_timer = 0;
                    unit.mode = UNIT_MODE_MOVE;
                    unit.path.erase(unit.path.begin());
                }

                // Step unit along movement
                fixed distance_to_target = unit.position.distance_to(unit.target_position);
                if (distance_to_target > movement_left) {
                    unit.position += DIRECTION_VEC2[unit.direction] * movement_left;
                    movement_left = fixed::from_raw(0);
                } else {
                    unit.position = unit.target_position;
                    movement_left -= distance_to_target;
                    if (unit.path.empty()) {
                        movement_left = fixed::from_raw(0);
                    }
                }
                // On finished movement
                if (unit.path.empty() && unit.position == unit.target_position) {
                    if (unit.order == ORDER_MOVE && unit.cell == unit.target_cell) {
                        log_info("reached target");
                        unit.order = ORDER_NONE;
                        unit.mode = UNIT_MODE_IDLE;
                    } else if (unit.order == ORDER_BUILD && unit.cell == unit.target_cell) {
                        // Since the unit is on top of the building space, we have to temporarily set the cell to empty so that we can check if the building space is free
                        map_cell_set_value(state.map, unit.cell, CELL_EMPTY);
                        if (map_cells_are_blocked(state.map, unit.building_cell, BUILDING_DATA.at(unit.building_type).cell_size())) {
                            unit.order = ORDER_NONE;
                            unit.mode = UNIT_MODE_IDLE;
                            map_cell_set_value(state.map, unit.cell, CELL_FILLED);
                            ui_show_status(state, "You can't build there.");
                        } else {
                            unit.building_id = building_create(state, unit.player_id, unit.building_type, unit.building_cell);
                            state.player_gold[unit.player_id] -= BUILDING_DATA.at(unit.building_type).cost;
                            unit.mode = UNIT_MODE_BUILD;
                            unit.build_timer = BUILD_TICK_DURATION;

                            // De-select the unit if necessary
                            selection_t selection = state.selection;
                            if (selection.type == SELECTION_TYPE_UNITS) {
                                for (auto unit_id_it = selection.ids.begin(); unit_id_it != selection.ids.end(); unit_id_it++) {
                                    if (*unit_id_it == unit_id) {
                                        // De-select the unit
                                        selection.ids.erase(unit_id_it);

                                        // If the selection is now empty, select the building instead
                                        if (selection.ids.empty()) {
                                            selection.type = SELECTION_TYPE_BUILDINGS;
                                            selection.ids.push_back(unit.building_id);
                                        }

                                        ui_set_selection(state, selection);
                                        break;
                                    }
                                }
                            }
                        }
                    } else {
                        log_info("something weird happened");
                        unit.order = ORDER_NONE;
                        unit.target_cell = unit.cell;
                        unit.mode = UNIT_MODE_IDLE;
                    }
                } // End on finished movement
            } // End while movement left != 0
        } // End if mode == MOVE

        if (unit.mode == UNIT_MODE_BUILD) {
            unit.build_timer--;
            if (unit.build_timer == 0) {
                uint16_t building_id = unit.building_id;
                uint32_t index = state.buildings.get_index_of(building_id);
                building_t& building = state.buildings[index];

                building.health++;
                if (building.health == BUILDING_DATA.at(unit.building_type).max_health) {
                    // On building finished
                    unit_eject_from_building(unit, state.map);
                    building.is_finished = true;

                    if (state.selection.type == SELECTION_TYPE_BUILDINGS && state.selection.ids[0] == building_id) {
                        // Trigger a re-select so that ui buttons are updated correctly
                        ui_set_selection(state, state.selection);
                    }
                } else {
                    unit.build_timer = BUILD_TICK_DURATION;
                }
            }
        }

        // Update animation
        Animation should_be_playing;
        if (unit.mode == UNIT_MODE_BUILD) {
            should_be_playing = ANIMATION_UNIT_BUILD;
        } else if (unit.mode == UNIT_MODE_MOVE) {
            should_be_playing = ANIMATION_UNIT_MOVE;
        } else {
            should_be_playing = ANIMATION_UNIT_IDLE;
        }
        if (unit.animation.animation != should_be_playing) {
            unit.animation = animation_start(should_be_playing);
        }
        animation_update(unit.animation);

        // Set sprite direction based on unit direction
        if (unit.animation.animation != ANIMATION_UNIT_BUILD) {
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
    } // End for each unit

    // UI update
    if (state.ui_move_animation.is_playing) {
        animation_update(state.ui_move_animation);
    }

    // Update UI status message timer
    if (state.ui_status_timer != 0) {
        state.ui_status_timer--;
    }
}

// UI

void ui_show_status(match_state_t& state, const char* message) {
    state.ui_status_message = std::string(message);
    state.ui_status_timer = UI_STATUS_DURATION;
}

selection_t selection_create(const id_array<unit_t>& units, const id_array<building_t>& buildings, uint8_t current_player_id, const rect_t& select_rect) {
    selection_t selection;
    selection.type = SELECTION_TYPE_NONE;

    // Check all the current player's units
    for (uint32_t index = 0; index < units.size(); index++) {
        const unit_t& unit = units[index];

        // Don't select other player's units
        if (unit.player_id != current_player_id) {
            continue;
        }
        // Don't select units which are building
        if (unit.mode == UNIT_MODE_BUILD) {
            continue;
        }

        if (unit_rect(unit).intersects(select_rect)) {
            selection.ids.push_back(units.get_id_of(index));
        }
    }
    // If we're selecting units, then return the unit selection
    if (!selection.ids.empty()) {
        selection.type = SELECTION_TYPE_UNITS;
        return selection;
    }

    // Otherwise, check the player's buildings
    for (uint32_t index = 0; index < buildings.size(); index++) {
        const building_t& building = buildings[index];

        // Don't select other player's buildings
        // TODO: remove this to allow enemy building selection
        if (building.player_id != current_player_id) {
            continue;
        }

        if (building_rect(building).intersects(select_rect)) {
            // Return here so that only one building can be selected at once
            selection.ids.push_back(buildings.get_id_of(index));
            selection.type = SELECTION_TYPE_BUILDINGS;
            return selection;
        }
    }

    return selection;
}

void ui_set_selection(match_state_t& state, selection_t& selection) {
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

// CAMERA

ivec2 camera_clamp(const ivec2& camera_offset, int map_width, int map_height) {
    return ivec2(std::clamp(camera_offset.x, 0, (map_width * TILE_SIZE) - SCREEN_WIDTH),
                 std::clamp(camera_offset.y, 0, (map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT));
}

ivec2 camera_centered_on_cell(const ivec2& cell) {
    return ivec2((cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2), (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2));
}

// UNITS

void unit_create(match_state_t& state, uint8_t player_id, UnitType type, const ivec2& cell) {
    auto it = UNIT_DATA.find(type);
    GOLD_ASSERT(it != UNIT_DATA.end());

    unit_t unit;
    unit.type = type;
    unit.player_id = player_id;
    unit.health = it->second.max_health;
    unit.cell = cell;
    unit.target_cell = cell;
    unit.position = cell_center_position(cell);
    unit.target_position = unit.position;
    unit.direction = DIRECTION_SOUTH;
    unit.mode = UNIT_MODE_IDLE;
    unit.order = ORDER_NONE;
    unit.animation = animation_start(ANIMATION_UNIT_IDLE);
    unit.path_timer = 0;
    unit.build_timer = 0;
    state.units.push_back(unit);

    map_cell_set_value(state.map, cell, CELL_FILLED);
}

rect_t unit_rect(const unit_t& unit) {
    return rect_t(ivec2(unit.position.x.integer_part() - 8, unit.position.y.integer_part() - 8), ivec2(16, 16));
}

void unit_eject_from_building(unit_t& unit, map_t& map) {
    unit.cell = map_get_first_free_cell_around_cells(map, unit.building_cell, BUILDING_DATA.at(unit.building_type).cell_size());
    unit.target_cell = unit.cell;
    map_cell_set_value(map, unit.cell, CELL_FILLED);
    unit.position = cell_center_position(unit.cell);
    unit.target_position = unit.position;
    unit.order = ORDER_NONE;
    unit.mode = UNIT_MODE_IDLE;
}

// BUILDINGS

uint8_t building_create(match_state_t& state, uint8_t player_id, BuildingType type, ivec2 cell) {
    auto it = BUILDING_DATA.find(type);
    GOLD_ASSERT(it != BUILDING_DATA.end());

    building_t building;
    building.player_id = player_id;
    building.cell = cell;
    building.type = type;
    building.health = 3;
    building.is_finished = false;

    map_cells_set_value(state.map, cell, it->second.cell_size(), CELL_FILLED);
    return state.buildings.push_back(building);
}

void building_destroy(match_state_t& state, uint8_t building_id) {
    uint32_t index = state.buildings.get_index_of(building_id);
    building_t& building = state.buildings[index];

    map_cells_set_value(state.map, building.cell, BUILDING_DATA.at(building.type).cell_size(), false);
    state.buildings.remove_at(index);
}

rect_t building_rect(const building_t& building) {
    return rect_t(building.cell * TILE_SIZE, BUILDING_DATA.at(building.type).cell_size() * TILE_SIZE);
}
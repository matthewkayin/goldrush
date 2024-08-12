#include "match.h"

#include "asserts.h"
#include "network.h"
#include "logger.h"
#include "input.h"
#include "lcg.h"
#include <cstring>
#include <unordered_map>
#include <array>
#include <algorithm>

static const uint32_t TICK_DURATION = 4;
static const uint8_t TICK_OFFSET = 4;
static const uint32_t INPUT_MAX_SIZE = 256;

static const int CAMERA_DRAG_MARGIN = 8;
static const int CAMERA_DRAG_SPEED = 8;

static const uint32_t CONTROL_GROUP_DOUBLE_CLICK_DURATION = 16;

// static const uint32_t PLAYER_STARTING_GOLD = 275;
static const uint32_t PLAYER_STARTING_GOLD = 1000;
static const uint32_t MAP_GOLD_CELL_AMOUNT = 300;

match_state_t match_init() {
    match_state_t state;

    // Init UI
    state.ui_mode = UI_MODE_NOT_STARTED;
    state.ui_buttonset = UI_BUTTONSET_NONE;
    state.camera_offset = xy(0, 0);
    state.ui_status_timer = 0;
    for (uint32_t i = 0; i < 9; i++) {
        state.control_groups[i] = (selection_t) {
            .type = SELECTION_TYPE_NONE
        };
    }
    state.control_group_double_click_timer = 0;

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
        for (uint8_t i = 0; i < TICK_OFFSET - 1; i++) {
            state.inputs[player_id].push_back(empty_input_list);
        }
    }
    state.tick_timer = 0;

    // Init map
    state.map_width = 64;
    state.map_height = 64;
    state.map_tiles = std::vector<uint32_t>(state.map_width * state.map_height);
    state.map_cells = std::vector<cell_t>(state.map_width * state.map_height, (cell_t) {
        .type = CELL_EMPTY,
        .value = 0
    });
    state.map_fog = std::vector<fog_t>(state.map_width * state.map_height, (fog_t) {
        .type = FOG_HIDDEN,
        .value = FOG_VALUE_NONE
    });
    state.is_fog_dirty = true;

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
    memset(is_spawn_direction_used, 0, sizeof(is_spawn_direction_used));
    uint8_t player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }
        player_count++;

        state.player_gold[player_id] = PLAYER_STARTING_GOLD;

        // Find an unused spawn direction
        int spawn_direction;
        do {
            spawn_direction = lcg_rand() % DIRECTION_COUNT;
        } while (is_spawn_direction_used[spawn_direction]);
        is_spawn_direction_used[spawn_direction] = true;

        // Determine player spawn point
        player_spawns[player_id] = xy(state.map_width / 2, state.map_height / 2) + 
                                   xy(DIRECTION_XY[spawn_direction].x * ((state.map_width * 6) / 16), 
                                      DIRECTION_XY[spawn_direction].y * ((state.map_height * 6) / 16));

        // Place player starting units
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(-1, -1));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(1, -1));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(0, 1));
    }
    state.camera_offset = ui_camera_clamp(ui_camera_centered_on_cell(player_spawns[network_get_player_id()]), state.map_width, state.map_height);

    // Place gold on the map
    int gold_target = 128;
    int gold_count = 0;
    while (gold_count < gold_target) {
        // Randomly find the start of a gold cluster
        xy gold_cell;
        gold_cell.x = lcg_rand() % state.map_width;
        gold_cell.y = lcg_rand() % state.map_height;
        if (map_is_cell_blocked(state, gold_cell)) {
            continue;
        }

        // Check that it's not too close to the player spawns
        bool is_patch_too_close_to_player = false;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status != PLAYER_STATUS_NONE && 
                xy::manhattan_distance(gold_cell, player_spawns[player_id]) < 6) {
                    is_patch_too_close_to_player = true;
                    break;
            }
        }
        if (is_patch_too_close_to_player) {
            continue;
        }

        // Place gold on the map around the cluster
        int cluster_size = std::min(3 + (lcg_rand() % 7), gold_target - gold_count);
        for (int i = 0; i < cluster_size; i++) {
            int gold_offset = lcg_rand() % 3;
            map_set_cell(state, gold_cell, (CellType)(CELL_GOLD1 + gold_offset), MAP_GOLD_CELL_AMOUNT);
            gold_count++;

            // Determine the position of the next gold cell
            int guess_direction = lcg_rand() % DIRECTION_COUNT;
            int direction = guess_direction;
            bool is_gold_cell_valid = false;
            do {
                direction = (direction + 1) % DIRECTION_COUNT;
                is_gold_cell_valid = map_is_cell_in_bounds(state, gold_cell + DIRECTION_XY[direction]) && !map_is_cell_blocked(state, gold_cell + DIRECTION_XY[direction]);
            } while (direction != guess_direction && !is_gold_cell_valid);
            if (direction == guess_direction) {
                // There are no free spaces to place gold in, so exit the loop for this cluster
                break;
            }
            gold_cell += DIRECTION_XY[direction];
        }
    }

    map_update_fog(state);

    if (!network_is_server()) {
        log_info("Client in match.");
        network_client_toggle_ready();
    } else {
        log_info("Server in match.");
        if (network_are_all_players_ready() && player_count > 1) {
            log_error("Clients all readied before server. This shouldn't be possible.");
        }
    }
    if (network_is_server() && player_count == 1) {
        log_info("Beginning singleplayer game");
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
                log_info("Checking if all players are ready...");
                if (network_are_all_players_ready()) {
                    log_info("Starting match.");
                    network_server_start_match();
                    state.ui_mode = UI_MODE_NONE;
                }
            } else if (!network_is_server() && network_event.type == NETWORK_EVENT_MATCH_START) {
                state.ui_mode = UI_MODE_NONE;
                break;
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

            if (state.inputs[player_id].empty() || state.inputs[player_id][0].empty()) {
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

            char input_str[32];
            char* input_str_ptr = input_str;
            for (const input_t& input : state.inputs[player_id][0]) {
                input_str_ptr += sprintf(input_str_ptr, "%u, ", input.type);
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

        uint8_t current_player_id = network_get_player_id();
        out_buffer[1] = current_player_id;
        out_buffer_length = 2; // Leaves space in buffer for <network message type><player_id>

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
        if (ui_get_ui_button_hovered(state) != -1) {
            ui_handle_button_pressed(state, ui_get_ui_button(state, ui_get_ui_button_hovered(state)));
        } else if (state.ui_mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui()) {
            if (building_can_be_placed(state, state.ui_building_type, ui_get_building_cell(state))) {
                input_t input;
                input.type = INPUT_BUILD;
                input.build.building_type = state.ui_building_type;
                input.build.target_cell = ui_get_building_cell(state);
                input.build.unit_id = state.selection.ids[0];
                state.input_queue.push_back(input);

                state.ui_buttonset = UI_BUTTONSET_MINER;
                state.ui_mode = UI_MODE_NONE;
            } else {
                ui_show_status(state, UI_STATUS_CANT_BUILD);
            }
        } else if (state.ui_mode == UI_MODE_NONE && MINIMAP_RECT.has_point(mouse_pos)) {
            // On begin minimap drag
            state.ui_mode = UI_MODE_MINIMAP_DRAG;
        } else if (state.ui_mode == UI_MODE_NONE && !ui_is_mouse_in_ui()) {
            // On begin selecting
            state.ui_mode = UI_MODE_SELECTING;
            state.select_origin = mouse_world_pos;
        } 
    } 

    // KEY PRESS
    if (!input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT) && state.ui_mode != UI_MODE_SELECTING && state.ui_mode != UI_MODE_MINIMAP_DRAG) {
        for (uint32_t key = KEY_1; key < KEY_9 + 1; key++) {
            if (input_is_key_just_pressed((Key)key)) {
                // Set control group
                if (input_is_key_pressed(KEY_CTRL)) {
                    if (state.selection.type == SELECTION_TYPE_UNITS || state.selection.type == SELECTION_TYPE_BUILDINGS) {
                        state.control_groups[key] = state.selection;
                    }
                // Append control group
                } else if (input_is_key_pressed(KEY_SHIFT)) {
                    if (state.selection.type == SELECTION_TYPE_UNITS && state.selection.type == state.control_groups[key].type) {
                        std::unordered_map<entity_id, bool> ids_in_control_group;
                        for (entity_id id : state.control_groups[key].ids) {
                            ids_in_control_group[id] = true;
                        }
                        for (entity_id id : state.selection.ids) {
                            if (ids_in_control_group.find(id) == ids_in_control_group.end()) {
                                state.control_groups[key].ids.push_back(id);
                                ids_in_control_group[id] = true;
                            }
                        }
                    }
                // Switch to control group
                } else {
                    if (state.control_group_double_click_key == key && state.control_group_double_click_timer > 0) {
                        std::vector<xy> selection_cells;
                        for (entity_id id : state.selection.ids) {
                            if (state.selection.type == SELECTION_TYPE_UNITS) {
                                uint32_t unit_index = state.units.get_index_of(id);
                                if (unit_index != INDEX_INVALID) {
                                    selection_cells.push_back(state.units[unit_index].cell);
                                }
                            } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
                                uint32_t building_index = state.buildings.get_index_of(id);
                                if (building_index != INDEX_INVALID) {
                                    selection_cells.push_back(state.units[building_index].cell);
                                }
                            }
                        }

                        rect_t selection_rect = create_bounding_rect_for_points(&selection_cells[0], selection_cells.size());
                        xy selection_center = selection_rect.position + (selection_rect.size / 2);
                        state.camera_offset = ui_camera_clamp(ui_camera_centered_on_cell(selection_center), state.map_width, state.map_height);
                        state.control_group_double_click_timer = 0;
                    } else if (state.control_groups[key].type != SELECTION_TYPE_NONE) {
                        ui_set_selection(state, state.control_groups[key]);
                        state.control_group_double_click_timer = CONTROL_GROUP_DOUBLE_CLICK_DURATION;
                        state.control_group_double_click_key = key;
                    }
                }

                break;
            }
        }

        for (int button_index = 0; button_index < 6; button_index++) {
            UiButton button = ui_get_ui_button(state, button_index);
            if (button == UI_BUTTON_NONE) {
                continue;
            }

            if (input_is_ui_hotkey_just_pressed(button)) {
                ui_handle_button_pressed(state, button);
                break;
            }
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
        state.camera_offset = ui_camera_clamp(ui_camera_centered_on_cell(map_pos / TILE_SIZE), state.map_width, state.map_height);
    }

    // LEFT MOUSE RELEASE
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        if (state.ui_mode == UI_MODE_SELECTING) {
            selection_t selection = ui_create_selection_from_rect(state);
            ui_set_selection(state, selection);
            state.ui_mode = UI_MODE_NONE;
        } else if (state.ui_mode == UI_MODE_MINIMAP_DRAG) {
            state.ui_mode = UI_MODE_NONE;
        }
    }

    // ORDER MOVEMENT
    bool is_player_ordering_move = (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT) && state.ui_mode == UI_MODE_NONE) ||
                                       (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT) && state.ui_mode == UI_MODE_ATTACK_MOVE) ;
    if (is_player_ordering_move && state.selection.type == SELECTION_TYPE_UNITS && (MINIMAP_RECT.has_point(mouse_pos) || !ui_is_mouse_in_ui())) {
        xy move_target;
        if (ui_is_mouse_in_ui()) {
            xy minimap_pos = mouse_pos - MINIMAP_RECT.position;
            move_target = xy((state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, 
                                (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        } else {
            move_target = mouse_world_pos;
        }

        // Create the move event
        input_t input;
        input.move.target_cell = move_target / TILE_SIZE;
        input.move.target_entity_id = map_get_fog(state, input.move.target_cell).type == FOG_REVEALED && 
                                        map_get_cell(state, input.move.target_cell).type == CELL_UNIT 
                                        ? map_get_cell(state, input.move.target_cell).value : ID_NULL;

        //                                          This is so that if they directly click their target, it acts the same as a regular right click on the target
        if (state.ui_mode == UI_MODE_ATTACK_MOVE && (input.move.target_entity_id == ID_NULL || map_get_fog(state, input.move.target_cell).type == FOG_HIDDEN)) {
            input.type = INPUT_ATTACK_MOVE;
        } else if (map_get_fog(state, input.move.target_cell).type == FOG_HIDDEN) {
            input.type = INPUT_BLIND_MOVE;
        } else {
            input.type = INPUT_MOVE;
        }

        input.move.unit_count = 0;
        memcpy(input.move.unit_ids, &state.selection.ids[0], state.selection.ids.size() * sizeof(uint16_t));
        input.move.unit_count = state.selection.ids.size();
        // The unit count should be greater than 0 because the selection type is SELECTION_TYPE_UNITS
        GOLD_ASSERT(input.move.unit_count != 0);
        state.input_queue.push_back(input);

        // Provide instant user feedback
        state.ui_move_cell = map_get_cell(state, input.move.target_cell);
        if (input.type == INPUT_BLIND_MOVE || map_get_cell(state, input.move.target_cell).type == CELL_EMPTY) {
            state.ui_move_animation = animation_create(ANIMATION_UI_MOVE);
            state.ui_move_position = mouse_world_pos;
        } else {
            state.ui_move_animation = animation_create(ANIMATION_UI_MOVE_GOLD);
            state.ui_move_position = cell_center(input.move.target_cell).to_xy();
        }

        if (state.ui_mode == UI_MODE_ATTACK_MOVE) {
            state.ui_mode = UI_MODE_NONE;
            ui_set_selection(state, state.selection);
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
        state.camera_offset = ui_camera_clamp(state.camera_offset, state.map_width, state.map_height);
    }

    // Update timers
    if (state.ui_status_timer != 0) {
        state.ui_status_timer--;
    }
    if (state.control_group_double_click_timer != 0) {
        state.control_group_double_click_timer--;
    }

    // Update particles
    if (animation_is_playing(state.ui_move_animation)) {
        animation_update(state.ui_move_animation);
    }

    // Update units
    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        if (state.units[unit_index].health == 0) {
            continue;
        }

        unit_update(state, unit_index);

        AnimationName expected_animation = unit_get_expected_animation(state.units[unit_index]);
        if (state.units[unit_index].animation.name != expected_animation || !animation_is_playing(state.units[unit_index].animation)) {
            state.units[unit_index].animation = animation_create(expected_animation);
        }
        animation_update(state.units[unit_index].animation);
        state.units[unit_index].animation.frame.y = unit_get_animation_vframe(state.units[unit_index]);
    }

    // Remove any dead units
    uint32_t unit_index = 0;
    while (unit_index < state.units.size()) {
        if (state.units[unit_index].health == 0) {
            // TODO play death animation
            unit_destroy(state, unit_index);
        } else {
            unit_index++;
        }
    }

    // Update buildings
    for (building_t& building : state.buildings) {
        if (building.health == 0) {
            continue;
        }

        building_update(state, building);
    }

    // Remove any dead buildings
    uint32_t building_index = 0;
    while (building_index < state.buildings.size()) {
        if (state.buildings[building_index].health == 0) {
            // TODO play death animation
            building_destroy(state, building_index);
        } else {
            building_index++;
        }
    }

    // Remove any dead buildings
    uint32_t remove_building_index = 0;
    while (remove_building_index < state.buildings.size()) {
        if (state.buildings[remove_building_index].health == 0) {
            // TODO: play death animation
            building_destroy(state, state.buildings.get_id_of(remove_building_index));
        } else {
            remove_building_index++;
        }
    }

    // Check the player's selection for dead units / buildings
    if (state.selection.type == SELECTION_TYPE_UNITS) {
        uint32_t index = 0;
        while (index < state.selection.ids.size()) {
            uint32_t unit_index = state.units.get_index_of(state.selection.ids[index]);
            if (unit_index == INDEX_INVALID) {
                state.selection.ids.erase(state.selection.ids.begin() + index);
            } else {
                index++;
            }
        }
        if (state.selection.ids.size() == 0) {
            selection_t selection_empty = (selection_t) {
                .type = SELECTION_TYPE_NONE,
                .ids = std::vector<entity_id>()
            };
            ui_set_selection(state, selection_empty);
            if (state.ui_mode == UI_MODE_ATTACK_MOVE) {
                state.ui_mode = UI_MODE_NONE;
            }
        }
    } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        uint32_t index = state.buildings.get_index_of(state.selection.ids[0]);
        if (index == INDEX_INVALID) {
            selection_t selection_empty = (selection_t) {
                .type = SELECTION_TYPE_NONE,
                .ids = std::vector<entity_id>()
            };
            ui_set_selection(state, selection_empty);
        }
    }

    // Update Fog of War
    if (state.is_fog_dirty) {
        map_update_fog(state);
    }
}

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const unit_t& unit : state.units) {
        if (unit.player_id == player_id) {
            population += UNIT_DATA.at(unit.type).population_cost;
        }
    }
    
    return population;
}

uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id) {
    uint32_t max_population = 5;
    for (const building_t& building : state.buildings) {
        if (building.player_id == player_id && building.type == BUILDING_HOUSE && building.is_finished) {
            max_population += 10;
        }
    }

    return max_population;
}

// INPUT

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_ATTACK_MOVE:
        case INPUT_BLIND_MOVE:
        case INPUT_MOVE: {
            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.move.target_entity_id, sizeof(entity_id));
            out_buffer_length += sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.move.unit_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

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
        case INPUT_BUILDING_ENQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_enqueue, sizeof(input_building_enqueue_t));
            out_buffer_length += sizeof(input_building_enqueue_t);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_dequeue, sizeof(input_building_dequeue_t));
            out_buffer_length += sizeof(input_building_dequeue_t);
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
        case INPUT_ATTACK_MOVE:
        case INPUT_BLIND_MOVE:
        case INPUT_MOVE: {
            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.move.target_entity_id, in_buffer + in_buffer_head, sizeof(entity_id));
            in_buffer_head += sizeof(entity_id);

            memcpy(&input.move.unit_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

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
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            memcpy(&input.building_enqueue, in_buffer + in_buffer_head, sizeof(input_building_enqueue_t));
            in_buffer_head += sizeof(input_building_enqueue_t);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            memcpy(&input.building_dequeue, in_buffer + in_buffer_head, sizeof(input_building_dequeue_t));
            in_buffer_head += sizeof(input_building_dequeue_t);
            break;
        }
        default:
            break;
    }

    return input;
}

void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_BLIND_MOVE:
        case INPUT_ATTACK_MOVE:
        case INPUT_MOVE: {
            // If we tried to move towards a unit, try and find the unit
            uint32_t target_unit_index = input.move.target_entity_id == ID_NULL ? INDEX_INVALID : state.units.get_index_of(input.move.target_entity_id);

            bool should_move_as_group = target_unit_index == INDEX_INVALID && input.move.unit_count > 1 && map_get_cell(state, input.move.target_cell).type == CELL_EMPTY;
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
                    if (map_is_cell_in_bounds(state, group_move_target) && xy::manhattan_distance(group_move_target, input.move.target_cell) <= 3) {
                        unit_target = group_move_target;
                    }
                } else if (target_unit_index != INDEX_INVALID) {
                    unit_target = state.units[target_unit_index].cell;
                }

                // Set the units target
                if (input.type == INPUT_BLIND_MOVE) {
                    unit_set_target(state, unit, (unit_target_t) {
                        .type = UNIT_TARGET_CELL,
                        .cell = unit_target
                    });
                } else if (input.type == INPUT_ATTACK_MOVE) {
                    unit_set_target(state, unit, (unit_target_t) {
                        .type = UNIT_TARGET_ATTACK,
                        .cell = unit_target
                    });
                } else if (target_unit_index != INDEX_INVALID) {
                    unit_set_target(state, unit, (unit_target_t) {
                        .type = UNIT_TARGET_UNIT,
                        .id = input.move.target_entity_id
                    });
                } else {
                    switch (map_get_cell(state, unit_target).type) {
                        case CELL_EMPTY:
                        case CELL_UNIT: {
                            unit_set_target(state, unit, (unit_target_t) {
                                .type = UNIT_TARGET_CELL,
                                .cell = unit_target
                            });
                            break;
                        }
                        case CELL_BUILDING: {
                            entity_id target_entity_id = map_get_cell(state, unit_target).value;
                            uint32_t building_index = state.buildings.get_index_of(target_entity_id);
                            if (building_index == INDEX_INVALID) {
                                unit_set_target(state, unit, (unit_target_t) {
                                    .type = UNIT_TARGET_CELL,
                                    .cell = unit_target
                                });
                            } else if (state.buildings[building_index].player_id != unit.player_id) {
                                unit_set_target(state, unit, (unit_target_t) {
                                    .type = UNIT_TARGET_BUILDING,
                                    .id = map_get_cell(state, unit_target).value
                                });
                            } else if (state.buildings[building_index].type == BUILDING_CAMP && state.buildings[building_index].is_finished && unit.gold_held > 0) {
                                unit_set_target(state, unit, (unit_target_t) {
                                    .type = UNIT_TARGET_CAMP,
                                    .id = map_get_cell(state, unit_target).value
                                });
                            } else {
                                unit_set_target(state, unit, (unit_target_t) {
                                    .type = UNIT_TARGET_CELL,
                                    .cell = get_nearest_free_cell_around_building(state, unit.cell, state.buildings[building_index])
                                });
                            }
                            break;
                        }
                        case CELL_GOLD1:
                        case CELL_GOLD2:
                        case CELL_GOLD3: {
                            if (unit.type == UNIT_MINER && !should_move_as_group) {
                                unit_set_target(state, unit, (unit_target_t) {
                                    .type = UNIT_TARGET_GOLD,
                                    .cell = unit_target
                                });
                            } else {
                                unit_set_target(state, unit, (unit_target_t) {
                                    .type = UNIT_TARGET_CELL,
                                    .cell = unit_target
                                });
                            }
                            break;
                        }
                    } // End switch unit target cell type
                } // End else (if not targeting unit)
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
                unit_set_target(state, unit, (unit_target_t) {
                    .type = UNIT_TARGET_NONE
                });
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
            xy unit_build_target = get_nearest_free_cell_within_rect(unit.cell, rect_t(input.build.target_cell, building_cell_size((BuildingType)input.build.building_type)));
            unit_set_target(state, unit, (unit_target_t) {
                .type = UNIT_TARGET_BUILD,
                .build = (unit_target_build_t) {
                    .unit_cell = unit_build_target,
                    .building_cell = input.build.target_cell,
                    .building_type = (BuildingType)input.build.building_type,
                    .building_id = ID_NULL
                }
            });
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
            for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
                if (state.units[unit_index].target.type == UNIT_TARGET_BUILD && state.units[unit_index].target.build.building_id == input.build_cancel.building_id) {
                    unit_stop_building(state, state.units.get_id_of(unit_index), building);
                }
            }
            // Destroy the building
            building_destroy(state, input.build_cancel.building_id);
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            uint32_t building_index = state.buildings.get_index_of(input.building_enqueue.building_id);
            if (building_index == INDEX_INVALID) {
                return;
            }
            if (state.player_gold[player_id] < building_queue_item_cost(input.building_enqueue.item)) {
                return;
            }

            state.player_gold[player_id] -= building_queue_item_cost(input.building_enqueue.item);
            building_enqueue(state.buildings[building_index], input.building_enqueue.item);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state.buildings.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID) {
                return;
            }
            if (state.buildings[building_index].queue.empty()) {
                return;
            }

            state.player_gold[player_id] += building_queue_item_cost(state.buildings[building_index].queue[0]);
            building_dequeue(state.buildings[building_index]);

            break;
        }
        default:
            break;
    }
}

// MISC

xy_fixed cell_center(xy cell) {
    return xy_fixed((cell * TILE_SIZE) + xy(TILE_SIZE / 2, TILE_SIZE / 2));
}

xy get_nearest_free_cell_within_rect(xy start_cell, rect_t rect) {
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

xy get_first_empty_cell_around_rect(const match_state_t& state, rect_t rect) {
    xy cell = rect.position + xy(-1, 0);
    int step_direction = DIRECTION_SOUTH;
    int step_count = 0;
    int x_step_amount = rect.size.x + 1;
    int y_step_amount = rect.size.y;

    while (map_is_cell_blocked(state, cell) || !map_is_cell_in_bounds(state, cell)) {
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

// Returns the nearest cell around the rect relative to start_cell
// If there are no free cells around the rect in a radius of 1, then this returns the start cell
xy get_nearest_free_cell_around_building(const match_state_t& state, xy start_cell, const building_t& building) {

    xy nearest_cell;
    int nearest_cell_dist = -1;

    xy building_size = building_cell_size(building.type);

    xy cell_begin[4] = { 
        building.cell + xy(-1, 0),
        building.cell + xy(0, building_size.y),
        building.cell + xy(building_size.x, building_size.y - 1),
        building.cell + xy(building_size.x - 1, -1)
    };
    xy cell_end[4] = { 
        cell_begin[0] + xy(0, building_size.y - 1),
        cell_begin[1] + xy(building_size.x - 1, 0),
        cell_begin[2] - xy(0, building_size.y - 1),
        cell_begin[3] - xy(building_size.x - 1, 0)
    };
    xy cell_step[4] = { xy(0, 1), xy(1, 0), xy(0, -1), xy(-1, 0) };
    uint32_t index = 0;
    xy cell = cell_begin[index];
    while (index < 4) {
        if (map_is_cell_in_bounds(state, cell)) {
            uint32_t cell_type = map_get_cell(state, cell).type;
            bool is_cell_free = cell_type == CELL_EMPTY || (cell_type == CELL_UNIT && xy::manhattan_distance(start_cell, cell) > 3);
            if (is_cell_free && (nearest_cell_dist == -1 || xy::manhattan_distance(start_cell, cell) < nearest_cell_dist)) {
                nearest_cell = cell;
                nearest_cell_dist = xy::manhattan_distance(start_cell, cell);
            }
        } 

        cell += cell_step[index];
        if (cell == cell_end[index]) {
            index++;
            if (index < 4) {
                cell = cell_begin[index];
            }
        }
    }

    return nearest_cell_dist != -1 ? nearest_cell : start_cell;
}

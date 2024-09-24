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

// This is a define because otherwise the compiler will whine that it's an unused var on release builds
#define INPUT_MAX_SIZE 256

static const uint32_t TICK_DURATION = 4;
static const uint8_t TICK_OFFSET = 4;

static const int CAMERA_DRAG_MARGIN = 8;
static const int CAMERA_DRAG_SPEED = 8;

static const uint32_t CONTROL_GROUP_DOUBLE_CLICK_DURATION = 16;
static const uint32_t UI_DOUBLE_CLICK_DURATION = 16;

static const uint32_t PLAYER_STARTING_GOLD = 1000;
const uint32_t MATCH_WINNING_GOLD_AMOUNT = 5000;
static const uint32_t MINE_STARTING_GOLD_AMOUNT = 2500;

const uint32_t MATCH_TAKING_DAMAGE_TIMER_DURATION = 30;
const uint32_t MATCH_TAKING_DAMAGE_FLICKER_TIMER_DURATION = 10;
const uint32_t MATCH_ALERT_DURATION = 90;
const uint32_t MATCH_ATTACK_ALERT_DURATION = 60 * 20;
const uint32_t MATCH_ATTACK_ALERT_DISTANCE = 20;

match_state_t match_init() {
    match_state_t state;

    // Init UI
    state.ui_mode = UI_MODE_NOT_STARTED;
    state.ui_buttonset = UI_BUTTONSET_NONE;
    state.camera_offset = xy(0, 0);
    state.ui_status_timer = 0;
    state.ui_rally_animation = animation_create(ANIMATION_RALLY_FLAG);
    for (uint32_t i = 0; i < 9; i++) {
        state.control_groups[i] = (selection_t) {
            .type = SELECTION_TYPE_NONE
        };
    }
    state.control_group_double_click_timer = 0;
    state.ui_selected_control_group = -1;

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
    log_info("Generating map...");
    state.map.width = 96;
    state.map.height = 96;
    state.map.tiles = std::vector<uint32_t>(state.map.width * state.map.height, TILE_ARIZONA_SAND1);
    state.map.cells = std::vector<cell_t>(state.map.width * state.map.height, (cell_t) {
        .type = CELL_EMPTY,
        .value = 0
    });
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == PLAYER_STATUS_NONE) {
            continue;
        }
        state.map.player_fog[player_id] = std::vector<FogType>(state.map.width * state.map.height, FOG_HIDDEN);
    }

    /*
    for (uint32_t i = 0; i < state.map.width * state.map.height; i++) {
        int new_base = lcg_rand() % 7;
        if (new_base < 4 && i % 3 == 0) {
            state.map.tiles[i] = new_base == 1 ? 2 : 1; 
        }
    }
    */

    uint32_t direction_mask[DIRECTION_COUNT] = {
        1,
        2,
        4,
        8,
        16,
        32,
        64, 
        128
    };
    uint32_t unique_index = 0;
    xy coords = xy(1, 1);
    for (uint32_t i = 0; i < 256; i++) {
        bool has_direction[DIRECTION_COUNT];
        for (uint32_t direction = 0; direction < DIRECTION_COUNT; direction++) {
            has_direction[direction] = i & direction_mask[direction];
        }
        bool is_unique_combination = true;
        for (uint32_t direction = DIRECTION_NORTHEAST; direction < DIRECTION_COUNT; direction += 2) {
            if (has_direction[direction] && !(has_direction[direction - 1] && has_direction[(direction + 1) % DIRECTION_COUNT])) {
                is_unique_combination = false;
                break;
            }
        }
        if (!is_unique_combination) {
            continue;
        }

        log_trace("unique index: %u, bitsum: %u\n%i%i%i\n%i %i\n%i%i%i", unique_index, i,
            has_direction[DIRECTION_NORTHWEST], has_direction[DIRECTION_NORTH], has_direction[DIRECTION_NORTHEAST],
            has_direction[DIRECTION_WEST], has_direction[DIRECTION_EAST],
            has_direction[DIRECTION_SOUTHWEST], has_direction[DIRECTION_SOUTH], has_direction[DIRECTION_SOUTHEAST]
        );
        xy cell = coords + xy(1, 1);
        state.map.tiles[cell.x + (cell.y * state.map.width)] = TILE_ARIZONA_WATER;
        for (uint32_t direction = 0; direction < DIRECTION_COUNT; direction++) {
            cell = coords + xy(1, 1) + DIRECTION_XY[direction];
            state.map.tiles[cell.x + (cell.y * state.map.width)] = has_direction[direction] ? TILE_ARIZONA_SAND1 : TILE_ARIZONA_WATER;
        }
        coords.x += 4;
        if (coords.x + 4 > state.map.width) {
            coords.x = 1;
            coords.y += 4;
        }

        unique_index++;
    }

    // Set blocked cells
    for (uint32_t index = 0; index < state.map.width * state.map.height; index++) {
        if (get_tile_data(state.map.tiles[index]).blocked) {
            state.map.cells[index].type = CELL_BLOCKED;
        }
    }

    // Init players
    log_trace("Initializing players...");
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
        player_spawns[player_id] = xy(state.map.width / 2, state.map.height / 2) + 
                                   xy(DIRECTION_XY[spawn_direction].x * ((state.map.width * 6) / 16), 
                                      DIRECTION_XY[spawn_direction].y * ((state.map.height * 6) / 16));
        player_spawns[0] = xy(16, 16);

        // Place player starting units
        unit_create(state, player_id, UNIT_WAGON, player_spawns[player_id] + xy(0, 0));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(-2, -1));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(-2, 0));
        unit_create(state, player_id, UNIT_MINER, player_spawns[player_id] + xy(2, 2));
    }
    state.camera_offset = camera_centered_on_cell(player_spawns[network_get_player_id()], state.map.width, state.map.height);

    // Place gold on the map
    log_trace("Placing gold...");
    /*
    std::vector<xy> gold_patch_cells;
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            gold_patch_cells.push_back(xy(12 + (24 * x), 12 + (24 * y)));
            xy mine_cell = xy(12 + (24 * x), 12 + (24 * y));
            entity_id mine_id = state.mines.push_back((mine_t) {
                .cell = mine_cell,
                .gold_left = MINE_STARTING_GOLD_AMOUNT,
                .is_occupied = false
            });
            map_set_cell_rect(state.map, rect_t(mine_cell, xy(MINE_SIZE, MINE_SIZE)), CELL_MINE, mine_id);
        }
    }
    */
    /*
    uint32_t target_mine_count = 7;
    uint32_t attempts = 0;
    while (state.mines.size() < target_mine_count) {
        xy mine_cell;
        mine_cell.x = 16 + (lcg_rand() % (state.map_width - 32));
        mine_cell.y = 16 + (lcg_rand() % (state.map_height - 32));

        // Check that it's not too close to the player spawns
        bool mine_cell_valid = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status != PLAYER_STATUS_NONE && 
                xy::manhattan_distance(mine_cell, player_spawns[player_id]) < 20) {
                    mine_cell_valid = false;
                    break;
            }
        }
        // Check that it's not too close to other mines
        for (mine_t& mine : state.mines) {
            if (xy::manhattan_distance(mine.cell, mine_cell) < 20) {
                mine_cell_valid = false;
                break;
            }
        }
        if (!mine_cell_valid) {
            attempts++;
            if (attempts == 5) {
                log_warn("Mine generation reached max attempts.");
                while (state.mines.size() != 0) {
                    map_set_cell_rect(state, rect_t(state.mines[0].cell, xy(MINE_SIZE, MINE_SIZE)), CELL_EMPTY);
                    state.mines.remove_at(0);
                }
                attempts = 0;
            }
            continue;
        }

        mine_t mine = (mine_t) {
            .cell = mine_cell,
            .gold_left = MINE_STARTING_GOLD_AMOUNT,
            .is_occupied = false
        };
        entity_id mine_id = state.mines.push_back(mine);
        map_set_cell_rect(state, rect_t(mine_cell, xy(MINE_SIZE, MINE_SIZE)), CELL_MINE, mine_id);
    }
    */

    // Place decorations on the map
    log_trace("Placing decorations...");
    /*
    for (int i = 0; i < state.map.width * state.map.height; i++) {
        if (lcg_rand() % 40 == 0 && i % 5 == 0 && state.map.cells[i].type == CELL_EMPTY) {
            bool is_gold_nearby = false;
            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                xy cell = xy(i % state.map.width, i / state.map.height) + DIRECTION_XY[direction];
                if (!map_is_cell_in_bounds(state.map, cell)) {
                    continue;
                }
                if (map_get_cell(state.map, cell).type != CELL_EMPTY) {
                    is_gold_nearby = true;
                    break;
                }
            }

            if (is_gold_nearby) {
                continue;
            }

            state.map.tiles[i] = (tile_t) {
                .base = 0,
                .decoration = (uint16_t)(1 + (rand() % 5))
            };
            state.map.decorations.push_back((decoration_t) {
                .index = (uint16_t)(rand() % 5),
                .cell = xy(i % state.map.width, i / state.map.width)
            });
            state.map.cells[i].type = CELL_BLOCKED;
        }
    }
    */
    log_info("Map complete!");

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == PLAYER_STATUS_NONE) {
            continue;
        }
        map_fog_reveal(state, player_id);
        map_update_fog(state, player_id);
        // state.map.is_fog_dirty = true;
    }

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

    // Wait for player to leave the match
    if (state.ui_mode == UI_MODE_MATCH_OVER) {
        if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
            network_disconnect();
            state.ui_mode = UI_MODE_LEAVE_MATCH;
        }
        return;
    }

    if (state.ui_mode == UI_MODE_MATCH_OVER) {
        return;
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
            log_info("Missing inputs for this frame. waiting...");
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
            entity_id nearest_builder_id = ui_get_nearest_builder(state, ui_get_building_cell(state));
            if (building_can_be_placed(state, state.ui_building_type, ui_get_building_cell(state), state.units.get_by_id(nearest_builder_id).cell)) {
                input_t input;
                input.type = INPUT_BUILD;
                input.build.building_type = state.ui_building_type;
                input.build.target_cell = ui_get_building_cell(state);
                input.build.unit_count = state.selection.ids.size();
                input.build.unit_ids[0] = nearest_builder_id;
                uint32_t index = 1;
                for (entity_id id : state.selection.ids) {
                    if (id == nearest_builder_id) {
                        continue;
                    }
                    input.build.unit_ids[index] = id;
                    index++;
                }
                state.input_queue.push_back(input);

                state.ui_buttonset = UI_BUTTONSET_MINER;
                state.ui_mode = UI_MODE_NONE;
            } else {
                ui_show_status(state, UI_STATUS_CANT_BUILD);
            }
        } else if (state.ui_mode == UI_MODE_NONE && ui_get_building_queue_index_hovered(state) != -1) {
            state.input_queue.push_back((input_t) {
                .type = INPUT_BUILDING_DEQUEUE,
                .building_dequeue = (input_building_dequeue_t) {
                    .building_id = state.selection.ids[0],
                    .index = (uint16_t)ui_get_building_queue_index_hovered(state)
                }
            });
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
        for (uint32_t key = KEY_1; key < KEY_0 + 1; key++) {
            if (input_is_key_just_pressed((Key)key)) {
                // Set control group
                if (input_is_key_pressed(KEY_CTRL)) {
                    if (state.selection.type == SELECTION_TYPE_UNITS || state.selection.type == SELECTION_TYPE_BUILDINGS) {
                        state.control_groups[key] = state.selection;
                        state.ui_selected_control_group = key - KEY_1;
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
                } else {
                    // Snap to control group location
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
                                    selection_cells.push_back(state.buildings[building_index].cell);
                                }
                            }
                        }

                        rect_t selection_rect = create_bounding_rect_for_points(&selection_cells[0], selection_cells.size());
                        xy selection_center = selection_rect.position + (selection_rect.size / 2);
                        state.camera_offset = camera_centered_on_cell(selection_center, state.map.width, state.map.height);
                        state.control_group_double_click_timer = 0;
                    // Switch to control group
                    } else if (state.control_groups[key].type != SELECTION_TYPE_NONE) {
                        ui_set_selection(state, state.control_groups[key]);
                        state.ui_selected_control_group = key - KEY_1;
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

        if (input_is_key_just_pressed(KEY_SPACE) && !state.attack_alerts.empty()) {
            attack_alert_t latest_attack_alert = state.attack_alerts[state.attack_alerts.size() - 1];
            state.camera_offset = camera_centered_on_cell(latest_attack_alert.cell, state.map.width, state.map.height);
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
        xy map_pos = xy((state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        state.camera_offset = camera_centered_on_cell(map_pos / TILE_SIZE, state.map.width, state.map.height);
    }

    // LEFT MOUSE RELEASE
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        if (state.ui_mode == UI_MODE_SELECTING) {
            selection_t selection = ui_create_selection_from_rect(state);
            // Double click selection
            if (selection.ids.size() == 1 && (selection.type == SELECTION_TYPE_UNITS || selection.type == SELECTION_TYPE_BUILDINGS)) {
                if (state.ui_double_click_timer == 0) {
                    state.ui_double_click_timer = UI_DOUBLE_CLICK_DURATION;
                } else if (state.ui_double_click_timer != 0 && state.selection.ids.size() == 1 && state.selection.type == selection.type && state.selection.ids[0] == selection.ids[0]) {
                    // TODO: add buildings to this
                    if (selection.type == SELECTION_TYPE_UNITS) {
                        rect_t screen_rect = rect_t(state.camera_offset, xy(SCREEN_WIDTH, SCREEN_HEIGHT));
                        UnitType double_click_unit_type = state.units.get_by_id(selection.ids[0]).type;
                        selection.ids.clear();
                        for (uint32_t index = 0; index < state.units.size(); index++) {
                            if (state.units[index].player_id == network_get_player_id() && 
                                state.units[index].type == double_click_unit_type &&
                                screen_rect.intersects(unit_get_rect(state.units[index]))) {
                                    selection.ids.push_back(state.units.get_id_of(index));
                            }
                        }
                    }
                    state.ui_double_click_timer = 0;
                }
            }
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
            move_target = xy((state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, 
                                (state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        } else {
            move_target = mouse_world_pos;
        }

        // Create the move event
        input_t input;
        input.move.target_cell = move_target / TILE_SIZE;
        input.move.target_entity_id = ID_NULL;
        CellType cell_type = map_get_cell(state.map, input.move.target_cell).type;
        FogType fog_type = map_get_fog(state.map, network_get_player_id(), input.move.target_cell);
        if ((cell_type == CELL_UNIT && fog_type == FOG_REVEALED) || ((cell_type == CELL_BUILDING || cell_type == CELL_MINE) && fog_type != FOG_HIDDEN)) {
            input.move.target_entity_id = map_get_cell(state.map, input.move.target_cell).value;
        }

        //                                          This is so that if they directly click their target, it acts the same as a regular right click on the target
        if (state.ui_mode == UI_MODE_ATTACK_MOVE && (input.move.target_entity_id == ID_NULL || map_get_fog(state.map, network_get_player_id(), input.move.target_cell) == FOG_HIDDEN)) {
            input.type = INPUT_ATTACK_MOVE;
        } else if (map_get_fog(state.map, network_get_player_id(), input.move.target_cell) == FOG_HIDDEN) {
            input.type = INPUT_BLIND_MOVE;
        } else if (input.move.target_entity_id != ID_NULL && map_get_cell(state.map, input.move.target_cell).type == CELL_UNIT) {
            input.type = INPUT_MOVE_UNIT;
        } else if (input.move.target_entity_id != ID_NULL && map_get_cell(state.map, input.move.target_cell).type == CELL_BUILDING) {
            input.type = INPUT_MOVE_BUILDING;
        } else if (input.move.target_entity_id != ID_NULL && map_get_cell(state.map, input.move.target_cell).type == CELL_MINE) {
            input.type = INPUT_MOVE_MINE;
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
        state.ui_move_cell = map_get_cell(state.map, input.move.target_cell);
        if (input.type == INPUT_BLIND_MOVE || map_get_cell(state.map, input.move.target_cell).type == CELL_EMPTY) {
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

    // RALLY POINT
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT) && 
        state.selection.type == SELECTION_TYPE_BUILDINGS && 
        (MINIMAP_RECT.has_point(mouse_pos) || !ui_is_mouse_in_ui())) {
        building_t& building = state.buildings.get_by_id(state.selection.ids[0]);
        if (building_is_finished(building) && BUILDING_DATA.at(building.type).can_rally) {
            xy move_target;
            if (ui_is_mouse_in_ui()) {
                xy minimap_pos = mouse_pos - MINIMAP_RECT.position;
                move_target = xy((state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, 
                                    (state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
            } else {
                move_target = mouse_world_pos;
            }

            input_t input;
            input.type = INPUT_RALLY;
            input.rally = (input_rally_t) {
                .building_id = state.selection.ids[0],
                .rally_point = move_target
            };
            state.input_queue.push_back(input);
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
        state.camera_offset = camera_clamp(state.camera_offset, state.map.width, state.map.height);
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
    animation_update(state.ui_rally_animation);

    // Update units
    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
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
        if ((state.units[unit_index].mode == UNIT_MODE_DEATH_FADE && !animation_is_playing(state.units[unit_index].animation)) ||
            (state.units[unit_index].mode == UNIT_MODE_FERRY && state.units[unit_index].health == 0)) {
            unit_destroy(state, unit_index);
        } else {
            unit_index++;
        }
    }

    // Update buildings
    for (building_t& building : state.buildings) {
        building_update(state, building);
    }

    // Remove any dead buildings
    uint32_t building_index = 0;
    while (building_index < state.buildings.size()) {
        if (state.buildings[building_index].mode == BUILDING_MODE_DESTROYED && state.buildings[building_index].queue_timer == 0) {
            // TODO play death animation
            building_destroy(state, building_index);
        } else {
            building_index++;
        }
    }

    // Check the player's selection for dead units / buildings
    if (state.selection.type == SELECTION_TYPE_UNITS || state.selection.type == SELECTION_TYPE_ENEMY_UNIT) {
        uint32_t index = 0;
        while (index < state.selection.ids.size()) {
            uint32_t unit_index = state.units.get_index_of(state.selection.ids[index]);
            if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0) {
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
    } else if (state.selection.type == SELECTION_TYPE_BUILDINGS || state.selection.type == SELECTION_TYPE_ENEMY_BUILDING) {
        uint32_t index = state.buildings.get_index_of(state.selection.ids[0]);
        if (index == INDEX_INVALID || state.buildings[index].health == 0) {
            selection_t selection_empty = (selection_t) {
                .type = SELECTION_TYPE_NONE,
                .ids = std::vector<entity_id>()
            };
            ui_set_selection(state, selection_empty);
        }
    }

    // Update Fog of War
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == PLAYER_STATUS_NONE) {
            continue;
        }
        map_update_fog(state, player_id);
    }

    // Update alerts
    auto it = state.alerts.begin();
    while (it != state.alerts.end()) {
        it->timer--;
        bool is_entity_invalid;
        switch (it->type) {
            case ALERT_UNIT_ATTACKED:
            case ALERT_UNIT_FINISHED:
                is_entity_invalid = state.units.get_index_of(it->id) == INDEX_INVALID;
                break;
            case ALERT_BUILDING_ATTACKED:
            case ALERT_BUILDING_FINISHED:
                is_entity_invalid = state.buildings.get_index_of(it->id) == INDEX_INVALID;
                break;
            case ALERT_MINE_COLLAPSED:
                is_entity_invalid = false;
                break;
        }
        if (it->timer == 0 || is_entity_invalid) {
            it = state.alerts.erase(it);
        } else {
            it++;
        }
    }

    // Check for winners
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == PLAYER_STATUS_NONE) {
            continue;
        }
        if (state.player_gold[player_id] >= MATCH_WINNING_GOLD_AMOUNT) {
            state.ui_mode = UI_MODE_MATCH_OVER;
            if (player_id == network_get_player_id()) {
                ui_show_status(state, "You win!");
            } else {
                ui_show_status(state, "You lose...");
            }
            break;
        }
    }
}

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const unit_t& unit : state.units) {
        if (unit.player_id == player_id && unit.health != 0) {
            population += UNIT_DATA.at(unit.type).population_cost;
        }
    }
    
    return population;
}

uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id) {
    uint32_t max_population = 10;
    for (const building_t& building : state.buildings) {
        if (building.player_id == player_id && building.type == BUILDING_HOUSE && building_is_finished(building)) {
            max_population += 10;
        }
    }

    return max_population;
}

bool map_is_cell_rect_blocked_pathfind(const match_state_t& state, xy origin, rect_t cell_rect, bool should_ignore_miners) {
    entity_id origin_id = state.map.cells[origin.x + (origin.y * state.map.width)].value;
    for (int x = cell_rect.position.x; x < cell_rect.position.x + cell_rect.size.x; x++) {
        for (int y = cell_rect.position.y; y < cell_rect.position.y + cell_rect.size.y; y++) {
            cell_t cell = state.map.cells[x + (y * state.map.width)];
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
                if (should_ignore_miners) {
                    const unit_t& unit = state.units.get_by_id(cell.value);
                    if ((unit.target.type == UNIT_TARGET_MINE || unit.target.type == UNIT_TARGET_CAMP) && xy::manhattan_distance(origin, xy(x, y)) > 1) {
                        continue;
                    }
                }
            }
            return true;
        }
    }

    return false;
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
    if (map_is_cell_rect_blocked_pathfind(state, from, rect_t(to, cell_size), should_ignore_miners) && cell_size.x + cell_size.y > 2) {
        xy nearest_alternate;
        int nearest_alternate_distance = -1;
        for (int x = 0; x < cell_size.x; x++) {
            for (int y = 0; y < cell_size.y; y++) {
                if (x == 0 && y == 0) {
                    continue;
                }
                xy alternate = to - xy(x, y);
                if (map_is_cell_rect_in_bounds(state.map, rect_t(alternate, cell_size)) && !map_is_cell_rect_blocked_pathfind(state, from, rect_t(alternate, cell_size), should_ignore_miners)) {
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
    int explored_indices[state.map.width * state.map.height];
    memset(explored_indices, -1, state.map.width * state.map.height * sizeof(int));
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
        explored_indices[smallest.cell.x + (smallest.cell.y * state.map.width)] = explored.size() - 1;
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
            if (!map_is_cell_rect_in_bounds(state.map, rect_t(child.cell, cell_size))) {
                continue;
            }
            // Don't consider blocked spaces, unless:
            // 1. the blocked space is a unit that is very far away. we pretend such spaces are blocked since the unit will probably move by the time we get there
            // 2. the blocked space is the target_cell. by allowing the path to go here we can avoid worst-case pathfinding even when the target_cell is blocked
            if (child.cell != to || xy::manhattan_distance(smallest.cell, child.cell) != 1) {
                if (map_is_cell_rect_blocked_pathfind(state, from, rect_t(child.cell, cell_size), should_ignore_miners)) {
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
            if (explored_indices[child.cell.x + (child.cell.y * state.map.width)] != -1) {
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
    if ((*path)[path->size() - 1] == to && map_is_cell_blocked(state.map, to)) {
        path->pop_back();
    }
}

void map_fog_reveal(match_state_t& state, uint8_t player_id) {
    // Reveal based on unit vision
    for (const unit_t& unit : state.units) {
        if (unit.player_id != player_id || unit.mode == UNIT_MODE_FERRY) {
            continue;
        }

        map_fog_reveal_at_cell(state.map, player_id, unit.cell, unit_cell_size(unit.type), UNIT_DATA.at(unit.type).sight);
    }

    for (const building_t& building : state.buildings) {
        if (building.player_id != player_id) {
            continue;
        }

        map_fog_reveal_at_cell(state.map, player_id, building.cell, building_cell_size(building.type), 4);
    }

    // Finally remove from the remembered buildings map for any buildings that no longer exist
}

void map_update_fog(match_state_t& state, uint8_t player_id) {
    // First remember any revealed buildings
    for (uint32_t building_index = 0; building_index < state.buildings.size(); building_index++) {
        building_t& building = state.buildings[building_index];
        if (!map_is_cell_rect_revealed(state.map, player_id, rect_t(building.cell, building_cell_size(building.type)))) {
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
        if (!map_is_cell_rect_revealed(state.map, player_id, rect_t(mine.cell, xy(MINE_SIZE, MINE_SIZE)))) {
            continue;
        }
        state.remembered_mines[state.mines.get_id_of(mine_index)] = (mine_t) {
            .cell = mine.cell,
            .gold_left = mine.gold_left,
            .is_occupied = mine.is_occupied
        };
    }

    // Now dim anything that is revealed
    for (uint32_t i = 0; i < state.map.width * state.map.height; i++) {
        if (state.map.player_fog[player_id][i] == FOG_REVEALED) {
            state.map.player_fog[player_id][i] = FOG_EXPLORED;
        }
    }

    map_fog_reveal(state, player_id);

    // Note the buildings are only removed from the list if the player can see where the building once was
    auto it = state.remembered_buildings.begin();
    while (it != state.remembered_buildings.end()) {
        if (state.buildings.get_index_of(it->first) == INDEX_INVALID && map_is_cell_rect_revealed(state.map, player_id, rect_t(it->second.cell, building_cell_size(it->second.type)))) {
            log_trace("deleting remembered building");
            it = state.remembered_buildings.erase(it);
        } else {
            it++;
        }
    }
}

// INPUT

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_ATTACK_MOVE:
        case INPUT_BLIND_MOVE:
        case INPUT_MOVE_UNIT:
        case INPUT_MOVE_BUILDING:
        case INPUT_MOVE_MINE:
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
            memcpy(out_buffer + out_buffer_length, &input.build.unit_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.build.unit_ids, input.build.unit_count * sizeof(entity_id));
            out_buffer_length += input.build.unit_count * sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.build.building_type, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);
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
        case INPUT_UNLOAD_ALL: {
            memcpy(out_buffer + out_buffer_length, &input.unload_all.unit_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, input.unload_all.unit_ids, input.unload_all.unit_count * sizeof(entity_id));
            out_buffer_length += input.unload_all.unit_count * sizeof(entity_id);
            break;
        }
        case INPUT_RALLY: {
            memcpy(out_buffer + out_buffer_length, &input.rally.building_id, sizeof(entity_id));
            out_buffer_length += sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.rally.rally_point, sizeof(xy));
            out_buffer_length += sizeof(xy);
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
        case INPUT_MOVE_UNIT:
        case INPUT_MOVE_BUILDING:
        case INPUT_MOVE_MINE:
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
            memcpy(&input.build.unit_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.build.unit_ids, in_buffer + in_buffer_head, input.build.unit_count * sizeof(entity_id));
            in_buffer_head += input.build.unit_count * sizeof(entity_id);

            memcpy(&input.build.building_type, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);
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
        case INPUT_UNLOAD_ALL: {
            memcpy(&input.unload_all.unit_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(input.unload_all.unit_ids, in_buffer + in_buffer_head, input.unload_all.unit_count * sizeof(entity_id));
            in_buffer_head += input.unload_all.unit_count * sizeof(entity_id);
            break;
        }
        case INPUT_RALLY: {
            memcpy(&input.rally.building_id, in_buffer + in_buffer_head, sizeof(entity_id));
            in_buffer_head += sizeof(entity_id);

            memcpy(&input.rally.rally_point, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);
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
        case INPUT_MOVE_UNIT:
        case INPUT_MOVE_BUILDING:
        case INPUT_MOVE_MINE:
        case INPUT_MOVE: {
            // If we tried to move towards a unit, try and find the unit
            uint32_t target_index;
            if (input.type == INPUT_MOVE_UNIT) {
                target_index = state.units.get_index_of(input.move.target_entity_id);
                if (target_index != INDEX_INVALID && state.units[target_index].health == 0) {
                    target_index = INDEX_INVALID;
                }
            } else if (input.type == INPUT_MOVE_BUILDING) {
                target_index = state.buildings.get_index_of(input.move.target_entity_id);
                if (target_index != INDEX_INVALID && state.buildings[target_index].health == 0) {
                    target_index = INDEX_INVALID;
                }
            } else if (input.type == INPUT_MOVE_MINE) {
                target_index = state.mines.get_index_of(input.move.target_entity_id);
            } else {
                target_index = INDEX_INVALID;
            }

            bool should_move_as_group = target_index == INDEX_INVALID && input.move.unit_count > 1 && map_get_cell(state.map, input.move.target_cell).type == CELL_EMPTY;
            xy group_center;

            if (should_move_as_group) {
                // Collect a list of all the unit cells
                std::vector<xy> unit_cells;
                unit_cells.reserve(input.move.unit_count);
                for (uint32_t i = 0; i < input.move.unit_count; i++) {
                    uint32_t unit_index = state.units.get_index_of(input.move.unit_ids[i]);
                    if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0) {
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
                if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0) {
                    continue;
                }
                unit_t& unit = state.units[unit_index];

                // Determine the unit's target
                xy unit_target = input.move.target_cell;
                if (should_move_as_group) {
                    xy group_move_target = input.move.target_cell + (unit.cell - group_center);
                    if (map_is_cell_in_bounds(state.map, group_move_target) && xy::manhattan_distance(group_move_target, input.move.target_cell) <= 3) {
                        unit_target = group_move_target;
                    }
                } else if (target_index != INDEX_INVALID) {
                    unit_target = input.move.target_cell;
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
                } else if (input.type == INPUT_MOVE_UNIT && target_index != INDEX_INVALID && unit_index != target_index) {
                    unit_set_target(state, unit, (unit_target_t) {
                        .type = UNIT_TARGET_UNIT,
                        .id = input.move.target_entity_id
                    });
                } else if (input.type == INPUT_MOVE_BUILDING && target_index != INDEX_INVALID) {
                    if (state.buildings[target_index].type == BUILDING_CAMP && building_is_finished(state.buildings[target_index]) && unit.gold_held > 0) {
                        unit_set_target(state, unit, (unit_target_t) {
                            .type = UNIT_TARGET_CAMP,
                            .id = input.move.target_entity_id
                        });
                    } else {
                        unit_set_target(state, unit, (unit_target_t) {
                            .type = UNIT_TARGET_BUILDING,
                            .id = input.move.target_entity_id
                        });
                    } 
                } else if (input.type == INPUT_MOVE_MINE && unit.type == UNIT_MINER) {
                    unit_set_target(state, unit, (unit_target_t) {
                        .type = UNIT_TARGET_MINE,
                        .id = input.move.target_entity_id
                    });
                } else {
                    unit_set_target(state, unit, (unit_target_t) {
                        .type = UNIT_TARGET_CELL,
                        .cell = unit_target
                    });
                }
                // End determine unit target
            }
            break;
        }
        case INPUT_STOP: {
            for (uint32_t i = 0; i < input.stop.unit_count; i++) {
                uint32_t unit_index = state.units.get_index_of(input.stop.unit_ids[i]);
                if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0) {
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
            uint32_t unit_index = state.units.get_index_of(input.build.unit_ids[0]);
            if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0) {
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

            for (uint32_t index = 1; index < input.build.unit_count; index++) {
                uint32_t helper_index = state.units.get_index_of(input.build.unit_ids[index]);
                if (helper_index == INDEX_INVALID || state.units[helper_index].health == 0) {
                    return;
                }
                unit_t& helper = state.units[helper_index];
                unit_set_target(state, helper, (unit_target_t) {
                    .type = UNIT_TARGET_BUILD_ASSIST,
                    .id = input.build.unit_ids[0]
                });
            }
            break;
        }
        case INPUT_BUILD_CANCEL: {
            uint32_t building_index = state.buildings.get_index_of(input.build_cancel.building_id);
            if (building_index == INDEX_INVALID || state.buildings[building_index].health == 0) {
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
            building.health = 0;
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            uint32_t building_index = state.buildings.get_index_of(input.building_enqueue.building_id);
            if (building_index == INDEX_INVALID || state.buildings[building_index].health == 0) {
                return;
            }
            if (state.player_gold[player_id] < building_queue_item_cost(input.building_enqueue.item)) {
                return;
            }
            if (state.buildings[building_index].queue.size() == BUILDING_QUEUE_MAX) {
                return;
            }

            state.player_gold[player_id] -= building_queue_item_cost(input.building_enqueue.item);
            building_enqueue(state, state.buildings[building_index], input.building_enqueue.item);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state.buildings.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID || state.buildings[building_index].health == 0) {
                return;
            }
            building_t& building = state.buildings[building_index];
            if (building.queue.empty()) {
                return;
            }

            uint32_t index = input.building_dequeue.index != BUILDING_QUEUE_MAX 
                                ? input.building_dequeue.index 
                                : building.queue.size() - 1;
            if (index == 0) {
                state.player_gold[player_id] += building_queue_item_cost(building.queue[0]);
                building_dequeue(state, building);
            } else {
                state.player_gold[player_id] += building_queue_item_cost(building.queue[index]);
                building.queue.erase(building.queue.begin() + index);
            }

            break;
        }
        case INPUT_UNLOAD_ALL: {
            log_trace("Handling unloading. unit count %u", input.unload_all.unit_count);
            for (uint16_t i = 0; i < input.unload_all.unit_count; i++) {
                entity_id id = input.unload_all.unit_ids[i];

                uint32_t unit_index = state.units.get_index_of(id);
                log_trace("Unloading for id of %u with index of %u", id, unit_index);
                if (unit_index == INDEX_INVALID) {
                    continue;
                }

                unit_t& unit = state.units[unit_index];
                log_trace("Units to unload: %z", unit.ferried_units.size());
                if (unit.ferried_units.empty()) {
                    continue;
                }

                log_trace("Unit cell is %xi", &unit.cell);
                for (uint32_t ferried_id_index = 0; ferried_id_index < unit.ferried_units.size(); ferried_id_index++) {
                    entity_id ferried_id = unit.ferried_units[ferried_id_index];
                    log_trace("Attempt unload for unit ferried id index %u ferried id %u", ferried_id_index, ferried_id);
                    unit_t& ferried_unit = state.units.get_by_id(ferried_id);
                    xy dropoff_cell = unit_get_best_unload_cell(state, unit, unit_cell_size(ferried_unit.type));
                    log_trace("Dropoff cell %xi", &dropoff_cell);
                    // If this is true, then no free spaces are available to unload
                    if (dropoff_cell == xy(-1, -1)) {
                        log_trace("No free cells!");
                        break;
                    }

                    ferried_unit.cell = dropoff_cell;
                    ferried_unit.position = unit_get_target_position(ferried_unit.type, ferried_unit.cell);
                    map_set_cell_rect(state.map, rect_t(ferried_unit.cell, unit_cell_size(ferried_unit.type)), CELL_UNIT, ferried_id);
                    ferried_unit.mode = UNIT_MODE_IDLE;
                    ferried_unit.target = (unit_target_t) { .type = UNIT_TARGET_NONE };
                    unit.ferried_units.erase(unit.ferried_units.begin() + ferried_id_index);
                    ferried_id_index--;
                }
            }
            break;
        }
        case INPUT_RALLY: {
            uint32_t building_index = state.buildings.get_index_of(input.rally.building_id);
            if (building_index == INDEX_INVALID || state.buildings[building_index].health == 0) {
                return;
            }
            state.buildings[building_index].rally_point = input.rally.rally_point;
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

xy get_first_empty_cell_around_rect(const match_state_t& state, xy cell_size, rect_t rect, xy preferred_cell) {
    // Setup search
    xy search_corners[4] = { 
        rect.position - cell_size,
        rect.position + xy(rect.size.x, -cell_size.y),
        rect.position + rect.size,
        rect.position + xy(-cell_size.x, rect.size.y)
    };
    
    // Determine cell start and step direction based on exit direction
    if (preferred_cell == xy(-1, -1)) {
        preferred_cell = rect.position + xy(0, rect.size.y);
    }
    rect_t cell_rect = rect_t(preferred_cell, cell_size);
    int step_direction = DIRECTION_SOUTH;
    Direction exit_direction;
    if (cell_rect.position.x >= rect.position.x && cell_rect.position.y == rect.position.y + rect.size.y) {
        exit_direction = DIRECTION_SOUTH;
        step_direction = DIRECTION_WEST;
    } else if (cell_rect.position.x < rect.position.x && cell_rect.position.y >= rect.position.y) {
        exit_direction = DIRECTION_WEST;
        step_direction = DIRECTION_NORTH;
    } else if (cell_rect.position.x >= rect.position.x && cell_rect.position.y < rect.position.y) {
        exit_direction = DIRECTION_NORTH;
        step_direction = DIRECTION_EAST;
    } else {
        exit_direction = DIRECTION_EAST;
        step_direction = DIRECTION_SOUTH;
    }
    xy start_cell = cell_rect.position;

    // Step along search until we find an empty cell
    while (!map_is_cell_rect_in_bounds(state.map, cell_rect) || map_is_cell_rect_blocked(state.map, cell_rect)) {
        cell_rect.position += DIRECTION_XY[step_direction];
        if (cell_rect.position == search_corners[step_direction / 2]) {
            step_direction = (step_direction + 2) % DIRECTION_COUNT;
        } else if (cell_rect.position == start_cell) {
            cell_rect.position += DIRECTION_XY[exit_direction];
            start_cell = cell_rect.position;
            search_corners[0] += xy(-1, -1);
            search_corners[1] += xy(1, -1);
            search_corners[2] += xy(1, 1);
            search_corners[3] += xy(-1, 1);
        }
    }

    return cell_rect.position;
}

// Returns the nearest cell around the rect relative to start_cell
// If there are no free cells around the rect in a radius of 1, then this returns the start cell
xy get_nearest_cell_around_rect(const match_state_t& state, rect_t start, rect_t rect, bool allow_blocked_cells) {
    xy nearest_cell;
    int nearest_cell_dist = -1;

    xy cell_begin[4] = { 
        rect.position + xy(-start.size.x, -(start.size.y - 1)),
        rect.position + xy(-(start.size.x - 1), rect.size.y),
        rect.position + xy(rect.size.x, rect.size.y - 1),
        rect.position + xy(rect.size.x - 1, -start.size.y)
    };
    xy cell_end[4] = { 
        xy(cell_begin[0].x, rect.position.y + rect.size.y - 1),
        xy(rect.position.x + rect.size.x - 1, cell_begin[1].y),
        xy(cell_begin[2].x, cell_begin[0].y),
        xy(cell_begin[0].x + 1, cell_begin[3].y)
    };
    xy cell_step[4] = { xy(0, 1), xy(1, 0), xy(0, -1), xy(-1, 0) };
    uint32_t index = 0;
    xy cell = cell_begin[index];
    while (index < 4) {
        if (map_is_cell_rect_in_bounds(state.map, rect_t(cell, start.size))) {
            if ((allow_blocked_cells || !map_is_cell_rect_blocked(state.map, rect_t(cell, start.size))) && (nearest_cell_dist == -1 || xy::manhattan_distance(start.position, cell) < nearest_cell_dist)) {
                nearest_cell = cell;
                nearest_cell_dist = xy::manhattan_distance(start.position, cell);
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

    return nearest_cell_dist != -1 ? nearest_cell : start.position;
}

rect_t mine_get_rect(xy mine_cell) {
    return rect_t(mine_cell * TILE_SIZE, xy(3, 3) * TILE_SIZE);
}

rect_t mine_get_block_building_rect(xy mine_cell) {
    return rect_t(mine_cell - xy(3, 3), xy(9, 9));
}

uint32_t mine_get_worker_count(const match_state_t& state, entity_id mine_id, uint8_t player_id) {
    const mine_t& mine = state.mines.get_by_id(mine_id);
    const unit_target_t camp_target = unit_target_nearest_camp(state, mine.cell + xy(1, 1), player_id);

    uint32_t worker_count = 0;
    for (const unit_t& unit : state.units) {
        if (unit.player_id != player_id) {
            continue;
        }
        if (unit.target.type == UNIT_TARGET_MINE && unit.target.id == mine_id) {
            worker_count++;
            continue;
        }
        if (unit.target.type == UNIT_TARGET_CAMP && camp_target.type == UNIT_TARGET_CAMP && unit.target.id == camp_target.id) {
            worker_count++;
            continue;
        }
    }

    return worker_count;
}
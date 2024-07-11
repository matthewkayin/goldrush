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

    // Init UI
    ui_mode = UI_MODE_NONE;
    camera_offset = ivec2(0, 0);
    // Init ui buttons
    ivec2 ui_button_padding = ivec2(4, 6);
    ivec2 ui_button_top_left = ivec2(SCREEN_WIDTH - 132 + 14, SCREEN_HEIGHT - UI_HEIGHT + 10);
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 3; x++) {
            int index = x + (y * 3);
            ui_buttons[index].enabled = index < 5;
            ui_buttons[index].icon = (ButtonIcon)x;
            ui_buttons[index].rect = rect_t(ui_button_top_left + ivec2((32 + ui_button_padding.x) * x, (32 + ui_button_padding.y) * y), ivec2(32, 32));
        }
    }
    ui_buttons[3].icon = BUTTON_ICON_BUILD;
    ui_buttons[4].icon = BUTTON_ICON_BUILD_HOUSE;
    ui_button_hovered = -1;
    ui_refresh_buttons();

    // Init map
    map_width = 64;
    map_height = 64;
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
        unit_create(player_id, spawn_point + ivec2(-1, -1));
        unit_create(player_id, spawn_point + ivec2(1, -1));
        unit_create(player_id, spawn_point + ivec2(0, 1));

        log_info("player %u spawn %vi", player_id, &spawn_point);
    }
    log_info("players initialized");

    // Init resources
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        player_gold[player_id] = 150;
    }

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
                // Deserialize input
                std::vector<input_t> tick_inputs;

                uint8_t* in_buffer = network_event.input.data;
                size_t in_buffer_length = network_event.input.data_length;

                uint8_t in_player_id = in_buffer[1];
                size_t in_buffer_head = 2;

                while (in_buffer_head < in_buffer_length) {
                    tick_inputs.push_back(input_deserialize(in_buffer, in_buffer_head));
                }

                inputs[in_player_id].push_back(tick_inputs);
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

        // Flush input
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
            input_serialize(out_buffer, out_buffer_length, input);
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
    tick_timer--;

    ivec2 mouse_pos = input_get_mouse_position();
    uint8_t current_player_id = network_get_player_id();

    // CAMERA DRAG
    if (ui_mode != UI_MODE_SELECTING && ui_mode != UI_MODE_MINIMAP_DRAG) {
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
    ivec2 mouse_world_pos = mouse_pos + camera_offset;
    bool is_mouse_in_ui = (mouse_pos.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
                            (mouse_pos.x <= 136 && mouse_pos.y >= SCREEN_HEIGHT - 136) ||
                            (mouse_pos.x >= SCREEN_WIDTH - 132 && mouse_pos.y >= SCREEN_HEIGHT - 106);

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

    // Building placement
    if (ui_mode == UI_MODE_BUILDING_PLACE) {
        if (is_mouse_in_ui) {
            ui_building_cell = ivec2(-1, -1);
        } else {
            ui_building_cell = mouse_world_pos / TILE_SIZE;
        }
    }

    // LEFT MOUSE CLICK
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        if (ui_button_hovered != -1) {
            ui_handle_button_pressed(ui_buttons[ui_button_hovered].icon);
        } else if (ui_mode == UI_MODE_BUILDING_PLACE) {
            if (!is_mouse_in_ui) {
                ivec2 building_cell_size = ivec2(building_data.at(ui_building_type).cell_width, building_data.at(ui_building_type).cell_height);
                bool building_can_be_placed = ui_building_cell.x + building_cell_size.x < map_width + 1 && 
                                              ui_building_cell.y + building_cell_size.y < map_height + 1 &&
                                              !cell_is_blocked(ui_building_cell, building_cell_size);
                if (building_can_be_placed) {
                    input_t input;
                    input.type = INPUT_BUILD;
                    input.build.building_type = ui_building_type;
                    input.build.target_cell = ui_building_cell;
                    for (uint8_t unit_id = units[current_player_id].first(); unit_id != units[current_player_id].end(); units[current_player_id].next(unit_id)) {
                        if (units[current_player_id][unit_id].is_selected) {
                            input.build.unit_id = unit_id;
                            break;
                        }
                    }
                    log_info("ordering build for unit %u", input.build.unit_id);
                    input_queue.push_back(input);

                    ui_mode = UI_MODE_MINER;
                }
            }
        } else if (MINIMAP_RECT.has_point(mouse_pos)) {
            ui_mode = UI_MODE_MINIMAP_DRAG;
        } else if (!is_mouse_in_ui) {
            // On begin selecting
            ui_mode = UI_MODE_SELECTING;
            select_origin = mouse_world_pos;
        } 
    }
    // SELECT RECT
    if (ui_mode == UI_MODE_SELECTING) {
        // Update select rect
        select_rect.position = ivec2(std::min(select_origin.x, mouse_world_pos.x), std::min(select_origin.y, mouse_world_pos.y));
        select_rect.size = ivec2(std::max(1, std::abs(select_origin.x - mouse_world_pos.x)), std::max(1, std::abs(select_origin.y - mouse_world_pos.y)));

        // Update unit selection
        for (uint8_t unit_id = units[current_player_id].first(); unit_id != units[current_player_id].end(); units[current_player_id].next(unit_id)) {
            unit_t& unit = units[current_player_id][unit_id];
            unit.is_selected = unit_get_rect(unit).intersects(select_rect);
        }
    } else if (ui_mode == UI_MODE_MINIMAP_DRAG) {
        ivec2 minimap_pos = mouse_pos - MINIMAP_RECT.position;
        minimap_pos.x = std::clamp(minimap_pos.x, 0, MINIMAP_RECT.size.x);
        minimap_pos.y = std::clamp(minimap_pos.y, 0, MINIMAP_RECT.size.y);
        ivec2 map_pos = ivec2((map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.size.x, (map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.size.y);
        camera_move_to_cell(map_pos / TILE_SIZE);
    }
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        if (ui_mode == UI_MODE_SELECTING) {
            ui_on_selection_changed();
        } else if (ui_mode == UI_MODE_MINIMAP_DRAG) {
            ui_mode = UI_MODE_NONE;
        }
    } 

    // Right-click move
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_RIGHT) && (ui_mode != UI_MODE_SELECTING && ui_mode != UI_MODE_MINIMAP_DRAG && ui_mode != UI_MODE_BUILDING_PLACE)) {
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
            for (uint8_t unit_id = units[current_player_id].first(); unit_id != units[current_player_id].end(); units[current_player_id].next(unit_id)) {
                if (!units[current_player_id][unit_id].is_selected) {
                    continue;
                }
                input.move.unit_ids[input.move.unit_count] = unit_id;
                input.move.unit_count++;
            }
            log_info("selected unit count %i", input.move.unit_count);
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
        for (uint8_t unit_id = units[player_id].first(); unit_id != units[player_id].end(); units[player_id].next(unit_id)) {
            unit_update(player_id, units[player_id][unit_id]);
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

            for (uint8_t i = 0; i < input.move.unit_count; i++) {
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
                if (cell_is_blocked(unit_target)) {
                    continue;
                }
                // Path to the target
                unit.path = pathfind(unit.cell, unit_target);
                unit.order_type = ORDER_MOVE;
            }
            break;
        } // End case INPUT_MOVE
        case INPUT_STOP: {
            for (uint8_t i = 0; i < input.stop.unit_count; i++) {
                unit_t& unit = units[player_id][input.stop.unit_ids[i]];

                unit.path.clear();
            }
            break;
        }
        case INPUT_BUILD: {
            unit_t& unit = units[player_id][input.build.unit_id];

            // Determine the unit's target
            ivec2 nearest_cell = input.build.target_cell;
            int nearest_cell_dist = ivec2::manhattan_distance(unit.cell, nearest_cell);
            for (int y = input.build.target_cell.y; y < input.build.target_cell.y + building_data.at(input.build.building_type).cell_height; y++) {
                for (int x = input.build.target_cell.x; x < input.build.target_cell.x + building_data.at(input.build.building_type).cell_width; x++) {
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

            unit.path = pathfind(unit.cell, nearest_cell);
            unit.order_type = ORDER_BUILD;
            unit.order_building_type = (BuildingType)input.build.building_type;
            unit.order_cell = input.build.target_cell;
        }
        default:
            break;
    }
}

// UI

void match_t::ui_on_selection_changed() {
    uint8_t current_player_id = network_get_player_id();

    uint32_t unit_count = 0;
    for (uint8_t unit_id = units[current_player_id].first(); unit_id != units[current_player_id].end(); units[current_player_id].next(unit_id)) {
        unit_t& unit = units[current_player_id][unit_id];
        if (!unit.is_selected) {
            continue;
        }
        unit_count++;
    }

    if (unit_count == 0) {
        ui_mode = UI_MODE_NONE;
    } else if (unit_count == 1) {
        ui_mode = UI_MODE_MINER;
    } else {
        ui_mode = UI_MODE_NONE;
    }
    ui_refresh_buttons();
}

void match_t::ui_refresh_buttons() {
    switch (ui_mode) {
        case UI_MODE_MINER: {
            for (uint32_t i = 0; i < 6; i++) {
                ui_buttons[i].enabled = i < 4;
            }
            ui_buttons[0].icon = BUTTON_ICON_MOVE;
            ui_buttons[1].icon = BUTTON_ICON_STOP;
            ui_buttons[2].icon = BUTTON_ICON_ATTACK;
            ui_buttons[3].icon = BUTTON_ICON_BUILD;

            break;
        }
        case UI_MODE_BUILD: {
            for (uint32_t i = 0; i < 6; i++) {
                ui_buttons[i].enabled = i == 0 || i == 5;
            }
            ui_buttons[0].icon = BUTTON_ICON_BUILD_HOUSE;
            ui_buttons[5].icon = BUTTON_ICON_CANCEL;
            
            break;
        }
        case UI_MODE_BUILDING_PLACE: {
            for (uint32_t i = 0; i < 6; i++) {
                ui_buttons[i].enabled = i == 5;
            }
            ui_buttons[5].icon = BUTTON_ICON_CANCEL;
            break;
        }
        case UI_MODE_NONE:
        default: {
            for (uint32_t i = 0; i < 6; i++) {
                ui_buttons[i].enabled = false;
            }
        }
            break;
    }
}

void match_t::ui_handle_button_pressed(ButtonIcon icon) {
    switch (icon) {
        case BUTTON_ICON_STOP: {
            input_t input;
            input.type = INPUT_STOP;
            input.stop.unit_count = 0;
            uint8_t player_id = network_get_player_id();
            for (uint8_t unit_id = units[player_id].first(); unit_id != units[player_id].end(); units[player_id].next(unit_id)) {
                if (!units[player_id][unit_id].is_selected) {
                    continue;
                }
                input.stop.unit_ids[input.stop.unit_count] = unit_id;
                input.stop.unit_count++;
            }
            if (input.stop.unit_count != 0) {
                input_queue.push_back(input);
            }
            break;
        }
        case BUTTON_ICON_BUILD: {
            ui_mode = UI_MODE_BUILD;
            ui_refresh_buttons();
            break;
        }
        case BUTTON_ICON_CANCEL: {
            if (ui_mode == UI_MODE_BUILD) {
                ui_mode = UI_MODE_MINER;
            } else if (ui_mode == UI_MODE_BUILDING_PLACE) {
                ui_mode = UI_MODE_BUILD;
            }
            ui_refresh_buttons();
            break;
        }
        case BUTTON_ICON_BUILD_HOUSE: {
            ui_mode = UI_MODE_BUILDING_PLACE;
            ui_building_type = BUILDING_HOUSE;
            ui_building_cell = ivec2(-1, -1);
            ui_refresh_buttons();
            break;
        }
        default:
            break;
    }
}

void match_t::camera_clamp() {
    camera_offset.x = std::clamp(camera_offset.x, 0, (map_width * TILE_SIZE) - SCREEN_WIDTH);
    camera_offset.y = std::clamp(camera_offset.y, 0, (map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT);
}

void match_t::camera_move_to_cell(ivec2 cell) {
    camera_offset = ivec2((cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2), (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2));
    camera_clamp();
}

// CELL HELPERS

bool match_t::cell_is_in_bounds(ivec2 cell) const {
    return !(cell.x < 0 || cell.y < 0 || cell.x >= map_width || cell.y >= map_height);
}

bool match_t::cell_is_blocked(ivec2 cell) const {
    return map_cells[cell.x + (cell.y * map_width)] != CELL_EMPTY;
}

bool match_t::cell_is_blocked(ivec2 cell, ivec2 cell_size) const {
    for (int y = cell.y; y < cell.y + cell_size.y; y++) {
        for (int x = cell.x; x < cell.x + cell_size.x; x++) {
            if (map_cells[x + (y * map_width)] != CELL_EMPTY) {
                return true;
            }
        }
    }

    return false;
}

void match_t::cell_set_value(ivec2 cell, int value) {
    map_cells[cell.x + (cell.y * map_width)] = value;
}

void match_t::cell_set_value(ivec2 cell, ivec2 cell_size, int value) {
    for (int y = cell.y; y < cell.y + cell_size.y; y++) {
        for (int x = cell.x; x < cell.x + cell_size.x; x++) {
            map_cells[x + (y * map_width)] = value;
        }
    }
}

// UNITS

void match_t::unit_create(uint8_t player_id, ivec2 cell) {
    unit_t unit;
    unit.is_selected = false;
    unit.mode = UNIT_MODE_IDLE;
    unit.cell = cell;
    unit.position = cell_center_position(cell);
    unit.direction = DIRECTION_SOUTH;
    units[player_id].push_back(unit);

    cell_set_value(cell, CELL_FILLED);
}

rect_t match_t::unit_get_rect(unit_t& unit) const {
    return rect_t(ivec2(unit.position.x.integer_part(), unit.position.y.integer_part()), ivec2(16, 16));
}

void match_t::unit_try_step(unit_t& unit) {
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
        unit.path_timer.start(unit_t::PATH_PAUSE_DURATION);
        return;
    }

    // Set state to begin movement
    cell_set_value(unit.cell, CELL_EMPTY);
    unit.cell = next_point;
    cell_set_value(unit.cell, CELL_FILLED);
    unit.target_position = cell_center_position(unit.cell);
    unit.path.erase(unit.path.begin());
    unit.mode = UNIT_MODE_STEP;
    unit.path_timer.stop();
}

void match_t::unit_update(uint8_t player_id, unit_t& unit) {
    // If we're not stepping but we have a path to follow, then try to move
    if (unit.mode != UNIT_MODE_STEP && !unit.path.empty()) {
        unit_try_step(unit);
    }
    // Update step
    if (unit.mode == UNIT_MODE_STEP) {
        fixed movement_left = fixed::from_int(1);
        while (movement_left.raw_value > 0) {
            fixed distance_to_target = unit.position.distance_to(unit.target_position);
            if (distance_to_target > movement_left) {
                unit.position += DIRECTION_VEC2[unit.direction] * movement_left;
                movement_left = 0;
            } else {
                unit.position = unit.target_position;
                unit.mode = UNIT_MODE_IDLE;
                movement_left -= distance_to_target;

                if (!unit.path.empty()) {
                    unit_try_step(unit);
                } else {
                    // on finished movement
                    if (unit.order_type == ORDER_MOVE) {
                        unit.order_type = ORDER_NONE;
                    } else if (unit.order_type == ORDER_BUILD) {
                        log_info("unit creating building.");
                        unit.building_id = building_create(player_id, unit.order_building_type, unit.order_cell);
                        unit.mode = UNIT_MODE_BUILD;
                        unit.is_selected = false;
                        ui_on_selection_changed();
                    }
                }

                if (unit.mode != UNIT_MODE_STEP) {
                    movement_left.raw_value = 0;
                }
            }
        }
    // Update path timer
    } else if (!unit.path_timer.is_stopped) {
        unit.path_timer.update();
        if (unit.path_timer.is_finished) {
            unit.path_timer.stop();
            ivec2 unit_target = unit.path[unit.path.size() - 1];
            unit.path.clear();
            if (!cell_is_blocked(unit_target)) {
                unit.path = pathfind(unit.cell, unit_target);
            } else {
                unit.order_type = ORDER_NONE;
            }
        }
    // Update building
    } else if (unit.mode == UNIT_MODE_BUILD) {
        building_t& building = buildings[player_id][unit.building_id];

        building.health++;
        if (building.health >= building_data.at(building.type).max_health) {
            building.health = building_data.at(building.type).max_health;
            building.is_finished = true;
        }
        if (building.is_finished) {
            // On building finished
            log_info("building finished");
            unit.cell = building_get_nearest_free_cell(building);
            cell_set_value(unit.cell, CELL_FILLED);
            unit.position = cell_center_position(unit.cell);
            unit.mode = UNIT_MODE_IDLE;
            unit.order_type = ORDER_NONE;
        }
    }

    // Update animation
    unit.animation.update();
    if (unit.mode == UNIT_MODE_STEP) {
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

// BUILDINGS

uint8_t match_t::building_create(uint8_t player_id, BuildingType type, ivec2 cell) {
    log_info("building create called");
    auto it = building_data.find(type);
    GOLD_ASSERT(it != building_data.end());

    building_t building;
    building.cell = cell;
    building.type = type;
    building.health = 3;
    building.is_finished = false;

    cell_set_value(cell, ivec2(it->second.cell_width, it->second.cell_height), CELL_FILLED);
    return buildings[player_id].push_back(building);
}

ivec2 match_t::building_get_nearest_free_cell(building_t& building) const {
    ivec2 cell = building.cell + ivec2(-1, 0);
    bool cell_is_valid = cell_is_in_bounds(cell) && !cell_is_blocked(cell);

    int step_count = 0;
    int x_step_amount = building_data.at(building.type).cell_width + 1;
    int y_step_amount = building_data.at(building.type).cell_height;
    Direction step_direction = DIRECTION_SOUTH;

    while (!cell_is_valid) {
        cell += DIRECTION_IVEC2[step_direction];
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

        cell_is_valid = cell_is_in_bounds(cell) && !cell_is_blocked(cell);
    }

    return cell;
}
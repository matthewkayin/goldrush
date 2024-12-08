#include "match.h"

#include "engine.h"
#include "network.h"
#include "logger.h"
#include <algorithm>

static const uint32_t TURN_DURATION = 4;
static const uint32_t TURN_OFFSET = 4;
static const uint32_t MATCH_DISCONNECT_GRACE = 10;

static const int CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_DRAG_SPEED = 16;

static const SDL_Rect UI_DISCONNECT_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 100, .y = 32,
    .w = 200, .h = 200
};
static const SDL_Rect UI_MATCH_OVER_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (250 / 2), .y = 128,
    .w = 250, .h = 60
};
static const SDL_Rect UI_MATCH_OVER_EXIT_BUTTON_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 32, .y = UI_MATCH_OVER_FRAME_RECT.y + 32,
    .w = 63, .h = 21
};
static const SDL_Rect UI_MENU_BUTTON_RECT = (SDL_Rect) {
    .x = 1, .y = 1, .w = 19, .h = 18
};
static const SDL_Rect UI_MENU_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (150 / 2), .y = 64,
    .w = 150, .h = 100
};
static const xy UI_FRAME_BOTTOM_POSITION = xy(136, SCREEN_HEIGHT - UI_HEIGHT);
static const xy BUILDING_QUEUE_TOP_LEFT = xy(164, 12);
static const xy UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX] = {
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT,
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(0, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36 * 2, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36 * 3, 35)
};

static const std::unordered_map<UiButton, SDL_Keycode> hotkeys = {
    { UI_BUTTON_STOP, SDLK_s },
    { UI_BUTTON_ATTACK, SDLK_a },
    { UI_BUTTON_DEFEND, SDLK_d },
    { UI_BUTTON_BUILD, SDLK_b },
    { UI_BUTTON_REPAIR, SDLK_r },
    { UI_BUTTON_CANCEL, SDLK_ESCAPE },
    { UI_BUTTON_UNLOAD, SDLK_x },
    { UI_BUTTON_BUILD_HOUSE, SDLK_e },
    { UI_BUTTON_BUILD_CAMP, SDLK_c },
    { UI_BUTTON_BUILD_SALOON, SDLK_s },
    { UI_BUTTON_BUILD_BUNKER, SDLK_b },
    { UI_BUTTON_UNIT_MINER, SDLK_r },
    { UI_BUTTON_UNIT_MINER, SDLK_e },
    { UI_BUTTON_UNIT_COWBOY, SDLK_c },
    { UI_BUTTON_UNIT_WAGON, SDLK_w },
    { UI_BUTTON_UNIT_BANDIT, SDLK_b }
};

match_state_t match_init() {
    match_state_t state;

    state.ui_mode = UI_MODE_MATCH_NOT_STARTED;
    state.ui_status_timer = 0;
    state.ui_buttonset = UI_BUTTONSET_NONE;

    map_init(state, 64, 64);

    Direction spawn_directions[MAX_PLAYERS] = { DIRECTION_NORTHWEST, DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST };
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        // Init input queues
        input_t empty_input;
        empty_input.type = INPUT_NONE;
        std::vector<input_t> empty_input_list = { empty_input };
        for (uint8_t i = 0; i < TURN_OFFSET - 1; i++) {
            state.inputs[player_id].push_back(empty_input_list);
        }

        // Determine player spawn
        xy player_spawn = xy(state.map_width / 2, state.map_height / 2) + (DIRECTION_XY[spawn_directions[player_id]] * ((state.map_width / 2) - 16));
        if (player_id == network_get_player_id()) {
            match_camera_center_on_cell(state, player_spawn);
        }
        entity_id camp_id = entity_create(state, BUILDING_CAMP, player_id, player_spawn);
        entity_t& camp = state.entities.get_by_id(camp_id);
        camp.health = ENTITY_DATA.at(camp.type).max_health;
        camp.mode = MODE_BUILDING_FINISHED;
        entity_create(state, UNIT_MINER, player_id, player_spawn + xy(-1, 0));
        entity_create(state, UNIT_MINER, player_id, player_spawn + xy(-1, 1));
    }
    for (uint32_t eindex = 0; eindex < state.entities.size(); eindex++) {
        log_trace("id %u type %u mode %u", state.entities.get_id_of(eindex), state.entities[eindex].type, state.entities[eindex].mode);
    }
    state.turn_timer = 0;
    state.ui_disconnect_timer = 0;

    network_toggle_ready();
    if (network_are_all_players_ready()) {
        log_info("Beginning singleplayer game.");
        state.ui_mode = UI_MODE_NONE;
    }

    return state;
}

void match_handle_input(match_state_t& state, SDL_Event event) {
    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED || state.ui_disconnect_timer > 0) {
        return;
    }

    // Match over button press
    if ((state.ui_mode == UI_MODE_MATCH_OVER_VICTORY || state.ui_mode == UI_MODE_MATCH_OVER_DEFEAT)) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && sdl_rect_has_point(UI_MATCH_OVER_EXIT_BUTTON_RECT, engine.mouse_position)) {
            network_disconnect();
            state.ui_mode = UI_MODE_LEAVE_MATCH;
        }
        return;
    }

    // Menu button press
    if (state.ui_mode == UI_MODE_MENU) {
        return;
    }

    // UI button press
    if (state.ui_button_pressed == -1 && event.type == SDL_MOUSEBUTTONDOWN && ui_get_ui_button_hovered(state) != -1) {
        state.ui_button_pressed = ui_get_ui_button_hovered(state);
        return;
    }
    // UI button hotkey press
    if (state.ui_button_pressed == -1 && event.type == SDL_KEYDOWN) {
        for (int button_index = 0; button_index < UI_BUTTONSET_SIZE; button_index++) {
            UiButton button = ui_get_ui_button(state, button_index);
            if (button == UI_BUTTON_NONE) {
                continue;
            }
            if (hotkeys.at(button) == event.key.keysym.sym) {
                ui_handle_ui_button_press(state, button);
                return;
            }
        }
    }

    // UI button release
    if (state.ui_button_pressed != -1 && 
        ((event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) || 
         (event.type == SDL_KEYUP && event.key.keysym.sym == hotkeys.at(ui_get_ui_button(state, state.ui_button_pressed))))) {
        UiButton button_pressed = ui_get_ui_button(state, state.ui_button_pressed);
        state.ui_button_pressed = -1;
        ui_handle_ui_button_press(state, button_pressed);
        return;
    }

    // Order movement
    if (event.type == SDL_MOUSEBUTTONDOWN && ui_get_selection_type(state) == SELECTION_TYPE_UNITS && 
            ((event.button.button == SDL_BUTTON_LEFT && ui_is_targeting(state)) ||
            (event.button.button == SDL_BUTTON_RIGHT && state.ui_mode == UI_MODE_NONE))) {
        input_t move_input = match_create_move_input(state);
        state.input_queue.push_back(move_input);

        if (move_input.type == INPUT_MOVE_REPAIR) {
            bool is_repair_target_valid = true;
            if (move_input.move.target_id == ID_NULL) {
                is_repair_target_valid = false;
            } else {
                const entity_t& repair_target = state.entities.get_by_id(move_input.move.target_id);
                if (repair_target.player_id != network_get_player_id() || !entity_is_building(repair_target.type)) {
                    is_repair_target_valid = false;
                }
            }

            if (!is_repair_target_valid) {
                ui_show_status(state, UI_STATUS_REPAIR_TARGET_INVALID);
                state.ui_mode = UI_MODE_NONE;
                ui_set_selection(state, state.selection);
                return;
            }
        }

        // Provide instant user feedback
        if (move_input.type == INPUT_MOVE_CELL || move_input.type == INPUT_MOVE_ATTACK_CELL || move_input.type == INPUT_MOVE_UNLOAD) {
            state.ui_move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
            state.ui_move_position = match_get_mouse_world_pos(state);
            state.ui_move_entity_id = ID_NULL;
        } else {
            state.ui_move_animation = animation_create(move_input.type == INPUT_MOVE_ATTACK_ENTITY ? ANIMATION_UI_MOVE_ATTACK_ENTITY : ANIMATION_UI_MOVE_ENTITY);
            state.ui_move_position = cell_center(move_input.move.target_cell).to_xy();
            state.ui_move_entity_id = move_input.move.target_id;
        }

        // Reset UI mode if targeting
        if (ui_is_targeting(state)) {
            state.ui_mode = UI_MODE_NONE;
            ui_set_selection(state, state.selection);
        }
        return;
    } 

    // Begin selecting
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (state.ui_mode == UI_MODE_NONE && !ui_is_mouse_in_ui()) {
            state.select_rect_origin = match_get_mouse_world_pos(state);
            state.ui_mode = UI_MODE_SELECTING;
        }
        return;
    } 

    // End selecting
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && state.ui_mode == UI_MODE_SELECTING) {
        state.ui_mode = UI_MODE_NONE;
        std::vector<entity_id> selection = ui_create_selection_from_rect(state);
        ui_set_selection(state, selection);
        return;
    }
}

void match_update(match_state_t& state) {
    network_service();

    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED && network_are_all_players_ready()) {
        log_trace("Match started.");
        state.ui_mode = UI_MODE_NONE;
    }
    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED) {
        return;
    }

    // Poll network events
    network_event_t network_event;
    while (network_poll_events(&network_event)) {
        switch (network_event.type) {
            case NETWORK_EVENT_INPUT: {
                // Deserialize input
                std::vector<input_t> tick_inputs;

                uint8_t* in_buffer = network_event.input.in_buffer;
                size_t in_buffer_head = 1;

                while (in_buffer_head < network_event.input.in_buffer_length) {
                    tick_inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
                }

                state.inputs[network_event.input.player_id].push_back(tick_inputs);
                break;
            }
            case NETWORK_EVENT_PLAYER_DISCONNECTED: {
                ui_add_chat_message(state, std::string(network_get_player(network_event.player_disconnected.player_id).name) + " disconnected.");

                // Determine if we should exit the match
                uint32_t player_count = 0;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (network_get_player(player_id).status != PLAYER_STATUS_NONE) {
                        player_count++;
                    }
                }
                if (player_count < 2) {
                    state.ui_mode = UI_MODE_MATCH_OVER_VICTORY;
                }
                break;
            }
            default: 
                break;
        }
    }

    // Turn loop
    if (state.turn_timer == 0) {
        bool all_inputs_received = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            // We didn't receive inputs, start the timer
            if (state.inputs[player_id].empty() || state.inputs[player_id][0].empty()) {
                all_inputs_received = false;
                log_trace("Inputs not received for player %u", player_id);
                continue;
            }
        }

        if (!all_inputs_received) {
            state.ui_disconnect_timer++;
            return;
        }

        // Reset the disconnect timer if we received inputs
        state.ui_disconnect_timer = 0;

        // All inputs received. Begin next turn
        // HANDLE INPUT
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            for (const input_t& input : state.inputs[player_id][0]) {
                match_input_handle(state, player_id, input);
            }
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        }

        state.turn_timer = TURN_DURATION;

        // FLUSH INPUT
        // Always send at least one input per tick
        if (state.input_queue.empty()) {
            input_t empty_input;
            empty_input.type = INPUT_NONE;
            state.input_queue.push_back(empty_input);
        }

        // Serialize the inputs
        uint8_t out_buffer[INPUT_BUFFER_SIZE];
        size_t out_buffer_length = 1; // Leaves space in the buffer for the network message type
        for (const input_t& input : state.input_queue) {
            match_input_serialize(out_buffer, out_buffer_length, input);
        }
        state.inputs[network_get_player_id()].push_back(state.input_queue);
        state.input_queue.clear();

        // Send them to other players
        network_send_input(out_buffer, out_buffer_length);
    } // End if tick timer is 0

    state.turn_timer--;

    // CAMERA DRAG
    if (state.ui_mode != UI_MODE_SELECTING && state.ui_mode != UI_MODE_MINIMAP_DRAG) {
        xy camera_drag_direction = xy(0, 0);
        if (engine.mouse_position.x < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (engine.mouse_position.x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (engine.mouse_position.y < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (engine.mouse_position.y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        state.camera_offset += camera_drag_direction * CAMERA_DRAG_SPEED;
        match_camera_clamp(state);
    }

    // SELECT RECT
    if (state.ui_mode == UI_MODE_SELECTING) {
        // Update select rect
        state.select_rect = (SDL_Rect) {
            .x = std::min(state.select_rect_origin.x, match_get_mouse_world_pos(state).x),
            .y = std::min(state.select_rect_origin.y, match_get_mouse_world_pos(state).y),
            .w = std::max(1, std::abs(state.select_rect_origin.x - match_get_mouse_world_pos(state).x)), 
            .h = std::max(1, std::abs(state.select_rect_origin.y - match_get_mouse_world_pos(state).y))
        };
    }

    // Update timers
    if (animation_is_playing(state.ui_move_animation)) {
        animation_update(state.ui_move_animation);
    }

    // Update entities
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        entity_update(state, entity_index);
    }

    // Remove any dead entities
    {
        uint32_t entity_index = 0;
        while (entity_index < state.entities.size()) {
            if ((state.entities[entity_index].mode == MODE_UNIT_DEATH_FADE && !animation_is_playing(state.entities[entity_index].animation)) || 
                    (entity_check_flag(state.entities[entity_index], ENTITY_FLAG_IS_GARRISONED) && state.entities[entity_index].health == 0) ||
                    (state.entities[entity_index].mode == MODE_BUILDING_DESTROYED && state.entities[entity_index].timer == 0)) {
                state.entities.remove_at(entity_index);
                // state.is_fog_dirty = true;
            } else {
                entity_index++;
            }
        }
    }

    if (state.ui_status_timer > 0) {
        state.ui_status_timer--;
    }
    for (uint32_t chat_index = 0; chat_index < state.ui_chat.size(); chat_index++) {
        state.ui_chat[chat_index].timer--;
        if (state.ui_chat[chat_index].timer == 0) {
            state.ui_chat.erase(state.ui_chat.begin() + chat_index);
            chat_index--;
        }
    }

    engine_set_cursor(ui_is_targeting(state) ? CURSOR_TARGET : CURSOR_DEFAULT);
}

void match_camera_clamp(match_state_t& state) {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, ((int)state.map_width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, ((int)state.map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT);
}

void match_camera_center_on_cell(match_state_t& state, xy cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    match_camera_clamp(state);
}

xy match_get_mouse_world_pos(const match_state_t& state) {
    return engine.mouse_position + state.camera_offset;
}

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const entity_t& entity : state.entities) {
        if (entity.player_id == player_id && entity_is_unit(entity.type) && entity.health != 0) {
            population += ENTITY_DATA.at(entity.type).unit_data.population_cost;
        }
    }
    
    return population;
}

uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id) {
    /*
    uint32_t max_population = 10;
    for (const entity_t& building : state.entities) {
        if (building.player_id == player_id && building.type == BUILDING_HOUSE && building_is_finished(building)) {
            max_population += 10;
        }
    }

    return max_population;
    */
   return 200;
}

input_t match_create_move_input(const match_state_t& state) {
    // Determine move target
    xy move_target;
    if (ui_is_mouse_in_ui()) {
        xy minimap_pos = engine.mouse_position - xy(MINIMAP_RECT.x, MINIMAP_RECT.y);
        move_target = xy((state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                            (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
    } else {
        move_target = match_get_mouse_world_pos(state);
    }

    // Create move input
    input_t input;
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        // TODO if they can't see this boi, then don't target him
        if (!entity_is_selectable(state.entities[entity_index])) {
            continue;
        }
        SDL_Rect entity_rect = entity_get_rect(state.entities[entity_index]);
        if (sdl_rect_has_point(entity_rect, move_target)) {
            input.move.target_id = state.entities.get_id_of(entity_index);
            break;
        }
    }

    if (state.ui_mode == UI_MODE_TARGET_UNLOAD) {
        input.type = INPUT_MOVE_UNLOAD;
    } else if (state.ui_mode == UI_MODE_TARGET_REPAIR) {
        input.type = INPUT_MOVE_REPAIR;
    } else if (input.move.target_id != ID_NULL && 
               (state.ui_mode == UI_MODE_TARGET_ATTACK || 
               state.entities.get_by_id(input.move.target_id).player_id != network_get_player_id())) {
        input.type = INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && state.ui_mode == UI_MODE_TARGET_ATTACK) {
        input.type = INPUT_MOVE_ATTACK_CELL;
    } else if (input.move.target_id != ID_NULL) {
        input.type = INPUT_MOVE_ENTITY;
    } else {
        input.type = INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = (uint16_t)state.selection.size();
    memcpy(input.move.entity_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));

    return input;
}

// Returns the nearest cell around the rect relative to start_cell
// If there are no free cells around the rect in a radius of 1, then this returns the start cell
xy match_get_nearest_cell_around_rect(const match_state_t& state, xy start, int start_size, xy rect_position, int rect_size, bool allow_blocked_cells) {
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

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.move.target_id, sizeof(entity_id));
            out_buffer_length += sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_ids, input.move.entity_count * sizeof(entity_id));
            out_buffer_length += input.move.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            memcpy(out_buffer + out_buffer_length, &input.stop.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.stop.entity_ids, input.stop.entity_count * sizeof(entity_id));
            out_buffer_length += input.stop.entity_count * sizeof(entity_id);
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
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.move.target_id, in_buffer + in_buffer_head, sizeof(entity_id));
            in_buffer_head += sizeof(entity_id);

            memcpy(&input.move.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.move.entity_ids, in_buffer + in_buffer_head, input.move.entity_count * sizeof(entity_id));
            in_buffer_head += input.move.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            memcpy(&input.stop.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.stop.entity_ids, in_buffer + in_buffer_head, input.stop.entity_count * sizeof(entity_id));
            in_buffer_head += input.stop.entity_count * sizeof(entity_id);
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
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            // Determine the target index
            uint32_t target_index = INDEX_INVALID;
            if (input.type == INPUT_MOVE_ENTITY || input.type == INPUT_MOVE_ATTACK_ENTITY || input.type == INPUT_MOVE_REPAIR) {
                target_index = state.entities.get_index_of(input.move.target_id);
                if (target_index != INDEX_INVALID && !entity_is_selectable(state.entities[target_index])) {
                    target_index = INDEX_INVALID;
                }
            }

            // Calculate group center
            xy group_center;
            bool should_move_as_group = target_index == INDEX_INVALID;
            uint32_t unit_count = 0;
            if (should_move_as_group) {
                std::vector<xy> unit_cells;
                unit_cells.reserve(input.move.entity_count);
                xy group_min;
                xy group_max;
                for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                    uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                        continue;
                    }

                    xy entity_cell = state.entities[entity_index].cell;
                    if (unit_count == 0) {
                        group_min = entity_cell;
                        group_max = entity_cell;
                    } else {
                        group_min.x = std::min(group_min.x, entity_cell.x);
                        group_min.y = std::min(group_min.y, entity_cell.y);
                        group_max.x = std::max(group_max.x, entity_cell.x);
                        group_max.y = std::max(group_max.y, entity_cell.y);
                    }

                    unit_count++;
                }

                SDL_Rect group_rect = (SDL_Rect) { 
                    .x = group_min.x, .y = group_min.y,
                    .w = group_max.x - group_min.x, .h = group_max.y - group_min.y
                };
                group_center = xy(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));

                // Don't move as group if we're not in a group
                // Also don't move as a group if the target is inside the group rect (this allows units to converge in on a cell)
                if (unit_count < 2 || sdl_rect_has_point(group_rect, input.move.target_cell)) {
                    should_move_as_group = false;
                }
            } // End calculate group center

            // Give each unit the move command
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                entity_t& entity = state.entities[entity_index];

                // Set the unit's target
                target_t target;
                target.type = (TargetType)input.type;
                if (target_index == INDEX_INVALID) {
                    target.cell = input.move.target_cell;
                    // If group-moving, use the group move cell, but only if the cell is valid
                    if (should_move_as_group) {
                        xy group_move_cell = input.move.target_cell + (entity.cell - group_center);
                        if (map_is_cell_in_bounds(state, group_move_cell) && 
                                xy::manhattan_distance(group_move_cell, input.move.target_cell) <= 3 &&
                                map_get_tile(state, group_move_cell).elevation == map_get_tile(state, input.move.target_cell).elevation) {
                            target.cell = group_move_cell;
                        }
                    }
                } else {
                    target.id = input.move.target_id;
                }
                entity_set_target(entity, target);
            } // End for each unit in move input
            break;
        } // End handle INPUT_MOVE
        case INPUT_STOP:
        case INPUT_DEFEND: {
            for (uint8_t id_index = 0; id_index < input.stop.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.stop.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                entity_t& entity = state.entities[entity_index];

                entity.path.clear();
                entity_set_target(entity, (target_t) {
                    .type = TARGET_NONE
                });
                if (input.type == INPUT_DEFEND) {
                    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, true);
                }
            }
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_enqueue.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }
            // if (state.player_gold[player_id] < building_queue_item_cost(input.building_enqueue.item)) {
                // return;
            // }
            if (state.entities[building_index].queue.size() == BUILDING_QUEUE_MAX) {
                return;
            }

            // state.player_gold[player_id] -= building_queue_item_cost(input.building_enqueue.item);
            entity_building_enqueue(state, state.entities[building_index], input.building_enqueue.item);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }
            entity_t& building = state.entities[building_index];
            if (building.queue.empty()) {
                return;
            }

            uint32_t index = input.building_dequeue.index == BUILDING_DEQUEUE_POP_FRONT
                                    ? building.queue.size() - 1
                                    : input.building_dequeue.index;
            
            // state.player_gold[player_id] += building_queue_item_cost(building.queue[index]);
            if (index == 0) {
                entity_building_dequeue(state, building);
            } else {
                building.queue.erase(building.queue.begin() + index);
            }
            break;
        }
        default:
            break;
    }
}

void match_render(const match_state_t& state) {
    // Prepare map render
    xy base_pos = xy(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
    xy base_coords = xy(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
    xy max_visible_tiles = xy(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Begin elevation passes
    for (uint16_t elevation = 0; elevation < 3; elevation++) {
        std::vector<render_sprite_params_t> ysorted_render_params;

        // Render map
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.map_width);
                // If current elevation is above the tile, then skip it
                if (state.map_tiles[map_index].elevation < elevation) {
                    continue;
                }

                // If current elevation is equal to the tile, then render the tile, otherwise render default ground tile beneath it
                uint16_t map_tile_index = state.map_tiles[map_index].elevation == elevation
                                            ? state.map_tiles[map_index].index
                                            : 1;
                SDL_Rect tile_src_rect = (SDL_Rect) {
                    .x = map_tile_index * TILE_SIZE,
                    .y = 0,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                SDL_Rect tile_dst_rect = (SDL_Rect) {
                    .x = base_pos.x + (x * TILE_SIZE),
                    .y = base_pos.y + (y * TILE_SIZE),
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                // Render a sand tile below front walls
                if (map_tile_index != 1 && elevation == 0) {
                    SDL_Rect below_tile_src_rect = tile_src_rect;
                    below_tile_src_rect.x = TILE_SIZE;
                    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILESET_ARIZONA].texture, &below_tile_src_rect, &tile_dst_rect);
                }
                SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILESET_ARIZONA].texture, &tile_src_rect, &tile_dst_rect);
            } // End for x of visible tiles
        } // End for y of visible tiles
        // End render map

        // Render entity corpses
        for (entity_t entity : state.entities) {
            if (!(entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED)) {
                continue;
            }

            if (entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            render_sprite_params_t render_params = match_create_entity_render_params(state, entity);
            render_sprite(render_params.sprite, render_params.frame, render_params.position, render_params.options, render_params.recolor_id);
        }

        // Select rings and healthbars
        static const int HEALTHBAR_HEIGHT = 4;
        static const int HEALTHBAR_PADDING = 3;
        static const int BUILDING_HEALTHBAR_PADDING = 5;
        for (entity_id id : state.selection) {
            const entity_t& entity = state.entities.get_by_id(id);
            if (entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            // Select ring
            render_sprite(entity_get_select_ring(entity, entity.player_id == PLAYER_NONE || entity.player_id == network_get_player_id()), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);

            // TOOD ignore all this code if is gold mine

            // Determine the healthbar rect
            SDL_Rect entity_rect = entity_get_rect(entity);
            entity_rect.x -= state.camera_offset.x;
            entity_rect.y -= state.camera_offset.y;
            xy healthbar_position = xy(entity_rect.x, entity_rect.y + entity_rect.h + (entity_is_unit(entity.type) ? HEALTHBAR_PADDING : BUILDING_HEALTHBAR_PADDING));
            match_render_healthbar(healthbar_position, xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, ENTITY_DATA.at(entity.type).max_health);
        }

        // Entities
        for (entity_t entity : state.entities) {
            if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
                continue;
            }
            if (entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            render_sprite_params_t render_params = match_create_entity_render_params(state, entity);
            SDL_Rect render_rect = (SDL_Rect) {
                .x = render_params.position.x,
                .y = render_params.position.y,
                .w = engine.sprites[render_params.sprite].frame_size.x,
                .h = engine.sprites[render_params.sprite].frame_size.y
            };
            // Perform culling in advance so that we can minimize y-sorting
            if (SDL_HasIntersection(&render_rect, &SCREEN_RECT) != SDL_TRUE) {
                continue;
            }
            render_params.options |= RENDER_SPRITE_NO_CULL;

            ysorted_render_params.push_back(render_params);
        }

        // UI move animation
        if (animation_is_playing(state.ui_move_animation) && 
            map_get_tile(state, state.ui_move_position / TILE_SIZE).elevation == elevation) {
            if (state.ui_move_animation.name == ANIMATION_UI_MOVE_CELL) {
                render_sprite(SPRITE_UI_MOVE, state.ui_move_animation.frame, state.ui_move_position - state.camera_offset, RENDER_SPRITE_CENTERED);
            } else if (state.ui_move_animation.frame.x % 2 == 0) {
                uint32_t entity_index = state.entities.get_index_of(state.ui_move_entity_id);
                if (entity_index != INDEX_INVALID) {
                    const entity_t& entity = state.entities[entity_index];
                    render_sprite(entity_get_select_ring(entity, state.ui_move_animation.name == ANIMATION_UI_MOVE_ENTITY), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);
                }
            }
        }


        // End Ysort
        ysort_render_params(ysorted_render_params, 0, ysorted_render_params.size() - 1);
        for (const render_sprite_params_t& params : ysorted_render_params) {
            render_sprite(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
    } // End for each elevation

    // Select rect
    if (state.ui_mode == UI_MODE_SELECTING) {
        SDL_Rect select_rect = state.select_rect;
        select_rect.x -= state.camera_offset.x;
        select_rect.y -= state.camera_offset.y;
        SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(engine.renderer, &select_rect);
    }

    // UI Chat
    for (uint32_t chat_index = 0; chat_index < state.ui_chat.size(); chat_index++) {
        log_trace("Rendering chat message: %s", state.ui_chat[chat_index].message.c_str());
        render_text(FONT_HACK, state.ui_chat[chat_index].message.c_str(), COLOR_WHITE, xy(16, MINIMAP_RECT.y - 40 - (chat_index * 16)));
    }

    // UI Status message
    if (state.ui_status_timer != 0) {
        render_text(FONT_HACK, state.ui_status_message.c_str(), COLOR_WHITE, xy(RENDER_TEXT_CENTERED, SCREEN_HEIGHT - 148));
    }

    // UI Disconnect frame
    if (state.ui_disconnect_timer > MATCH_DISCONNECT_GRACE) {
        render_ninepatch(SPRITE_UI_FRAME, UI_DISCONNECT_FRAME_RECT, 16);
        render_text(FONT_WESTERN8, "Waiting for players...", COLOR_GOLD, xy(UI_DISCONNECT_FRAME_RECT.x + 16, UI_DISCONNECT_FRAME_RECT.y + 8));
        int player_text_y = 32;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == PLAYER_STATUS_NONE || !(state.inputs[player_id].empty() || state.inputs[player_id][0].empty())) {
                continue;
            }

            render_text(FONT_WESTERN8, network_get_player(player_id).name, COLOR_GOLD, xy(UI_DISCONNECT_FRAME_RECT.x + 24, UI_DISCONNECT_FRAME_RECT.y + player_text_y));
            player_text_y += 24;
        }
    }

    // UI Match over
    if (state.ui_mode == UI_MODE_MATCH_OVER_VICTORY || state.ui_mode == UI_MODE_MATCH_OVER_DEFEAT) {
        render_ninepatch(SPRITE_UI_FRAME, UI_MATCH_OVER_FRAME_RECT, 16);
        render_text(FONT_WESTERN8, state.ui_mode == UI_MODE_MATCH_OVER_VICTORY ? "Victory!" : "Defeat!", COLOR_GOLD, xy(RENDER_TEXT_CENTERED, UI_MATCH_OVER_FRAME_RECT.y + 10));
        bool exit_button_hovered = sdl_rect_has_point(UI_MATCH_OVER_EXIT_BUTTON_RECT, engine.mouse_position);
        render_sprite(SPRITE_UI_PARCHMENT_BUTTONS, xy(2, exit_button_hovered ? 1 : 0), xy(UI_MATCH_OVER_EXIT_BUTTON_RECT.x, UI_MATCH_OVER_EXIT_BUTTON_RECT.y + (exit_button_hovered ? -1 : 0)), RENDER_SPRITE_NO_CULL);
    }

    // Menu button
    render_sprite(SPRITE_UI_MENU_BUTTON, xy(sdl_rect_has_point(UI_MENU_BUTTON_RECT, engine.mouse_position) || state.ui_mode == UI_MODE_MENU ? 1 : 0, 0), xy(UI_MENU_BUTTON_RECT.x, UI_MENU_BUTTON_RECT.y), RENDER_SPRITE_NO_CULL);
    if (state.ui_mode == UI_MODE_MENU) {
        /*
        render_ninepatch(SPRITE_UI_FRAME, UI_MENU_RECT, 16);
        render_text(FONT_WESTERN8, "Game Menu", COLOR_GOLD, xy(RENDER_TEXT_CENTERED, UI_MENU_RECT.position.y + 10));
        for (int i = UI_MENU_BUTTON_NONE + 1; i < UI_MENU_BUTTON_COUNT; i++) {
            render_sprite(SPRITE_UI_MENU_PARCHMENT_BUTTONS, xy(i - (UI_MENU_BUTTON_NONE + 1), ui_menu_get_parchment_button_hovered() == i ? 1 : 0), ui_menu_get_parchment_button_rect((UiMenuButton)i).position, RENDER_SPRITE_NO_CULL);
        }
        */
    }

    // UI frames
    render_sprite(SPRITE_UI_MINIMAP, xy(0, 0), xy(0, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_MINIMAP].frame_size.y));
    render_sprite(SPRITE_UI_FRAME_BOTTOM, xy(0, 0), UI_FRAME_BOTTOM_POSITION);
    render_sprite(SPRITE_UI_FRAME_BUTTONS, xy(0, 0), xy(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x + engine.sprites[SPRITE_UI_FRAME_BOTTOM].frame_size.x, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y));

    // UI Buttons
    for (int i = 0; i < UI_BUTTONSET_SIZE; i++) {
        UiButton ui_button = ui_get_ui_button(state, i); 
        if (ui_button == UI_BUTTON_NONE) {
            continue;
        }

        bool is_button_hovered = ui_get_ui_button_hovered(state) == i && ui_button_requirements_met(state, ui_button);
        bool is_button_pressed = state.ui_button_pressed == i;
        xy offset = is_button_pressed ? xy(1, 1) : (is_button_hovered ? xy(0, -1) : xy(0, 0));
        int button_state = 0;
        if (!ui_button_requirements_met(state, ui_button)) {
            button_state = 2;
        } else if (is_button_hovered) {
            button_state = 1;
        }
        render_sprite(SPRITE_UI_BUTTON, xy(button_state, 0), xy(UI_BUTTON_RECT[i].x, UI_BUTTON_RECT[i].y) + offset);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(ui_button - 1, button_state), xy(UI_BUTTON_RECT[i].x, UI_BUTTON_RECT[i].y) + offset);
    }

    // UI Selection list
    const xy SELECTION_LIST_TOP_LEFT = UI_FRAME_BOTTOM_POSITION + xy(12 + 16, 12);
    if (state.selection.size() == 1) {
        const entity_t& entity = state.entities.get_by_id(state.selection[0]);
        const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

        match_render_text_with_text_frame(entity_data.name, SELECTION_LIST_TOP_LEFT);
        render_sprite(SPRITE_UI_BUTTON, xy(0, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(entity_data.ui_button - 1, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);

        // TODO don't render healthbar for gold mine
        xy healthbar_position = SELECTION_LIST_TOP_LEFT + xy(0, 18 + 35);
        xy healthbar_size = xy(64, 12);
        match_render_healthbar(healthbar_position, healthbar_size, entity.health, entity_data.max_health);

        char health_text[10];
        sprintf(health_text, "%i/%i", entity.health, entity_data.max_health);
        SDL_Surface* health_text_surface = TTF_RenderText_Solid(engine.fonts[FONT_HACK], health_text, COLOR_WHITE);
        GOLD_ASSERT(health_text_surface != NULL);
        SDL_Texture* health_text_texture = SDL_CreateTextureFromSurface(engine.renderer, health_text_surface);
        GOLD_ASSERT(health_text_texture != NULL);
        SDL_Rect health_text_src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = health_text_surface->w, .h = health_text_surface->h };
        SDL_Rect health_text_dst_rect = (SDL_Rect) {
            .x = healthbar_position.x + (healthbar_size.x / 2) - (health_text_surface->w / 2),
            .y = healthbar_position.y + (healthbar_size.y / 2) - (health_text_surface->h / 2),
            .w = health_text_src_rect.w,
            .h = health_text_src_rect.h
        };
        SDL_RenderCopy(engine.renderer, health_text_texture, &health_text_src_rect, &health_text_dst_rect);
        SDL_DestroyTexture(health_text_texture);
        SDL_FreeSurface(health_text_surface);
    } else {
        for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
            const entity_t& entity = state.entities.get_by_id(state.selection[0]);
            const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

            xy icon_position = SELECTION_LIST_TOP_LEFT + xy(((selection_index % 10) * 34) - 12, (selection_index / 10) * 34);
            render_sprite(SPRITE_UI_BUTTON, xy(0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(entity_data.ui_button - 1, 0), icon_position, RENDER_SPRITE_NO_CULL);
            match_render_healthbar(icon_position + xy(1, 32 - 5), xy(32- 2, 4), entity.health, entity_data.max_health);
        }
    }

    // UI Building queues
    if (state.selection.size() == 1 && !state.entities.get_by_id(state.selection[0]).queue.empty()) {
        const entity_t& building = state.entities.get_by_id(state.selection[0]);
        for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
            render_sprite(SPRITE_UI_BUTTON, xy(0, 0), UI_BUILDING_QUEUE_POSITIONS[building_queue_index], RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(building_queue_item_icon(building.queue[building_queue_index]) - 1, 0), UI_BUILDING_QUEUE_POSITIONS[building_queue_index], RENDER_SPRITE_NO_CULL);
        }

        static const SDL_Rect BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT = (SDL_Rect) {
            .x = UI_FRAME_BOTTOM_POSITION.x + BUILDING_QUEUE_TOP_LEFT.x + 32 + 4,
            .y = UI_FRAME_BOTTOM_POSITION.y + BUILDING_QUEUE_TOP_LEFT.y + 32 - 8,
            .w = 32 * 3 + (4 * 2), .h = 6
        };
        if (building.timer == BUILDING_QUEUE_BLOCKED) {
            render_text(FONT_WESTERN8, "Build more houses.", COLOR_GOLD, xy(BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y - 12));
        } else if (building.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
            render_text(FONT_WESTERN8, "Exit is blocked.", COLOR_GOLD, xy(BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y - 12));
        } else {
            SDL_Rect building_queue_progress_bar_rect = (SDL_Rect) {
                .x = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x,
                .y = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y,
                .w = (BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.w * (int)(building_queue_item_duration(building.queue[0]) - building.timer)) / (int)building_queue_item_duration(building.queue[0]),
                .h = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.h,
            };
            SDL_SetRenderDrawColor(engine.renderer, COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b, COLOR_WHITE.a);
            SDL_RenderFillRect(engine.renderer, &building_queue_progress_bar_rect);
            SDL_SetRenderDrawColor(engine.renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
            SDL_RenderDrawRect(engine.renderer, &BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT);
        }
    }

    // Resource counters
    char gold_text[8];
    // sprintf(gold_text, "%u", state.player_gold[network_get_player_id()]);
    sprintf(gold_text, "0 haha");
    render_text(FONT_WESTERN8, gold_text, COLOR_WHITE, xy(SCREEN_WIDTH - 172 + 18, 4));
    render_sprite(SPRITE_UI_GOLD, xy(0, 0), xy(SCREEN_WIDTH - 172, 2), RENDER_SPRITE_NO_CULL);

    char population_text[8];
    sprintf(population_text, "%u/%u", match_get_player_population(state, network_get_player_id()), match_get_player_max_population(state, network_get_player_id()));
    render_text(FONT_WESTERN8, population_text, COLOR_WHITE, xy(SCREEN_WIDTH - 88 + 22, 4));
    render_sprite(SPRITE_UI_HOUSE, xy(0, 0), xy(SCREEN_WIDTH - 88, 0), RENDER_SPRITE_NO_CULL);
}

render_sprite_params_t match_create_entity_render_params(const match_state_t& state, const entity_t& entity) {
    render_sprite_params_t render_params = (render_sprite_params_t) {
        .sprite = entity_get_sprite(entity),
        .frame = entity_get_animation_frame(entity),
        .position = entity.position.to_xy() - state.camera_offset,
        .options = 0,
        .recolor_id = entity.mode == MODE_BUILDING_DESTROYED ? (uint8_t)RECOLOR_NONE : entity.player_id
    };
    // Adjust render position for units because they are centered
    if (entity_is_unit(entity.type)) {
        render_params.position.x -= engine.sprites[render_params.sprite].frame_size.x / 2;
        render_params.position.y -= engine.sprites[render_params.sprite].frame_size.y / 2;
    }
    if (entity_should_flip_h(entity)) {
        render_params.options |= RENDER_SPRITE_FLIP_H;
    }

    return render_params;
}

void match_render_healthbar(xy position, xy size, int health, int max_health) {
    GOLD_ASSERT(max_health != 0);

    SDL_Rect healthbar_rect = (SDL_Rect) { .x = position.x, .y = position.y, .w = size.x, .h = size.y };
    if (SDL_HasIntersection(&healthbar_rect, &SCREEN_RECT) != SDL_TRUE) {
        return;
    }

    SDL_Rect healthbar_subrect = healthbar_rect;
    healthbar_subrect.w = (healthbar_rect.w * health) / max_health;
    SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;

    SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
    SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
    SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
    SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
}

void match_render_text_with_text_frame(const char* text, xy position) {
    SDL_Surface* text_surface = TTF_RenderText_Solid(engine.fonts[FONT_WESTERN8], text, COLOR_OFFBLACK);
    if (text_surface == NULL) {
        log_error("Unable to render text to surface: %s", TTF_GetError());
        return;
    }

    int frame_width = (text_surface->w / 15) + 1;
    if (text_surface->w % 15 != 0) {
        frame_width++;
    }
    for (int frame_x = 0; frame_x < frame_width; frame_x++) {
        int x_frame = 1;
        if (frame_x == 0) {
            x_frame = 0;
        } else if (frame_x == frame_width - 1) {
            x_frame = 2;
        }
        render_sprite(SPRITE_UI_TEXT_FRAME, xy(x_frame, 0), position + xy(frame_x * 15, 0), RENDER_SPRITE_NO_CULL);
    }

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(engine.renderer, text_surface);
    if (text_texture == NULL) {
        log_error("Unable to creature texture from text surface: %s", SDL_GetError());
        return;
    }

    SDL_Rect src_rect = (SDL_Rect) { .x = 0, . y = 0, .w = text_surface->w, .h = text_surface->h };
    SDL_Rect dst_rect = (SDL_Rect) { .x = position.x + ((frame_width * 15) / 2) - (text_surface->w / 2), .y = position.y + 2, .w = src_rect.w, .h = src_rect.h };

    SDL_RenderCopy(engine.renderer, text_texture, &src_rect, &dst_rect);

    SDL_DestroyTexture(text_texture);
    SDL_FreeSurface(text_surface);
}
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

match_state_t match_init() {
    match_state_t state;

    state.ui_mode = UI_MODE_MATCH_NOT_STARTED;
    state.ui_status_timer = 0;

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
        entity_create_unit(state, UNIT_MINER, player_id, player_spawn + xy(-1, -1));
        entity_create_unit(state, UNIT_MINER, player_id, player_spawn + xy(1, -1));
        entity_create_unit(state, UNIT_MINER, player_id, player_spawn + xy(-2, 0));
        entity_create_unit(state, UNIT_MINER, player_id, player_spawn + xy(2, 0));
        entity_create_unit(state, UNIT_MINER, player_id, player_spawn + xy(-1, 1));
        entity_create_unit(state, UNIT_MINER, player_id, player_spawn + xy(1, 1));
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

    // Order movement
    if (event.type == SDL_MOUSEBUTTONDOWN && ui_get_selection_type(state) == SELECTION_TYPE_UNITS && 
            ((event.button.button == SDL_BUTTON_LEFT && ui_is_targeting(state)) ||
            (event.button.button == SDL_BUTTON_RIGHT && state.ui_mode == UI_MODE_NONE))) {
        input_t move_input = match_create_move_input(state);
        state.input_queue.push_back(move_input);

        // Provide instant user feedback
        if (move_input.type == INPUT_MOVE_CELL) {
            state.ui_move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
            state.ui_move_position = match_get_mouse_world_pos(state);
            state.ui_move_entity_id = ID_NULL;
        } else {
            state.ui_move_animation = animation_create(ANIMATION_UI_MOVE_ENTITY);
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
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (state.ui_mode == UI_MODE_SELECTING) {
            state.ui_mode = UI_MODE_NONE;
            std::vector<entity_id> selection = ui_create_selection_from_rect(state);
            ui_set_selection(state, selection);
        }
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
        SDL_Rect entity_rect = entity_get_rect(state.entities[entity_index]);
        if (sdl_rect_has_point(entity_rect, move_target)) {
            input.move.target_id = state.entities.get_id_of(entity_index);
            break;
        }
    }

    if (state.ui_mode == UI_MODE_TARGET_UNLOAD) {

    } else if (state.ui_mode == UI_MODE_TARGET_REPAIR) {

    } else if (input.move.target_id != ID_NULL) {
        // INPUT_MOVE_ENTITY is given priority over attack move because attack move is treated as a cell move
        // If they are A-moving and directly click their target, then it should be an entity move instead
        input.type = INPUT_MOVE_ENTITY;
    } else if (state.ui_mode == UI_MODE_TARGET_ATTACK) {
        input.type = INPUT_MOVE_ATTACK;
    } else {
        input.type = INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = (uint16_t)state.selection.size();
    memcpy(input.move.entity_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));

    return input;
}

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.move.target_id, sizeof(entity_id));
            out_buffer_length += sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_ids, input.move.entity_count * sizeof(entity_id));
            out_buffer_length += input.move.entity_count * sizeof(entity_id);
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
        case INPUT_MOVE_ATTACK:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.move.target_id, in_buffer + in_buffer_head, sizeof(entity_id));
            in_buffer_head += sizeof(entity_id);

            memcpy(&input.move.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.move.entity_ids, in_buffer + in_buffer_head, input.move.entity_count * sizeof(entity_id));
            in_buffer_head += input.move.entity_count * sizeof(entity_id);
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
        case INPUT_MOVE_ATTACK:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            // Determine the target index
            uint32_t target_index = INDEX_INVALID;
            if (input.type == INPUT_MOVE_ENTITY || input.type == INPUT_MOVE_REPAIR) {
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
            render_sprite(entity_get_select_ring(entity), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);

            // TOOD ignore all this code if is gold mine

            // Determine the healthbar rect
            SDL_Rect entity_rect = entity_get_rect(entity);
            entity_rect.x -= state.camera_offset.x;
            entity_rect.y -= state.camera_offset.y;
            SDL_Rect healthbar_rect = (SDL_Rect) {
                .x = entity_rect.x,
                .y = entity_rect.y + entity_rect.h + (entity_is_unit(entity.type) ? HEALTHBAR_PADDING : BUILDING_HEALTHBAR_PADDING),
                .w = entity_rect.w,
                .h = HEALTHBAR_HEIGHT
            };
            SDL_Rect healthbar_subrect = healthbar_rect;
            healthbar_subrect.w = (healthbar_rect.w * entity.health) / ENTITY_DATA.at(entity.type).max_health;

            // Cull the healthbar
            if (SDL_HasIntersection(&healthbar_rect, &SCREEN_RECT) == SDL_TRUE) {
                // Render the healthbar
                SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
                SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
                SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
                SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
                SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
            }
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
                    render_sprite(entity_get_select_ring(entity), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);
                }
            }
        }

        // Entities
        for (entity_t entity : state.entities) {
            if (entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            render_sprite_params_t render_params = (render_sprite_params_t) {
                .sprite = entity_get_sprite(entity),
                .frame = entity_get_animation_frame(entity),
                .position = entity.position.to_xy() - state.camera_offset,
                .options = RENDER_SPRITE_NO_CULL,
                .recolor_id = entity.player_id
            };
            SDL_Rect render_rect = (SDL_Rect) {
                .x = render_params.position.x,
                .y = render_params.position.y,
                .w = engine.sprites[render_params.sprite].frame_size.x,
                .h = engine.sprites[render_params.sprite].frame_size.y
            };
            // Adjust the render rect for units because units are centered when rendering
            if (entity_is_unit(entity.type)) {
                render_rect.x -= render_rect.w / 2;
                render_rect.y -= render_rect.h / 2;
                render_params.options |= RENDER_SPRITE_CENTERED;
            }
            if (entity_should_flip_h(entity)) {
                render_params.options |= RENDER_SPRITE_FLIP_H;
            }

            if (SDL_HasIntersection(&render_rect, &SCREEN_RECT) != SDL_TRUE) {
                continue;
            }

            ysorted_render_params.push_back(render_params);
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
        render_text(FONT_HACK, state.ui_status_message.c_str(), COLOR_WHITE, xy(RENDER_SPRITE_CENTERED, SCREEN_HEIGHT - 148));
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
}
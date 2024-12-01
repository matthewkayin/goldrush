#include "match.h"

#include "engine.h"
#include "network.h"
#include "logger.h"
#include <algorithm>

static const uint32_t TICK_DURATION = 4;
static const uint32_t TICK_OFFSET = 4;

static const int CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_DRAG_SPEED = 16;

match_state_t match_init() {
    match_state_t state;

    state.ui_mode = UI_MODE_MATCH_NOT_STARTED;

    map_init(state, 64, 64);

    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        input_t empty_input;
        empty_input.type = INPUT_NONE;
        std::vector<input_t> empty_input_list = { empty_input };
        for (uint8_t i = 0; i < TICK_OFFSET - 1; i++) {
            state.inputs[player_id].push_back(empty_input_list);
        }
    }
    state.tick_timer = 0;

    network_toggle_ready();
    if (network_are_all_players_ready()) {
        log_info("Beginning singleplayer game.");
        state.ui_mode = UI_MODE_NONE;
    }
    state.camera_offset = xy(0, 0);

    entity_create_unit(state, UNIT_MINER, 0, xy(1, 1));
    entity_create_unit(state, UNIT_MINER, 0, xy(2, 2));

    return state;
}

void match_handle_input(match_state_t& state, SDL_Event event) {
    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED) {
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (state.ui_mode == UI_MODE_NONE && !ui_is_mouse_in_ui()) {
            state.select_rect_origin = match_get_mouse_world_pos(state);
            state.ui_mode = UI_MODE_SELECTING;
        }
    } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (state.ui_mode == UI_MODE_SELECTING) {
            state.ui_mode = UI_MODE_NONE;
            std::vector<entity_id> selection = ui_create_selection_from_rect(state);
            ui_set_selection(state, selection);
        }
    }
}

void match_update(match_state_t& state) {
    network_service();

    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED && network_are_all_players_ready()) {
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
                break;
            }
            default: 
                break;
        }
    }

    // Tick loop
    if (state.tick_timer == 0) {
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
            return;
        }

        // All inputs received. Begin next tick
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

        state.tick_timer = TICK_DURATION;

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

    state.tick_timer--;

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

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        default:
            break;
    }
}

input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head) {
    input_t input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        default:
            break;
    }

    return input;
}

void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input) {
    switch (input.type) {
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
        for (entity_id id : state.selection) {
            const entity_t& entity = state.entities.get_by_id(id);
            if (entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            // Select ring
            xy render_pos;
            if (entity_is_unit(entity.type)) {
                render_pos = entity.position.to_xy();
            } else {
                SDL_Rect entity_rect = entity_get_rect(entity);
                render_pos = xy(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2));
            }
            render_sprite(entity_get_select_ring(entity), xy(0, 0), render_pos - state.camera_offset, RENDER_SPRITE_CENTERED);
        }

        for (entity_t entity : state.entities) {
            if (entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            render_sprite_params_t render_params = (render_sprite_params_t) {
                .sprite = entity_get_sprite(entity),
                .frame = xy(0, 0), // TODO
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

    // UI frames
    render_sprite(SPRITE_UI_MINIMAP, xy(0, 0), xy(0, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_MINIMAP].frame_size.y));
    render_sprite(SPRITE_UI_FRAME_BOTTOM, xy(0, 0), UI_FRAME_BOTTOM_POSITION);
    render_sprite(SPRITE_UI_FRAME_BUTTONS, xy(0, 0), xy(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x + engine.sprites[SPRITE_UI_FRAME_BOTTOM].frame_size.x, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y));
}
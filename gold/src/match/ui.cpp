#include "match/ui.h"

#include "core/network.h"
#include "core/input.h"
#include "core/logger.h"
#include "menu/match_setting.h"
#include "render/sprite.h"
#include "render/render.h"
#include <algorithm>

static const uint32_t TURN_OFFSET = 2;
static const uint32_t TURN_DURATION = 4;

static const uint32_t UI_STATUS_DURATION = 32;

static const int MATCH_CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_SPEED = 16; // TODO: move to options
static const Rect SCREEN_RECT = (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
static const Rect MINIMAP_RECT = (Rect) { .x = 4, .y = SCREEN_HEIGHT - 132, .w = 128, .h = 128 };
static const int SOUND_LISTEN_MARGIN = 128;
static const uint32_t SOUND_COOLDOWN_DURATION = 5;

// INIT

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise) {
    MatchUiState state;

    state.mode = MATCH_UI_MODE_NOT_STARTED;
    state.camera_offset = ivec2(0, 0);
    state.select_origin = ivec2(-1, -1);
    state.is_minimap_dragging = false;
    state.turn_timer = 0;
    state.turn_counter = 0;
    state.disconnect_timer = 0;
    memset(state.sound_cooldown_timers, 0, sizeof(state.sound_cooldown_timers));

    // Populate match player info using network player info
    MatchPlayer players[MAX_PLAYERS];
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const NetworkPlayer& network_player = network_get_player(player_id);
        if (network_player.status == NETWORK_PLAYER_STATUS_NONE) {
            memset(&players[player_id], 0, sizeof(MatchPlayer));
            continue;
        }

        players[player_id].active = true;
        strcpy(players[player_id].name, network_player.name);
        // Use the player_id as the "team" in a FFA game to ensure everyone is on a separate team
        players[player_id].team = network_get_match_setting(MATCH_SETTING_TEAMS) == MATCH_SETTING_TEAMS_ENABLED 
                                        ? network_player.team
                                        : player_id;
        players[player_id].recolor_id = network_player.recolor_id;
    }
    state.match = match_init(lcg_seed, noise, players);

    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
            continue;
        }

        for (uint8_t i = 0; i < TURN_OFFSET - 1; i++) {
            state.inputs[player_id].push_back({ (MatchInput) { .type = MATCH_INPUT_NONE } });
        }
    }

    // Init camera
    for (const Entity& entity : state.match.entities) {
        // TODO choose the first Town Hall instead
        if (entity.type == ENTITY_MINER && entity.player_id == network_get_player_id()) {
            match_ui_center_camera_on_cell(state, entity.cell);
            break;
        }
    }

    network_set_player_ready(true);

    return state;
}

// HANDLE EVENT

void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_INPUT: {
            // Deserialize input
            std::vector<MatchInput> inputs;

            const uint8_t* in_buffer = event.input.in_buffer;
            size_t in_buffer_head = 1; // Advance the head by once since the first byte will contain the network message type

            while (in_buffer_head < event.input.in_buffer_length) {
                inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
            }

            state.inputs[event.input.player_id].push_back(inputs);
            break;
        }
        default:
            break;
    }
}

// UPDATE

void match_ui_handle_left_mouse_press(MatchUiState& state) {
    // Begin minimap drag
    if (MINIMAP_RECT.has_point(input_get_mouse_position())) {
        state.is_minimap_dragging = true;
        return;
    }

    // Begin selecting
    if (!match_ui_is_mouse_in_ui()) {
        state.select_origin = input_get_mouse_position() + state.camera_offset;
        return;
    }
}

void match_ui_handle_left_mouse_release(MatchUiState& state) {
    // End minimap drag
    if (state.is_minimap_dragging) {
        state.is_minimap_dragging = false;
        return;
    }

    // End selecting
    if (match_ui_is_selecting(state)) {
        ivec2 world_pos = input_get_mouse_position() + state.camera_offset;
        Rect select_rect = (Rect) {
            .x = std::min(world_pos.x, state.select_origin.x),
            .y = std::min(world_pos.y, state.select_origin.y),
            .w = std::max(1, std::abs(state.select_origin.x - world_pos.x)),
            .h = std::max(1, std::abs(state.select_origin.y - world_pos.y)),
        };

        std::vector<EntityId> selection = match_ui_create_selection(state, select_rect);
        match_ui_set_selection(state, selection);
        state.select_origin = ivec2(-1, -1);

        return;
    }
}

void match_ui_update(MatchUiState& state) {
    if (state.mode == MATCH_UI_MODE_NOT_STARTED) {
        // Check that all players are ready
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                return;
            }
        }
        // If we reached here, then all players are ready
        state.mode = MATCH_UI_MODE_NONE;
        log_info("Match started.");
    }

    // Turn loop
    if (state.turn_timer == 0) {
        bool all_inputs_received = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
                continue;
            }

            if (state.inputs[player_id].empty() || state.inputs[player_id][0].empty()) {
                all_inputs_received = false;
                continue;
            }
        }

        if (!all_inputs_received) {
            state.disconnect_timer++;
            return;
        }

        // Reset the disconnect timer if we recevied inputs
        state.disconnect_timer = 0;

        // All inputs received. Begin next turn
        state.turn_timer = TURN_DURATION;
        state.turn_counter++;

        // Handle input
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
                continue;
            }

            for (const MatchInput& input : state.inputs[player_id][0]) {
                match_handle_input(state.match, input);
            }
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        }

        // Flush input
        // Always send at least one input per turn
        if (state.input_queue.empty()) {
            state.input_queue.push_back((MatchInput) { .type = MATCH_INPUT_NONE });
        }

        // Serialize the inputs
        uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
        size_t out_buffer_length = 1;
        for (const MatchInput& input : state.input_queue) {
            match_input_serialize(out_buffer, out_buffer_length, input);
        }
        state.inputs[network_get_player_id()].push_back(state.input_queue);
        state.input_queue.clear();

        // Send inputs to other players
        network_send_input(out_buffer, out_buffer_length);
    } 
    // End if tick timer is 0

    state.turn_timer--;

    // Order movement
    if (input_is_action_just_pressed(INPUT_RIGHT_CLICK) && 
            match_ui_get_selection_type(state, state.selection) == MATCH_UI_SELECTION_UNITS &&
            state.mode != MATCH_UI_MODE_BUILDING_PLACE && !state.is_minimap_dragging &&
            (MINIMAP_RECT.has_point(input_get_mouse_position()) || !match_ui_is_mouse_in_ui())) {
        match_ui_order_move(state);
    }

    // Handle input
    if (input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
        match_ui_handle_left_mouse_press(state);
    }
    if (input_is_action_just_released(INPUT_LEFT_CLICK)) {
        match_ui_handle_left_mouse_release(state);
    }

    // Match update
    match_update(state.match);

    // Match events
    while (!state.match.events.empty()) {
        MatchEvent event = state.match.events[0];
        state.match.events.erase(state.match.events.begin());
        switch (event.type) {
            case MATCH_EVENT_SOUND: {
                if (state.sound_cooldown_timers[event.sound.sound] != 0) {
                    break;
                }
                Rect listen_rect = (Rect) {
                    .x = state.camera_offset.x - SOUND_LISTEN_MARGIN,
                    .y = state.camera_offset.y - SOUND_LISTEN_MARGIN,
                    .w = SCREEN_WIDTH + (SOUND_LISTEN_MARGIN * 2),
                    .h = SCREEN_HEIGHT + (SOUND_LISTEN_MARGIN * 2),
                };
                if (listen_rect.has_point(event.sound.position)) {
                    sound_play(event.sound.sound);
                    state.sound_cooldown_timers[event.sound.sound] = SOUND_COOLDOWN_DURATION;
                }
                
                break;
            }
        }
    }

    // Camera drag
    if (!match_ui_is_selecting(state)) {
        ivec2 camera_drag_direction = ivec2(0, 0);
        if (input_get_mouse_position().x < MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (input_get_mouse_position().x > SCREEN_WIDTH - MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (input_get_mouse_position().y < MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (input_get_mouse_position().y > SCREEN_HEIGHT - MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        state.camera_offset += camera_drag_direction * CAMERA_SPEED;
        match_ui_clamp_camera(state);
    }

    // Minimap drag
    if (state.is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (state.match.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state.match.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        match_ui_center_camera_on_cell(state, map_pos / TILE_SIZE);
    }

    // Update timers
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        if (state.sound_cooldown_timers[sound] != 0) {
            state.sound_cooldown_timers[sound]--;
        }
    }

    // Clear hidden units from selection
    {
        int selection_index = 0;
        while (selection_index < state.selection.size()) {
            Entity& selected_entity = state.match.entities.get_by_id(state.selection[selection_index]);
            if (!entity_is_selectable(selected_entity) || !match_is_entity_visible_to_player(state.match, selected_entity, network_get_player_id())) {
                state.selection.erase(state.selection.begin() + selection_index);
            } else {
                selection_index++;
            }
        }
    }

    // Update Minimap
    uint8_t player_team = network_get_player(network_get_player_id()).team;
    for (int y = 0; y < state.match.map.height; y++) {
        for (int x = 0; x < state.match.map.width; x++) {
            MinimapPixel pixel; 
            Tile tile = map_get_tile(state.match.map, ivec2(x, y));
            if (tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_SAND3) {
                pixel = MINIMAP_PIXEL_SAND;
            } else if (tile.sprite == SPRITE_TILE_WATER) {
                pixel = MINIMAP_PIXEL_WATER;
            } else {
                pixel = MINIMAP_PIXEL_WALL;
            }
            render_minimap_putpixel(MINIMAP_LAYER_TILE, ivec2(x, y), pixel);

            MinimapPixel fog_pixel;
            int fog_value = match_get_fog(state.match, player_team, ivec2(x, y));
            if (fog_value == FOG_HIDDEN) {
                fog_pixel = MINIMAP_PIXEL_OFFBLACK;
            } else if (fog_value == FOG_EXPLORED) {
                fog_pixel = MINIMAP_PIXEL_OFFBLACK_TRANSPARENT;
            } else {
                fog_pixel = MINIMAP_PIXEL_TRANSPARENT;
            }
            render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), fog_pixel);
        }
    }
    // Minimap entities
    for (const Entity& entity : state.match.entities) {
        if (!entity_is_selectable(entity) || !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        MinimapPixel pixel = entity.type == ENTITY_GOLDMINE ? MINIMAP_PIXEL_GOLD : (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + entity.player_id);
        int entity_cell_size = entity_get_data(entity.type).cell_size;
        Rect entity_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, pixel);
    }
    // Minimap camera rect
    Rect camera_rect = (Rect) {
        .x = state.camera_offset.x / TILE_SIZE,
        .y = state.camera_offset.y / TILE_SIZE,
        .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
        .h = ((SCREEN_HEIGHT - MATCH_UI_HEIGHT) / TILE_SIZE) - 1
    };
    render_minimap_draw_rect(MINIMAP_LAYER_FOG, camera_rect, MINIMAP_PIXEL_WHITE);
    render_update_minimap_texture();
}

// UPDATE FUNCTIONS

void match_ui_clamp_camera(MatchUiState& state) {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, ((int)state.match.map.width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, ((int)state.match.map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_UI_HEIGHT);
}

void match_ui_center_camera_on_cell(MatchUiState& state, ivec2 cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    match_ui_clamp_camera(state);
}

bool match_ui_is_mouse_in_ui() {
    ivec2 mouse_position = input_get_mouse_position();
    return (mouse_position.y >= SCREEN_HEIGHT - MATCH_UI_HEIGHT) ||
           (mouse_position.x <= 136 && mouse_position.y >= SCREEN_HEIGHT - 136) ||
           (mouse_position.x >= SCREEN_WIDTH - 132 && mouse_position.y >= SCREEN_HEIGHT - 106);
}

bool match_ui_is_selecting(const MatchUiState& state) {
    return state.select_origin.x != -1;
}

std::vector<EntityId> match_ui_create_selection(const MatchUiState& state, Rect select_rect) {
    std::vector<EntityId> selection;

    // Select player units
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id != network_get_player_id() ||
                !entity_is_unit(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select player buildings
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id != network_get_player_id() ||
                entity_is_unit(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select enemy units
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id == network_get_player_id() ||
                !entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select enemy buildings
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id == network_get_player_id() ||
                entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select gold mines
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.type != ENTITY_GOLDMINE ||
                !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }

    return selection;
}

void match_ui_set_selection(MatchUiState& state, std::vector<EntityId>& selection) {
    state.selection = selection;
}

MatchUiSelectionType match_ui_get_selection_type(const MatchUiState& state, const std::vector<EntityId>& selection) {
    if (selection.empty()) {
        return MATCH_UI_SELECTION_NONE;
    }

    const Entity& entity = state.match.entities.get_by_id(selection[0]);
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_UI_SELECTION_UNITS;
        } else {
            return MATCH_UI_SELECTION_ENEMY_UNIT;
        }
    } else if (entity_is_building(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_UI_SELECTION_BUILDINGS;
        } else {
            return MATCH_UI_SELECTION_ENEMY_BUILDING;
        }
    } else {
        return MATCH_UI_SELECTION_GOLD;
    }
}

void match_ui_order_move(MatchUiState& state) {
    // Determine move target
    ivec2 move_target;
    if (match_ui_is_mouse_in_ui()) {
        ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
        move_target = ivec2((state.match.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                            (state.match.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
    } else {
        move_target = input_get_mouse_position() + state.camera_offset;
    }

    // Create move input
    MatchInput input;
    input.move.shift_command = input_is_action_pressed(INPUT_SHIFT);
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;
    /*
    int fog_value = map_get_fog(state.match_state, network_get_player_id(), input.move.target_cell);
    // don't target enemies in hidden fog or when minimap clicking
    if (fog_value != FOG_HIDDEN && !ui_is_mouse_in_ui()) {
        for (uint32_t entity_index = 0; entity_index < state.match_state.entities.size(); entity_index++) {
            const Entity& entity = state.match.entities[entity_index];
            // don't target unselectable entities, unless it's gold and the player doesn't know that the gold is unselectable
            // It's also saying, don't target a hidden mine
            if (!entity_is_selectable(entity) || 
                    (fog_value == FOG_EXPLORED && entity.type != ENTITY_GOLD_MINE) ||
                    (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && network_get_player(entity.player_id).team != network_get_player(network_get_player_id()).team && !map_is_cell_detected(state.match_state, network_get_player_id(), entity.cell))) {
                continue;
            }

            Rect entity_rect = entity_get_rect(state.match.entities[entity_index]);
            if (entity_rect.has_point(move_target)) {
                input.move.target_id = state.match.entities.get_id_of(entity_index);
                break;
            }
        }
    }
    */

    if (state.mode == MATCH_UI_MODE_TARGET_UNLOAD) {
        input.type = MATCH_INPUT_MOVE_UNLOAD;
    } else if (state.mode == MATCH_UI_MODE_TARGET_REPAIR) {
        input.type = MATCH_INPUT_MOVE_REPAIR;
    } else if (state.mode == MATCH_UI_MODE_TARGET_SMOKE) {
        input.type = MATCH_INPUT_MOVE_SMOKE;
    } else if (input.move.target_id != ID_NULL && state.match.entities.get_by_id(input.move.target_id).type != ENTITY_GOLDMINE &&
               (state.mode == MATCH_UI_MODE_TARGET_ATTACK || 
                network_get_player(state.match.entities.get_by_id(input.move.target_id).player_id).team != network_get_player(network_get_player_id()).team)) {
        input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && state.mode == MATCH_UI_MODE_TARGET_ATTACK) {
        input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    } else if (input.move.target_id != ID_NULL) {
        input.type = MATCH_INPUT_MOVE_ENTITY;
    } else {
        input.type = MATCH_INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = (uint16_t)state.selection.size();
    memcpy(input.move.entity_ids, &state.selection[0], state.selection.size() * sizeof(EntityId));

    if (input.type == MATCH_INPUT_MOVE_REPAIR) {
        bool is_repair_target_valid = true;
        if (input.move.target_id == ID_NULL) {
            is_repair_target_valid = false;
        } else {
            const Entity& repair_target = state.match.entities.get_by_id(input.move.target_id);
            if (repair_target.player_id != network_get_player_id() || !entity_is_building(repair_target.type)) {
                is_repair_target_valid = false;
            }
        }

        if (!is_repair_target_valid) {
            match_ui_show_status(state, MATCH_UI_STATUS_REPAIR_TARGET_INVALID);
            state.mode = MATCH_UI_MODE_NONE;
            return;
        }
    }

    state.input_queue.push_back(input);
}

void match_ui_show_status(MatchUiState& state, const char* message) {
    state.status_message = std::string(message);
    state.status_timer = UI_STATUS_DURATION;
}

// RENDER

void match_ui_render(const MatchUiState& state) {
    static std::vector<RenderSpriteParams> render_sprite_params[RENDER_TOTAL_LAYER_COUNT];

    ivec2 base_pos = ivec2(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - MATCH_UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Render map
    for (int y = 0; y < max_visible_tiles.y; y++) {
        for (int x = 0; x < max_visible_tiles.x; x++) {
            int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.match.map.width);
            Tile tile = state.match.map.tiles[map_index];
            // Render sand below walls
            if (!(tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_WATER)) {
                render_sprite_params[match_ui_get_render_layer(tile.elevation == 0 ? 0 : tile.elevation - 1, RENDER_LAYER_TILE)].push_back((RenderSpriteParams) {
                    .sprite = SPRITE_TILE_SAND1,
                    .frame = ivec2(0, 0),
                    .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                });
            }

            // Render tile
            render_sprite_params[match_ui_get_render_layer(tile.elevation, RENDER_LAYER_TILE)].push_back((RenderSpriteParams) {
                .sprite = tile.sprite,
                .frame = tile.frame,
                .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                .options = RENDER_SPRITE_NO_CULL,
                .recolor_id = 0
            });

            // Decorations
            Cell cell = state.match.map.cells[CELL_LAYER_GROUND][map_index];
            if (cell.type >= CELL_DECORATION_1 && cell.type <= CELL_DECORATION_5) {
                render_sprite_params[match_ui_get_render_layer(tile.elevation, RENDER_LAYER_ENTITY)].push_back((RenderSpriteParams) {
                    .sprite = SPRITE_DECORATION,
                    .frame = ivec2(cell.type - CELL_DECORATION_1, 0),
                    .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                });
            }
        }  // End for each x
    } // End for each y

    // Select rings and healthbars
    for (EntityId id : state.selection) {
        const Entity& entity = state.match.entities.get_by_id(id);

        // Select ring
        SpriteName select_ring_sprite = SPRITE_SELECT_RING_GOLDMINE;
        if (entity_is_unit(entity.type)) {
            select_ring_sprite = SPRITE_SELECT_RING_UNIT;
        }
        if (select_ring_sprite != SPRITE_SELECT_RING_GOLDMINE && entity.player_id != network_get_player_id()) {
            select_ring_sprite = (SpriteName)(select_ring_sprite + 1);
        }

        Rect entity_rect = entity_get_rect(entity);
        ivec2 entity_center_position = entity_is_unit(entity.type) 
                ? entity.position.to_ivec2()
                : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2)); 
        entity_center_position -= state.camera_offset;
        uint16_t entity_elevation = entity_get_elevation(entity, state.match.map);
        render_sprite_params[match_ui_get_render_layer(entity_elevation, RENDER_LAYER_SELECT_RING)].push_back((RenderSpriteParams) {
            .sprite = select_ring_sprite,
            .frame = ivec2(0, 0),
            .position = entity_center_position,
            .options = RENDER_SPRITE_CENTERED,
            .recolor_id = 0
        });
    }

    // Entities
    for (const Entity& entity : state.match.entities) {
        const EntityData& entity_data = entity_get_data(entity.type);

        RenderSpriteParams params = (RenderSpriteParams) {
            .sprite = entity_data.sprite,
            .frame = entity_get_animation_frame(entity),
            .position = entity.position.to_ivec2() - state.camera_offset,
            .options = RENDER_SPRITE_NO_CULL,
            .recolor_id = entity.type == ENTITY_GOLDMINE ? 0 : state.match.players[entity.player_id].recolor_id
        };

        const SpriteInfo& sprite_info = render_get_sprite_info(params.sprite);
        if (entity_is_unit(entity.type)) {
            params.position.x -= sprite_info.frame_width / 2;
            params.position.y -= sprite_info.frame_height / 2;
            if (entity.direction > DIRECTION_SOUTH) {
                params.options |= RENDER_SPRITE_FLIP_H;
            }
        }

        Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };
        if (!render_rect.intersects(SCREEN_RECT)) {
            continue;
        }

        render_sprite_params[match_ui_get_render_layer(entity_get_elevation(entity, state.match.map), RENDER_LAYER_ENTITY)].push_back(params);
    }

    // Render sprite params
    for (int render_layer = 0; render_layer < RENDER_TOTAL_LAYER_COUNT; render_layer++) {
        // If this layer is one of the entity layers, y sort it
        int elevation = render_layer / RENDER_LAYER_COUNT;
        int render_layer_without_elevation = render_layer - (elevation * RENDER_LAYER_COUNT);
        if (render_layer_without_elevation == RENDER_LAYER_ENTITY) {
            match_ui_ysort_render_params(render_sprite_params[render_layer], 0, render_sprite_params[render_layer].size() - 1);
        }

        for (const RenderSpriteParams& params : render_sprite_params[render_layer]) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
        render_sprite_params[render_layer].clear();
    }

    // Fog of War
    int player_team = network_get_player(network_get_player_id()).team;
    for (int fog_pass = 0; fog_pass < 2; fog_pass++) {
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                ivec2 fog_cell = base_coords + ivec2(x, y);
                int fog = match_get_fog(state.match, player_team, fog_cell);
                if (fog > 0) {
                    continue;
                }
                if (fog_pass == 1 && fog == FOG_EXPLORED) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(state.match.map, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if ((fog_pass == 0 && match_get_fog(state.match, player_team, neighbor_cell) < 1) ||
                        (fog_pass != 0 && match_get_fog(state.match, player_team, neighbor_cell) == FOG_HIDDEN)) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }

                for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                    int prev_direction = direction - 1;
                    int next_direction = (direction + 1) % DIRECTION_COUNT;
                    if ((neighbors & DIRECTION_MASK[prev_direction]) != DIRECTION_MASK[prev_direction] ||
                        (neighbors & DIRECTION_MASK[next_direction]) != DIRECTION_MASK[next_direction]) {
                        continue;
                    }
                    if (!map_is_cell_in_bounds(state.match.map, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if ((fog_pass == 0 && match_get_fog(state.match, player_team, neighbor_cell) < 1) ||
                        (fog_pass != 0 && match_get_fog(state.match, player_team, neighbor_cell) == FOG_HIDDEN)) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                int autotile_index = map_neighbors_to_autotile_index(neighbors);
                #ifndef GOLD_DEBUG_FOG_DISABLED
                    render_sprite_frame(
                            fog_pass == 0 ? SPRITE_FOG_EXPLORED : SPRITE_FOG_HIDDEN, 
                            ivec2(autotile_index % AUTOTILE_HFRAMES, autotile_index / AUTOTILE_HFRAMES), 
                            base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                #endif
            }
        }
    }

    // Select rect
    if (match_ui_is_selecting(state)) {
        ivec2 mouse_world_pos = ivec2(input_get_mouse_position().x, std::min(input_get_mouse_position().y, SCREEN_HEIGHT - MATCH_UI_HEIGHT)) + state.camera_offset;
        ivec2 rect_start = ivec2(std::min(state.select_origin.x, mouse_world_pos.x), std::min(state.select_origin.y, mouse_world_pos.y)) - state.camera_offset;
        ivec2 rect_end = ivec2(std::max(state.select_origin.x, mouse_world_pos.x), std::max(state.select_origin.y, mouse_world_pos.y)) - state.camera_offset;
        render_rect(rect_start, rect_end);
    }

    // UI frames
    const SpriteInfo& minimap_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
    const SpriteInfo& bottom_panel_sprite_info = render_get_sprite_info(SPRITE_UI_BOTTOM_PANEL);
    const SpriteInfo& button_panel_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_PANEL);
    render_sprite_frame(SPRITE_UI_MINIMAP, ivec2(0, 0), ivec2(0, SCREEN_HEIGHT - minimap_sprite_info.frame_height), 0, 0);
    render_sprite_frame(SPRITE_UI_BOTTOM_PANEL, ivec2(0, 0), ivec2(minimap_sprite_info.frame_width, SCREEN_HEIGHT - bottom_panel_sprite_info.frame_height), 0, 0);
    render_sprite_frame(SPRITE_UI_BUTTON_PANEL, ivec2(0, 0), ivec2(minimap_sprite_info.frame_width + bottom_panel_sprite_info.frame_width, SCREEN_HEIGHT - button_panel_sprite_info.frame_height), 0, 0);

    render_sprite_batch();

    render_minimap(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state.match.map.width, state.match.map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
}

// RENDER FUNCTIONS

uint16_t match_ui_get_render_layer(uint16_t elevation, RenderLayer layer) {
    return (elevation * RENDER_LAYER_COUNT) + layer;
}

int match_ui_ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high) {
    RenderSpriteParams pivot = params[high];
    int i = low - 1;

    for (int j = low; j <= high - 1; j++) {
        if (params[j].position.y < pivot.position.y) {
            i++;
            RenderSpriteParams temp = params[j];
            params[j] = params[i];
            params[i] = temp;
        }
    }

    RenderSpriteParams temp = params[high];
    params[high] = params[i + 1];
    params[i + 1] = temp;

    return i + 1;
}

void match_ui_ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high) {
    if (low < high) {
        int partition_index = match_ui_ysort_render_params_partition(params, low, high);
        match_ui_ysort_render_params(params, low, partition_index - 1);
        match_ui_ysort_render_params(params, partition_index + 1, high);
    }
}
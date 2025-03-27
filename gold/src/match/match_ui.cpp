#include "match_ui.h"

#include "core/network.h"
#include "core/input.h"
#include "core/logger.h"
#include "menu/match_setting.h"
#include "render/sprite.h"
#include "render/render.h"
#include <algorithm>

static const int MATCH_CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_SPEED = 16; // TODO: move to options
static const Rect SCREEN_RECT = (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
static const Rect MINIMAP_RECT = (Rect) { .x = 4, .y = SCREEN_HEIGHT - 132, .w = 128, .h = 128 };

// INIT

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise) {
    MatchUiState state;

    state.mode = MATCH_UI_MODE_NOT_STARTED;
    state.camera_offset = ivec2(0, 0);
    state.select_origin = ivec2(-1, -1);

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

    // Init minimap texture
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
            render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), MINIMAP_PIXEL_TRANSPARENT);
        }
    }

    network_set_player_ready(true);

    return state;
}

// HANDLE EVENT

void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event) {

}

// UPDATE

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

    // Handle input
    if (input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
        // Begin selecting
        state.select_origin = input_get_mouse_position() + state.camera_offset;
    }
    if (input_is_action_just_released(INPUT_LEFT_CLICK)) {
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
    }

    match_update(state.match);

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
            EntityId cell = state.match.map.cells[map_index];
            if (cell >= CELL_DECORATION_1 && cell <= CELL_DECORATION_5) {
                render_sprite_params[match_ui_get_render_layer(tile.elevation, RENDER_LAYER_ENTITY)].push_back((RenderSpriteParams) {
                    .sprite = SPRITE_DECORATION,
                    .frame = ivec2(cell - CELL_DECORATION_1, 0),
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
            .frame = ivec2(0, 0),
            .position = entity.position.to_ivec2() - state.camera_offset,
            .options = RENDER_SPRITE_NO_CULL,
            .recolor_id = entity.type == ENTITY_GOLDMINE ? 0 : state.match.players[entity.player_id].recolor_id
        };

        const SpriteInfo& sprite_info = render_get_sprite_info(params.sprite);
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

    // Minimap
    for (int y = 0; y < state.match.map.height; y++) {
        for (int x = 0; x < state.match.map.width; x++) {
            render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), MINIMAP_PIXEL_TRANSPARENT);
        }
    }
    // Minimap camera rect
    Rect camera_rect = (Rect) {
        .x = state.camera_offset.x / TILE_SIZE,
        .y = state.camera_offset.y / TILE_SIZE,
        .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
        .h = ((SCREEN_HEIGHT - MATCH_UI_HEIGHT) / TILE_SIZE) - 1
    };
    for (int y = camera_rect.y; y < camera_rect.y + camera_rect.h + 1; y++) {
        render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(camera_rect.x, y), MINIMAP_PIXEL_WHITE);
        render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(camera_rect.x + camera_rect.w, y), MINIMAP_PIXEL_WHITE);
    }
    for (int x = camera_rect.x + 1; x < camera_rect.x + camera_rect.w; x++) {
        render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, camera_rect.y), MINIMAP_PIXEL_WHITE);
        render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, camera_rect.y + camera_rect.h), MINIMAP_PIXEL_WHITE);
    }
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
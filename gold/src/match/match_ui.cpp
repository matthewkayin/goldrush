#include "match_ui.h"

#include "core/network.h"
#include "core/input.h"
#include "core/logger.h"
#include "render/sprite.h"
#include "render/render.h"
#include <algorithm>

static const int MATCH_CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_SPEED = 16; // TODO: move to options

enum RenderLayer {
    RENDER_LAYER_TILE,
    RENDER_LAYER_MINE_SELECT_RING,
    RENDER_LAYER_MINE,
    RENDER_LAYER_SELECT_RING,
    RENDER_LAYER_ENTITY,
    RENDER_LAYER_COUNT
};
static const uint16_t ELEVATION_COUNT = 3;
static const uint32_t RENDER_TOTAL_LAYER_COUNT = (RENDER_LAYER_COUNT * ELEVATION_COUNT);

struct RenderSpriteParams {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    uint32_t options;
    int recolor_id;
};

void match_ui_clamp_camera(MatchUiState& state);

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise) {
    MatchUiState state;

    state.mode = MATCH_UI_MODE_NOT_STARTED;
    state.camera_offset = ivec2(0, 0);
    state.match = match_init(lcg_seed, noise);

    network_set_player_ready(true);

    return state;
}

void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event) {

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

    match_update(state.match);

    // Camera drag
    ivec2 camera_drag_direction = ivec2(0, 0);
    ivec2 mouse_position = input_get_mouse_position();
    if (mouse_position.x < MATCH_CAMERA_DRAG_MARGIN) {
        camera_drag_direction.x = -1;
    } else if (mouse_position.x > SCREEN_WIDTH - MATCH_CAMERA_DRAG_MARGIN) {
        camera_drag_direction.x = 1;
    }
    if (mouse_position.y < MATCH_CAMERA_DRAG_MARGIN) {
        camera_drag_direction.y = -1;
    } else if (mouse_position.y > SCREEN_HEIGHT - MATCH_CAMERA_DRAG_MARGIN) {
        camera_drag_direction.y = 1;
    }
    state.camera_offset += camera_drag_direction * CAMERA_SPEED;
    match_ui_clamp_camera(state);
}

void match_ui_clamp_camera(MatchUiState& state) {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, ((int)state.match.map.width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, ((int)state.match.map.width * TILE_SIZE) - SCREEN_WIDTH + MATCH_UI_HEIGHT);
}

uint16_t match_get_render_layer(uint16_t elevation, RenderLayer layer) {
    return (elevation * RENDER_LAYER_COUNT) + layer;
}

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
            render_sprite_params[match_get_render_layer(tile.elevation, RENDER_LAYER_TILE)].push_back((RenderSpriteParams) {
                .sprite = (SpriteName)tile.sprite,
                .frame = ivec2(tile.autotile_index, 0),
                .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                .options = RENDER_SPRITE_NO_CULL,
                .recolor_id = 0
            });

            // Decorations
            EntityId cell = state.match.map.cells[map_index];
            if (cell >= CELL_DECORATION_1 && cell <= CELL_DECORATION_5) {
                render_sprite_params[match_get_render_layer(tile.elevation, RENDER_LAYER_ENTITY)].push_back((RenderSpriteParams) {
                    .sprite = (SpriteName)(SPRITE_TILE_DECORATION0 + (cell - CELL_DECORATION_1)),
                    .frame = ivec2(0, 0),
                    .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                });
            }
        }  // End for each x
    } // End for each y

    // TODO ysort entity layer
    for (int render_layer = 0; render_layer < RENDER_TOTAL_LAYER_COUNT; render_layer++) {
        for (const RenderSpriteParams& params : render_sprite_params[render_layer]) {
            render_sprite(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
        render_sprite_params[render_layer].clear();
    }
}
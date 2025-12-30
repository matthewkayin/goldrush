#include "editor.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"
#include "core/input.h"
#include "match/map.h"
#include "container/id_array.h"
#include "match/state.h"
#include "core/options.h"
#include "render/render.h"
#include "shell/ysort.h"
#include <algorithm>
#include <vector>
#include <string>

// Camera
static const int CAMERA_DRAG_MARGIN = 4;

// Console
static const size_t CONSOLE_MESSAGE_MAX_LENGTH = 256;
static const uint32_t CONSOLE_CURSOR_BLINK_DURATION = 30;
static const ivec2 CONSOLE_MESSAGE_POSITION = ivec2(8, SCREEN_HEIGHT - 24);

struct EditorState {
    Map map;
    IdArray<Entity, MATCH_MAX_ENTITIES> entities;
    MatchPlayer players[MAX_PLAYERS];

    ivec2 camera_offset;
    bool is_minimap_dragging;

    std::string console_message;
    bool console_cursor_visible;
    uint32_t console_cursor_blink_timer;
};
static EditorState state;

void editor_clamp_camera();
void editor_handle_input();
void editor_handle_console_message(const std::vector<std::string>& words);

RenderSpriteParams editor_create_entity_render_params(const Entity& entity);

void editor_init() {
    map_init(state.map, MAP_TYPE_TOMBSTONE, 96, 96);

    memset(state.players, 0, sizeof(state.players));
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        MatchPlayer& player = state.players[player_id];
        player.team = (uint32_t)player_id;
        player.recolor_id = (int)player_id;
    }

    if (option_get_value(OPTION_DISPLAY) != RENDER_DISPLAY_WINDOWED) {
        option_set_value(OPTION_DISPLAY, RENDER_DISPLAY_WINDOWED);
    }

    state.camera_offset = ivec2(0, 0);
    state.is_minimap_dragging = false;

    log_info("Initialized map editor.");
}

void editor_update() {
    // Camera drag
    if (!state.is_minimap_dragging) {
        ivec2 camera_drag_direction = ivec2(0, 0);
        if (input_get_mouse_position().x < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (input_get_mouse_position().x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (input_get_mouse_position().y < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (input_get_mouse_position().y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        state.camera_offset += camera_drag_direction * option_get_value(OPTION_CAMERA_SPEED);
        editor_clamp_camera();
    }

    // Update console cursor blinker
    if (input_is_text_input_active()) {
        state.console_cursor_blink_timer--;
        if (state.console_cursor_blink_timer == 0) {
            state.console_cursor_visible = !state.console_cursor_visible;
            state.console_cursor_blink_timer = CONSOLE_CURSOR_BLINK_DURATION;
        }
    }

    editor_handle_input();
}

void editor_clamp_camera() {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, (state.map.width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, (state.map.height * TILE_SIZE) - SCREEN_HEIGHT);
}

void editor_handle_input() {
    // Begin chat
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && !input_is_text_input_active()) {
        state.console_message = "";
        state.console_cursor_blink_timer = CONSOLE_CURSOR_BLINK_DURATION;
        state.console_cursor_visible = true;
        input_start_text_input(&state.console_message, CONSOLE_MESSAGE_MAX_LENGTH);
        sound_play(SOUND_UI_CLICK);
        return;
    }

    // End chat
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && input_is_text_input_active()) {
        std::vector<std::string> console_message_words;
        while (!state.console_message.empty()) {
            size_t space_index = state.console_message.find(' ');
            std::string word;
            if (space_index == std::string::npos) {
                word = state.console_message;
                state.console_message = "";
            } else {
                word = state.console_message.substr(0, space_index);
                state.console_message = state.console_message.substr(space_index + 1);
            }
            console_message_words.push_back(word);
        }

        if (!console_message_words.empty()) {
            editor_handle_console_message(console_message_words);
        }

        if (state.console_message == "/")
        sound_play(SOUND_UI_CLICK);
        input_stop_text_input();
        return;
    }
}

void editor_handle_console_message(const std::vector<std::string>& words) {
    GOLD_ASSERT(!words.empty());
}

void editor_render() {
    std::vector<RenderSpriteParams> ysort_params;

    ivec2 base_pos = ivec2(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT / TILE_SIZE) + 1);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Begin elevation passes
    static const int ELEVATION_COUNT = 2;
    for (uint32_t elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        // Render map
        for (int y = 0; y < max_visible_tiles.y; y++) {
            if (base_coords.y + y >= state.map.height) {
                continue;
            }
            for (int x = 0; x < max_visible_tiles.x; x++) {
                int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.map.width);
                Tile tile = state.map.tiles[map_index];

                ivec2 tile_params_position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE);
                RenderSpriteParams tile_params = (RenderSpriteParams) {
                    .sprite = tile.sprite,
                    .frame = tile.frame,
                    .position = tile_params_position,
                    .ysort_position = tile_params_position.y,
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                };

                bool should_render_on_ground_level = 
                    map_is_tile_ground(state.map, base_coords + ivec2(x, y)) || 
                    map_is_tile_ramp(state.map, base_coords + ivec2(x, y));
                if (elevation == 0 && 
                        !map_is_tile_ground(state.map, base_coords + ivec2(x, y)) &&
                        !map_is_tile_water(state.map, base_coords + ivec2(x, y))) {
                    render_sprite_frame(map_get_plain_ground_tile_sprite(state.map.type), ivec2(0, 0), base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                }
                if ((should_render_on_ground_level && elevation == 0) || 
                        (!should_render_on_ground_level && elevation == tile.elevation)) {
                    render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                } 

                // Decorations
                Cell cell = state.map.cells[CELL_LAYER_GROUND][map_index];
                if (cell.type == CELL_DECORATION && tile.elevation == elevation) {
                    SpriteName decoration_sprite = map_get_decoration_sprite(state.map.type);
                    const SpriteInfo& decoration_sprite_info = render_get_sprite_info(decoration_sprite);
                    const int decoration_extra_height = decoration_sprite_info.frame_height - TILE_SIZE;
                    ysort_params.push_back((RenderSpriteParams) {
                        .sprite = decoration_sprite,
                        .frame = ivec2(cell.decoration_hframe, 0),
                        .position = ivec2(tile_params_position.x, tile_params_position.y - decoration_extra_height),
                        .ysort_position = tile_params_position.y,
                        .options = RENDER_SPRITE_NO_CULL,
                        .recolor_id = 0
                    });
                }
            }  // End for each x
        } // End for each y

        // For each cell layer
        for (int cell_layer = CELL_LAYER_UNDERGROUND; cell_layer < CELL_LAYER_GROUND + 1; cell_layer++) {
            // TODO: render select ring of selected entity
            // Select rings and healthbars

            // Underground entities
            if (cell_layer == CELL_LAYER_UNDERGROUND) {
                for (const Entity& entity : state.entities) {
                    const EntityData& entity_data = entity_get_data(entity.type);
                    if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                            entity_get_elevation(entity, state.map) != elevation) {
                        continue;
                    }
                    if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
                        continue;
                    }

                    RenderSpriteParams params = editor_create_entity_render_params(entity);
                    render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                }
            }
        } // End for each cell layer
    }

    // Console
    if (input_is_text_input_active()) {
        render_text(FONT_HACK_SHADOW, state.console_message.c_str(), CONSOLE_MESSAGE_POSITION + ivec2(1, 1));
        render_text(FONT_HACK_WHITE, state.console_message.c_str(), CONSOLE_MESSAGE_POSITION);
        if (state.console_cursor_visible) {
            int prompt_width = render_get_text_size(FONT_HACK_WHITE, state.console_message.c_str()).x;
            ivec2 cursor_pos = CONSOLE_MESSAGE_POSITION + ivec2(prompt_width - 1, -1);
            render_text(FONT_HACK_SHADOW, "|", cursor_pos + ivec2(1, 1));
            render_text(FONT_HACK_WHITE, "|", cursor_pos);
        }
    }
}

RenderSpriteParams editor_create_entity_render_params(const Entity& entity) {
    ivec2 params_position = entity.position.to_ivec2() - state.camera_offset;
    RenderSpriteParams params = (RenderSpriteParams) {
        .sprite = entity_get_data(entity.type).sprite,
        .frame = entity_get_animation_frame(entity),
        .position = params_position,
        .ysort_position = params_position.y,
        .options = 0,
        .recolor_id = entity.type == ENTITY_GOLDMINE || entity.mode == MODE_BUILDING_DESTROYED ? 0 : state.players[entity.player_id].recolor_id
    };

    const SpriteInfo& sprite_info = render_get_sprite_info(params.sprite);

    params.position.x -= sprite_info.frame_width / 2;
    params.position.y -= sprite_info.frame_height / 2;
    if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
        if (entity.mode == MODE_UNIT_BALLOON_DEATH) {
            params.position.y += ((int)entity.timer * ENTITY_SKY_POSITION_Y_OFFSET) / (int)ENTITY_BALLOON_DEATH_DURATION;
        } else {
            params.position.y += ENTITY_SKY_POSITION_Y_OFFSET;
        }
    }
    if (entity.direction > DIRECTION_SOUTH) {
        params.options |= RENDER_SPRITE_FLIP_H;
    }

    return params;
}

#endif
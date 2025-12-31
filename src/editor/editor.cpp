#include "editor.h"

#ifdef GOLD_DEBUG

#include "editor/document.h"
#include "editor/menu_new.h"
#include "editor/ui.h"
#include "editor/action.h"
#include "core/logger.h"
#include "core/input.h"
#include "core/ui.h"
#include "match/map.h"
#include "container/id_array.h"
#include "match/state.h"
#include "core/options.h"
#include "render/render.h"
#include "shell/ysort.h"
#include <algorithm>
#include <vector>
#include <string>

static const Rect TOOLBAR_RECT = (Rect) {
    .x = 0, .y = 0,
    .w = SCREEN_WIDTH, .h = 22
};
static const Rect STATUS_RECT = (Rect) {
    .x = 0, .y = SCREEN_HEIGHT - 18,
    .w = SCREEN_WIDTH, .h = 18
};
static const Rect SIDEBAR_RECT = (Rect) {
    .x = 0, .y = TOOLBAR_RECT.h,
    .w = 144,
    .h = SCREEN_HEIGHT - TOOLBAR_RECT.h - STATUS_RECT.h
};
static const Rect MINIMAP_RECT = (Rect) {
    .x = 8, .y = SCREEN_HEIGHT - 132 - 22,
    .w = 128, .h = 128
};
static const Rect CANVAS_RECT = (Rect) {
    .x = SIDEBAR_RECT.w,
    .y = TOOLBAR_RECT.y + TOOLBAR_RECT.h,
    .w = SCREEN_WIDTH - SIDEBAR_RECT.w,
    .h = SCREEN_HEIGHT - STATUS_RECT.h - TOOLBAR_RECT.h
};

enum EditorTool {
    EDITOR_TOOL_BRUSH
};

struct EditorState {
    EditorTool tool;
    UI ui;

    EditorDocument* document;

    std::vector<EditorAction> actions;

    EditorMenuNew menu_new;

    ivec2 camera_drag_previous_offset;
    ivec2 camera_drag_mouse_position;
    ivec2 camera_offset;
    bool is_minimap_dragging;
    bool is_painting;
    uint32_t tool_value;
};
static EditorState state;

// Update
bool editor_is_in_menu();
void editor_clamp_camera();
void editor_center_camera_on_cell(ivec2 cell);
void editor_handle_toolbar_action(const std::string& column, const std::string& action);
void editor_set_tool(EditorTool tool);
ivec2 editor_get_hovered_cell();

// Render
RenderSpriteParams editor_create_entity_render_params(const Entity& entity);
MinimapPixel editor_get_minimap_pixel_for_cell(ivec2 cell);
MinimapPixel editor_get_minimap_pixel_for_entity(const Entity& entity);

void editor_init() {
    input_set_mouse_capture_enabled(false);

    if (option_get_value(OPTION_DISPLAY) != RENDER_DISPLAY_WINDOWED) {
        option_set_value(OPTION_DISPLAY, RENDER_DISPLAY_WINDOWED);
    }

    state.ui = ui_init();
    state.document = editor_document_init_blank(MAP_TYPE_TOMBSTONE, MAP_SIZE_SMALL);

    state.menu_new.mode = EDITOR_MENU_NEW_CLOSED;

    state.camera_offset = ivec2(0, 0);
    state.is_minimap_dragging = false;
    state.camera_drag_mouse_position = ivec2(-1, -1);

    log_info("Initialized map editor.");
}

void editor_quit() {
    if (state.document != NULL) {
        editor_document_free(state.document);
    }
}

void editor_update() {
    ui_begin(state.ui);
    state.ui.input_enabled = 
        !state.is_minimap_dragging && 
        state.camera_drag_mouse_position.x == -1 &&
        !editor_is_in_menu();
    ui_small_frame_rect(state.ui, TOOLBAR_RECT);
    ui_begin_row(state.ui, ivec2(3, 3), 2);
        static const std::vector<std::vector<std::string>> TOOLBAR_OPTIONS = {
            { "File", "New", "Open", "Save" },
            { "Edit", "Undo" },
            { "Tool", "Brush" }
        };
        std::string column, action;
        if (ui_toolbar(state.ui, &column, &action, TOOLBAR_OPTIONS, 2)) {
            editor_handle_toolbar_action(column, action);
        }
    ui_end_container(state.ui);

    // Sidebar
    ui_small_frame_rect(state.ui, SIDEBAR_RECT);
    {
        ui_begin_column(state.ui, ivec2(SIDEBAR_RECT.x + 4, SIDEBAR_RECT.y + 4), 4);
            char tool_text[64];
            sprintf(tool_text, "%s Tool", TOOLBAR_OPTIONS[2][state.tool + 1].c_str());
            ui_text(state.ui, FONT_HACK_GOLD, tool_text);

            switch (state.tool) {
                case EDITOR_TOOL_BRUSH: {
                    editor_menu_dropdown(state.ui, "Value:", &state.tool_value, { "Water", "Lowground", "Highground" }, SIDEBAR_RECT);
                    break;
                }
            }
        ui_end_container(state.ui);
    }

    // Status bar
    ui_small_frame_rect(state.ui, STATUS_RECT);
    char status_text[512];
    status_text[0] = '\0';
    char* status_text_ptr = status_text;
    if (CANVAS_RECT.has_point(input_get_mouse_position()) && 
            !state.is_minimap_dragging &&
            state.camera_drag_mouse_position.x == -1 &&
            !editor_is_in_menu()) {
        ivec2 cell = editor_get_hovered_cell();
        status_text_ptr += sprintf(status_text_ptr, "Cell: <%i, %i>", cell.x, cell.y);
    }
    if (status_text[0] != '\0') {
        ui_element_position(state.ui, ivec2(4, SCREEN_HEIGHT - 15));
        ui_text(state.ui, FONT_HACK_GOLD, status_text);
    }

    // Menu new
    if (state.menu_new.mode == EDITOR_MENU_NEW_OPEN) {
        editor_menu_new_update(state.menu_new, state.ui);
    }
    if (state.menu_new.mode == EDITOR_MENU_NEW_CREATE) {
        MapType map_type = (MapType)state.menu_new.map_type;
        MapSize map_size = (MapSize)state.menu_new.map_size;
        if (state.menu_new.use_noise_gen_params) {
            NoiseGenParams params = editor_menu_new_create_noise_gen_params(state.menu_new);
            state.document = editor_document_init_generated(map_type, params);
        } else {
            state.document = editor_document_init_blank(map_type, map_size);
        }
        state.menu_new.mode = EDITOR_MENU_NEW_CLOSED;
    }

    // Camera drag
    if (input_is_action_just_pressed(INPUT_ACTION_RIGHT_CLICK) && 
            !state.is_minimap_dragging && 
            !editor_is_in_menu() &&
            !state.is_painting &&
            CANVAS_RECT.has_point(input_get_mouse_position())) {
        state.camera_drag_previous_offset = state.camera_offset;
        state.camera_drag_mouse_position = input_get_mouse_position();
    }
    if (input_is_action_just_released(INPUT_ACTION_RIGHT_CLICK)) {
        state.camera_drag_mouse_position = ivec2(-1, -1);
    }
    if (state.camera_drag_mouse_position.x != -1) {
        ivec2 mouse_position_difference = input_get_mouse_position() - state.camera_drag_mouse_position;
        state.camera_offset = state.camera_drag_previous_offset - mouse_position_difference;
        editor_clamp_camera();
    }

    // Paint
    if (input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !state.is_minimap_dragging &&
            !editor_is_in_menu() &&
            state.tool == EDITOR_TOOL_BRUSH &&
            CANVAS_RECT.has_point(input_get_mouse_position())) {
        state.is_painting = true;
    }
    if (state.is_painting && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state.is_painting = false;
    }
    if (state.is_painting && CANVAS_RECT.has_point(input_get_mouse_position())) {
        ivec2 cell = editor_get_hovered_cell();
        if (editor_document_get_noise_map_value(state.document, cell) != (uint8_t)state.tool_value) {
            // TODO: paint
        }
    }

    // Minimap drag
    if (MINIMAP_RECT.has_point(input_get_mouse_position()) &&
            state.camera_drag_mouse_position.x == -1 &&
            !editor_is_in_menu() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state.is_minimap_dragging = true;
    }
    if (state.is_minimap_dragging && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state.is_minimap_dragging = false;
    }
    if (state.is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (state.document->map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state.document->map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        editor_center_camera_on_cell(map_pos / TILE_SIZE);
    }
}

bool editor_is_in_menu() {
    return state.menu_new.mode == EDITOR_MENU_NEW_OPEN;
}

void editor_clamp_camera() {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, (state.document->map.width * TILE_SIZE) - CANVAS_RECT.w);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, (state.document->map.height * TILE_SIZE) - CANVAS_RECT.h);
}

void editor_center_camera_on_cell(ivec2 cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    editor_clamp_camera();
}

void editor_handle_toolbar_action(const std::string& column, const std::string& action) {
    if (column == "File") {
        if (action == "New") {
            state.menu_new = editor_menu_new_open();
        }
    } else if (column == "Tool") {
        if (action == "Brush") {
            editor_set_tool(EDITOR_TOOL_BRUSH);
        }
    }
}

void editor_set_tool(EditorTool tool) {
    state.tool = tool;
    switch (state.tool) {
        case EDITOR_TOOL_BRUSH: {
            state.tool_value = NOISE_VALUE_LOWGROUND;
            break;
        }
    }
}

ivec2 editor_get_hovered_cell() {
    return ((input_get_mouse_position() - ivec2(CANVAS_RECT.x, CANVAS_RECT.y)) + state.camera_offset) / TILE_SIZE;
}

void editor_render() {
    if (state.document != NULL) {
        std::vector<RenderSpriteParams> ysort_params;

        ivec2 base_pos = ivec2(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
        ivec2 base_coords = ivec2(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
        ivec2 max_visible_tiles = ivec2(CANVAS_RECT.w / TILE_SIZE, CANVAS_RECT.h / TILE_SIZE);
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
                for (int x = 0; x < max_visible_tiles.x; x++) {
                    if (base_coords.x + x >= state.document->map.width || base_coords.y + y >= state.document->map.height) {
                        continue;
                    }

                    int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.document->map.width);
                    Tile tile = state.document->map.tiles[map_index];

                    ivec2 tile_params_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE);
                    RenderSpriteParams tile_params = (RenderSpriteParams) {
                        .sprite = tile.sprite,
                        .frame = tile.frame,
                        .position = tile_params_position,
                        .ysort_position = tile_params_position.y,
                        .options = RENDER_SPRITE_NO_CULL,
                        .recolor_id = 0
                    };

                    bool should_render_on_ground_level = 
                        map_is_tile_ground(state.document->map, base_coords + ivec2(x, y)) || 
                        map_is_tile_ramp(state.document->map, base_coords + ivec2(x, y));
                    if (elevation == 0 && 
                            !map_is_tile_ground(state.document->map, base_coords + ivec2(x, y)) &&
                            !map_is_tile_water(state.document->map, base_coords + ivec2(x, y))) {
                        render_sprite_frame(map_get_plain_ground_tile_sprite(state.document->map.type), ivec2(0, 0), ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                    }
                    if ((should_render_on_ground_level && elevation == 0) || 
                            (!should_render_on_ground_level && elevation == tile.elevation)) {
                        render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                    } 

                    // Decorations
                    Cell cell = state.document->map.cells[CELL_LAYER_GROUND][map_index];
                    if (cell.type == CELL_DECORATION && tile.elevation == elevation) {
                        SpriteName decoration_sprite = map_get_decoration_sprite(state.document->map.type);
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
                    for (const Entity& entity : state.document->entities) {
                        const EntityData& entity_data = entity_get_data(entity.type);
                        if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                                entity_get_elevation(entity, state.document->map) != elevation) {
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

        // Render ysort params
        ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
        for (const RenderSpriteParams& params : ysort_params) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }

        if (CANVAS_RECT.has_point(input_get_mouse_position()) &&
                !state.is_minimap_dragging &&
                state.camera_drag_mouse_position.x == -1 &&
                !editor_is_in_menu()) {
            ivec2 cell = editor_get_hovered_cell();
            Rect rect = (Rect) {
                .x = ((cell.x * TILE_SIZE) - state.camera_offset.x) + CANVAS_RECT.x,
                .y = ((cell.y * TILE_SIZE) - state.camera_offset.y) + CANVAS_RECT.y,
                .w = TILE_SIZE, .h = TILE_SIZE
            };
            render_draw_rect(rect, RENDER_COLOR_WHITE);
        }
    }

    // UI covers
    {
        Rect src_rect = (Rect) {
            .x = RENDER_COLOR_OFFBLACK, .y = 0, .w = 1, .h = 1
        };
        render_sprite(SPRITE_UI_SWATCH, src_rect, TOOLBAR_RECT, RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_SWATCH, src_rect, SIDEBAR_RECT, RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_SWATCH, src_rect, STATUS_RECT, RENDER_SPRITE_NO_CULL);
    }

    ui_render(state.ui);

    // Minimap frame
    {
        const SpriteInfo& minimap_frame_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
        Rect src_rect = (Rect) {
            .x = 0, .y = 0, 
            .w = minimap_frame_sprite_info.frame_width, 
            .h = minimap_frame_sprite_info.frame_width 
        };
        Rect dst_rect = (Rect) {
            .x = MINIMAP_RECT.x - 4, .y = MINIMAP_RECT.y - 4, 
            .w = minimap_frame_sprite_info.frame_width,
            .h = minimap_frame_sprite_info.frame_width
        };
        render_sprite(SPRITE_UI_MINIMAP, src_rect, dst_rect, RENDER_SPRITE_NO_CULL);
    }

    if (state.document != NULL) {
        // MINIMAP
        // Minimap tiles
        for (int y = 0; y < state.document->map.height; y++) {
            for (int x = 0; x < state.document->map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_TILE, ivec2(x, y), editor_get_minimap_pixel_for_cell(ivec2(x, y)));
            }
        }
        // Minimap entities
        for (uint32_t entity_index = 0; entity_index < state.document->entity_count; entity_index++) {
            const Entity& entity = state.document->entities[entity_index];
            int entity_cell_size = entity_get_data(entity.type).cell_size;
            Rect entity_rect = (Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size, .h = entity_cell_size
            };
            render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, editor_get_minimap_pixel_for_entity(entity));
        }
        // Clear fog layer
        for (int y = 0; y < state.document->map.height; y++) {
            for (int x = 0; x < state.document->map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), MINIMAP_PIXEL_TRANSPARENT);
            }
        }
        // Minimap camera rect
        Rect camera_rect = (Rect) {
            .x = state.camera_offset.x / TILE_SIZE,
            .y = state.camera_offset.y / TILE_SIZE,
            .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
            .h = (SCREEN_HEIGHT / TILE_SIZE)
        };
        render_minimap_draw_rect(MINIMAP_LAYER_FOG, camera_rect, MINIMAP_PIXEL_WHITE);
        render_minimap_queue_render(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state.document->map.width, state.document->map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
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
        .recolor_id = entity.type == ENTITY_GOLDMINE || entity.mode == MODE_BUILDING_DESTROYED ? 0 : state.document->players[entity.player_id].recolor_id
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

MinimapPixel editor_get_minimap_pixel_for_cell(ivec2 cell) {
    switch (map_get_tile(state.document->map, cell).sprite) {
        case SPRITE_TILE_SAND1:
        case SPRITE_TILE_SAND2:
        case SPRITE_TILE_SAND3:
            return MINIMAP_PIXEL_SAND;
        case SPRITE_TILE_SAND_WATER:
            return MINIMAP_PIXEL_WATER;
        case SPRITE_TILE_SNOW1:
        case SPRITE_TILE_SNOW2:
        case SPRITE_TILE_SNOW3:
            return MINIMAP_PIXEL_SNOW;
        case SPRITE_TILE_SNOW_WATER:
            return MINIMAP_PIXEL_SNOW_WATER;
        default:
            return MINIMAP_PIXEL_WALL;
    }
}

MinimapPixel editor_get_minimap_pixel_for_entity(const Entity& entity) {
    if (entity.type == ENTITY_GOLDMINE) {
        return MINIMAP_PIXEL_GOLD;
    }
    if (entity_check_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER)) {
        return MINIMAP_PIXEL_WHITE;
    }
    return (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + state.document->players[entity.player_id].recolor_id);
}

#endif
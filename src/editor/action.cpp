#include "action.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"

// Action

EditorAction editor_action_create_brush(const std::vector<EditorActionBrushStroke>& stroke, uint8_t new_value) {
    EditorAction action;
    action.type = EDITOR_ACTION_BRUSH;
    action.brush.stroke_size = stroke.size();
    action.brush.stroke = (EditorActionBrushStroke*)malloc(action.brush.stroke_size * sizeof(EditorActionBrushStroke));
    memcpy(action.brush.stroke, &stroke[0], action.brush.stroke_size * sizeof(EditorActionBrushStroke));
    action.brush.new_value = new_value;

    return action;
}

EditorAction editor_action_create_fill(const std::vector<int>& indices, uint8_t previous_value, uint8_t new_value) {
    EditorAction action;
    action.type = EDITOR_ACTION_FILL;
    action.fill.index_count = indices.size();
    action.fill.indices = (int*)malloc(action.fill.index_count * sizeof(int));
    memcpy(action.fill.indices, &indices[0], action.fill.index_count * sizeof(int));
    action.fill.previous_value = previous_value;
    action.fill.new_value = new_value;

    return action;
}

void editor_action_destroy(EditorAction& action) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            free(action.brush.stroke);
            break;
        }
        case EDITOR_ACTION_FILL: {
            free(action.fill.indices);
            break;
        }
    }
}

void editor_action_execute(EditorDocument* document, const EditorAction& action, EditorActionMode mode) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            // Don't do anything for do because we have already been painting the tiles
            if (mode == EDITOR_ACTION_MODE_DO) {
                break;
            }

            for (uint32_t index = 0; index < action.brush.stroke_size; index++) {
                uint8_t value = mode == EDITOR_ACTION_MODE_UNDO 
                    ? action.brush.stroke[index].previous_value
                    : action.brush.new_value;
                document->noise->map[action.brush.stroke[index].cell.x + (action.brush.stroke[index].cell.y * document->noise->width)] = value;
            }

            int lcg_seed = document->tile_bake_seed;
            map_bake_tiles(document->map, document->noise, &lcg_seed);
            map_bake_front_walls(document->map);
            break;
        }
        case EDITOR_ACTION_FILL: {
            const uint8_t value = mode == EDITOR_ACTION_MODE_UNDO
                ? action.fill.previous_value
                : action.fill.new_value;
            for (uint32_t index = 0; index < action.fill.index_count; index++) {
                document->noise->map[action.fill.indices[index]] = value;
            }

            int lcg_seed = document->tile_bake_seed;
            map_bake_tiles(document->map, document->noise, &lcg_seed);
            map_bake_front_walls(document->map);
            break;
        }
    }
}

#endif
#include "action.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"

// Action

EditorAction editor_action_create_brush(const std::vector<EditorActionBrushStroke>& stroke) {
    EditorAction action;
    action.type = EDITOR_ACTION_BRUSH;
    action.brush.stroke_size = stroke.size();
    action.brush.stroke = (EditorActionBrushStroke*)malloc(action.brush.stroke_size * sizeof(EditorActionBrushStroke));
    memcpy(action.brush.stroke, &stroke[0], action.brush.stroke_size * sizeof(EditorActionBrushStroke));
    action.brush.skip_do = false;

    return action;
}

void editor_action_destroy(EditorAction& action) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            free(action.brush.stroke);
            break;
        }
    }
}

void editor_action_execute(EditorDocument* document, const EditorAction& action, EditorActionMode mode) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            // Don't do anything for do because we have already been painting the tiles
            if (action.brush.skip_do && mode == EDITOR_ACTION_MODE_DO) {
                break;
            }

            for (uint32_t index = 0; index < action.brush.stroke_size; index++) {
                uint8_t value = mode == EDITOR_ACTION_MODE_UNDO 
                    ? action.brush.stroke[index].previous_value
                    : action.brush.stroke[index].new_value;
                document->noise->map[action.brush.stroke[index].index] = value;
            }

            int lcg_seed = document->tile_bake_seed;
            map_bake_tiles(document->map, document->noise, &lcg_seed);
            map_bake_front_walls(document->map);
            break;
        }
    }
}

#endif
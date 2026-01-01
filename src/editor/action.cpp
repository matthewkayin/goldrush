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

void editor_action_destroy(EditorAction& action) {
    if (action.type == EDITOR_ACTION_BRUSH) {
        free(action.brush.stroke);
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
                editor_document_set_noise_map_value(document, action.brush.stroke[index].cell, value);
            }
            break;
        }
    }
}

#endif
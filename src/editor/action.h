#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/document.h"
#include "math/gmath.h"
#include "match/noise.h"
#include <vector>

enum EditorActionMode {
    EDITOR_ACTION_MODE_DO,
    EDITOR_ACTION_MODE_UNDO,
    EDITOR_ACTION_MODE_REDO
};

enum EditorActionType {
    EDITOR_ACTION_BRUSH
};

struct EditorActionBrushStroke {
    ivec2 cell;
    uint8_t previous_value;
};

struct EditorActionBrush {
    size_t stroke_size;
    EditorActionBrushStroke* stroke;
    uint8_t new_value;
};

struct EditorAction {
    EditorActionType type;
    union {
        EditorActionBrush brush;
    };
};

// Action

EditorAction editor_action_create_brush(const std::vector<EditorActionBrushStroke>& stroke, uint8_t new_value);
void editor_action_destroy(EditorAction& action);
void editor_action_execute(EditorDocument* document, const EditorAction& action, EditorActionMode mode);

#endif
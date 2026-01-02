#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/document.h"
#include "math/gmath.h"
#include "match/noise.h"
#include <vector>

enum EditorActionMode {
    EDITOR_ACTION_MODE_DO,
    EDITOR_ACTION_MODE_UNDO
};

enum EditorActionType {
    EDITOR_ACTION_BRUSH,
    EDITOR_ACTION_DECORATE,
    EDITOR_ACTION_DECORATE_BULK
};

struct EditorActionBrushStroke {
    int index;
    uint8_t previous_value;
    uint8_t new_value;
};

struct EditorActionBrush {
    uint32_t stroke_size;
    EditorActionBrushStroke* stroke;
};

static const int EDITOR_ACTION_DECORATE_REMOVE_DECORATION = -1;

struct EditorActionDecorate {
    int index;
    int previous_hframe;
    int new_hframe;
};

struct EditorActionDecorateBulk {
    uint32_t size;
    EditorActionDecorate* changes;
};

struct EditorAction {
    EditorActionType type;
    union {
        EditorActionBrush brush;
        EditorActionDecorate decorate;
        EditorActionDecorateBulk decorate_bulk;
    };
};

// Action

EditorAction editor_action_create_brush(const std::vector<EditorActionBrushStroke>& stroke);
EditorAction editor_action_create_decorate_bulk(const std::vector<EditorActionDecorate>& changes);
void editor_action_destroy(EditorAction& action);
void editor_action_execute(EditorDocument* document, const EditorAction& action, EditorActionMode mode);

#endif
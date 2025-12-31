#pragma once

#include "defines.h"
#include "math/gmath.h"
#include "match/noise.h"
#include <vector>

enum EditorActionType {
    EDITOR_ACTION_BRUSH
};

struct EditorActionBrush {
    ivec2* cells;
    uint8_t* previous_values;
    uint8_t new_value;
};

struct EditorActionBrushStroke {
    std::vector<bool> cell_has_been_painted;
    std::vector<ivec2> cells;
    std::vector<uint8_t> previous_values;
};

struct EditorAction {
    EditorActionType type;
    union {
        EditorActionBrush brush;
    };
};

EditorActionBrushStroke editor_action_brush_stroke_init(const Noise* noise);
void editor_action_brush_stroke_paint_cell(EditorActionBrushStroke& stroke, const int noise_width, ivec2 cell, uint8_t previous_value);
EditorAction editor_action_create_brush(const EditorActionBrushStroke& stroke, uint8_t new_value);
void editor_action_destroy(EditorAction& action);
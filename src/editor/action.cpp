#include "action.h"

EditorActionBrushStroke editor_action_brush_stroke_init(const Noise* noise) {
    EditorActionBrushStroke stroke;
    stroke.cell_has_been_painted = std::vector<bool>(noise->width * noise->height, false);
}

void editor_action_brush_stroke_paint_cell(EditorActionBrushStroke& stroke, const int noise_width, ivec2 cell, uint8_t previous_value) {
    if (stroke.cell_has_been_painted[cell.x + (cell.y * noise_width)]) {
        return;
    }
    stroke.cells.push_back(cell);
    stroke.previous_values.push_back(previous_value);
    stroke.cell_has_been_painted[cell.x + (cell.y * noise_width)] = true;
}

EditorAction editor_action_create_brush(const EditorActionBrushStroke& stroke, uint8_t new_value) {
    EditorAction action;
    action.type = EDITOR_ACTION_BRUSH;
    action.brush.cells = (ivec2*)malloc(stroke.cells.size() * sizeof(ivec2));
    memcpy(action.brush.cells, &stroke.cells[0], stroke.cells.size() * sizeof(ivec2));
    action.brush.previous_values = (uint8_t*)malloc(stroke.previous_values.size() * sizeof(uint8_t));
    memcpy(action.brush.previous_values, &stroke.previous_values[0], stroke.previous_values.size() * sizeof(uint8_t));
    action.brush.new_value = new_value;

    return action;
}

void editor_action_destroy(EditorAction& action) {
    if (action.type == EDITOR_ACTION_BRUSH) {
        free(action.brush.cells);
        free(action.brush.previous_values);
    }
}
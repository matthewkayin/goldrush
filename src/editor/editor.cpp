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

static const std::vector<std::vector<std::string>> TOOLBAR_OPTIONS = {
    { "File", "New", "Open", "Save" },
    { "Edit", "Undo", "Redo", "Copy", "Cut", "Paste" },
    { "Tool", "Brush", "Fill", "Rect", "Select", "Decorate" }
};

static const std::unordered_map<InputAction, std::vector<std::string>> TOOLBAR_SHORTCUTS = {
    { INPUT_ACTION_EDITOR_SAVE, { "File", "Save" }},
    { INPUT_ACTION_EDITOR_UNDO, { "Edit", "Undo" }},
    { INPUT_ACTION_EDITOR_REDO, { "Edit", "Redo" }},
    { INPUT_ACTION_EDITOR_COPY, { "Edit", "Copy" }},
    { INPUT_ACTION_EDITOR_CUT, { "Edit", "Cut" }},
    { INPUT_ACTION_EDITOR_PASTE, { "Edit", "Paste" }},
    { INPUT_ACTION_EDITOR_TOOL_BRUSH, { "Tool", "Brush" }},
    { INPUT_ACTION_EDITOR_TOOL_FILL, { "Tool", "Fill" }},
    { INPUT_ACTION_EDITOR_TOOL_RECT, { "Tool", "Rect" }},
    { INPUT_ACTION_EDITOR_TOOL_SELECT, { "Tool", "Select" }},
    { INPUT_ACTION_EDITOR_TOOL_DECORATE, { "Tool", "Decorate" }},
};

enum EditorTool {
    EDITOR_TOOL_BRUSH,
    EDITOR_TOOL_FILL,
    EDITOR_TOOL_RECT,
    EDITOR_TOOL_SELECT,
    EDITOR_TOOL_DECORATE
};

struct EditorClipboard {
    int width;
    int height;
    std::vector<uint8_t> values;
};

struct EditorState {
    UI ui;
    EditorDocument* document;

    // Actions
    size_t action_head;
    std::vector<EditorAction> actions;

    // Menus
    EditorMenuNew menu_new;

    // Tools
    EditorTool tool;
    uint32_t tool_value;
    std::vector<EditorActionBrushStroke> tool_brush_stroke;
    ivec2 tool_rect_origin;
    ivec2 tool_rect_end;
    ivec2 tool_select_origin;
    ivec2 tool_select_end;
    Rect tool_select_selection;
    bool is_painting;
    bool is_pasting;

    // Camera
    ivec2 camera_drag_previous_offset;
    ivec2 camera_drag_mouse_position;
    ivec2 camera_offset;
    bool is_minimap_dragging;

    // Clipboard
    EditorClipboard clipboard;
};
static EditorState state;

// Update
void editor_free_current_document();
bool editor_is_in_menu();
void editor_clamp_camera();
void editor_center_camera_on_cell(ivec2 cell);
void editor_handle_toolbar_action(const std::string& column, const std::string& action);
void editor_set_tool(EditorTool tool);
ivec2 editor_canvas_to_world_space(ivec2 point);
ivec2 editor_get_hovered_cell();
void editor_push_action(const EditorAction& action);
void editor_do_action(const EditorAction& action);
void editor_undo_action();
void editor_redo_action();
void editor_clear_actions();
void editor_fill();
Rect editor_tool_rect_get_rect();
std::vector<EditorActionBrushStroke> editor_tool_rect_get_brush_stroke();
SpriteName editor_get_noise_preview_sprite(uint8_t value);
Rect editor_tool_select_get_select_rect();
Rect editor_cell_rect_to_world_space(const Rect& rect);
void editor_tool_select_clear_selection();
void editor_clipboard_clear();
bool editor_clipboard_is_empty();
void editor_clipboard_copy();
void editor_clipboard_paste(ivec2 cell);
void editor_flatten_rect(const Rect& rect);
bool editor_is_using_noise_tool();
void editor_generate_decorations();

// Render
RenderSpriteParams editor_create_entity_render_params(const Entity& entity);
MinimapPixel editor_get_minimap_pixel_for_cell(ivec2 cell);
MinimapPixel editor_get_minimap_pixel_for_entity(const Entity& entity);

void editor_init() {
    input_set_mouse_capture_enabled(false);

    if (option_get_value(OPTION_DISPLAY) != RENDER_DISPLAY_WINDOWED) {
        option_set_value(OPTION_DISPLAY, RENDER_DISPLAY_WINDOWED);
    }
    log_debug("Set display to windowed.");

    state.ui = ui_init();
    state.document = editor_document_init_blank(MAP_TYPE_TOMBSTONE, MAP_SIZE_SMALL);
    log_debug("Initialized document");

    state.action_head = 0;

    state.menu_new.mode = EDITOR_MENU_NEW_CLOSED;

    state.camera_offset = ivec2(0, 0);
    state.is_minimap_dragging = false;
    state.is_painting = false;
    state.is_pasting = false;
    state.camera_drag_mouse_position = ivec2(-1, -1);

    log_info("Initialized map editor.");
}

void editor_free_current_document() {
    if (state.document != NULL) {
        editor_document_free(state.document);
        editor_tool_select_clear_selection();
        editor_clipboard_clear();
        state.document = NULL;
    }
    editor_clear_actions();
}

void editor_quit() {
    editor_free_current_document();
}

void editor_update() {
    std::string toolbar_column, toolbar_action;
    bool toolbar_was_clicked;

    ui_begin(state.ui);
    state.ui.input_enabled = 
        !state.is_minimap_dragging && 
        state.camera_drag_mouse_position.x == -1 &&
        !editor_is_in_menu();

    // Toolbar
    ui_small_frame_rect(state.ui, TOOLBAR_RECT);
    ui_element_position(state.ui, ivec2(3, 3));
    toolbar_was_clicked = ui_toolbar(state.ui, &toolbar_column, &toolbar_action, TOOLBAR_OPTIONS, 2);

    // Sidebar
    ui_small_frame_rect(state.ui, SIDEBAR_RECT);
    {
        ui_begin_column(state.ui, ivec2(SIDEBAR_RECT.x + 4, SIDEBAR_RECT.y + 4), 4);
            char tool_text[64];
            sprintf(tool_text, "%s Tool", TOOLBAR_OPTIONS[2][state.tool + 1].c_str());
            ui_text(state.ui, FONT_HACK_GOLD, tool_text);

            switch (state.tool) {
                case EDITOR_TOOL_BRUSH: 
                case EDITOR_TOOL_FILL:
                case EDITOR_TOOL_RECT: {
                    editor_menu_dropdown(state.ui, "Value:", &state.tool_value, { "Water", "Lowground", "Highground" }, SIDEBAR_RECT);
                    break;
                }
                case EDITOR_TOOL_SELECT: {
                    if (state.is_pasting) {
                        sprintf(tool_text, "Pasting %ux%u", state.clipboard.width, state.clipboard.height);
                        ui_text(state.ui, FONT_HACK_GOLD, tool_text);
                    }
                    break;
                }
                case EDITOR_TOOL_DECORATE: {
                    if (ui_slim_button(state.ui, "Generate")) {
                        editor_generate_decorations();
                    }
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

    // Toolbar click handle
    if (toolbar_was_clicked) {
        state.is_pasting = false;
        editor_handle_toolbar_action(toolbar_column, toolbar_action);
        return;
    }

    // Editor shortcuts
    if (!editor_is_in_menu() && 
            !state.is_minimap_dragging && 
            !state.is_painting &&
            !state.is_pasting &&
            state.camera_drag_mouse_position.x == -1) {
        for (auto it : TOOLBAR_SHORTCUTS) {
            InputAction shortcut = it.first;
            if (input_is_action_just_pressed(shortcut)) {
                editor_handle_toolbar_action(it.second[0], it.second[1]);
                return;
            }
        }
    }

    // Menu new
    if (state.menu_new.mode == EDITOR_MENU_NEW_OPEN) {
        editor_menu_new_update(state.menu_new, state.ui);
    }
    if (state.menu_new.mode == EDITOR_MENU_NEW_CREATE) {
        editor_free_current_document();

        MapType map_type = (MapType)state.menu_new.map_type;
        MapSize map_size = (MapSize)state.menu_new.map_size;
        if (state.menu_new.use_noise_gen_params) {
            NoiseGenParams params = editor_menu_new_create_noise_gen_params(state.menu_new);
            state.document = editor_document_init_generated(map_type, params);
        } else {
            state.document = editor_document_init_blank(map_type, map_size);
        }
        state.menu_new.mode = EDITOR_MENU_NEW_CLOSED;
        return;
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
        return;
    }
    if (state.camera_drag_mouse_position.x != -1) {
        ivec2 mouse_position_difference = input_get_mouse_position() - state.camera_drag_mouse_position;
        state.camera_offset = state.camera_drag_previous_offset - mouse_position_difference;
        editor_clamp_camera();
    }

    // Use tool
    if (input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !state.is_minimap_dragging &&
            !editor_is_in_menu() &&
            !state.is_pasting &&
            state.camera_drag_mouse_position.x == -1 &&
            CANVAS_RECT.has_point(input_get_mouse_position())) {
        if (state.tool == EDITOR_TOOL_BRUSH && !state.is_painting) {
            state.is_painting = true;
            state.tool_brush_stroke.clear();
        } else if (state.tool == EDITOR_TOOL_FILL) {
            editor_fill();
            return;
        } else if (state.tool == EDITOR_TOOL_RECT && !state.is_painting) {
            state.is_painting = true;
            state.tool_rect_origin = editor_get_hovered_cell();
            state.tool_rect_end = editor_get_hovered_cell();
        } else if (state.tool == EDITOR_TOOL_SELECT && !state.is_painting) {
            const ivec2 world_space_mouse_pos = editor_canvas_to_world_space(input_get_mouse_position());
            state.tool_select_origin = world_space_mouse_pos;
            state.tool_select_end = state.tool_select_origin;
            state.is_painting = true;
        }
    }

    // Paint / Selection move
    if (state.is_painting && CANVAS_RECT.has_point(input_get_mouse_position())) {
        if (state.tool == EDITOR_TOOL_BRUSH) {
            ivec2 cell = editor_get_hovered_cell();
            if (editor_document_get_noise_map_value(state.document, cell) != (uint8_t)state.tool_value) {
                state.tool_brush_stroke.push_back((EditorActionBrushStroke) {
                    .index = cell.x + (cell.y * state.document->noise->width),
                    .previous_value = editor_document_get_noise_map_value(state.document, cell),
                    .new_value = (uint8_t)state.tool_value
                });
                editor_document_set_noise_map_value(state.document, cell, state.tool_value);
            }
        } else if (state.tool == EDITOR_TOOL_RECT) {
            state.tool_rect_end = editor_get_hovered_cell();
        } else if (state.tool == EDITOR_TOOL_SELECT) {
            state.tool_select_end = editor_canvas_to_world_space(input_get_mouse_position()); 
        }
    }

    // Brush release
    if (state.is_painting && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        const bool was_painting = state.is_painting;
        state.is_painting = false;

        if (state.tool == EDITOR_TOOL_BRUSH) {
            if (!state.tool_brush_stroke.empty()) {
                EditorAction action = editor_action_create_brush(state.tool_brush_stroke);
                editor_push_action(action);
            }
        } else if (state.tool == EDITOR_TOOL_RECT) {
            std::vector<EditorActionBrushStroke> stroke = editor_tool_rect_get_brush_stroke();
            EditorAction action = editor_action_create_brush(stroke);
            editor_do_action(action);
        } else if (state.tool == EDITOR_TOOL_SELECT && was_painting) {
            if (state.tool_select_end == state.tool_select_origin) {
                editor_tool_select_clear_selection();
                return;
            }
            Rect select_rect = editor_tool_select_get_select_rect();
            if (!input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                editor_tool_select_clear_selection();
            }
            state.tool_select_selection = select_rect;
        } else if (state.tool == EDITOR_TOOL_SELECT && !was_painting) {
            ivec2 origin_cell = state.tool_select_origin / TILE_SIZE;
            ivec2 end_cell = state.tool_select_end / TILE_SIZE;
            if (origin_cell == end_cell) {
                return;
            }
        }

        return;
    }

    // Decorate paint
    if (state.tool == EDITOR_TOOL_DECORATE && 
            !editor_is_in_menu() &&
            !state.is_minimap_dragging &&
            state.camera_drag_mouse_position.x == -1 &&
            CANVAS_RECT.has_point(input_get_mouse_position()) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        ivec2 cell = editor_get_hovered_cell();
        Cell map_cell = map_get_cell(*state.document->map, CELL_LAYER_GROUND, cell);
        if (map_cell.type == CELL_DECORATION || map_cell.type == CELL_EMPTY) {
            const SpriteName decoration_sprite = map_get_decoration_sprite(state.document->map->type);
            const SpriteInfo& decoration_sprite_info = render_get_sprite_info(decoration_sprite);
            editor_do_action((EditorAction) {
                .type = EDITOR_ACTION_DECORATE,
                .decorate = (EditorActionDecorate) {
                    .index = cell.x + (cell.y * state.document->map->width),
                    .previous_hframe = map_cell.type == CELL_DECORATION
                        ? map_cell.decoration_hframe
                        : EDITOR_ACTION_DECORATE_REMOVE_DECORATION,
                    .new_hframe = map_cell.type == CELL_DECORATION
                        ? EDITOR_ACTION_DECORATE_REMOVE_DECORATION
                        : (rand() % decoration_sprite_info.hframes)
                }
            });
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
        return;
    }
    if (state.is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (state.document->map->width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state.document->map->height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        editor_center_camera_on_cell(map_pos / TILE_SIZE);
    }

    // Paste
    if (state.is_pasting && 
            CANVAS_RECT.has_point(input_get_mouse_position()) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        ivec2 paste_cell = editor_get_hovered_cell();
        editor_clipboard_paste(paste_cell);
        state.is_pasting = false;
    }
}

bool editor_is_in_menu() {
    return state.menu_new.mode == EDITOR_MENU_NEW_OPEN;
}

void editor_clamp_camera() {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, (state.document->map->width * TILE_SIZE) - CANVAS_RECT.w);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, (state.document->map->height * TILE_SIZE) - CANVAS_RECT.h);
}

void editor_center_camera_on_cell(ivec2 cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    editor_clamp_camera();
}

void editor_handle_toolbar_action(const std::string& column, const std::string& action) {
    if (state.is_painting ||
            editor_is_in_menu() ||
            state.is_minimap_dragging ||
            state.camera_drag_mouse_position.x != -1) {
        return;
    }

    if (column == "File") {
        if (action == "New") {
            state.menu_new = editor_menu_new_open();
        }
    } else if (column == "Edit") {
        if (action == "Undo") {
            editor_undo_action();
        } else if (action == "Redo") {
            editor_redo_action();
        } else if ((action == "Copy" || action == "Cut") && 
                !state.is_pasting &&
                editor_is_using_noise_tool()) {
            editor_clipboard_copy();
            if (action == "Cut") {
                editor_flatten_rect(state.tool_select_selection);
                editor_tool_select_clear_selection();
            }
        } else if (action == "Paste" &&
                !state.is_pasting &&
                !editor_clipboard_is_empty() &&
                editor_is_using_noise_tool()) {
            editor_set_tool(EDITOR_TOOL_SELECT);
            editor_tool_select_clear_selection();
            state.is_pasting = true;
            log_debug("begin pasting");
        }
    } else if (column == "Tool") {
        for (uint32_t index = 1; index < TOOLBAR_OPTIONS[2].size(); index++) {
            if (action == TOOLBAR_OPTIONS[2][index]) {
                editor_set_tool((EditorTool)(index - 1));
            }
        }
    }
}

void editor_set_tool(EditorTool tool) {
    if (state.is_painting || state.is_pasting) {
        return;
    }

    state.tool = tool;
    switch (state.tool) {
        case EDITOR_TOOL_BRUSH:
        case EDITOR_TOOL_FILL: 
        case EDITOR_TOOL_RECT: {
            if (state.tool_value > NOISE_VALUE_HIGHGROUND) {
                state.tool_value = NOISE_VALUE_LOWGROUND;
            }
            break;
        }
        case EDITOR_TOOL_SELECT:
        case EDITOR_TOOL_DECORATE: {
            break;
        }
    }
}

ivec2 editor_canvas_to_world_space(ivec2 point) {
    return point - ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + state.camera_offset;
}

ivec2 editor_get_hovered_cell() {
    return editor_canvas_to_world_space(input_get_mouse_position()) / TILE_SIZE;
}

// Adds an action to the stack but does not execute the action
void editor_push_action(const EditorAction& action) {
    while (state.actions.size() > state.action_head) {
        editor_action_destroy(state.actions.back());
        state.actions.pop_back();
    }
    state.actions.push_back(action);
    state.action_head++;
}

// Adds an action to the stack and also executes the action
void editor_do_action(const EditorAction& action) {
    editor_push_action(action);
    editor_action_execute(state.document, action, EDITOR_ACTION_MODE_DO);
}

void editor_undo_action() {
    if (state.action_head == 0) {
        return;
    }

    state.action_head--;
    editor_action_execute(state.document, state.actions[state.action_head], EDITOR_ACTION_MODE_UNDO);
}

void editor_redo_action() {
    if (state.action_head == state.actions.size()) {
        return;
    }

    editor_action_execute(state.document, state.actions[state.action_head], EDITOR_ACTION_MODE_DO);
    state.action_head++;
}

void editor_clear_actions() {
    for (EditorAction& action : state.actions) {
        editor_action_destroy(action);
    }
    state.actions.clear();
}

void editor_fill() {
    ivec2 cell = editor_get_hovered_cell();
    if (editor_document_get_noise_map_value(state.document, cell) == (uint8_t)state.tool_value) {
        return;
    }

    std::vector<int> fill_indices;
    const uint8_t previous_value = editor_document_get_noise_map_value(state.document, cell);
    const uint8_t new_value = (uint8_t)state.tool_value;

    std::vector<ivec2> frontier;
    std::vector<bool> is_explored(state.document->noise->width * state.document->noise->height, false);
    frontier.push_back(cell);

    while (!frontier.empty()) {
        ivec2 next = frontier.back();
        frontier.pop_back();
        
        if (is_explored[next.x + (next.y * state.document->noise->width)]) {
            continue;
        }

        fill_indices.push_back(next.x + (next.y * state.document->noise->width));
        is_explored[next.x + (next.y * state.document->noise->width)] = true;

        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(*state.document->map, child)) {
                continue;
            }
            if (is_explored[child.x + (child.y * state.document->noise->width)]) {
                continue;
            }
            if (editor_document_get_noise_map_value(state.document, child) != previous_value) {
                continue;
            }
            frontier.push_back(child);
        }
    }

    std::sort(fill_indices.begin(), fill_indices.end());

    std::vector<EditorActionBrushStroke> stroke;
    for (int index : fill_indices) {
        stroke.push_back((EditorActionBrushStroke) {
            .index = index,
            .previous_value = previous_value,
            .new_value = new_value
        });
    }
    EditorAction action = editor_action_create_brush(stroke);
    editor_do_action(action);
}

Rect editor_tool_rect_get_rect() {
    // The +1 on the width and height is to ensure that the rect is inclusive of the end cell
    return (Rect) {
        .x = std::min(state.tool_rect_origin.x, state.tool_rect_end.x),
        .y = std::min(state.tool_rect_origin.y, state.tool_rect_end.y),
        .w = std::abs(state.tool_rect_origin.x - state.tool_rect_end.x) + 1,
        .h = std::abs(state.tool_rect_origin.y - state.tool_rect_end.y) + 1
    };
}

std::vector<EditorActionBrushStroke> editor_tool_rect_get_brush_stroke() {
    Rect rect = editor_tool_rect_get_rect();

    std::vector<EditorActionBrushStroke> stroke;
    std::function<void(ivec2)> stroke_push_back = [&stroke](ivec2 cell) {
        stroke.push_back((EditorActionBrushStroke) {
            .index = cell.x + (cell.y * state.document->noise->width),
            .previous_value = editor_document_get_noise_map_value(state.document, cell),
            .new_value = (uint8_t)state.tool_value
        });
    };
    // Rect top row
    for (int x = rect.x; x < rect.x + rect.w; x++) {
        stroke_push_back(ivec2(x, rect.y));
    }
    // Rect sides
    for (int y = rect.y + 1; y < rect.y + rect.h - 1; y++) {
        stroke_push_back(ivec2(rect.x, y));
        stroke_push_back(ivec2(rect.x + rect.w - 1, y));
    }
    // Rect bottom row
    for (int x = rect.x; x < rect.x + rect.w; x++) {
        stroke_push_back(ivec2(x, rect.y + rect.h - 1));
    }

    return stroke;
}

SpriteName editor_get_noise_preview_sprite(uint8_t value) {
    switch (value) {
        case NOISE_VALUE_WATER: {
            return map_choose_water_tile_sprite(state.document->map->type);
        }
        case NOISE_VALUE_LOWGROUND: {
            return map_get_plain_ground_tile_sprite(state.document->map->type);
        }
        case NOISE_VALUE_HIGHGROUND: {
            return SPRITE_TILE_WALL_SOUTH_EDGE;
        }
        default: {
            GOLD_ASSERT(false);
            return SPRITE_TILE_NULL;
        }
    }
}

Rect editor_tool_select_get_select_rect() {
    ivec2 origin_cell = state.tool_select_origin / TILE_SIZE;
    ivec2 end_cell = state.tool_select_end / TILE_SIZE;
    return (Rect) {
        .x = std::min(origin_cell.x, end_cell.x),
        .y = std::min(origin_cell.y, end_cell.y),
        .w = std::abs(origin_cell.x - end_cell.x) + 1,
        .h = std::abs(origin_cell.y - end_cell.y) + 1
    };
}

void editor_tool_select_clear_selection() {
    state.tool_select_selection = (Rect) {
        .x = -1, .y = -1, .w = 0, .h = 0
    };
}

Rect editor_cell_rect_to_world_space(const Rect& rect) {
    return (Rect) {
        .x = (rect.x * TILE_SIZE) - state.camera_offset.x,
        .y = (rect.y * TILE_SIZE) - state.camera_offset.y,
        .w = rect.w * TILE_SIZE,
        .h = rect.h * TILE_SIZE
    };
}

void editor_clipboard_clear() {
    state.clipboard.width = 0;
    state.clipboard.height = 0;
    state.clipboard.values.clear();
}

bool editor_clipboard_is_empty() {
    return state.clipboard.width == 0;
}

void editor_clipboard_copy() {
    if (state.tool_select_selection.x == -1) {
        return;
    }

    editor_clipboard_clear();
    state.clipboard.width = state.tool_select_selection.w;
    state.clipboard.height = state.tool_select_selection.h;
    state.clipboard.values.reserve(state.clipboard.width * state.clipboard.height);
    for (int y = 0; y < state.clipboard.height; y++) {
        for (int x = 0; x < state.clipboard.width; x++) {
            ivec2 map_cell = ivec2(state.tool_select_selection.x + x, state.tool_select_selection.y + y);
            state.clipboard.values.push_back(editor_document_get_noise_map_value(state.document, map_cell));
        }
    }
}

void editor_clipboard_paste(ivec2 top_left_cell) {
    std::vector<EditorActionBrushStroke> stroke;

    for (int y = 0; y < state.clipboard.height; y++) {
        for (int x = 0; x < state.clipboard.width; x++) {
            ivec2 cell = top_left_cell + ivec2(x, y);
            if (!map_is_cell_in_bounds(*state.document->map, cell)) {
                continue;
            }

            int index = cell.x + (cell.y * state.document->noise->width);
            stroke.push_back((EditorActionBrushStroke) {
                .index = index,
                .previous_value = state.document->noise->map[index],
                .new_value = state.clipboard.values[x + (y * state.clipboard.width)]
            });
        }
    }

    EditorAction action = editor_action_create_brush(stroke);
    editor_do_action(action);
}

bool editor_is_using_noise_tool() {
    return state.tool == EDITOR_TOOL_BRUSH || 
        state.tool == EDITOR_TOOL_FILL || 
        state.tool == EDITOR_TOOL_RECT || 
        state.tool == EDITOR_TOOL_SELECT;
}

void editor_flatten_rect(const Rect& rect) {
    std::vector<EditorActionBrushStroke> stroke;

    for (int y = rect.y; y < rect.y + rect.w; y++) {
        for (int x = rect.x; x < rect.x + rect.h; x++) {
            stroke.push_back((EditorActionBrushStroke) {
                .index = x + (y * state.document->noise->width),
                .previous_value = state.document->noise->map[x + (y * state.document->noise->width)],
                .new_value = NOISE_VALUE_LOWGROUND
            });
        }
    }

    EditorAction action = editor_action_create_brush(stroke);
    editor_do_action(action);
}

void editor_generate_decorations() {
    std::vector<EditorActionDecorate> changes;
    std::vector<int> change_indices(state.document->map->width * state.document->map->height, -1);

    // Clear all decorations and mark the changes
    for (int index = 0; index < state.document->map->width * state.document->map->height; index++) {
        Cell map_cell = state.document->map->cells[CELL_LAYER_GROUND][index];
        if (map_cell.type != CELL_DECORATION) {
            continue;
        }

        changes.push_back((EditorActionDecorate) {
            .index = index,
            .previous_hframe = map_cell.decoration_hframe,
            .new_hframe = EDITOR_ACTION_DECORATE_REMOVE_DECORATION
        });
        change_indices[index] = (int)changes.size() - 1;

        state.document->map->cells[CELL_LAYER_GROUND][index] = (Cell) {
            .type = CELL_EMPTY,
            .id = ID_NULL
        };
    }

    // Get a list of goldmine cells
    std::vector<ivec2> goldmine_cells;
    for (uint32_t entity_index = 0; entity_index < state.document->entity_count; entity_index++) {
        if (state.document->entities[entity_index].type == ENTITY_GOLDMINE) {
            goldmine_cells.push_back(state.document->entities[entity_index].cell);
        }
    }

    // Generate decorations
    int generate_decorations_seed = rand();
    map_generate_decorations(*state.document->map, state.document->noise, &generate_decorations_seed, goldmine_cells);

    // Record all changes
    for (int index = 0; index < state.document->map->width * state.document->map->height; index++) {
        Cell map_cell = state.document->map->cells[CELL_LAYER_GROUND][index];
        if (map_cell.type != CELL_DECORATION) {
            continue;
        }

        // If change indices is not -1, it means that we previously recorded this cell
        // as a change from decorated to undecorated, so now we will update that change
        // as a change from decorated with one hframe to decorated with another hframe
        if (change_indices[index] != -1) {
            changes[change_indices[index]].new_hframe = map_cell.decoration_hframe;
        } else {
            changes.push_back((EditorActionDecorate) {
                .index = index,
                .previous_hframe = EDITOR_ACTION_DECORATE_REMOVE_DECORATION,
                .new_hframe = map_cell.decoration_hframe
            });
        }
    }

    EditorAction action = editor_action_create_decorate_bulk(changes);
    editor_push_action(action);
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
                    if (base_coords.x + x >= state.document->map->width || base_coords.y + y >= state.document->map->height) {
                        continue;
                    }

                    int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.document->map->width);
                    Tile tile = state.document->map->tiles[map_index];

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
                        map_is_tile_ground(*state.document->map, base_coords + ivec2(x, y)) || 
                        map_is_tile_ramp(*state.document->map, base_coords + ivec2(x, y));
                    if (elevation == 0 && 
                            !map_is_tile_ground(*state.document->map, base_coords + ivec2(x, y)) &&
                            !map_is_tile_water(*state.document->map, base_coords + ivec2(x, y))) {
                        render_sprite_frame(map_get_plain_ground_tile_sprite(state.document->map->type), ivec2(0, 0), ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                    }
                    if ((should_render_on_ground_level && elevation == 0) || 
                            (!should_render_on_ground_level && elevation == tile.elevation)) {
                        render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                    } 

                    // Decorations
                    Cell cell = state.document->map->cells[CELL_LAYER_GROUND][map_index];
                    if (cell.type == CELL_DECORATION && tile.elevation == elevation) {
                        SpriteName decoration_sprite = map_get_decoration_sprite(state.document->map->type);
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
                    for (uint32_t entity_index = 0; entity_index < state.document->entity_count; entity_index++) {
                        const Entity& entity = state.document->entities[entity_index];
                        const EntityData& entity_data = entity_get_data(entity.type);
                        if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                                entity_get_elevation(entity, *state.document->map) != elevation) {
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

    // Tool rect preview
    if (state.is_painting && state.tool == EDITOR_TOOL_RECT) {
        Rect rect = editor_tool_rect_get_rect();
        std::function<void(ivec2)> render_rect_preview = [](ivec2 cell) {
            ivec2 cell_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + (cell * TILE_SIZE) - state.camera_offset;
            render_sprite_frame(editor_get_noise_preview_sprite(state.tool_value), ivec2(0, 0), cell_position, RENDER_SPRITE_NO_CULL, 0);
        };

        for (int x = rect.x; x < rect.x + rect.w; x++) {
            render_rect_preview(ivec2(x, rect.y));
            render_rect_preview(ivec2(x, rect.y + rect.h - 1));
        }
        for (int y = rect.y + 1; y < rect.y + rect.h - 1; y++) {
            render_rect_preview(ivec2(rect.x, y));
            render_rect_preview(ivec2(rect.x + rect.w - 1, y));
        }
    }

    // Paste preview
    if (state.is_pasting && state.tool == EDITOR_TOOL_SELECT && CANVAS_RECT.has_point(input_get_mouse_position())) {
        const ivec2 hovered_cell = editor_get_hovered_cell();

        // Draw preview cells
        for (int y = 0; y < state.clipboard.height; y++) {
            for (int x = 0; x < state.clipboard.width; x++) {
                uint8_t clipboard_value = state.clipboard.values[x + (y * state.clipboard.width)];
                SpriteName sprite = editor_get_noise_preview_sprite(clipboard_value);
                ivec2 cell = hovered_cell + ivec2(x, y);
                ivec2 cell_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + (cell * TILE_SIZE) - state.camera_offset;
                render_sprite_frame(sprite, ivec2(0, 0), cell_position, 0, 0);
            }
        }

        // Draw a rect around the paste area
        const Rect paste_rect = (Rect) {
            .x = hovered_cell.x,
            .y = hovered_cell.y,
            .w = state.clipboard.width,
            .h = state.clipboard.height
        };
        Rect rendered_rect = editor_cell_rect_to_world_space(paste_rect);
        rendered_rect.x += CANVAS_RECT.x;
        rendered_rect.y += CANVAS_RECT.y;
        render_draw_rect(rendered_rect, RENDER_COLOR_WHITE);
    }

    // Selection
    {
        if ((state.is_painting && state.tool == EDITOR_TOOL_SELECT) || state.tool_select_selection.x != -1) {
            const Rect rect = state.is_painting && state.tool == EDITOR_TOOL_SELECT 
                ? editor_tool_select_get_select_rect()
                : state.tool_select_selection;
            Rect rendered_rect = editor_cell_rect_to_world_space(rect);
            rendered_rect.x += CANVAS_RECT.x;
            rendered_rect.y += CANVAS_RECT.y;
            if (CANVAS_RECT.intersects(rendered_rect)) {
                render_draw_rect(rendered_rect, RENDER_COLOR_WHITE);
            }
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
        for (int y = 0; y < state.document->map->height; y++) {
            for (int x = 0; x < state.document->map->width; x++) {
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
        for (int y = 0; y < state.document->map->height; y++) {
            for (int x = 0; x < state.document->map->width; x++) {
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
        render_minimap_queue_render(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state.document->map->width, state.document->map->height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
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
    switch (map_get_tile(*state.document->map, cell).sprite) {
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
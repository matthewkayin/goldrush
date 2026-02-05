#include "editor.h"

#ifdef GOLD_DEBUG

#include "scenario/scenario.h"
#include "editor/menu_new.h"
#include "editor/menu_file.h"
#include "editor/menu_players.h"
#include "editor/menu_edit_squad.h"
#include "editor/menu_constant.h"
#include "editor/menu_map_size.h"
#include "editor/menu.h"
#include "editor/action.h"
#include "core/logger.h"
#include "core/input.h"
#include "core/ui.h"
#include "core/filesystem.h"
#include "match/map.h"
#include "container/id_array.h"
#include "match/state.h"
#include "core/options.h"
#include "render/render.h"
#include "render/ysort.h"
#include "util/util.h"
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
    { "File", "New", "Open", "Save", "Save As" },
    { "Edit", "Undo", "Redo", "Copy", "Cut", "Paste", "Players", "Map Size" },
    { "Tool", "Brush", "Fill", "Rect", "Select", "Decorate", "Add Entity", "Edit Entity", "Squads", "Player Spawn", "Constants" },
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
    { INPUT_ACTION_EDITOR_TOOL_ADD_ENTITY, { "Tool", "Add Entity" }},
    { INPUT_ACTION_EDITOR_TOOL_EDIT_ENTITY, { "Tool", "Edit Entity" }},
    { INPUT_ACTION_EDITOR_TOOL_SQUADS, { "Tool", "Squads" }},
};

static const SDL_DialogFileFilter EDITOR_FILE_FILTERS[] = {
    { "JSON files", "json" }
};

static const uint32_t TOOL_ENTITY_ROW_SIZE = 4;
static const uint32_t TOOL_ADD_ENTITY_VISIBLE_ROW_COUNT = 4;
static const uint32_t TOOL_SQUADS_VISIBLE_ROW_COUNT = 3;

enum EditorTool {
    EDITOR_TOOL_BRUSH,
    EDITOR_TOOL_FILL,
    EDITOR_TOOL_RECT,
    EDITOR_TOOL_SELECT,
    EDITOR_TOOL_DECORATE,
    EDITOR_TOOL_ADD_ENTITY,
    EDITOR_TOOL_EDIT_ENTITY,
    EDITOR_TOOL_SQUADS,
    EDITOR_TOOL_PLAYER_SPAWN,
    EDITOR_TOOL_CONSTANTS
};

struct EditorClipboard {
    int width;
    int height;
    std::vector<uint8_t> values;
};

enum EditorMenuType {
    EDITOR_MENU_TYPE_NONE,
    EDITOR_MENU_TYPE_NEW,
    EDITOR_MENU_TYPE_FILE,
    EDITOR_MENU_TYPE_PLAYERS,
    EDITOR_MENU_TYPE_EDIT_SQUAD,
    EDITOR_MENU_TYPE_CONSTANT,
    EDITOR_MENU_TYPE_MAP_SIZE
};

struct EditorMenu {
    EditorMenuType type;
    EditorMenuMode mode;
    std::variant<
        EditorMenuNew,
        EditorMenuPlayers,
        EditorMenuEditSquad,
        EditorMenuConstant,
        EditorMenuMapSize
    > menu;
};

struct EditorState {
    SDL_Window* window;

    Scenario* scenario;
    std::string scenario_path;
    std::string scenario_map_short_path;
    std::string scenario_script_short_path;
    bool scenario_is_saved;
    UI ui;
    int ui_toolbar_id;
    bool should_playtest;

    // Actions
    size_t action_head;
    std::vector<EditorAction> actions;

    // Menus
    EditorMenu menu;

    // Tools
    EditorTool tool;
    uint32_t tool_value;
    std::vector<EditorActionBrushStroke> tool_brush_stroke;
    ivec2 tool_rect_origin;
    ivec2 tool_rect_end;
    ivec2 tool_select_origin;
    ivec2 tool_select_end;
    Rect tool_select_selection;
    uint32_t tool_add_entity_player_id;
    uint32_t tool_scroll;
    uint32_t tool_edit_entity_gold_held;
    ivec2 tool_edit_entity_offset;
    int tool_size;
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
bool editor_is_toolbar_open();
void editor_clamp_camera();
void editor_center_camera_on_cell(ivec2 cell);
void editor_handle_toolbar_action(const std::string& column, const std::string& action);
void editor_set_tool(EditorTool tool);
ivec2 editor_canvas_to_world_space(ivec2 point);
ivec2 editor_get_hovered_cell();
uint32_t editor_get_hovered_entity();
bool editor_is_hovered_cell_valid();
bool editor_can_single_use_tool_be_used();
const char* editor_get_noise_value_str(uint8_t noise_value);
void editor_push_action(const EditorAction& action);
void editor_do_action(const EditorAction& action);
void editor_undo_action();
void editor_redo_action();
void editor_clear_actions();
bool editor_tool_brush_should_paint();
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
std::vector<EditorActionDecorate> editor_clear_deocrations();
bool editor_tool_is_scroll_enabled();
int editor_tool_get_scroll_max();
void editor_tool_edit_entity_set_selection(uint32_t entity_index);
void editor_validate_tool_value();
void editor_tool_edit_entity_delete_entity(uint32_t entity_index);
uint32_t editor_get_entity_squad(uint32_t entity_index);
void editor_remove_entity_from_squad(uint32_t squad_index, uint32_t entity_index);
std::vector<std::string> editor_get_player_name_dropdown_items();
ivec2 editor_get_player_spawn_camera_offset(ivec2 cell);
std::string editor_get_scenario_folder_path();
static void SDLCALL editor_save_callback(void* user_data, const char* const* filelist, int filter);
static void SDLCALL editor_open_callback(void* user_data, const char* const* filelist, int filter);
void editor_set_scenario_paths_to_defaults();
void editor_save(const char* path);

// Render
ivec2 editor_entity_get_animation_frame(EntityType type);
ivec2 editor_entity_get_render_position(EntityType type, ivec2 cell);
RenderSpriteParams editor_create_entity_render_params(const ScenarioEntity& entity);
MinimapPixel editor_get_minimap_pixel_for_cell(ivec2 cell);
MinimapPixel editor_get_minimap_pixel_for_entity(const std::vector<uint32_t>& selection, uint32_t entity_index);

void editor_init(SDL_Window* window) {
    state.window = window;
    state.should_playtest = false;

    input_set_mouse_capture_enabled(false);

    if (option_get_value(OPTION_DISPLAY) != RENDER_DISPLAY_WINDOWED) {
        option_set_value(OPTION_DISPLAY, RENDER_DISPLAY_WINDOWED);
    }
    log_debug("Set display to windowed.");

    state.ui = ui_init();
    state.scenario = scenario_init_blank(MAP_TYPE_TOMBSTONE, MAP_SIZE_SMALL);
    editor_set_scenario_paths_to_defaults();
    state.scenario_is_saved = false;
    log_debug("Initialized document");

    state.action_head = 0;

    state.menu.type = EDITOR_MENU_TYPE_NONE;
    state.menu.mode = EDITOR_MENU_MODE_CLOSED;

    state.camera_offset = ivec2(0, 0);
    state.is_minimap_dragging = false;
    state.is_painting = false;
    state.is_pasting = false;
    state.tool_add_entity_player_id = 0;
    state.camera_drag_mouse_position = ivec2(-1, -1);

    log_info("Initialized map editor.");
}

void editor_free_current_document() {
    if (state.scenario != NULL) {
        scenario_free(state.scenario);
        editor_tool_select_clear_selection();
        editor_clipboard_clear();
        state.scenario = NULL;
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
    state.ui_toolbar_id = state.ui.next_element_id + 1;
    toolbar_was_clicked = ui_toolbar(state.ui, &toolbar_column, &toolbar_action, TOOLBAR_OPTIONS, 2);

    // Sidebar
    ui_small_frame_rect(state.ui, SIDEBAR_RECT);
    {
        ui_begin_column(state.ui, ivec2(SIDEBAR_RECT.x + 4, SIDEBAR_RECT.y + 4), 4);
            char tool_text[64];
            sprintf(tool_text, "%s Tool", TOOLBAR_OPTIONS[2][state.tool + 1].c_str());
            ui_text(state.ui, FONT_HACK_GOLD, tool_text);

            switch (state.tool) {
                case EDITOR_TOOL_BRUSH: {
                    editor_menu_dropdown(state.ui, "Value:", &state.tool_value, { "Water", "Lowground", "Highground", "Ramp" }, SIDEBAR_RECT);
                    break;
                }
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
                    if (ui_slim_button(state.ui, "Clear")) {
                        std::vector<EditorActionDecorate> changes = editor_clear_deocrations();
                        editor_push_action((EditorAction) {
                            .type = EDITOR_ACTION_DECORATE_BULK,
                            .data = (EditorActionDecorateBulk) {
                                .changes = changes
                            }
                        });
                    }
                    if (ui_slim_button(state.ui, "Generate")) {
                        editor_generate_decorations();
                    }
                    break;
                }
                case EDITOR_TOOL_ADD_ENTITY: {
                    std::vector<std::string> items = editor_get_player_name_dropdown_items();
                    bool was_dropdown_clicked = ui_dropdown(state.ui, UI_DROPDOWN_MINI, &state.tool_add_entity_player_id, items, false);

                    for (uint32_t row = state.tool_scroll; row < state.tool_scroll + TOOL_ADD_ENTITY_VISIBLE_ROW_COUNT; row++) {
                        ui_begin_row(state.ui, ivec2(0, 0), 2);
                            for (uint32_t col = 0; col < TOOL_ENTITY_ROW_SIZE; col++) {
                                if ((row * TOOL_ENTITY_ROW_SIZE) + col >= ENTITY_TYPE_COUNT) {
                                    continue;
                                }
                                EntityType entity_type = (EntityType)((row * TOOL_ENTITY_ROW_SIZE) + col);
                                if (ui_icon_button(state.ui, entity_get_data(entity_type).icon, state.tool_value == entity_type) && !was_dropdown_clicked) {
                                    state.tool_value = entity_type;
                                }
                            }
                        ui_end_container(state.ui);
                    }
                    break;
                }
                case EDITOR_TOOL_EDIT_ENTITY: {
                    if (state.tool_value == INDEX_INVALID) {
                        break;
                    }

                    const ScenarioEntity& entity = state.scenario->entities[state.tool_value];

                    ui_icon_button(state.ui, entity_get_data(entity.type).icon, true);

                    if (entity.type == ENTITY_GOLDMINE) {
                        char gold_text[16];
                        sprintf(gold_text, "Gold: %u", state.tool_edit_entity_gold_held);

                        ui_text(state.ui, FONT_HACK_GOLD, gold_text);

                        ui_slider(state.ui, &state.tool_edit_entity_gold_held, NULL, (UiSliderParams) {
                            .display = UI_SLIDER_DISPLAY_NO_VALUE,
                            .size = UI_SLIDER_SIZE_MINI,
                            .min = 0,
                            .max = 20000,
                            .step = 50
                        }); 
                        if (input_is_action_just_released(INPUT_ACTION_LEFT_CLICK) && state.tool_edit_entity_gold_held != entity.gold_held) {
                            ScenarioEntity edited_entity = entity;
                            edited_entity.gold_held = state.tool_edit_entity_gold_held;
                            editor_do_action((EditorAction) {
                                .type = EDITOR_ACTION_EDIT_ENTITY,
                                .data = (EditorActionEditEntity) {
                                    .index = state.tool_value,
                                    .previous_value = entity,
                                    .new_value = edited_entity
                                }
                            });
                        }
                    } else {
                        std::vector<std::string> items = editor_get_player_name_dropdown_items();
                        uint32_t entity_player_id = entity.player_id;
                        if (ui_dropdown(state.ui, UI_DROPDOWN_MINI, &entity_player_id, items, false)) {
                            ScenarioEntity edited_entity = entity;
                            edited_entity.player_id = (uint8_t)entity_player_id;
                            editor_do_action((EditorAction) {
                                .type = EDITOR_ACTION_EDIT_ENTITY,
                                .data = (EditorActionEditEntity) {
                                    .index = state.tool_value,
                                    .previous_value = entity,
                                    .new_value = edited_entity
                                }
                            });
                        }
                    }

                    if (ui_button(state.ui, "Delete") || input_is_action_just_pressed(INPUT_ACTION_EDITOR_DELETE)) {
                        editor_tool_edit_entity_delete_entity(state.tool_value);
                    }

                    break;
                }
                case EDITOR_TOOL_SQUADS: {
                    ui_begin_row(state.ui, ivec2(0, 0), 2);
                        std::vector<std::string> squad_dropdown_items;
                        for (uint32_t squad_index = 0; squad_index < state.scenario->squads.size(); squad_index++) {
                            squad_dropdown_items.push_back(std::string(state.scenario->squads[squad_index].name));
                        }
                        ui_dropdown(state.ui, UI_DROPDOWN_MINI, &state.tool_value, squad_dropdown_items, false, 9);
                        if (ui_sprite_button(state.ui, SPRITE_UI_EDITOR_PLUS, false, false)) {
                            editor_do_action((EditorAction) {
                                .type = EDITOR_ACTION_ADD_SQUAD
                            });
                            state.tool_value = state.scenario->squads.size() - 1;
                        }
                        if (ui_sprite_button(state.ui, SPRITE_UI_EDITOR_EDIT, state.scenario->squads.empty(), false)) {
                            state.menu = (EditorMenu) {
                                .type = EDITOR_MENU_TYPE_EDIT_SQUAD,
                                .mode = EDITOR_MENU_MODE_OPEN,
                                .menu = editor_menu_edit_squad_open(state.scenario->squads[state.tool_value], state.scenario)
                            };
                        }
                        if (ui_sprite_button(state.ui, SPRITE_UI_EDITOR_TRASH, state.scenario->squads.empty(), false)) {
                            editor_do_action((EditorAction) {
                                .type = EDITOR_ACTION_REMOVE_SQUAD,
                                .data = (EditorActionRemoveSquad) {
                                    .index = state.tool_value,
                                    .value = state.scenario->squads[state.tool_value]
                                }
                            }); 
                            state.tool_value = 0;
                        }
                    ui_end_container(state.ui);

                    if (!state.scenario->squads.empty()) {
                        const ScenarioSquad& squad = state.scenario->squads[state.tool_value];
                        char squad_info_text[64];
                        sprintf(squad_info_text, "%s / %s", state.scenario->players[squad.player_id].name, bot_squad_type_str(squad.type));
                        ui_text(state.ui, FONT_HACK_GOLD, squad_info_text);

                        if (squad.type == BOT_SQUAD_TYPE_PATROL) {
                            char patrol_cell_text[64];
                            sprintf(patrol_cell_text, "Patrol Cell: <%i, %i>", squad.patrol_cell.x, squad.patrol_cell.y);
                            ui_text(state.ui, FONT_HACK_GOLD, patrol_cell_text);
                        }

                        ui_text(state.ui, FONT_HACK_GOLD, "Entities");

                        for (uint32_t row = state.tool_scroll; row < state.tool_scroll + TOOL_SQUADS_VISIBLE_ROW_COUNT; row++) {
                            ui_begin_row(state.ui, ivec2(0, 0), 2);
                                for (uint32_t col = 0; col < TOOL_ENTITY_ROW_SIZE; col++) {
                                    uint32_t squad_entity_index = col + (row * TOOL_ENTITY_ROW_SIZE);
                                    if (squad_entity_index >= squad.entity_count) {
                                        continue;
                                    }
                                    uint32_t entity_index = squad.entities[squad_entity_index];
                                    EntityType entity_type = state.scenario->entities[entity_index].type;
                                    if (ui_icon_button(state.ui, entity_get_data(entity_type).icon, true)) {
                                        editor_remove_entity_from_squad(state.tool_value, entity_index);
                                    }
                                }
                            ui_end_container(state.ui);
                        }
                    }

                    break;
                }
                case EDITOR_TOOL_CONSTANTS: {
                    // Constant selector row
                    ui_begin_row(state.ui, ivec2(0, 0), 2);
                        // Build items list
                        std::vector<std::string> constant_dropdown_items;
                        for (uint32_t constant_index = 0; constant_index < state.scenario->constants.size(); constant_index++) {
                            constant_dropdown_items.push_back(std::string(state.scenario->constants[constant_index].name));
                        }

                        // Dropdown
                        ui_dropdown(state.ui, UI_DROPDOWN_MINI, &state.tool_value, constant_dropdown_items, false, 9);
                        
                        // Add button
                        if (ui_sprite_button(state.ui, SPRITE_UI_EDITOR_PLUS, false, false)) {
                            editor_do_action((EditorAction) {
                                .type = EDITOR_ACTION_ADD_CONSTANT
                            });
                            state.tool_value = state.scenario->constants.size() - 1;
                        }

                        // Edit button
                        if (ui_sprite_button(state.ui, SPRITE_UI_EDITOR_EDIT, state.scenario->constants.empty(), false)) {
                            state.menu = (EditorMenu) {
                                .type = EDITOR_MENU_TYPE_CONSTANT,
                                .mode = EDITOR_MENU_MODE_OPEN,
                                .menu = editor_menu_constant_open(state.scenario->constants[state.tool_value].name)
                            };
                        }

                        // Delete button
                        if (ui_sprite_button(state.ui, SPRITE_UI_EDITOR_TRASH, state.scenario->constants.empty(), false)) {
                            editor_do_action((EditorAction) {
                                .type = EDITOR_ACTION_REMOVE_CONSTANT,
                                .data = (EditorActionRemoveConstant) {
                                    .index = state.tool_value,
                                    .value = state.scenario->constants[state.tool_value]
                                }
                            });
                            state.tool_value = std::clamp(state.tool_value, 0U, (uint32_t)state.scenario->constants.size() - 1U);
                        }
                    ui_end_container(state.ui);

                    // Constant info
                    if (!state.scenario->constants.empty()) {
                        const ScenarioConstant& constant = state.scenario->constants[state.tool_value];

                        // Populate constant type dropdown items
                        std::vector<std::string> constant_type_items;
                        for (uint32_t constant_type = 0; constant_type < SCENARIO_CONSTANT_TYPE_COUNT; constant_type++) {
                            constant_type_items.push_back(scenario_constant_type_str((ScenarioConstantType)constant_type));
                        }

                        // Constant type dropdown
                        uint32_t constant_type = constant.type;
                        if (editor_menu_dropdown(state.ui, "Type:", &constant_type, constant_type_items, SIDEBAR_RECT, 8)) {
                            if (constant_type != constant.type) {
                                ScenarioConstant edited_constant = constant;
                                scenario_constant_set_type(edited_constant, (ScenarioConstantType)constant_type);
                                editor_do_action((EditorAction) {
                                    .type = EDITOR_ACTION_EDIT_CONSTANT,
                                    .data = (EditorActionEditConstant) {
                                        .index = state.tool_value,
                                        .previous_value = constant,
                                        .new_value = edited_constant
                                    }
                                });
                            }
                        }

                        switch (constant.type) {
                            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                                char value_str[32];
                                if (constant.entity_index >= state.scenario->entity_count) {
                                    sprintf(value_str, "Entity: INVALID");
                                } else {
                                    sprintf(value_str, "Entity: %u", constant.entity_index);
                                }
                                ui_text(state.ui, FONT_HACK_GOLD, value_str);
                                break;
                            }
                            case SCENARIO_CONSTANT_TYPE_CELL: {
                                char value_str[32];
                                sprintf(value_str, "Cell: <%i, %i>", constant.cell.x, constant.cell.y);
                                ui_text(state.ui, FONT_HACK_GOLD, value_str);
                                break;
                            }
                            case SCENARIO_CONSTANT_TYPE_COUNT: {
                                GOLD_ASSERT(false);
                                break;
                            }
                        }
                    }

                    break;
                }
                case EDITOR_TOOL_PLAYER_SPAWN:
                    break;
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
        status_text_ptr += sprintf(status_text_ptr, "Cell: <%i, %i> Value: %s ", cell.x, cell.y, editor_get_noise_value_str(state.scenario->noise->map[cell.x + (cell.y * state.scenario->noise->width)]));

        uint32_t entity_index = editor_get_hovered_entity();
        if (entity_index != INDEX_INVALID) {
            status_text_ptr += sprintf(status_text_ptr, "Entity: %u", entity_index);
        }
    } else if (!CANVAS_RECT.has_point(input_get_mouse_position())) {
        status_text_ptr += sprintf(status_text_ptr, "%s", state.scenario_path.empty() ? "Untitled" : state.scenario_path.c_str());
        if (!state.scenario_is_saved) {
            status_text_ptr += sprintf(status_text_ptr, "*");
        }
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

    // Menus
    if (editor_is_in_menu()) {
        switch (state.menu.type) {
            case EDITOR_MENU_TYPE_NEW: {
                EditorMenuNew& menu = std::get<EditorMenuNew>(state.menu.menu);
                editor_menu_new_update(menu, state.ui, state.menu.mode);

                if (state.menu.mode == EDITOR_MENU_MODE_SUBMIT) {
                    editor_free_current_document();

                    MapType map_type = (MapType)menu.map_type;
                    MapSize map_size = (MapSize)menu.map_size;
                    if (menu.use_noise_gen_params) {
                        NoiseGenParams params = editor_menu_new_create_noise_gen_params(menu);
                        state.scenario = scenario_init_generated(map_type, params);
                    } else {
                        state.scenario = scenario_init_blank(map_type, map_size);
                    }
                    editor_set_scenario_paths_to_defaults();
                    state.camera_offset = ivec2(0, 0);
                }

                break;
            }
            case EDITOR_MENU_TYPE_PLAYERS: {
                EditorMenuPlayers& menu = std::get<EditorMenuPlayers>(state.menu.menu);
                editor_menu_players_update(menu, state.ui, state.menu.mode, state.scenario);

                // No submit handle here because the players menu just edits 
                // the scenario directly for better or worse

                break;
            }
            case EDITOR_MENU_TYPE_EDIT_SQUAD: {
                EditorMenuEditSquad& menu = std::get<EditorMenuEditSquad>(state.menu.menu);
                editor_menu_edit_squad_update(menu, state.ui, state.menu.mode);

                if (state.menu.mode == EDITOR_MENU_MODE_SUBMIT) {
                    const ScenarioSquad previous_squad = state.scenario->squads[state.tool_value];
                    ScenarioSquad edited_squad = previous_squad;
                    strncpy(edited_squad.name, menu.squad_name.c_str(), MAX_USERNAME_LENGTH);
                    edited_squad.player_id = menu.squad_player + 1;
                    edited_squad.type = editor_menu_edit_squad_get_selected_squad_type(menu);
                    if (edited_squad.player_id != previous_squad.player_id) {
                        edited_squad.entity_count = 0;
                    }
                    if (edited_squad.type != previous_squad.type) {
                        edited_squad.patrol_cell = ivec2(-1, -1);
                    }

                    if (!scenario_squads_are_equal(edited_squad, previous_squad)) {
                        editor_do_action((EditorAction) {
                            .type = EDITOR_ACTION_EDIT_SQUAD,
                            .data = (EditorActionEditSquad) {
                                .index = state.tool_value,
                                .previous_value = previous_squad,
                                .new_value = edited_squad
                            }
                        });
                    }
                }

                break;
            }
            case EDITOR_MENU_TYPE_CONSTANT: {
                EditorMenuConstant& menu = std::get<EditorMenuConstant>(state.menu.menu);
                editor_menu_constant_update(menu, state.ui, state.menu.mode);

                if (state.menu.mode == EDITOR_MENU_MODE_SUBMIT) {
                    const ScenarioConstant previous_constant = state.scenario->constants[state.tool_value];

                    if (strcmp(menu.value.c_str(), previous_constant.name) != 0) {
                        ScenarioConstant edited_constant = previous_constant;
                        strncpy(edited_constant.name, menu.value.c_str(), SCENARIO_CONSTANT_NAME_BUFFER_LENGTH);

                        editor_do_action((EditorAction) {
                            .type = EDITOR_ACTION_EDIT_CONSTANT,
                            .data = (EditorActionEditConstant) {
                                .index = state.tool_value,
                                .previous_value = previous_constant,
                                .new_value = edited_constant
                            }
                        });
                    }
                }

                break;
            }
            case EDITOR_MENU_TYPE_MAP_SIZE: {
                EditorMenuMapSize& menu = std::get<EditorMenuMapSize>(state.menu.menu);
                editor_menu_map_size_update(menu, state.ui, state.menu.mode);

                if (state.menu.mode == EDITOR_MENU_MODE_SUBMIT) {
                    MapSize map_size = (MapSize)menu.map_size;
                    Scenario* new_scenario = scenario_init_blank(state.scenario->map.type, map_size);

                    // Copy map
                    int smallest_map_width = std::min(state.scenario->map.width, new_scenario->map.width);
                    int smallest_map_height = std::min(state.scenario->map.height, new_scenario->map.height);
                    for (int y = 0; y < smallest_map_height; y++) {
                        for (int x = 0; x < smallest_map_width; x++) {
                            new_scenario->noise->map[x + (y * new_scenario->map.width)] = state.scenario->noise->map[x + (y * state.scenario->map.width)];
                            new_scenario->noise->forest[x + (y * new_scenario->map.width)] = state.scenario->noise->forest[x + (y * state.scenario->map.width)];
                        }
                    }
                    scenario_bake_map(new_scenario);

                    // Copy player data
                    new_scenario->player_spawn = ivec2(
                        std::clamp(state.scenario->player_spawn.x, 0, new_scenario->map.width - 1),
                        std::clamp(state.scenario->player_spawn.y, 0, new_scenario->map.height - 1)
                    );
                    memcpy(new_scenario->player_allowed_entities, state.scenario->player_allowed_entities, sizeof(new_scenario->player_allowed_entities));
                    new_scenario->player_allowed_upgrades = state.scenario->player_allowed_upgrades;
                    memcpy(new_scenario->bot_config, state.scenario->bot_config, sizeof(new_scenario->bot_config));

                    // Copy entities
                    for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
                        const ScenarioEntity& entity = state.scenario->entities[entity_index];
                        if (!map_is_cell_rect_in_bounds(new_scenario->map, entity.cell, entity_get_data(entity.type).cell_size)) {
                            continue;
                        }

                        new_scenario->entities[new_scenario->entity_count] = entity;
                        new_scenario->entity_count++;
                    }

                    editor_free_current_document();
                    state.scenario = new_scenario;
                }

                break;
            }
            case EDITOR_MENU_TYPE_FILE:
                break;
            case EDITOR_MENU_TYPE_NONE:
                GOLD_ASSERT(false);
                break;
        }

        // Close menu
        if (state.menu.mode != EDITOR_MENU_MODE_OPEN) {
            state.menu = (EditorMenu) {
                .type = EDITOR_MENU_TYPE_NONE,
                .mode = EDITOR_MENU_MODE_CLOSED
            };
            if (input_is_text_input_active()) {
                input_stop_text_input();
            }
        }

        // Prevents click-through on menu close
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

    // Entity edit - select entity
    if (state.tool == EDITOR_TOOL_EDIT_ENTITY &&
            editor_can_single_use_tool_be_used() && 
            editor_is_hovered_cell_valid() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        editor_tool_edit_entity_set_selection(editor_get_hovered_entity());
    }

    // Squad edit - Add entity to squad
    if (state.tool == EDITOR_TOOL_SQUADS && 
            !state.scenario->squads.empty() &&
            editor_can_single_use_tool_be_used() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        uint32_t entity_index = editor_get_hovered_entity();
        if (entity_index != INDEX_INVALID) {
            uint32_t entity_squad_index = editor_get_entity_squad(entity_index);

            // Select entity squad
            if (input_is_action_pressed(INPUT_ACTION_CTRL) && 
                    entity_squad_index != state.scenario->squads.size()) {
                state.tool_value = entity_squad_index;
                return;
            }

            // If entity is a part of selected squad, remove it from the squad
            if (input_is_action_pressed(INPUT_ACTION_SHIFT) && entity_squad_index == state.tool_value) {
                editor_remove_entity_from_squad(state.tool_value, entity_index);
                return;
            } 

            // Otherwise add entity to squad
            if (!input_is_action_just_pressed(INPUT_ACTION_SHIFT) &&
                    entity_squad_index == state.scenario->squads.size() &&
                    state.scenario->squads[state.tool_value].entity_count < 
                        SCENARIO_SQUAD_MAX_ENTITIES && 
                    state.scenario->entities[entity_index].player_id == 
                        state.scenario->squads[state.tool_value].player_id) {
                ScenarioSquad edited_squad = state.scenario->squads[state.tool_value];
                edited_squad.entities[edited_squad.entity_count] = entity_index;
                edited_squad.entity_count++;
                editor_do_action((EditorAction) {
                    .type = EDITOR_ACTION_EDIT_SQUAD,
                    .data = (EditorActionEditSquad) {
                        .index = state.tool_value,
                        .previous_value = state.scenario->squads[state.tool_value],
                        .new_value = edited_squad
                    }
                });

                return;
            }
        }
    }

    // Squad edit - Set patrol cell
    if (state.tool == EDITOR_TOOL_SQUADS &&
            !state.scenario->squads.empty() &&
            editor_can_single_use_tool_be_used() &&
            state.scenario->squads[state.tool_value].type == BOT_SQUAD_TYPE_PATROL &&
            input_is_action_pressed(INPUT_ACTION_SHIFT) &&
            editor_is_hovered_cell_valid() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        ScenarioSquad edited_squad = state.scenario->squads[state.tool_value];
        edited_squad.patrol_cell = editor_get_hovered_cell();
        editor_do_action((EditorAction) {
            .type = EDITOR_ACTION_EDIT_SQUAD,
            .data = (EditorActionEditSquad) {
                .index = state.tool_value,
                .previous_value = state.scenario->squads[state.tool_value],
                .new_value = edited_squad
            }
        });
    }

    // Player spawn place
    if (state.tool == EDITOR_TOOL_PLAYER_SPAWN &&
            editor_can_single_use_tool_be_used() &&
            editor_is_hovered_cell_valid() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        editor_do_action((EditorAction) {
            .type = EDITOR_ACTION_SET_PLAYER_SPAWN,
            .data = (EditorActionSetPlayerSpawn) {
                .previous_value = state.scenario->player_spawn,
                .new_value = editor_get_hovered_cell()
            }
        });
    }

    // Constants tool - set value
    if (state.tool == EDITOR_TOOL_CONSTANTS &&
            !state.scenario->constants.empty() &&
            editor_can_single_use_tool_be_used() &&
            editor_is_hovered_cell_valid() && 
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        const ScenarioConstant& constant = state.scenario->constants[state.tool_value];
        ScenarioConstant edited_constant = constant;

        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                edited_constant.entity_index = editor_get_hovered_entity();
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                edited_constant.cell = editor_get_hovered_cell();
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }

        editor_do_action((EditorAction) {
            .type = EDITOR_ACTION_EDIT_CONSTANT,
            .data = (EditorActionEditConstant) {
                .index = state.tool_value,
                .previous_value = constant,
                .new_value = edited_constant
            }
        });
    }

    // Use tool
    if (input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !state.is_minimap_dragging &&
            !editor_is_in_menu() &&
            !editor_is_toolbar_open() &&
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
        } else if (state.tool == EDITOR_TOOL_SELECT && !state.is_painting) {
            state.is_painting = true;
        } else if (state.tool == EDITOR_TOOL_EDIT_ENTITY && !state.is_painting && state.tool_value != INDEX_INVALID) {
            state.tool_edit_entity_offset = editor_get_hovered_cell() - state.scenario->entities[state.tool_value].cell;
            state.is_painting = true;
        }
    }

    // Camera drag during selection
    if (state.is_painting && state.tool == EDITOR_TOOL_SELECT) {
        const int CAMERA_DRAG_MARGIN = 16;

        ivec2 mouse_in_canvas_position = input_get_mouse_position() - ivec2(CANVAS_RECT.x, CANVAS_RECT.y);
        ivec2 drag_direction = ivec2(0, 0);
        if (mouse_in_canvas_position.x < CAMERA_DRAG_MARGIN) {
            drag_direction.x = -1;
        } else if (mouse_in_canvas_position.x >= CANVAS_RECT.w - CAMERA_DRAG_MARGIN) {
            drag_direction.x = 1;
        }
        if (mouse_in_canvas_position.y < CAMERA_DRAG_MARGIN) {
            drag_direction.y = -1;
        } else if (mouse_in_canvas_position.y >= CANVAS_RECT.h - CAMERA_DRAG_MARGIN) {
            drag_direction.y = 1;
        }
        state.camera_offset += drag_direction * 8;
        editor_clamp_camera();
    }

    // Paint / Selection move
    if (state.is_painting && CANVAS_RECT.has_point(input_get_mouse_position())) {
        if (state.tool == EDITOR_TOOL_BRUSH) {
            if (editor_tool_brush_should_paint()) {
                ivec2 cell = editor_get_hovered_cell();
                state.tool_brush_stroke.push_back((EditorActionBrushStroke) {
                    .index = cell.x + (cell.y * state.scenario->noise->width),
                    .previous_value = scenario_get_noise_map_value(state.scenario, cell),
                    .new_value = (uint8_t)state.tool_value
                });
                scenario_set_noise_map_value(state.scenario, cell, state.tool_value);
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
                editor_push_action((EditorAction) {
                    .type = EDITOR_ACTION_BRUSH,
                    .data = (EditorActionBrush) {
                        .stroke = state.tool_brush_stroke
                    }
                });
            }
        } else if (state.tool == EDITOR_TOOL_RECT) {
            editor_do_action((EditorAction) {
                .type = EDITOR_ACTION_BRUSH,
                .data = (EditorActionBrush) {
                    .stroke = editor_tool_rect_get_brush_stroke()
                }
            });
        } else if (state.tool == EDITOR_TOOL_SELECT && was_painting) {
            if (state.tool_select_end == state.tool_select_origin) {
                const bool is_tool_select_end_inside_existing_selection =
                    state.tool_select_selection.x != -1 && state.tool_select_selection.has_point(state.tool_select_end);
                if (!is_tool_select_end_inside_existing_selection) {
                    editor_tool_select_clear_selection();
                }
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
        } else if (state.tool == EDITOR_TOOL_EDIT_ENTITY) {
            if (CANVAS_RECT.has_point(input_get_mouse_position()) && 
                    editor_is_hovered_cell_valid() &&
                    editor_get_hovered_cell() - state.tool_edit_entity_offset != state.scenario->entities[state.tool_value].cell) {
                ScenarioEntity edited_entity = state.scenario->entities[state.tool_value];
                edited_entity.cell = editor_get_hovered_cell() - state.tool_edit_entity_offset;
                editor_do_action((EditorAction) {
                    .type = EDITOR_ACTION_EDIT_ENTITY,
                    .data = (EditorActionEditEntity) {
                        .index = state.tool_value,
                        .previous_value = state.scenario->entities[state.tool_value],
                        .new_value = edited_entity
                    }
                });
            }
        }

        return;
    }

    // Decorate paint
    if (state.tool == EDITOR_TOOL_DECORATE && 
            editor_can_single_use_tool_be_used() && 
            editor_is_hovered_cell_valid() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        ivec2 cell = editor_get_hovered_cell();
        Cell map_cell = map_get_cell(state.scenario->map, CELL_LAYER_GROUND, cell);
        const SpriteName decoration_sprite = map_get_decoration_sprite(state.scenario->map.type);
        const SpriteInfo& decoration_sprite_info = render_get_sprite_info(decoration_sprite);
        editor_do_action((EditorAction) {
            .type = EDITOR_ACTION_DECORATE,
            .data = (EditorActionDecorate) {
                .index = cell.x + (cell.y * state.scenario->map.width),
                .previous_hframe = map_cell.type == CELL_DECORATION
                    ? map_cell.decoration_hframe
                    : EDITOR_ACTION_DECORATE_REMOVE_DECORATION,
                .new_hframe = map_cell.type == CELL_DECORATION
                    ? EDITOR_ACTION_DECORATE_REMOVE_DECORATION
                    : (rand() % decoration_sprite_info.hframes)
            }
        });
    }

    // Entity add
    if (state.tool == EDITOR_TOOL_ADD_ENTITY && 
            editor_can_single_use_tool_be_used() && 
            editor_is_hovered_cell_valid() &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        editor_do_action((EditorAction) {
            .type = EDITOR_ACTION_ADD_ENTITY,
            .data = (EditorActionAddEntity) {
                .type = (EntityType)state.tool_value,
                .player_id = state.tool_value == ENTITY_GOLDMINE 
                    ? (uint8_t)PLAYER_NONE 
                    : (uint8_t)state.tool_add_entity_player_id,
                .cell = editor_get_hovered_cell()
            }
        });
    }

    // Entity place scroll
    if (editor_tool_is_scroll_enabled()) {
        state.tool_scroll = (uint32_t)std::clamp((int)state.tool_scroll - input_get_mouse_scroll(), 0, editor_tool_get_scroll_max());
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
            (state.scenario->map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state.scenario->map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
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

    // Launch test run
    if (!editor_is_in_menu() && 
            !state.is_minimap_dragging && 
            state.camera_drag_mouse_position.x == -1 && 
            !editor_is_toolbar_open() &&
            !input_is_action_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !input_is_action_pressed(INPUT_ACTION_RIGHT_CLICK) &&
            input_is_action_just_pressed(INPUT_ACTION_F5)) {
        if (!state.scenario_path.empty() && !state.scenario_is_saved) {
            editor_save(state.scenario_path.c_str());
        }
        editor_begin_playtest();
    }
}

bool editor_is_in_menu() {
    return state.menu.type != EDITOR_MENU_TYPE_NONE; 
}

bool editor_is_toolbar_open() {
    return state.ui.element_selected == state.ui_toolbar_id;
}

void editor_clamp_camera() {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, (state.scenario->map.width * TILE_SIZE) - CANVAS_RECT.w);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, (state.scenario->map.height * TILE_SIZE) - CANVAS_RECT.h);
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
            state.menu = (EditorMenu) {
                .type = EDITOR_MENU_TYPE_NEW,
                .mode = EDITOR_MENU_MODE_OPEN,
                .menu = editor_menu_new_open()
            };
        } else if (action == "Save") {
            if (state.scenario_path == "") {
                state.menu.type = EDITOR_MENU_TYPE_FILE;
                SDL_ShowSaveFileDialog(editor_save_callback, NULL, state.window, EDITOR_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str());
            } else {
                editor_save(state.scenario_path.c_str());
            }
        } else if (action == "Save As") {
            state.menu.type = EDITOR_MENU_TYPE_FILE;
            SDL_ShowSaveFileDialog(editor_save_callback, NULL, state.window, EDITOR_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str());
        } else if (action == "Open") {
            state.menu.type = EDITOR_MENU_TYPE_FILE;
            log_debug("%s", editor_get_scenario_folder_path().c_str());
            SDL_ShowOpenFileDialog(editor_open_callback, NULL, state.window, EDITOR_FILE_FILTERS, 1, editor_get_scenario_folder_path().c_str(), false);
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
        } else if (action == "Players") {
            state.menu = (EditorMenu) {
                .type = EDITOR_MENU_TYPE_PLAYERS,
                .mode = EDITOR_MENU_MODE_OPEN,
                .menu = editor_menu_players_open(state.scenario)
            };
        } else if (action == "Map Size") {
            state.menu = (EditorMenu) {
                .type = EDITOR_MENU_TYPE_MAP_SIZE,
                .mode = EDITOR_MENU_MODE_OPEN,
                .menu = editor_menu_map_size_open(state.scenario->map.width)
            };
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
        case EDITOR_TOOL_BRUSH: {
            if (state.tool_value > NOISE_VALUE_RAMP) {
                state.tool_value = NOISE_VALUE_LOWGROUND;
            }
            break;
        }
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
        case EDITOR_TOOL_ADD_ENTITY: {
            state.tool_value = ENTITY_GOLDMINE;
            state.tool_scroll = 0;
            break;
        }
        case EDITOR_TOOL_EDIT_ENTITY: {
            state.tool_value = INDEX_INVALID;
            break;
        }
        case EDITOR_TOOL_SQUADS: {
            state.tool_value = 0;
            state.tool_scroll = 0;
            break;
        }
        case EDITOR_TOOL_CONSTANTS: {
            state.tool_value = 0;
            state.tool_size = 1;
            break;
        }
        case EDITOR_TOOL_PLAYER_SPAWN:
            break;
    }
}

ivec2 editor_canvas_to_world_space(ivec2 point) {
    return point - ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + state.camera_offset;
}

ivec2 editor_get_hovered_cell() {
    return editor_canvas_to_world_space(input_get_mouse_position()) / TILE_SIZE;
}

uint32_t editor_get_hovered_entity() {
    ivec2 cell = editor_get_hovered_cell();

    for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = state.scenario->entities[entity_index];
        const int entity_cell_size = entity_get_data(entity.type).cell_size;
        Rect entity_cell_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        if (entity_cell_rect.has_point(cell)) {
            return entity_index;
        }
    }

    return INDEX_INVALID;
}

bool editor_is_hovered_cell_valid() {
    ivec2 cell = editor_get_hovered_cell();
    const uint32_t entity_index = editor_get_hovered_entity();
    if (state.tool == EDITOR_TOOL_DECORATE) {
        Cell map_cell = map_get_cell(state.scenario->map, CELL_LAYER_GROUND, cell);
        return map_cell.type == CELL_DECORATION || map_cell.type == CELL_EMPTY;
    }
    if (state.tool == EDITOR_TOOL_ADD_ENTITY || 
            (state.tool == EDITOR_TOOL_EDIT_ENTITY && state.is_painting)) {
        if (state.tool == EDITOR_TOOL_EDIT_ENTITY) {
            cell -= state.tool_edit_entity_offset;
        }
        EntityType entity_type = state.tool == EDITOR_TOOL_ADD_ENTITY
            ? (EntityType)state.tool_value
            : state.scenario->entities[state.tool_value].type;
        const EntityData& entity_data = entity_get_data(entity_type);
        return !map_is_cell_rect_occupied(state.scenario->map, entity_data.cell_layer, cell, entity_data.cell_size);
    }
    if (state.tool == EDITOR_TOOL_SQUADS && !state.scenario->squads.empty()) {
        if (state.scenario->squads[state.tool_value].type == BOT_SQUAD_TYPE_PATROL && 
                input_is_action_pressed(INPUT_ACTION_SHIFT)) {
            return true;
        }

        if (entity_index == INDEX_INVALID) {
            return false;
        }

        if (state.scenario->squads[state.tool_value].entity_count == SCENARIO_SQUAD_MAX_ENTITIES) {
            return false;
        }

        uint32_t squad_index = editor_get_entity_squad(entity_index);
        if (squad_index < state.scenario->squads.size() && squad_index != state.tool_value) {
            return false;
        }
        if (state.scenario->entities[entity_index].player_id != state.scenario->squads[state.tool_value].player_id) {
            return false;
        }

        return true;
    }
    if (state.tool == EDITOR_TOOL_CONSTANTS && !state.scenario->constants.empty()) {
        const ScenarioConstant& constant = state.scenario->constants[state.tool_value];
        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                return entity_index != INDEX_INVALID;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                return true;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                return false;
            }
        }
    }
    return true;
}

bool editor_can_single_use_tool_be_used() {
    return !editor_is_in_menu() &&
        !state.is_minimap_dragging &&
        state.camera_drag_mouse_position.x == -1 &&
        CANVAS_RECT.has_point(input_get_mouse_position()) &&
        !editor_is_toolbar_open();
}

const char* editor_get_noise_value_str(uint8_t noise_value) {
    switch (noise_value) {
        case NOISE_VALUE_WATER: 
            return "Water";
        case NOISE_VALUE_LOWGROUND:
            return "Lowground";
        case NOISE_VALUE_HIGHGROUND:
            return "Highground";
        case NOISE_VALUE_RAMP:
            return "Ramp";
        default:
            GOLD_ASSERT(false);
            return "";
    }
}

// Adds an action to the stack but does not execute the action
void editor_push_action(const EditorAction& action) {
    while (state.actions.size() > state.action_head) {
        state.actions.pop_back();
    }
    state.actions.push_back(action);
    state.action_head++;
}

// Adds an action to the stack and also executes the action
void editor_do_action(const EditorAction& action) {
    editor_push_action(action);
    editor_action_execute(state.scenario, action, EDITOR_ACTION_MODE_DO);
}

void editor_undo_action() {
    if (state.action_head == 0) {
        return;
    }

    state.action_head--;
    editor_action_execute(state.scenario, state.actions[state.action_head], EDITOR_ACTION_MODE_UNDO);

    editor_validate_tool_value();
}

void editor_redo_action() {
    if (state.action_head == state.actions.size()) {
        return;
    }

    editor_action_execute(state.scenario, state.actions[state.action_head], EDITOR_ACTION_MODE_DO);
    state.action_head++;

    editor_validate_tool_value();
}

void editor_clear_actions() {
    state.actions.clear();
}

bool editor_tool_brush_should_paint() {
    ivec2 cell = editor_get_hovered_cell();
    if (state.tool_value == NOISE_VALUE_RAMP && !map_can_ramp_be_placed_on_tile(map_get_tile(state.scenario->map, cell))) {
        return false;
    }
    return scenario_get_noise_map_value(state.scenario, cell) != (uint8_t)state.tool_value;
}

void editor_fill() {
    ivec2 cell = editor_get_hovered_cell();
    if (scenario_get_noise_map_value(state.scenario, cell) == (uint8_t)state.tool_value) {
        return;
    }

    std::vector<int> fill_indices;
    const uint8_t previous_value = scenario_get_noise_map_value(state.scenario, cell);
    const uint8_t new_value = (uint8_t)state.tool_value;

    std::vector<ivec2> frontier;
    std::vector<bool> is_explored(state.scenario->noise->width * state.scenario->noise->height, false);
    frontier.push_back(cell);

    while (!frontier.empty()) {
        ivec2 next = frontier.back();
        frontier.pop_back();
        
        if (is_explored[next.x + (next.y * state.scenario->noise->width)]) {
            continue;
        }

        fill_indices.push_back(next.x + (next.y * state.scenario->noise->width));
        is_explored[next.x + (next.y * state.scenario->noise->width)] = true;

        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(state.scenario->map, child)) {
                continue;
            }
            if (is_explored[child.x + (child.y * state.scenario->noise->width)]) {
                continue;
            }
            if (scenario_get_noise_map_value(state.scenario, child) != previous_value) {
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
    editor_do_action((EditorAction) {
        .type = EDITOR_ACTION_BRUSH,
        .data = (EditorActionBrush) {
            .stroke = stroke
        }
    });
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
            .index = cell.x + (cell.y * state.scenario->noise->width),
            .previous_value = scenario_get_noise_map_value(state.scenario, cell),
            .new_value = (uint8_t)state.tool_value
        });
    };
    for (int y = rect.y; y < rect.y + rect.h; y++) {
        for (int x = rect.x; x < rect.x + rect.w; x++) {
            stroke_push_back(ivec2(x, y));
        }
    }

    return stroke;
}

SpriteName editor_get_noise_preview_sprite(uint8_t value) {
    switch (value) {
        case NOISE_VALUE_WATER: {
            return map_choose_water_tile_sprite(state.scenario->map.type);
        }
        case NOISE_VALUE_LOWGROUND: {
            return map_get_plain_ground_tile_sprite(state.scenario->map.type);
        }
        case NOISE_VALUE_HIGHGROUND:
        case NOISE_VALUE_RAMP: {
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
            state.clipboard.values.push_back(scenario_get_noise_map_value(state.scenario, map_cell));
        }
    }
}

void editor_clipboard_paste(ivec2 top_left_cell) {
    std::vector<EditorActionBrushStroke> stroke;

    for (int y = 0; y < state.clipboard.height; y++) {
        for (int x = 0; x < state.clipboard.width; x++) {
            ivec2 cell = top_left_cell + ivec2(x, y);
            if (!map_is_cell_in_bounds(state.scenario->map, cell)) {
                continue;
            }

            int index = cell.x + (cell.y * state.scenario->noise->width);
            stroke.push_back((EditorActionBrushStroke) {
                .index = index,
                .previous_value = state.scenario->noise->map[index],
                .new_value = state.clipboard.values[x + (y * state.clipboard.width)]
            });
        }
    }

    editor_do_action((EditorAction) {
        .type = EDITOR_ACTION_BRUSH,
        .data = (EditorActionBrush) {
            .stroke = stroke
        }
    });
}

bool editor_is_using_noise_tool() {
    return state.tool == EDITOR_TOOL_BRUSH || 
        state.tool == EDITOR_TOOL_FILL || 
        state.tool == EDITOR_TOOL_RECT || 
        state.tool == EDITOR_TOOL_SELECT;
}

void editor_flatten_rect(const Rect& rect) {
    std::vector<EditorActionBrushStroke> stroke;

    for (int y = rect.y; y < rect.y + rect.h; y++) {
        for (int x = rect.x; x < rect.x + rect.w; x++) {
            stroke.push_back((EditorActionBrushStroke) {
                .index = x + (y * state.scenario->noise->width),
                .previous_value = state.scenario->noise->map[x + (y * state.scenario->noise->width)],
                .new_value = NOISE_VALUE_LOWGROUND
            });
        }
    }

    editor_do_action((EditorAction) {
        .type = EDITOR_ACTION_BRUSH,
        .data = (EditorActionBrush) {
            .stroke = stroke
        }
    });
}

void editor_generate_decorations() {
    std::vector<EditorActionDecorate> changes = editor_clear_deocrations();

    // Mark change indices
    std::vector<int> change_indices(state.scenario->map.width * state.scenario->map.height, -1);
    for (int change_index = 0; change_index < (int)changes.size(); change_index++) {
        change_indices[changes[change_index].index] = change_index;
    }

    // Get a list of goldmine cells
    std::vector<ivec2> goldmine_cells;
    for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
        if (state.scenario->entities[entity_index].type == ENTITY_GOLDMINE) {
            goldmine_cells.push_back(state.scenario->entities[entity_index].cell);
        }
    }

    // Generate decorations
    int generate_decorations_seed = rand();
    map_generate_decorations(state.scenario->map, state.scenario->noise, &generate_decorations_seed, goldmine_cells);

    // Record all changes
    for (int index = 0; index < state.scenario->map.width * state.scenario->map.height; index++) {
        Cell map_cell = state.scenario->map.cells[CELL_LAYER_GROUND][index];
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

    editor_push_action((EditorAction) {
        .type = EDITOR_ACTION_DECORATE_BULK,
        .data = (EditorActionDecorateBulk) {
            .changes = changes
        }
    });
} 

std::vector<EditorActionDecorate> editor_clear_deocrations() {
    std::vector<EditorActionDecorate> changes;

    // Clear all decorations and mark the changes
    for (int index = 0; index < state.scenario->map.width * state.scenario->map.height; index++) {
        Cell map_cell = state.scenario->map.cells[CELL_LAYER_GROUND][index];
        if (map_cell.type != CELL_DECORATION) {
            continue;
        }

        changes.push_back((EditorActionDecorate) {
            .index = index,
            .previous_hframe = map_cell.decoration_hframe,
            .new_hframe = EDITOR_ACTION_DECORATE_REMOVE_DECORATION
        });

        state.scenario->map.cells[CELL_LAYER_GROUND][index] = (Cell) {
            .type = CELL_EMPTY,
            .id = ID_NULL
        };
    }

    return changes;
}

bool editor_tool_is_scroll_enabled() {
    if (editor_is_in_menu() || 
            state.is_minimap_dragging ||
            state.is_painting ||
            state.camera_drag_mouse_position.x != -1 ||
            state.ui.element_selected != -1 ||
            !SIDEBAR_RECT.has_point(input_get_mouse_position())) {
        return false;
    }
    if (state.tool == EDITOR_TOOL_ADD_ENTITY) {
        return true;
    }
    if (state.tool == EDITOR_TOOL_SQUADS && !state.scenario->squads.empty()) {
        return true;
    }
    return false;
}

int editor_tool_get_scroll_max() {
    if (state.tool == EDITOR_TOOL_ADD_ENTITY) {
        int count = ENTITY_TYPE_COUNT / TOOL_ENTITY_ROW_SIZE;
        if (ENTITY_TYPE_COUNT % TOOL_ENTITY_ROW_SIZE != 0) {
            count++;
        }
        return count - (int)TOOL_ADD_ENTITY_VISIBLE_ROW_COUNT;
    }
    if (state.tool == EDITOR_TOOL_SQUADS && !state.scenario->squads.empty()) {
        const int squad_entity_count = (int)state.scenario->squads[state.tool_value].entity_count;
        int count = squad_entity_count / (int)TOOL_ENTITY_ROW_SIZE;
        if (squad_entity_count != 0 && squad_entity_count % TOOL_ENTITY_ROW_SIZE != 0) {
            count++;
        }
        return std::max(0, count - (int)TOOL_SQUADS_VISIBLE_ROW_COUNT);
    }
    return 0;
}

void editor_tool_edit_entity_set_selection(uint32_t entity_index) {
    state.tool_value = entity_index;

    if (state.tool_value == INDEX_INVALID) {
        return;
    }

    editor_validate_tool_value();
    const ScenarioEntity& entity = state.scenario->entities[entity_index];
    state.tool_edit_entity_gold_held = entity.gold_held;
}

void editor_validate_tool_value() {
    if (state.tool == EDITOR_TOOL_EDIT_ENTITY && state.tool_value >= state.scenario->entity_count) {
        state.tool_value = INDEX_INVALID;
    }
    if (state.tool == EDITOR_TOOL_SQUADS && state.tool_value >= state.scenario->squads.size()) {
        state.tool_value = 0;
    }
}

void editor_tool_edit_entity_delete_entity(uint32_t entity_index) {
    uint32_t entity_squad_index = INDEX_INVALID;
    for (uint32_t squad_index = 0; squad_index < state.scenario->squads.size(); squad_index++) {
        for (uint32_t squad_entity_index = 0; squad_entity_index < state.scenario->squads[squad_index].entity_count; squad_entity_index++) {
            if (state.scenario->squads[squad_index].entities[squad_entity_index] == entity_index) {
                entity_squad_index = squad_index;
                break;
            }
        }
    }

    std::vector<uint32_t> constant_indices;
    for (uint32_t constant_index = 0; constant_index < state.scenario->constants.size(); constant_index++) {
        if (state.scenario->constants[constant_index].type == SCENARIO_CONSTANT_TYPE_ENTITY && state.scenario->constants[constant_index].entity_index == entity_index) {
            constant_indices.push_back(constant_index);
        }
    }

    editor_do_action((EditorAction) {
        .type = EDITOR_ACTION_REMOVE_ENTITY,
        .data = (EditorActionRemoveEntity) {
            .index = entity_index,
            .value = state.scenario->entities[entity_index],
            .squad_index = entity_squad_index,
            .constant_indices = constant_indices
        }
    });
    state.tool_value = INDEX_INVALID;
}

uint32_t editor_get_entity_squad(uint32_t entity_index) {
    for (uint32_t squad_index = 0; squad_index < state.scenario->squads.size(); squad_index++) {
        for (uint32_t squad_entity_index = 0; squad_entity_index < state.scenario->squads[squad_index].entity_count; squad_entity_index++) {
            if (entity_index == state.scenario->squads[squad_index].entities[squad_entity_index]) {
                return squad_index;
            }
        }
    }

    return state.scenario->squads.size();
}

void editor_remove_entity_from_squad(uint32_t squad_index, uint32_t entity_index) {
    ScenarioSquad edited_squad = state.scenario->squads[squad_index];

    uint32_t squad_entity_index;
    for (squad_entity_index = 0; squad_entity_index < edited_squad.entity_count; squad_entity_index++) {
        if (edited_squad.entities[squad_entity_index] == entity_index) {
            break;
        }
    }
    GOLD_ASSERT(squad_entity_index != edited_squad.entity_count);

    edited_squad.entities[squad_entity_index] = edited_squad.entities[edited_squad.entity_count - 1];
    edited_squad.entity_count--;

    editor_do_action((EditorAction) {
        .type = EDITOR_ACTION_EDIT_SQUAD,
        .data = (EditorActionEditSquad) {
            .index = squad_index,
            .previous_value = state.scenario->squads[squad_index],
            .new_value = edited_squad
        }
    });
}

std::vector<uint32_t> editor_get_selected_entities() {
    std::vector<uint32_t> entities;

    if (state.tool == EDITOR_TOOL_EDIT_ENTITY && state.tool_value != INDEX_INVALID) {
        entities.push_back(state.tool_value);
    }

    if (state.tool == EDITOR_TOOL_SQUADS && !state.scenario->squads.empty()) {
        const ScenarioSquad& squad = state.scenario->squads[state.tool_value];
        for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_count; squad_entity_index++) {
            entities.push_back(squad.entities[squad_entity_index]);
        }
    }

    if (state.tool == EDITOR_TOOL_CONSTANTS && !state.scenario->constants.empty()) {
        const ScenarioConstant& constant = state.scenario->constants[state.tool_value];
        if (constant.type == SCENARIO_CONSTANT_TYPE_ENTITY && constant.entity_index < state.scenario->entity_count) {
            entities.push_back(constant.entity_index);
        }
    }

    return entities;
}

std::vector<std::string> editor_get_player_name_dropdown_items() {
    std::vector<std::string> items;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        char player_text[64];
        sprintf(player_text, "%u: %s", player_id, state.scenario->players[player_id].name);
        items.push_back(std::string(player_text));
    }
    
    return items;
}

ivec2 editor_get_player_spawn_camera_offset(ivec2 cell) {
    ivec2 camera_offset = ivec2(
        (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2),
        (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2));
    camera_offset.x = std::clamp(camera_offset.x, 0, (state.scenario->map.width * TILE_SIZE) - SCREEN_WIDTH);
    camera_offset.y = std::clamp(camera_offset.y, 0, (state.scenario->map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);

    return camera_offset;
}

std::string editor_get_scenario_folder_path() {
    std::string base_path = SDL_GetBasePath();
    // The substring is to remove the trailing "bin/"
    return base_path.substr(0, base_path.size() - 4) + "res" + GOLD_PATH_SEPARATOR + "scenario" + GOLD_PATH_SEPARATOR;
}

static void SDLCALL editor_save_callback(void* /*user_data*/, const char* const* filelist, int /*filter*/) {
    state.menu.type = EDITOR_MENU_TYPE_NONE;

    if (!filelist) {
        log_error("Error occured while saving files: %s", SDL_GetError());
        return;
    }

    if (!*filelist) {
        return;
    }

    editor_save(*filelist);
}

void editor_set_scenario_paths_to_defaults() {
    state.scenario_path = "";
    state.scenario_map_short_path = "map.dat";
    state.scenario_script_short_path = "script.lua";
}

void editor_save(const char* path) {
    std::string full_path = std::string(path);
    if (!string_ends_with(full_path, ".json")) {
        full_path += ".json";
    }
    bool success = scenario_save_file(state.scenario, full_path.c_str(), state.scenario_map_short_path.c_str(), state.scenario_script_short_path.c_str());
    if (success) {
        state.scenario_path = full_path;
        state.scenario_is_saved = true;
    }
}

static void SDLCALL editor_open_callback(void* /*user_data*/, const char* const* filelist, int /*filter*/) {
    state.menu.type = EDITOR_MENU_TYPE_NONE;

    if (!filelist) {
        log_error("Error occured while opening files: %s", SDL_GetError());
        return;
    }

    if (!*filelist) {
        return;
    }

    std::string map_short_path;
    std::string script_short_path;
    Scenario* opened_scenario = scenario_open_file(*filelist, &map_short_path, &script_short_path);
    if (opened_scenario == NULL) {
        return;
    }

    editor_free_current_document();
    state.scenario = opened_scenario;
    state.scenario_path = std::string(*filelist);
    state.scenario_map_short_path = map_short_path;
    state.scenario_script_short_path = script_short_path;
    state.scenario_is_saved = true;
}

bool editor_requests_playtest() {
    return state.should_playtest;
}

void editor_begin_playtest() {
    input_set_mouse_capture_enabled(true);
    SDL_SetWindowMouseGrab(state.window, true);
    state.should_playtest = true;
}

void editor_end_playtest() {
    input_set_mouse_capture_enabled(false);
    SDL_SetWindowMouseGrab(state.window, false);
    state.should_playtest = false;
}

const Scenario* editor_get_scenario() {
    return state.scenario;
}

std::string editor_get_scenario_script_path() {
    const std::string folder_path = filesystem_get_path_folder(state.scenario_path.c_str());
    return folder_path + state.scenario_script_short_path;
}

void editor_render() {
    std::vector<uint32_t> selection;

    if (state.scenario != NULL) {
        std::vector<RenderSpriteParams> ysort_params;
        selection = editor_get_selected_entities();

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
                    if (base_coords.x + x >= state.scenario->map.width || base_coords.y + y >= state.scenario->map.height) {
                        continue;
                    }

                    int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.scenario->map.width);
                    Tile tile = state.scenario->map.tiles[map_index];

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
                        map_is_tile_ground(state.scenario->map, base_coords + ivec2(x, y)) || 
                        map_is_tile_ramp(state.scenario->map, base_coords + ivec2(x, y));
                    if (elevation == 0 && 
                            !map_is_tile_ground(state.scenario->map, base_coords + ivec2(x, y)) &&
                            !map_is_tile_water(state.scenario->map, base_coords + ivec2(x, y))) {
                        render_sprite_frame(map_get_plain_ground_tile_sprite(state.scenario->map.type), ivec2(0, 0), tile_params_position, RENDER_SPRITE_NO_CULL, 0);
                    }
                    if ((should_render_on_ground_level && elevation == 0) || 
                            (!should_render_on_ground_level && elevation == tile.elevation)) {
                        render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                    } 

                    // Decorations
                    Cell cell = state.scenario->map.cells[CELL_LAYER_GROUND][map_index];
                    if (cell.type == CELL_DECORATION && tile.elevation == elevation) {
                        SpriteName decoration_sprite = map_get_decoration_sprite(state.scenario->map.type);
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
                // Underground entities
                if (cell_layer == CELL_LAYER_UNDERGROUND) {
                    for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
                        const ScenarioEntity& entity = state.scenario->entities[entity_index];
                        const EntityData& entity_data = entity_get_data(entity.type);
                        if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                                map_get_tile(state.scenario->map, entity.cell).elevation != elevation) {
                            continue;
                        }

                        RenderSpriteParams params = editor_create_entity_render_params(entity);
                        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                    }
                }
            } // End for each cell layer
        }

        // Entities
        for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
            // Don't render entity while we are moving it because 
            // it will be rendered by the move preview
            if (state.tool == EDITOR_TOOL_EDIT_ENTITY && state.is_painting && entity_index == state.tool_value) {
                continue;
            }

            const ScenarioEntity& entity = state.scenario->entities[entity_index];
            const EntityData& entity_data = entity_get_data(entity.type);
            if (entity_data.cell_layer != CELL_LAYER_GROUND) {
                continue;
            }

            RenderSpriteParams params = editor_create_entity_render_params(entity);
            const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_data(entity.type).sprite);
            const Rect render_rect = (Rect) {
                .x = params.position.x, .y = params.position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };

            if (!CANVAS_RECT.intersects(render_rect)) {
                continue;
            }
            params.options |= RENDER_SPRITE_NO_CULL;

            ysort_params.push_back(params);
        }

        // Render ysort params
        ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
        for (const RenderSpriteParams& params : ysort_params) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }

        // Balloon shadows
        for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
            const ScenarioEntity& entity = state.scenario->entities[entity_index];
            if (entity.type != ENTITY_BALLOON) {
                continue;
            }
            const EntityData& entity_data = entity_get_data(entity.type);
            ivec2 balloon_position = (entity.cell * TILE_SIZE) + ((ivec2(entity_data.cell_size, entity_data.cell_size) * TILE_SIZE) / 2);
            render_sprite_frame(SPRITE_UNIT_BALLOON_SHADOW, ivec2(0, 0), ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + balloon_position + ivec2(-5, 3) - state.camera_offset, 0, 0);
        }

        // Sky entities
        ysort_params.clear();
        for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
            // Don't render entity while we are moving it because 
            // it will be rendered by the move preview
            if (state.tool == EDITOR_TOOL_EDIT_ENTITY && state.is_painting && entity_index == state.tool_value) {
                continue;
            }

            const ScenarioEntity& entity = state.scenario->entities[entity_index];
            const EntityData& entity_data = entity_get_data(entity.type);
            if (entity_data.cell_layer != CELL_LAYER_SKY) {
                continue;
            }

            RenderSpriteParams params = editor_create_entity_render_params(entity);
            const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_data(entity.type).sprite);
            const Rect render_rect = (Rect) {
                .x = params.position.x, .y = params.position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };

            if (!CANVAS_RECT.intersects(render_rect)) {
                continue;
            }
            params.options |= RENDER_SPRITE_NO_CULL;

            ysort_params.push_back(params);
        }

        ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
        for (const RenderSpriteParams& params : ysort_params) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
    }

    // Entity add preview and
    // Entity edit->move preview and
    // Mouse hover rect
    if (CANVAS_RECT.has_point(input_get_mouse_position()) &&
            !state.is_minimap_dragging &&
            state.camera_drag_mouse_position.x == -1 &&
            !editor_is_in_menu() &&
            !editor_is_toolbar_open()) {
        if (state.tool == EDITOR_TOOL_ADD_ENTITY || 
                (state.tool == EDITOR_TOOL_EDIT_ENTITY && state.is_painting)) {
            // Determine preview entity type
            EntityType entity_type = state.tool == EDITOR_TOOL_ADD_ENTITY
                ? (EntityType)state.tool_value
                : state.scenario->entities[state.tool_value].type;
            const EntityData& entity_data = entity_get_data(entity_type);

            // Determine cell
            ivec2 cell = editor_get_hovered_cell();
            if (state.tool == EDITOR_TOOL_EDIT_ENTITY) {
                cell -= state.tool_edit_entity_offset;
            }

            // Determine preview position
            ivec2 entity_position = editor_entity_get_render_position(entity_type, cell);
            const SpriteInfo& sprite_info = render_get_sprite_info(entity_data.sprite);
            entity_position.x -= sprite_info.frame_width / 2;
            entity_position.y -= sprite_info.frame_height / 2;
            if (entity_type == ENTITY_BALLOON) {
                entity_position.y += ENTITY_SKY_POSITION_Y_OFFSET;
            }

            // Determine preview recolor ID
            uint8_t recolor_id;
            if (entity_type == ENTITY_GOLDMINE) {
                recolor_id = 0;
            } else if (state.tool == EDITOR_TOOL_ADD_ENTITY) {
                recolor_id = state.tool_add_entity_player_id;
            } else {
                recolor_id = state.scenario->entities[state.tool_value].player_id;
            }

            render_sprite_frame(entity_data.sprite, editor_entity_get_animation_frame(entity_type), entity_position, RENDER_SPRITE_NO_CULL, recolor_id);

            // Render blocked rects for goldmines
            for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
                const ScenarioEntity& entity = state.scenario->entities[entity_index];
                if (entity.type != ENTITY_GOLDMINE) {
                    continue;
                }

                Rect block_rect = entity_goldmine_get_block_building_rect(entity.cell);
                Rect render_rect = editor_cell_rect_to_world_space(block_rect);
                render_rect.x += CANVAS_RECT.x;
                render_rect.y += CANVAS_RECT.y;

                if (CANVAS_RECT.intersects(render_rect)) {
                    render_draw_rect(render_rect, RENDER_COLOR_GOLD);
                }
            }
        }

        ivec2 cell = editor_get_hovered_cell();
        int cell_size = 1;
        if (state.tool == EDITOR_TOOL_CONSTANTS && !state.scenario->constants.empty()) {
            const ScenarioConstant& constant = state.scenario->constants[state.tool_value];
            if (constant.type == SCENARIO_CONSTANT_TYPE_ENTITY && editor_is_hovered_cell_valid()) {
                uint32_t entity_index = editor_get_hovered_entity();
                const ScenarioEntity& entity = state.scenario->entities[entity_index];
                cell = entity.cell;
                cell_size = entity_get_data(entity.type).cell_size;
            }
        }
        Rect rect = (Rect) {
            .x = ((cell.x * TILE_SIZE) - state.camera_offset.x) + CANVAS_RECT.x,
            .y = ((cell.y * TILE_SIZE) - state.camera_offset.y) + CANVAS_RECT.y,
            .w = cell_size * TILE_SIZE, .h = cell_size * TILE_SIZE
        };
        render_draw_rect(rect, state.scenario == NULL || editor_is_hovered_cell_valid() ? RENDER_COLOR_WHITE : RENDER_COLOR_RED);
    }

    // Constants - chosen value preview
    if (state.tool == EDITOR_TOOL_CONSTANTS && !state.scenario->constants.empty()) {
        const ScenarioConstant& constant = state.scenario->constants[state.tool_value];
        ivec2 cell = ivec2(-1, -1);
        int cell_size = 1;
        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                // Nothing to do for this constant type because
                // it will already be rendered in the entity selection
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                cell = constant.cell;
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }

        if (cell.x != -1) {
            Rect rect = (Rect) {
                .x = ((cell.x * TILE_SIZE) - state.camera_offset.x) + CANVAS_RECT.x,
                .y = ((cell.y * TILE_SIZE) - state.camera_offset.y) + CANVAS_RECT.y,
                .w = cell_size * TILE_SIZE, .h = cell_size * TILE_SIZE
            };
            render_draw_rect(rect, RENDER_COLOR_WHITE);
        }
    }

    // Tool rect preview
    if (state.is_painting && state.tool == EDITOR_TOOL_RECT) {
        Rect rect = editor_tool_rect_get_rect();
        for (int y = rect.y; y < rect.y + rect.h; y++) {
            for (int x = rect.x; x < rect.x + rect.w; x++) {
                ivec2 cell = ivec2(x, y);
                ivec2 cell_position = ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + (cell * TILE_SIZE) - state.camera_offset;
                render_sprite_frame(editor_get_noise_preview_sprite(state.tool_value), ivec2(0, 0), cell_position, RENDER_SPRITE_NO_CULL, 0);
            }
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

    // Squad patrol cell
    if (state.tool == EDITOR_TOOL_SQUADS && 
            !state.scenario->squads.empty() &&
            state.scenario->squads[state.tool_value].type == BOT_SQUAD_TYPE_PATROL &&
            state.scenario->squads[state.tool_value].patrol_cell.x != -1) {
        ivec2 cell = state.scenario->squads[state.tool_value].patrol_cell;
        Rect rect = (Rect) {
            .x = ((cell.x * TILE_SIZE) - state.camera_offset.x) + CANVAS_RECT.x,
            .y = ((cell.y * TILE_SIZE) - state.camera_offset.y) + CANVAS_RECT.y,
            .w = TILE_SIZE, .h = TILE_SIZE
        };
        render_draw_rect(rect, RENDER_COLOR_WHITE);
    }

    // Entity selection rect
    for (uint32_t entity_index : selection) {
        const ScenarioEntity& entity = state.scenario->entities[entity_index];
        const int entity_cell_size = entity_get_data(entity.type).cell_size;
        const Rect& entity_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        Rect render_rect = editor_cell_rect_to_world_space(entity_rect);
        render_rect.x += CANVAS_RECT.x;
        render_rect.y += CANVAS_RECT.y;

        if (CANVAS_RECT.intersects(render_rect)) {
            render_draw_rect(render_rect, RENDER_COLOR_WHITE);
        }
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

    // Camera rect
    ivec2 camera_cell = ivec2(-1, -1);
    if (state.tool == EDITOR_TOOL_PLAYER_SPAWN) {
        camera_cell = state.scenario->player_spawn;
    }
    if (state.tool == EDITOR_TOOL_CONSTANTS && 
            !state.scenario->constants.empty() && 
            state.scenario->constants[state.tool_value].type == SCENARIO_CONSTANT_TYPE_CELL &&
            input_is_action_pressed(INPUT_ACTION_SHIFT)) {
        camera_cell = state.scenario->constants[state.tool_value].cell;
    }
    if (camera_cell.x != -1) {
        ivec2 camera_offset = editor_get_player_spawn_camera_offset(camera_cell);
        const Rect rendered_rect = (Rect) {
            .x = camera_offset.x + CANVAS_RECT.x - state.camera_offset.x,
            .y = camera_offset.y + CANVAS_RECT.y - state.camera_offset.y,
            .w = SCREEN_WIDTH,
            .h = SCREEN_HEIGHT - 86
        };
        if (CANVAS_RECT.intersects(rendered_rect)) {
            render_draw_rect(rendered_rect, RENDER_COLOR_PLAYER_UI0);
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

    if (state.scenario != NULL) {
        // MINIMAP
        // Minimap tiles
        for (int y = 0; y < state.scenario->map.height; y++) {
            for (int x = 0; x < state.scenario->map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_TILE, ivec2(x, y), editor_get_minimap_pixel_for_cell(ivec2(x, y)));
            }
        }
        // Minimap entities
        for (uint32_t entity_index = 0; entity_index < state.scenario->entity_count; entity_index++) {
            const ScenarioEntity& entity = state.scenario->entities[entity_index];
            int entity_cell_size = entity_get_data(entity.type).cell_size;
            Rect entity_rect = (Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size, .h = entity_cell_size
            };
            render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, editor_get_minimap_pixel_for_entity(selection, entity_index));
        }
        // Clear fog layer
        for (int y = 0; y < state.scenario->map.height; y++) {
            for (int x = 0; x < state.scenario->map.width; x++) {
                render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), MINIMAP_PIXEL_TRANSPARENT);
            }
        }
        // Player spawn rect
        if (state.tool == EDITOR_TOOL_PLAYER_SPAWN) {
            ivec2 camera_offset = editor_get_player_spawn_camera_offset(state.scenario->player_spawn);
            Rect spawn_rect = (Rect) {
                .x = camera_offset.x / TILE_SIZE,
                .y = camera_offset.y / TILE_SIZE,
                .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
                .h = ((SCREEN_HEIGHT - 86) / TILE_SIZE)
            };
            render_minimap_draw_rect(MINIMAP_LAYER_FOG, spawn_rect, MINIMAP_PIXEL_PLAYER0);
        }
        // Minimap selection rect
        if ((state.is_painting && state.tool == EDITOR_TOOL_SELECT) || state.tool_select_selection.x != -1) {
            const Rect rect = state.is_painting && state.tool == EDITOR_TOOL_SELECT 
                ? editor_tool_select_get_select_rect()
                : state.tool_select_selection;
            render_minimap_draw_rect(MINIMAP_LAYER_FOG, rect, MINIMAP_PIXEL_WHITE);
        }
        // Minimap camera rect
        Rect camera_rect = (Rect) {
            .x = state.camera_offset.x / TILE_SIZE,
            .y = state.camera_offset.y / TILE_SIZE,
            .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
            .h = (SCREEN_HEIGHT / TILE_SIZE)
        };
        render_minimap_draw_rect(MINIMAP_LAYER_FOG, camera_rect, MINIMAP_PIXEL_WHITE);
        render_minimap_queue_render(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state.scenario->map.width, state.scenario->map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
    }
}

ivec2 editor_entity_get_animation_frame(EntityType type) {
    if (type == ENTITY_GOLDMINE) {
        return ivec2(0, 0);
    }
    if (entity_is_building(type)) {
        return ivec2(3, 0);
    }
    return ivec2(0, 0);
}

ivec2 editor_entity_get_render_position(EntityType type, ivec2 cell) {
    const EntityData& entity_data = entity_get_data(type);
    return ivec2(CANVAS_RECT.x, CANVAS_RECT.y) + 
        ((cell * TILE_SIZE) + 
            (ivec2(entity_data.cell_size, entity_data.cell_size) * TILE_SIZE / 2)) - 
        state.camera_offset;
}

RenderSpriteParams editor_create_entity_render_params(const ScenarioEntity& entity) {
    ivec2 params_position = editor_entity_get_render_position(entity.type, entity.cell);
    RenderSpriteParams params = (RenderSpriteParams) {
        .sprite = entity_get_data(entity.type).sprite,
        .frame = editor_entity_get_animation_frame(entity.type),
        .position = params_position,
        .ysort_position = params_position.y,
        .options = 0,
        .recolor_id = entity.type == ENTITY_GOLDMINE ? 0 : entity.player_id
    };
    
    const SpriteInfo& sprite_info = render_get_sprite_info(params.sprite);
    params.position.x -= sprite_info.frame_width / 2;
    params.position.y -= sprite_info.frame_height / 2;
    if (entity.type == ENTITY_BALLOON) {
        params.position.y += ENTITY_SKY_POSITION_Y_OFFSET;
    }

    return params;
}

MinimapPixel editor_get_minimap_pixel_for_cell(ivec2 cell) {
    switch (map_get_tile(state.scenario->map, cell).sprite) {
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

MinimapPixel editor_get_minimap_pixel_for_entity(const std::vector<uint32_t>& selection, uint32_t entity_index) {
    for (uint32_t index : selection) {
        if (entity_index == index) {
            return MINIMAP_PIXEL_WHITE;
        }
    }

    const ScenarioEntity& entity = state.scenario->entities[entity_index];
    if (entity.type == ENTITY_GOLDMINE) {
        return MINIMAP_PIXEL_GOLD;
    }
    return (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + entity.player_id);
}

#endif
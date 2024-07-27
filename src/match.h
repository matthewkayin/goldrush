#pragma once

#include "defines.h"
#include "util.h"
#include "sprite.h"
#include "container.h"
#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>

#define MAX_UNITS 200
#define MAX_PARTICLES 256

const uint32_t CELL_EMPTY = 0;
const uint32_t CELL_UNIT = 1 << 16;
const uint32_t CELL_BUILDING = 2 << 16;
const uint32_t CELL_GOLD1 = 3 << 16;
const uint32_t CELL_GOLD2 = 4 << 16;
const uint32_t CELL_GOLD3 = 5 << 16;

const uint32_t CELL_TYPE_MASK = 0xffff0000;
const uint32_t CELL_ID_MASK = 0x0000ffff;
const int UI_HEIGHT = 88;
const rect_t MINIMAP_RECT = rect_t(xy(4, SCREEN_HEIGHT - 132), xy(128, 128));

// Input

enum InputType {
    INPUT_NONE,
    INPUT_MOVE,
    INPUT_STOP,
    INPUT_BUILD,
    INPUT_BUILD_CANCEL
};

struct input_move_t {
    xy target_cell;
    uint8_t unit_count;
    entity_id unit_ids[MAX_UNITS];
};

struct input_stop_t {
    uint8_t unit_count;
    entity_id unit_ids[MAX_UNITS];
};

struct input_build_t {
    uint16_t unit_id;
    uint8_t building_type;
    xy target_cell;
};

struct input_build_cancel_t {
    entity_id building_id;
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
        input_stop_t stop;
        input_build_t build;
        input_build_cancel_t build_cancel;
    };
};

// UI

enum UiMode {
    UI_MODE_NOT_STARTED,
    UI_MODE_NONE,
    UI_MODE_SELECTING,
    UI_MODE_MINIMAP_DRAG,
    UI_MODE_BUILDING_PLACE
};

enum UiButtonset {
    UI_BUTTONSET_NONE,
    UI_BUTTONSET_UNIT,
    UI_BUTTONSET_MINER,
    UI_BUTTONSET_BUILD,
    UI_BUTTONSET_CANCEL
};

enum SelectionType {
    SELECTION_TYPE_NONE,
    SELECTION_TYPE_UNITS,
    SELECTION_TYPE_BUILDINGS
};

struct selection_t {
    SelectionType type;
    std::vector<entity_id> ids;
};

// Unit

enum UnitType {
    UNIT_MINER
};

enum BuildingType {
    BUILDING_NONE,
    BUILDING_HOUSE,
    BUILDING_CAMP
};

struct path_t {
    std::vector<xy> points;
    xy target;
};

enum UnitTargetType {
    UNIT_TARGET_NONE,
    UNIT_TARGET_CELL,
    UNIT_TARGET_BUILD,
    UNIT_TARGET_UNIT,
    UNIT_TARGET_CAMP,
    UNIT_TARGET_GOLD
};

enum UnitMode {
    UNIT_MODE_NONE,
    UNIT_MODE_MOVING,
    UNIT_MODE_BUILDING,
    UNIT_MODE_MINING
};

struct unit_t {
    UnitType type;
    uint8_t player_id;

    animation_t animation;
    uint32_t health;

    Direction direction;
    xy_fixed position;
    xy cell;
    UnitTargetType target;
    xy target_cell;
    entity_id target_entity_id;
    std::vector<xy> path;

    BuildingType building_type;
    entity_id building_id;
    xy building_cell;

    uint32_t gold_held;

    uint32_t timer;
};

struct unit_data_t {
    Sprite sprite;
    uint32_t max_health;
    fixed speed;
};

extern const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA;

// Building

struct building_t {
    uint8_t player_id;
    BuildingType type;
    uint32_t health;

    xy cell;

    bool is_finished;
};

struct building_data_t {
    int cell_width;
    int cell_height;
    uint32_t cost;
    uint32_t max_health;
    int builder_positions_x[3];
    int builder_positions_y[3];
    int builder_flip_h[3];
};

extern const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA;

struct match_state_t {
    // UI
    UiMode ui_mode;
    UiButtonset ui_buttonset;
    xy camera_offset;
    std::string ui_status_message;
    uint32_t ui_status_timer;
    xy select_origin;
    rect_t select_rect;
    selection_t selection;
    BuildingType ui_building_type;
    uint32_t ui_move_value;
    animation_t ui_move_animation;
    xy ui_move_position;

    // Inputs
    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    // Map
    std::vector<uint32_t> map_tiles;
    std::vector<uint32_t> map_cells;
    uint32_t map_width;
    uint32_t map_height;

    // Entities
    id_array<unit_t> units;
    id_array<building_t> buildings;

    // Players
    uint32_t player_gold[MAX_PLAYERS];
};

match_state_t match_init();
void match_update(match_state_t& state);

// Input
void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);

// UI
void match_ui_show_status(match_state_t& state, const char* message);
UiButton match_get_ui_button(const match_state_t& state, int index);
int match_get_ui_button_hovered(const match_state_t& state);
const rect_t& match_get_ui_button_rect(int index);
void match_ui_handle_button_pressed(match_state_t& state, UiButton button);
bool match_is_mouse_in_ui();
xy match_ui_building_cell(const match_state_t& state);
selection_t match_ui_create_selection_from_rect(const match_state_t& state);
void match_ui_set_selection(match_state_t& state, selection_t& selection);
xy match_camera_clamp(xy camera_offset, int map_width, int map_height);
xy match_camera_centered_on_cell(xy cell);

// Map
xy_fixed match_cell_center(xy cell);
xy match_get_nearest_free_cell_within_rect(xy start_cell, rect_t rect);
xy match_get_first_empty_cell_around_rect(const match_state_t& state, rect_t rect);
xy match_get_nearest_free_cell_around_building(const match_state_t& state, xy start_cell, const building_t& building);
bool match_map_is_cell_in_bounds(const match_state_t& state, xy cell);
bool match_map_is_cell_blocked(const match_state_t& state, xy cell);
bool match_map_is_cell_rect_blocked(const match_state_t& state, rect_t cell_rect);
uint32_t match_map_get_cell_value(const match_state_t& state, xy cell);
uint32_t match_map_get_cell_type(const match_state_t& state, xy cell);
entity_id match_map_get_cell_id(const match_state_t& state, xy cell);
void match_map_set_cell_value(match_state_t& state, xy cell, uint32_t type, uint32_t id = 0);
void match_map_set_cell_rect_value(match_state_t& state, rect_t cell_rect, uint32_t type, uint32_t id = 0);
bool match_map_is_cell_gold(const match_state_t& state, xy cell);
void match_map_decrement_gold(match_state_t& state, xy cell);

// Unit
void match_unit_create(match_state_t& state, uint8_t player_id, UnitType type, const xy& cell);
void match_unit_update(match_state_t& state);
void match_unit_path_to_target(const match_state_t& state, unit_t& unit);
void match_unit_on_movement_finished(match_state_t& state, entity_id unit_id);
rect_t match_unit_get_rect(const unit_t& unit);
UnitMode match_unit_get_mode(const unit_t& unit);
AnimationName match_unit_get_expected_animation(const unit_t& unit);
int match_unit_get_animation_vframe(const unit_t& unit);
void match_unit_stop_building(match_state_t& state, unit_t& unit, const building_t& building);
void match_unit_try_return_gold(const match_state_t& state, unit_t& unit);
void match_unit_try_target_nearest_gold(const match_state_t& state, unit_t& unit);

// Building
entity_id match_building_create(match_state_t& state, uint8_t player_id, BuildingType type, xy cell);
void match_building_destroy(match_state_t& state, entity_id building_id);
xy match_building_cell_size(BuildingType type);
rect_t match_building_get_rect(const building_t& building);
bool match_building_can_be_placed(const match_state_t& state, BuildingType type, xy cell);
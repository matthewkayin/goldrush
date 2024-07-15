#pragma once

#include "defines.h"
#include "util.h"
#include "container.h"
#include "animation.h"
#include "match_input.h"
#include "map.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <array>

const int UI_HEIGHT = 88;
const rect_t MINIMAP_RECT = rect_t(ivec2(4, SCREEN_HEIGHT - 132), ivec2(128, 128));

// ENUMS

enum Button {
    BUTTON_NONE,
    BUTTON_MOVE,
    BUTTON_STOP,
    BUTTON_ATTACK,
    BUTTON_BUILD,
    BUTTON_CANCEL,
    BUTTON_BUILD_HOUSE,
    BUTTON_BUILD_CAMP,
    BUTTON_COUNT
};

enum MatchMode {
    MATCH_MODE_NOT_STARTED,
    MATCH_MODE_RUNNING
};

enum UiMode {
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

enum BuildingType {
    BUILDING_HOUSE,
    BUILDING_CAMP,
    BUILDING_TYPE_COUNT
};

enum OrderType {
    ORDER_NONE,
    ORDER_MOVE,
    ORDER_BUILD
};

struct order_move_t {
    ivec2 target_cell;
};

struct order_build_t {
    BuildingType building_type;
    ivec2 building_cell;
    uint16_t building_id;
    ivec2 unit_cell;
};

struct order_t {
    OrderType type;
    union {
        order_move_t move;
        order_build_t build;
    };
};

enum UnitType {
    UNIT_MINER,
    UNIT_TYPE_COUNT
};

enum UnitMode {
    UNIT_MODE_IDLE,
    UNIT_MODE_STEP,
    UNIT_MODE_BUILD
};

struct unit_data_t {
    uint32_t cost;
    uint32_t max_health;
};

struct unit_t {
    uint8_t player_id;
    UnitType type;
    uint32_t health;

    order_t order;
    int direction;

    ivec2 cell;
    vec2 position;
    vec2 target_position;
    std::vector<ivec2> path;

    uint32_t path_timer;
    uint32_t build_timer;

    animation_state_t animation;
};

struct building_data_t {
    int cell_width;
    int cell_height;
    uint32_t cost;
    uint32_t max_health;
    int builder_positions_x[3];
    int builder_positions_y[3];
    bool builder_flip_h[3];

    ivec2 cell_size() const {
        return ivec2(cell_width, cell_height);
    }
    ivec2 builder_positions(int i) const {
        return ivec2(builder_positions_x[i], builder_positions_y[i]);
    }
};

struct building_t {
    uint8_t player_id;
    BuildingType type;
    ivec2 cell;
    uint32_t health;
    bool is_finished;
    bool is_selected;
};

// Selection

enum SelectionType {
    SELECTION_TYPE_NONE,
    SELECTION_TYPE_UNITS,
    SELECTION_TYPE_BUILDINGS
};

struct selection_t {
    SelectionType type;
    std::vector<uint16_t> ids;
};

// State

struct match_state_t {
    MatchMode mode;

    // Inputs
    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    // UI
    UiMode ui_mode;
    UiButtonset ui_buttonset;
    ivec2 camera_offset;
    ivec2 select_origin;
    rect_t select_rect;
    selection_t selection;

    ivec2 ui_move_position;
    animation_state_t ui_move_animation;

    int ui_button_hovered;

    BuildingType ui_building_type;
    ivec2 ui_building_cell;

    std::string ui_status_message;
    uint32_t ui_status_timer;

    map_t map;

    // Units and players
    id_array<unit_t> units;
    id_array<building_t> buildings;
    uint32_t player_gold[MAX_PLAYERS];
};

match_state_t match_init();
void match_update(match_state_t& state);

inline vec2 cell_center_position(const ivec2& cell) {
    return ivec2(cell * TILE_SIZE) + ivec2(TILE_SIZE / 2, TILE_SIZE / 2);
}

void ui_show_status(match_state_t& state, const char* message);
void ui_set_selection(match_state_t& state, selection_t& selection);
selection_t selection_create(const id_array<unit_t>& units, const id_array<building_t>& buildings, uint8_t current_player_id, const rect_t& select_rect);

ivec2 camera_clamp(const ivec2& camera_offset, int map_width, int map_height);
ivec2 camera_centered_on_cell(const ivec2& cell);

void unit_create(match_state_t& state, uint8_t player_id, UnitType type, const ivec2& cell);
rect_t unit_rect(const unit_t& unit);
bool unit_is_moving(const unit_t& unit);
bool unit_is_building(const unit_t& unit);

uint8_t building_create(match_state_t& state, uint8_t player_id, BuildingType type, ivec2 cell);
rect_t building_rect(const building_t& building);
void building_destroy(match_state_t& state, uint8_t building_id);

extern const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA;
extern const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA;
extern const std::unordered_map<UiButtonset, std::array<Button, 6>> UI_BUTTONS;
extern const rect_t UI_BUTTON_RECT[6];
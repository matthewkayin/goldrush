#pragma once

#include "defines.h"
#include "util.h"
#include "container.h"
#include "animation.h"
#include "match_input.h"
#include <vector>
#include <unordered_map>
#include <string>

const int UI_HEIGHT = 88;
const rect_t MINIMAP_RECT = rect_t(ivec2(4, SCREEN_HEIGHT - 132), ivec2(128, 128));

// ENUMS

enum ButtonIcon {
    BUTTON_ICON_MOVE,
    BUTTON_ICON_STOP,
    BUTTON_ICON_ATTACK,
    BUTTON_ICON_BUILD,
    BUTTON_ICON_BUILD_HOUSE,
    BUTTON_ICON_CANCEL,
    BUTTON_ICON_COUNT
};

enum BuildingType {
    BUILDING_HOUSE
};

enum CellValue {
    CELL_EMPTY,
    CELL_FILLED
};

enum MatchMode {
    MATCH_MODE_NOT_STARTED,
    MATCH_MODE_RUNNING
};

enum OrderType {
    ORDER_NONE,
    ORDER_MOVE,
    ORDER_BUILD
};

enum UiMode {
    UI_MODE_NONE,
    UI_MODE_SELECTING,
    UI_MODE_MINIMAP_DRAG,
    UI_MODE_UNIT,
    UI_MODE_MINER,
    UI_MODE_BUILD,
    UI_MODE_BUILDING_PLACE,
    UI_MODE_BUILDING_IN_PROGRESS
};

enum UnitMode {
    UNIT_MODE_IDLE,
    UNIT_MODE_STEP,
    UNIT_MODE_BUILD
};

enum UnitType {
    UNIT_MINER
};

struct timer_t {
    uint32_t value;
    bool is_finished = false;
    bool is_stopped = true;

    void start(uint32_t duration) {
        value = duration;
        is_stopped = false;
        is_finished = false;
    }
    void stop() {
        is_stopped = true;
        is_finished = false;
    }
    void update() {
        if (is_stopped) {
            return;
        }

        value--;
        if (value == 0) {
            is_finished = true;
        }
    }
};

struct ui_button_t {
    bool enabled;
    ButtonIcon icon;
    rect_t rect;
};

struct order_move_t {
    ivec2 cell;
};

struct order_build_t {
    ivec2 cell;
    BuildingType type;
};

struct unit_data_t {
    uint32_t cost;
    uint32_t max_health;
};

struct unit_t {
    UnitType type;
    UnitMode mode;

    uint32_t health;

    bool is_selected;
    OrderType order_type;
    ivec2 order_cell;
    BuildingType order_building_type;

    int direction;
    ivec2 cell;
    vec2 position;
    vec2 target_position;

    std::vector<ivec2> path;
    timer_t path_timer;
    static const uint32_t PATH_PAUSE_DURATION = 60;

    uint8_t building_id;
    timer_t build_timer;
    static const uint32_t BUILD_TICK_DURATION = 8;

    animation_t animation;
};

struct building_data_t {
    int cell_width;
    int cell_height;
    uint32_t cost;
    uint32_t max_health;
    int builder_positions_x[3];
    int builder_positions_y[3];

    ivec2 cell_size() const {
        return ivec2(cell_width, cell_height);
    }
    ivec2 builder_positions(int i) const {
        return ivec2(builder_positions_x[i], builder_positions_y[i]);
    }
};

struct building_t {
    BuildingType type;
    ivec2 cell;
    uint32_t health;
    bool is_finished;
    bool is_selected;
};

struct match_t {
    MatchMode mode;

    // Inputs
    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    // UI
    UiMode ui_mode;
    ivec2 camera_offset;
    ivec2 select_origin;
    rect_t select_rect;
    uint8_t selected_building_id;
    bool is_selecting_building;

    ivec2 ui_move_position;
    animation_t ui_move_animation;

    ui_button_t ui_buttons[6];
    int ui_button_hovered;

    BuildingType ui_building_type;
    ivec2 ui_building_cell;

    std::string ui_status_message;
    timer_t ui_status_timer;
    static const uint32_t UI_STATUS_DURATION = 60;

    // Map
    std::vector<int> map_tiles;
    std::vector<int> map_cells;
    int map_width;
    int map_height;

    // Units and players
    id_array<unit_t> units[MAX_PLAYERS];
    id_array<building_t> buildings[MAX_PLAYERS];
    uint32_t player_gold[MAX_PLAYERS];

    void init();
    void update();
    void handle_input(uint8_t player_id, const input_t& input);

    void ui_show_status(const char* message);
    void ui_on_selection_changed();
    void ui_refresh_buttons();
    void ui_handle_button_pressed(ButtonIcon icon);
    void ui_try_begin_building_place(BuildingType building_type);
    void camera_clamp();
    void camera_move_to_cell(ivec2 cell);

    bool cell_is_in_bounds(ivec2 cell) const;
    bool cell_is_blocked(ivec2 cell) const;
    bool cell_is_blocked(ivec2 cell, ivec2 cell_size) const;
    void cell_set_value(ivec2 cell, int value);
    void cell_set_value(ivec2 cell, ivec2 cell_size, int value);

    void unit_create(uint8_t player_id, UnitType type, ivec2 cell);
    rect_t unit_get_rect(const unit_t& unit) const;
    void unit_try_step(unit_t& unit);
    void unit_update(uint8_t player_id, unit_t& unit);
    void unit_eject_from_building(unit_t& unit, building_t& building);
    std::vector<ivec2> pathfind(ivec2 from, ivec2 to);

    uint8_t building_create(uint8_t player_id, BuildingType type, ivec2 cell);
    void building_destroy(uint8_t player_id, uint8_t building_id);
    rect_t building_get_rect(const building_t& building) const;
    ivec2 building_get_nearest_free_cell(building_t& building) const;
};

const std::unordered_map<uint32_t, unit_data_t> unit_data = {
    { UNIT_MINER, (unit_data_t) {
        .cost = 50,
        .max_health = 20
    }}
};

const std::unordered_map<uint32_t, building_data_t> building_data = {
    { BUILDING_HOUSE, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 100,
        .max_health = 100,
        .builder_positions_x = { 3, 16, -4 },
        .builder_positions_y = { 15, 15, 3 },
    }}
};
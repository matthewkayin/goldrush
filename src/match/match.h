#pragma once

#include "defines.h"
#include "util.h"
#include "container.h"
#include "animation.h"
#include "match_input.h"
#include <vector>
#include <unordered_map>

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
    UI_MODE_MINER,
    UI_MODE_BUILD,
    UI_MODE_BUILDING_PLACE
};

enum UnitMode {
    UNIT_MODE_IDLE,
    UNIT_MODE_STEP,
    UNIT_MODE_BUILD
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

struct unit_t {
    bool is_selected;
    UnitMode mode;
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
    int builder_position_x[3];
    int builder_position_y[3];
};

const std::unordered_map<uint32_t, building_data_t> building_data = {
    { BUILDING_HOUSE, (building_data_t) {
        .cell_width = 2, .cell_height = 2,
        .cost = 100,
        .max_health = 100,
        .builder_position_x = { 3, 16, -4 },
        .builder_position_y = { 15, 15, 3 }
    }}
};

struct building_t {
    BuildingType type;
    ivec2 cell;
    uint32_t health;
    bool is_finished;
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

    ivec2 ui_move_position;
    animation_t ui_move_animation;

    ui_button_t ui_buttons[6];
    int ui_button_hovered;

    BuildingType ui_building_type;
    ivec2 ui_building_cell;

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

    void ui_on_selection_changed();
    void ui_refresh_buttons();
    void ui_handle_button_pressed(ButtonIcon icon);
    void camera_clamp();
    void camera_move_to_cell(ivec2 cell);

    bool cell_is_in_bounds(ivec2 cell) const;
    bool cell_is_blocked(ivec2 cell) const;
    bool cell_is_blocked(ivec2 cell, ivec2 cell_size) const;
    void cell_set_value(ivec2 cell, int value);
    void cell_set_value(ivec2 cell, ivec2 cell_size, int value);

    void unit_create(uint8_t player_id, ivec2 cell);
    rect_t unit_get_rect(unit_t& unit) const;
    void unit_try_step(unit_t& unit);
    void unit_update(uint8_t player_id, unit_t& unit);
    std::vector<ivec2> pathfind(ivec2 from, ivec2 to);

    uint8_t building_create(uint8_t player_id, BuildingType type, ivec2 cell);
    ivec2 building_get_nearest_free_cell(building_t& building) const;
};
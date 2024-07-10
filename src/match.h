#pragma once

#include "defines.h"
#include "util.h"
#include "container.h"
#include <vector>
#include <unordered_map>

const uint32_t MAX_UNITS = 200;
const uint32_t MAX_BUILDINGS = 64;
const int CELL_EMPTY = 0;
const int CELL_FILLED = 1;
const int UI_HEIGHT = 88;
const rect_t MINIMAP_RECT = rect_t(ivec2(4, SCREEN_HEIGHT - 132), ivec2(128, 128));

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

enum Animation {
    ANIMATION_UI_MOVE,
    ANIMATION_UNIT_IDLE,
    ANIMATION_UNIT_MOVE,
    ANIMATION_UNIT_ATTACK,
    ANIMATION_UI_BUTTON_HOVER,
    ANIMATION_UI_BUTTON_UNHOVER,
};

struct animation_t {
    Animation animation;
    uint32_t timer;
    ivec2 frame = ivec2(0, 0);
    bool is_playing = false;

    void play(Animation animation);
    void update();
    void stop();
};

enum ButtonIcon {
    BUTTON_ICON_MOVE,
    BUTTON_ICON_STOP,
    BUTTON_ICON_ATTACK,
    BUTTON_ICON_BUILD,
    BUTTON_ICON_BUILD_HOUSE,
    BUTTON_ICON_CANCEL,
    BUTTON_ICON_COUNT
};

struct ui_button_t {
    bool enabled;
    ButtonIcon icon;
    rect_t rect;
};

struct unit_t {
    bool is_selected;

    bool is_moving;
    int direction;
    vec2 position;
    vec2 target_position;
    ivec2 cell;
    std::vector<ivec2> path;
    timer_t path_timer;
    static const uint32_t PATH_PAUSE_DURATION = 60;

    animation_t animation;

    rect_t get_rect() {
        ivec2 size = ivec2(16, 16);
        return rect_t(ivec2(position.x.integer_part(), position.y.integer_part()) - (size / 2), size);
    }
};

enum BuildingType {
    BUILDING_HOUSE
};

struct building_data_t {
    int cell_width;
    int cell_height;
    uint32_t cost;
    uint32_t max_health;
};

const std::unordered_map<uint32_t, building_data_t> building_data = {
    { BUILDING_HOUSE, (building_data_t) {
        .cell_width = 2, .cell_height = 2,
        .cost = 100,
        .max_health = 100
    }}
};

struct building_t {
    BuildingType type;
    ivec2 cell;
    uint32_t health;
    bool is_finished;

    rect_t get_rect() {
        auto it = building_data.find(type);
        return rect_t(cell * TILE_SIZE, ivec2(it->second.cell_width, it->second.cell_height) * TILE_SIZE);
    }
};


enum InputType {
    INPUT_NONE,
    INPUT_MOVE
};

struct input_move_t {
    ivec2 target_cell;
    uint8_t unit_count;
    uint8_t unit_ids[MAX_UNITS];
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
    };
};

enum MatchMode {
    MATCH_MODE_NOT_STARTED,
    MATCH_MODE_RUNNING
};

enum UiMode {
    UI_MODE_NONE,
    UI_MODE_SELECTING,
    UI_MODE_MINIMAP_DRAG,
    UI_MODE_MINER,
    UI_MODE_BUILD,
    UI_MODE_BUILDING_PLACE
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
    swiss_array<unit_t, MAX_UNITS> units[MAX_PLAYERS];
    swiss_array<building_t, MAX_BUILDINGS> buildings[MAX_PLAYERS];
    uint32_t player_gold[MAX_PLAYERS];

    void init();
    void update();
    void handle_input(uint8_t player_id, const input_t& input);
    void input_flush();
    void input_deserialize(uint8_t* in_buffer, size_t in_buffer_length);

    void ui_refresh_buttons();
    void ui_handle_button_pressed(ButtonIcon icon);
    void camera_clamp();
    void camera_move_to_cell(ivec2 cell);

    bool cell_is_blocked(ivec2 cell) const;
    bool cell_is_blocked(ivec2 cell, ivec2 cell_size) const;
    void cell_set_value(ivec2 cell, int value);
    void cell_set_value(ivec2 cell, ivec2 cell_size, int value);

    void unit_spawn(uint8_t player_id, ivec2 cell);
    void unit_try_move(unit_t& unit);
    void unit_update(unit_t& unit);
    std::vector<ivec2> pathfind(ivec2 from, ivec2 to);

    void building_create(uint8_t player_id, BuildingType type, ivec2 cell);
};
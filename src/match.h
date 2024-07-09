#pragma once

#include "defines.h"
#include "util.h"
#include <vector>
#include <unordered_map>

const uint32_t MAX_UNITS = 200;
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
    ivec2 frame;
    bool is_playing = false;

    void play(Animation animation);
    void update();
    void stop();
};

enum ButtonIcon {
    BUTTON_ICON_MOVE,
    BUTTON_ICON_STOP,
    BUTTON_ICON_ATTACK
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

struct match_t {
    MatchMode mode;

    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    ivec2 camera_offset;

    bool is_minimap_dragging;
    bool is_selecting;
    ivec2 select_origin;
    rect_t select_rect;

    ivec2 ui_move_position;
    animation_t ui_move_animation;

    ui_button_t ui_buttons[6];
    int ui_button_hovered;

    std::vector<int> map_tiles;
    std::vector<int> map_cells;
    int map_width;
    int map_height;

    std::vector<unit_t> units[MAX_PLAYERS];

    void init();
    void update();
    void handle_input(uint8_t player_id, const input_t& input);
    void input_flush();
    void input_deserialize(uint8_t* in_buffer, size_t in_buffer_length);

    void camera_clamp();
    void camera_move_to_cell(ivec2 cell);

    bool cell_is_blocked(ivec2 cell);
    void cell_set_value(ivec2 cell, int value);

    void unit_spawn(uint8_t player_id, ivec2 cell);
    void unit_try_move(unit_t& unit);
    void unit_update(unit_t& unit);

    std::vector<ivec2> pathfind(ivec2 from, ivec2 to);
};
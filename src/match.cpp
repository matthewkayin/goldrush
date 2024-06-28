#include "match.h"

#include "defines.h"
#include "engine.h"
#include "logger.h"
#include "util.h"
#include "network.h"
#include <vector>
#include <cstdlib>
#include <algorithm>

static const uint32_t MAX_UNITS = 200;

static const int CAMERA_DRAG_MARGIN = 16;
static const int CAMERA_DRAG_SPEED = 8;

struct unit_t {
    bool exists;
    bool is_selected;
    vec2 position;

    unit_t() {
        exists = false;
        is_selected = false;
    }

    rect get_rect() {
        return rect(ivec2(position.x.integer_part(), position.y.integer_part()), engine_get_sprite_frame_size(SPRITE_UNIT_MINER));
    }
};

struct match_state_t {
    ivec2 camera_offset;

    bool is_selecting;
    ivec2 select_origin;
    rect select_rect;

    std::vector<int> map_tiles;
    int map_width;
    int map_height;

    unit_t units[MAX_PLAYERS][MAX_UNITS];
};
static match_state_t state;

void match_init() {
    state.camera_offset = ivec2(0, 0);
    state.is_selecting = false;

    // Init map
    state.map_width = 40;
    state.map_height = 24;
    state.map_tiles = std::vector<int>(state.map_width * state.map_height);
    for (int i = 0; i < state.map_width * state.map_height; i += 3) {
        state.map_tiles[i] = 1;
    }
    for (int y = 0; y < state.map_height; y++) {
        for (int x = 0; x < state.map_width; x++) {
            if (y == 0 || y == state.map_height - 1 || x == 0 || x == state.map_width - 1) {
                state.map_tiles[x + (y * state.map_width)] = 2;
            }
        }
    }

    // Init units
    state.units[0][0].exists = true;
    state.units[0][0].position = vec2(fp8::integer(16), fp8::integer(32));
    state.units[0][1].exists = true;
    state.units[0][1].position = vec2(fp8::integer(32), fp8::integer(48));
    state.units[0][2].exists = true;
    state.units[0][2].position = vec2(fp8::integer(64), fp8::integer(16));
}

void match_update() {
    ivec2 mouse_pos = input_get_mouse_position();
    uint8_t current_player_id = network_get_player_id();

    // CAMERA DRAG
    ivec2 camera_drag_direction = ivec2(0, 0);
    if (mouse_pos.x < CAMERA_DRAG_MARGIN) {
        camera_drag_direction.x = -1;
    } else if (mouse_pos.x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
        camera_drag_direction.x = 1;
    }
    if (mouse_pos.y < CAMERA_DRAG_MARGIN) {
        camera_drag_direction.y = -1;
    } else if (mouse_pos.y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
        camera_drag_direction.y = 1;
    }
    state.camera_offset += camera_drag_direction * CAMERA_DRAG_SPEED;
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, (state.map_width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, (state.map_height * TILE_SIZE) - SCREEN_HEIGHT);

    // SELECT RECT
    ivec2 mouse_world_pos = mouse_pos + state.camera_offset;
    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        // On begin selecting
        state.is_selecting = true;
        state.select_origin = mouse_world_pos;
    }
    if (state.is_selecting) {
        // Update select rect
        state.select_rect.position = ivec2(std::min(state.select_origin.x, mouse_world_pos.x), std::min(state.select_origin.y, mouse_world_pos.y));
        state.select_rect.size = ivec2(std::max(1, std::abs(state.select_origin.x - mouse_world_pos.x)), std::max(1, std::abs(state.select_origin.y - mouse_world_pos.y)));

        // Update unit selection
        for (uint32_t unit_id = 0; unit_id < MAX_UNITS; unit_id++) {
            unit_t& unit = state.units[current_player_id][unit_id];
            if (!unit.exists) {
                continue;
            }
            unit.is_selected = unit.get_rect().intersects(state.select_rect);
        }
    }
    if (input_is_mouse_button_just_released(MOUSE_BUTTON_LEFT)) {
        // On finished selecting
        state.is_selecting = false;
    } 
}

void match_render() {
    uint8_t current_player_id = network_get_player_id();

    render_map(state.camera_offset, &state.map_tiles[0], state.map_width, state.map_height);

    // Select rings
    for (uint32_t unit_id = 0; unit_id < MAX_UNITS; unit_id++) {
        unit_t unit = state.units[current_player_id][unit_id];
        if (!unit.exists || !unit.is_selected) {
            continue;
        }

        render_sprite(state.camera_offset, SPRITE_SELECT_RING, ivec2(0, 0), unit.position + vec2(fp8::integer(0), fp8::integer(17)));
    }

    // Units
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (uint32_t unit_id = 0; unit_id < MAX_UNITS; unit_id++) {
            unit_t unit = state.units[player_id][unit_id];
            if (!unit.exists) {
                continue;
            }

            render_sprite(state.camera_offset, SPRITE_UNIT_MINER, ivec2(0, 0), unit.position);
        }
    }

    // Select rect
    if (state.is_selecting) {
        render_rect(rect(state.select_rect.position - state.camera_offset, state.select_rect.size), COLOR_WHITE);
    }
}
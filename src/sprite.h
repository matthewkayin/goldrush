#pragma once

#include <unordered_map>

enum UiButton {
    UI_BUTTON_NONE,
    UI_BUTTON_MOVE,
    UI_BUTTON_STOP,
    UI_BUTTON_ATTACK,
    UI_BUTTON_BUILD,
    UI_BUTTON_CANCEL,
    UI_BUTTON_BUILD_HOUSE,
    UI_BUTTON_BUILD_CAMP,
    UI_BUTTON_COUNT
};

enum Sprite {
    SPRITE_TILES,
    SPRITE_TILE_GOLD,
    SPRITE_UI_FRAME,
    SPRITE_UI_FRAME_BOTTOM,
    SPRITE_UI_FRAME_BUTTONS,
    SPRITE_UI_MINIMAP,
    SPRITE_UI_BUTTON,
    SPRITE_UI_BUTTON_ICON,
    SPRITE_UI_GOLD,
    SPRITE_UI_MOVE,
    SPRITE_SELECT_RING,
    SPRITE_SELECT_RING_HOUSE,
    SPRITE_SELECT_RING_GOLD,
    SPRITE_MINER_BUILDING,
    SPRITE_UNIT_MINER,
    SPRITE_BUILDING_HOUSE,
    SPRITE_BUILDING_CAMP,
    SPRITE_COUNT
};

struct sprite_params_t {
    const char* path;
    int h_frames;
    int v_frames;
};

const std::unordered_map<uint32_t, sprite_params_t> sprite_params = {
    { SPRITE_TILES, (sprite_params_t) {
        .path = "sprite/tiles.png",
        .h_frames = -1,
        .v_frames = -1
    }},
    { SPRITE_TILE_GOLD, (sprite_params_t) {
        .path = "sprite/tile_gold.png",
        .h_frames = -1,
        .v_frames = -1
    }},
    { SPRITE_UI_FRAME, (sprite_params_t) {
        .path = "sprite/frame.png",
        .h_frames = 3,
        .v_frames = 3
    }},
    { SPRITE_UI_FRAME_BOTTOM, (sprite_params_t) {
        .path = "sprite/ui_frame_bottom.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_UI_FRAME_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_frame_buttons.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_UI_MINIMAP, (sprite_params_t) {
        .path = "sprite/ui_minimap.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_UI_BUTTON, (sprite_params_t) {
        .path = "sprite/ui_button.png",
        .h_frames = 2,
        .v_frames = 1
    }},
    { SPRITE_UI_BUTTON_ICON, (sprite_params_t) {
        .path = "sprite/ui_button_icon.png",
        .h_frames = UI_BUTTON_COUNT - 1,
        .v_frames = 2
    }},
    { SPRITE_UI_GOLD, (sprite_params_t) {
        .path = "sprite/ui_gold.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_UI_MOVE, (sprite_params_t) {
        .path = "sprite/ui_move.png",
        .h_frames = 5,
        .v_frames = 1
    }},
    { SPRITE_SELECT_RING, (sprite_params_t) {
        .path = "sprite/select_ring.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_SELECT_RING_HOUSE, (sprite_params_t) {
        .path = "sprite/select_ring_house.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_SELECT_RING_GOLD, (sprite_params_t) {
        .path = "sprite/select_ring_gold.png",
        .h_frames = 2,
        .v_frames = 1
    }},
    { SPRITE_MINER_BUILDING, (sprite_params_t) {
        .path = "sprite/unit_miner_building.png",
        .h_frames = 2,
        .v_frames = 1
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "sprite/unit_miner.png",
        .h_frames = 8,
        .v_frames = 8
    }},
    { SPRITE_BUILDING_HOUSE, (sprite_params_t) {
        .path = "sprite/building_house.png",
        .h_frames = 4,
        .v_frames = 1
    }},
    { SPRITE_BUILDING_CAMP, (sprite_params_t) {
        .path = "sprite/building_camp.png",
        .h_frames = 4,
        .v_frames = 1
    }}  
};

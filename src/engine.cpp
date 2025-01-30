#include "engine.h"

#include "logger.h"
#include "asserts.h"
#include "animation.h"
#include "lcg.h"
#include <unordered_map>

const player_color_t PLAYER_COLORS[MAX_PLAYERS] = {
    (player_color_t) {
        .name = "Blue",
        .skin_color = (SDL_Color) { .r = 125, .g = 181, .b = 164, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 }
    },
    (player_color_t) {
        .name = "Red",
        .skin_color = (SDL_Color) { .r = 219, .g = 151, .b = 114, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 }
    },
    (player_color_t) {
        .name = "Green",
        .skin_color = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 77, .g = 135, .b = 115, .a = 255 },
    },
    (player_color_t) {
        .name = "Purple",
        .skin_color = (SDL_Color) { .r = 184, .g = 169, .b = 204, .a = 255 },
        .clothes_color = (SDL_Color) { .r = 144, .g = 119, .b = 153, .a = 255 },
    }
};

struct font_params_t {
    const char* path;
    int size;
    SDL_Color color;
    bool ignore_y_bearing;
};

static const std::unordered_map<uint32_t, font_params_t> font_params = {
    { FONT_HACK_WHITE, (font_params_t) {
        .path = "font/hack.ttf",
        .size = 10,
        .color = COLOR_WHITE,
        .ignore_y_bearing = true
    }},
    { FONT_WESTERN8_OFFBLACK, (font_params_t) {
        .path = "font/western.ttf",
        .size = 8,
        .color = COLOR_OFFBLACK,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN8_WHITE, (font_params_t) {
        .path = "font/western.ttf",
        .size = 8,
        .color = COLOR_WHITE,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN8_RED, (font_params_t) {
        .path = "font/western.ttf",
        .size = 8,
        .color = COLOR_RED,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN8_GOLD, (font_params_t) {
        .path = "font/western.ttf",
        .size = 8,
        .color = COLOR_GOLD,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN16_OFFBLACK, (font_params_t) {
        .path = "font/western.ttf",
        .size = 16,
        .color = COLOR_OFFBLACK,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN16_WHITE, (font_params_t) {
        .path = "font/western.ttf",
        .size = 16,
        .color = COLOR_WHITE,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN16_GOLD, (font_params_t) {
        .path = "font/western.ttf",
        .size = 16,
        .color = COLOR_GOLD,
        .ignore_y_bearing = false
    }},
    { FONT_WESTERN32_OFFBLACK, (font_params_t) {
        .path = "font/western.ttf",
        .size = 32,
        .color = COLOR_OFFBLACK,
        .ignore_y_bearing = false
    }},
    { FONT_M3X6_OFFBLACK, (font_params_t) {
        .path = "font/m3x6.ttf",
        .size = 16,
        .color = COLOR_OFFBLACK,
        .ignore_y_bearing = false
    }},
    { FONT_M3X6_DARKBLACK, (font_params_t) {
        .path = "font/m3x6.ttf",
        .size = 16,
        .color = COLOR_DARKBLACK,
        .ignore_y_bearing = false
    }},
};

enum SpriteImportStrategy {
    SPRITE_IMPORT_DEFAULT,
    SPRITE_IMPORT_RECOLOR,
    SPRITE_IMPORT_RECOLOR_AND_LOW_TRANSPARENCY,
    SPRITE_IMPORT_TILE_SIZE,
    SPRITE_IMPORT_TILESET,
    SPRITE_IMPORT_FOG_OF_WAR
};

struct sprite_params_t {
    const char* path;
    int hframes;
    int vframes;
    SpriteImportStrategy strategy;
};

static const std::unordered_map<uint32_t, sprite_params_t> SPRITE_PARAMS = {
    { SPRITE_TILESET_ARIZONA, (sprite_params_t) {
        .path = "sprite/tileset_arizona.png",
        .hframes = -1,
        .vframes = -1,
        .strategy = SPRITE_IMPORT_TILESET
    }},
    { SPRITE_TILE_DECORATION, (sprite_params_t) {
        .path = "sprite/tile_decorations.png",
        .hframes = 16,
        .vframes = 16,
        .strategy = SPRITE_IMPORT_TILE_SIZE
    }},
    { SPRITE_TILE_GOLD, (sprite_params_t) {
        .path = "sprite/tile_gold.png",
        .hframes = 3,
        .vframes = 2,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_FRAME, (sprite_params_t) {
        .path = "sprite/ui_frame.png",
        .hframes = 3,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_FRAME_BOTTOM, (sprite_params_t) {
        .path = "sprite/ui_frame_bottom.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_FRAME_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_frame_buttons.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_MINIMAP, (sprite_params_t) {
        .path = "sprite/ui_minimap.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_BUTTON, (sprite_params_t) {
        .path = "sprite/ui_button.png",
        .hframes = 3,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_BUTTON_ICON, (sprite_params_t) {
        .path = "sprite/ui_button_icon.png",
        .hframes = UI_BUTTON_COUNT - 1,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_TOOLTIP_FRAME, (sprite_params_t) {
        .path = "sprite/ui_tooltip_frame.png",
        .hframes = 3,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_TEXT_FRAME, (sprite_params_t) {
        .path = "sprite/ui_text_frame.png",
        .hframes = 3,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_TEXT_FRAME_2, (sprite_params_t) {
        .path = "sprite/ui_text_frame2.png",
        .hframes = 3,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_TABS, (sprite_params_t) {
        .path = "sprite/ui_frame_tabs.png",
        .hframes = 2,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_GOLD, (sprite_params_t) {
        .path = "sprite/ui_gold.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_HOUSE, (sprite_params_t) {
        .path = "sprite/ui_house.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_MOVE, (sprite_params_t) {
        .path = "sprite/ui_move.png",
        .hframes = 5,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_PARCHMENT_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_parchment_buttons.png",
        .hframes = 3,
        .vframes = 2,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_OPTIONS_DROPDOWN, (sprite_params_t) {
        .path = "sprite/ui_options_dropdown.png",
        .hframes = 1,
        .vframes = 5,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_CONTROL_GROUP_FRAME, (sprite_params_t) {
        .path = "sprite/ui_control_group_frame.png",
        .hframes = 3,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_MENU_BUTTON, (sprite_params_t) {
        .path = "sprite/ui_menu_button.png",
        .hframes = 2,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_UI_MENU_PARCHMENT_BUTTONS, (sprite_params_t) {
        .path = "sprite/ui_menu_parchment_buttons.png",
        .hframes = 1,
        .vframes = 2,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_UNIT_1, (sprite_params_t) {
        .path = "sprite/select_ring.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_UNIT_1_ENEMY, (sprite_params_t) {
        .path = "sprite/select_ring_attack.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_UNIT_2, (sprite_params_t) {
        .path = "sprite/select_ring_wagon.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_UNIT_2_ENEMY, (sprite_params_t) {
        .path = "sprite/select_ring_wagon_attack.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_BUILDING_2, (sprite_params_t) {
        .path = "sprite/select_ring_building2x2.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_BUILDING_2_ENEMY, (sprite_params_t) {
        .path = "sprite/select_ring_building2x2_attack.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_BUILDING_3, (sprite_params_t) {
        .path = "sprite/select_ring_building3x3.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_BUILDING_3_ENEMY, (sprite_params_t) {
        .path = "sprite/select_ring_building3x3_attack.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_BUILDING_4, (sprite_params_t) {
        .path = "sprite/select_ring_building4x4.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_BUILDING_4_ENEMY, (sprite_params_t) {
        .path = "sprite/select_ring_building4x4_attack.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_GOLD, (sprite_params_t) {
        .path = "sprite/select_ring_gold.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_MINE, (sprite_params_t) {
        .path = "sprite/select_ring_mine.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_SELECT_RING_MINE_ENEMY, (sprite_params_t) {
        .path = "sprite/select_ring_mine_attack.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_MINER_BUILDING, (sprite_params_t) {
        .path = "sprite/unit_miner_building.png",
        .hframes = 2,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "sprite/unit_miner.png",
        .hframes = 15,
        .vframes = 6,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_COWBOY, (sprite_params_t) {
        .path = "sprite/unit_cowboy.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_WAGON, (sprite_params_t) {
        .path = "sprite/unit_wagon.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_WAR_WAGON, (sprite_params_t) {
        .path = "sprite/unit_war_wagon.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_BANDIT, (sprite_params_t) {
        .path = "sprite/unit_bandit.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_JOCKEY, (sprite_params_t) {
        .path = "sprite/unit_jockey.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_SAPPER, (sprite_params_t) {
        .path = "sprite/unit_sapper.png",
        .hframes = 15,
        .vframes = 6,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_TINKER, (sprite_params_t) {
        .path = "sprite/unit_tinker.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_SOLDIER, (sprite_params_t) {
        .path = "sprite/unit_soldier.png",
        .hframes = 20,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_CANNON, (sprite_params_t) {
        .path = "sprite/unit_cannon.png",
        .hframes = 21,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_UNIT_SPY, (sprite_params_t) {
        .path = "sprite/unit_spy.png",
        .hframes = 15,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_RECOLOR_AND_LOW_TRANSPARENCY
    }},
    { SPRITE_BUILDING_HALL, (sprite_params_t) {
        .path = "sprite/building_hall.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_HOUSE, (sprite_params_t) {
        .path = "sprite/building_house.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_CAMP, (sprite_params_t) {
        .path = "sprite/building_camp.png",
        .hframes = 5,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_SALOON, (sprite_params_t) {
        .path = "sprite/building_saloon.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_BUNKER, (sprite_params_t) {
        .path = "sprite/building_bunker.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_COOP, (sprite_params_t) {
        .path = "sprite/building_coop.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_SMITH, (sprite_params_t) {
        .path = "sprite/building_smith.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_SMITH_ANIMATION, (sprite_params_t) {
        .path = "sprite/building_smith_animation.png",
        .hframes = 9,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_BUILDING_BARRACKS, (sprite_params_t) {
        .path = "sprite/building_barracks.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_SHERIFFS, (sprite_params_t) {
        .path = "sprite/building_sheriffs.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_MINE, (sprite_params_t) {
        .path = "sprite/building_mine.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_BUILDING_DESTROYED_2, (sprite_params_t) {
        .path = "sprite/building_destroyed2x2.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_BUILDING_DESTROYED_3, (sprite_params_t) {
        .path = "sprite/building_destroyed3x3.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_BUILDING_DESTROYED_4, (sprite_params_t) {
        .path = "sprite/building_destroyed4x4.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_BUILDING_DESTROYED_BUNKER, (sprite_params_t) {
        .path = "sprite/building_destroyed_bunker.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_BUILDING_DESTROYED_MINE, (sprite_params_t) {
        .path = "sprite/building_destroyed_mine.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_FOG_OF_WAR, (sprite_params_t) {
        .path = "sprite/fog_of_war.png",
        .hframes = 47 * 2,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_FOG_OF_WAR
    }},
    { SPRITE_RALLY_FLAG, (sprite_params_t) {
        .path = "sprite/rally_flag.png",
        .hframes = 6,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_RECOLOR
    }},
    { SPRITE_MENU_CLOUDS, (sprite_params_t) {
        .path = "sprite/menu_clouds.png",
        .hframes = 3,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_MENU_REFRESH, (sprite_params_t) {
        .path = "sprite/menu_refresh.png",
        .hframes = 2,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_MENU_BUTTON, (sprite_params_t) {
        .path = "sprite/menu_button.png",
        .hframes = 1,
        .vframes = 2,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_MENU_USERNAME, (sprite_params_t) {
        .path = "sprite/menu_username.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_PARTICLE_SPARKS, (sprite_params_t) {
        .path = "sprite/particle_sparks.png",
        .hframes = 4,
        .vframes = 3,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_PARTICLE_BUNKER_COWBOY, (sprite_params_t) {
        .path = "sprite/particle_bunker_cowboy.png",
        .hframes = 2,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_PARTICLE_EXPLOSION, (sprite_params_t) {
        .path = "sprite/particle_explosion.png",
        .hframes = 6,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_PARTICLE_CANNON_EXPLOSION, (sprite_params_t) {
        .path = "sprite/particle_cannon_explosion.png",
        .hframes = 4,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_PARTICLE_SMOKE, (sprite_params_t) {
        .path = "sprite/particle_smoke.png",
        .hframes = 9,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }},
    { SPRITE_PROJECTILE_SMOKE, (sprite_params_t) {
        .path = "sprite/projectile_smoke.png",
        .hframes = 1,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
    }}
};

enum TileType {
    TILE_TYPE_SINGLE,
    TILE_TYPE_AUTO
};

struct tile_data_t {
    TileType type;
    int source_x;
    int source_y;
};

static const std::unordered_map<uint32_t, tile_data_t> TILE_DATA = {
    { TILE_NULL, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 16
    }},
    { TILE_SAND, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 0,
        .source_y = 0
    }},
    { TILE_SAND2, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 16,
        .source_y = 0
    }},
    { TILE_SAND3, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 32,
        .source_y = 0
    }},
    { TILE_WATER, (tile_data_t) {
        .type = TILE_TYPE_AUTO,
        .source_x = 0,
        .source_y = 16
    }},
    { TILE_WALL_NW_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 0
    }},
    { TILE_WALL_NE_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 0
    }},
    { TILE_WALL_SW_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 32
    }},
    { TILE_WALL_SE_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 32
    }},
    { TILE_WALL_NORTH_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 0
    }},
    { TILE_WALL_WEST_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 16
    }},
    { TILE_WALL_EAST_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 16
    }},
    { TILE_WALL_SOUTH_EDGE, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 32
    }},
    { TILE_WALL_SW_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 48,
        .source_y = 48
    }},
    { TILE_WALL_SOUTH_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 64,
        .source_y = 48
    }},
    { TILE_WALL_SE_FRONT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 80,
        .source_y = 48
    }},
    { TILE_WALL_NW_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 0
    }},
    { TILE_WALL_NE_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 0
    }},
    { TILE_WALL_SW_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 16
    }},
    { TILE_WALL_SE_INNER_CORNER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 16
    }},
    { TILE_WALL_SOUTH_STAIR_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 32
    }},
    { TILE_WALL_SOUTH_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 32
    }},
    { TILE_WALL_SOUTH_STAIR_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 128,
        .source_y = 32
    }},
    { TILE_WALL_SOUTH_STAIR_FRONT_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 96,
        .source_y = 48
    }},
    { TILE_WALL_SOUTH_STAIR_FRONT_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 112,
        .source_y = 48
    }},
    { TILE_WALL_SOUTH_STAIR_FRONT_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 128,
        .source_y = 48
    }},
    { TILE_WALL_NORTH_STAIR_LEFT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 48
    }},
    { TILE_WALL_NORTH_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 48
    }},
    { TILE_WALL_NORTH_STAIR_RIGHT, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 176,
        .source_y = 48
    }},
    { TILE_WALL_WEST_STAIR_TOP, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 0
    }},
    { TILE_WALL_WEST_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 16
    }},
    { TILE_WALL_WEST_STAIR_BOTTOM, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 144,
        .source_y = 32
    }},
    { TILE_WALL_EAST_STAIR_TOP, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 0
    }},
    { TILE_WALL_EAST_STAIR_CENTER, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 16
    }},
    { TILE_WALL_EAST_STAIR_BOTTOM, (tile_data_t) {
        .type = TILE_TYPE_SINGLE,
        .source_x = 160,
        .source_y = 32
    }},
};

struct cursor_params_t {
    const char* path;
    int hot_x;
    int hot_y;
};

static const std::unordered_map<Cursor, cursor_params_t> CURSOR_PARAMS = {
    { CURSOR_DEFAULT, (cursor_params_t) {
        .path = "sprite/ui_cursor.png",
        .hot_x = 0, .hot_y = 0
    }},
    { CURSOR_TARGET, (cursor_params_t) {
        .path = "sprite/ui_cursor_target.png",
        .hot_x = 9, .hot_y = 9
    }},
};

struct sound_params_t {
    const char* path;
    int variants;
};

static const std::unordered_map<Sound, sound_params_t> SOUND_PARAMS = {
    { SOUND_DEATH, (sound_params_t) {
        .path = "sfx/death",
        .variants = 9
    }},
    { SOUND_UI_SELECT, (sound_params_t) {
        .path = "sfx/ui_select",
        .variants = 1
    }},
    { SOUND_MUSKET, (sound_params_t) {
        .path = "sfx/musket",
        .variants = 9
    }},
    { SOUND_GUN, (sound_params_t) {
        .path = "sfx/gun",
        .variants = 5
    }},
    { SOUND_EXPLOSION, (sound_params_t) {
        .path = "sfx/explosion",
        .variants = 4
    }},
    { SOUND_PICKAXE, (sound_params_t) {
        .path = "sfx/pickaxe",
        .variants = 4
    }},
    { SOUND_HAMMER, (sound_params_t) {
        .path = "sfx/hammer",
        .variants = 4
    }},
    { SOUND_BUILDING_PLACE, (sound_params_t) {
        .path = "sfx/building_place",
        .variants = 1
    }},
    { SOUND_SWORD, (sound_params_t) {
        .path = "sfx/sword",
        .variants = 3
    }},
    { SOUND_PUNCH, (sound_params_t) {
        .path = "sfx/punch",
        .variants = 3
    }},
    { SOUND_DEATH_CHICKEN, (sound_params_t) {
        .path = "sfx/death_chicken",
        .variants = 4
    }},
};

engine_t engine;

bool engine_init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        log_error("SDL failed to initialize: %s", SDL_GetError());
        return false;
    }

    // Init TTF
    if (TTF_Init() == -1) {
        log_error("SDL_ttf failed to initialize: %s", TTF_GetError());
        return false;
    }

    // Init IMG
    int img_flags = IMG_INIT_PNG;
    if (!(IMG_Init(img_flags) & img_flags)) {
        log_error("SDL_image failed to initialize: %s", IMG_GetError());
        return false;
    }

    // Init Mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        log_error("Error initializing SDL_mixer: %s", Mix_GetError());
        return false;
    }

    engine.window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
    if (engine.window == NULL) {
        log_error("Error creating window: %s", SDL_GetError());
        return false;
    }

    animation_init();

    if (!engine_init_renderer()) {
        return false;
    }

    // Load sound effects
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        const sound_params_t& params = SOUND_PARAMS.at((Sound)sound);
        engine.sound_index.push_back(engine.sounds.size());

        for (int variant = 0; variant < params.variants; variant++) {
            char sound_path[256];
            if (params.variants == 1) {
                sprintf(sound_path, "%s%s.wav", GOLD_RESOURCE_PATH, params.path);
            } else {
                sprintf(sound_path, "%s%s%i.wav", GOLD_RESOURCE_PATH, params.path, (variant + 1));
            }
            Mix_Chunk* sound_variant = Mix_LoadWAV(sound_path);
            if (sound_variant == NULL) {
                log_error("Unable to load sound at path %s", variant + 1, sound_path);
                return false;
            }

            engine.sounds.push_back(sound_variant);
        }
    }

    engine.keystate = SDL_GetKeyboardState(NULL);
    engine.render_debug_info = false;

    log_info("%s initialized.", APP_NAME);
    return true;
}

bool engine_init_renderer() {
    uint32_t renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    engine.renderer = SDL_CreateRenderer(engine.window, -1, renderer_flags);
    if (engine.renderer == NULL) {
        log_error("Error creating renderer: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(engine.renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_SetRenderTarget(engine.renderer, NULL);

    // Load fonts
    log_trace("Loading fonts...");
    engine.fonts.reserve(FONT_COUNT);
    for (uint32_t i = 0; i < FONT_COUNT; i++) {
        // Get the font params
        auto font_params_it = font_params.find(i);
        if (font_params_it == font_params.end()) {
            log_error("Font params not defined for font id %u", i);
            return false;
        }

        // Load the font
        char font_path[128];
        sprintf(font_path, "%s%s", GOLD_RESOURCE_PATH, font_params_it->second.path);
        TTF_Font* ttf_font = TTF_OpenFont(font_path, font_params_it->second.size);
        if (ttf_font == NULL) {
            log_error("Error loading font %s: %s", font_params.at(i).path, TTF_GetError());
            return false;
        }

        font_t font;
        for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
            char text[2];
            text[0] = (char)(32 + glyph_index);
            text[1] = '\0';

            SDL_Surface* glyph_surface = TTF_RenderText_Solid(ttf_font, text, font_params_it->second.color);
            if (glyph_surface == NULL) {
                log_error("Error creating glyph surface %s", TTF_GetError());
                return false;
            }

            font.glyphs[glyph_index].texture = SDL_CreateTextureFromSurface(engine.renderer, glyph_surface);
            if (font.glyphs[glyph_index].texture == NULL) {
                log_error("Unable to create glyph texture from surface %s", SDL_GetError());
                return false;
            }

            font.glyphs[glyph_index].width = glyph_surface->w;
            font.glyphs[glyph_index].height = glyph_surface->h;
            TTF_GlyphMetrics(ttf_font, (char)(32 + glyph_index), &font.glyphs[glyph_index].bearing_x, NULL, NULL, &font.glyphs[glyph_index].bearing_y, &font.glyphs[glyph_index].advance);
            if (font_params_it->second.ignore_y_bearing) {
                font.glyphs[glyph_index].bearing_y = 0;
            }
        }
        engine.fonts.push_back(font);

        TTF_CloseFont(ttf_font);
    }

    // Load sprites
    log_trace("Loading sprites...");
    engine.sprites.reserve(SPRITE_COUNT);
    for (uint32_t i = 0; i < SPRITE_COUNT; i++) {
        // Get the sprite params
        auto sprite_params_it = SPRITE_PARAMS.find(i);
        if (sprite_params_it == SPRITE_PARAMS.end()) {
            log_error("Sprite params not defined to sprite %u", i);
            return false;
        }

        // Load the sprite
        sprite_t sprite;
        char sprite_path[128];
        sprintf(sprite_path, "%s%s", GOLD_RESOURCE_PATH, sprite_params_it->second.path);
        SDL_Surface* sprite_surface = IMG_Load(sprite_path);
        if (sprite_surface == NULL) {
            log_error("Error loading sprite %s: %s", sprite_path, IMG_GetError());
            return false;
        }

        switch (sprite_params_it->second.strategy) {
            case SPRITE_IMPORT_DEFAULT: {
                sprite.texture = SDL_CreateTextureFromSurface(engine.renderer, sprite_surface);
                if (sprite.texture == NULL) {
                    log_error("Error creating texture for sprite %s: %s", sprite_path, SDL_GetError());
                    return false;
                }
                sprite.hframes = sprite_params_it->second.hframes;
                sprite.vframes = sprite_params_it->second.vframes;
                sprite.frame_size = xy(sprite_surface->w / sprite.hframes, sprite_surface->h / sprite.vframes);
                break;
            }
            case SPRITE_IMPORT_RECOLOR: 
            case SPRITE_IMPORT_RECOLOR_AND_LOW_TRANSPARENCY: {
                // Re-color texture creation
                uint32_t* sprite_pixels = (uint32_t*)sprite_surface->pixels;
                uint32_t skin_reference_pixel = SDL_MapRGBA(sprite_surface->format, RECOLOR_SKIN_REF.r, RECOLOR_SKIN_REF.g, RECOLOR_SKIN_REF.b, RECOLOR_SKIN_REF.a);
                uint32_t clothes_reference_pixel = SDL_MapRGBA(sprite_surface->format, RECOLOR_CLOTHES_REF.r, RECOLOR_CLOTHES_REF.g, RECOLOR_CLOTHES_REF.b, RECOLOR_CLOTHES_REF.a);

                for (uint32_t player_color = 0; player_color < MAX_PLAYERS; player_color++) {
                    SDL_Surface* recolored_surface = SDL_CreateRGBSurfaceWithFormat(0, sprite_surface->w, sprite_surface->h, 32, sprite_surface->format->format);
                    uint32_t* recolor_pixels = (uint32_t*)recolored_surface->pixels;
                    SDL_LockSurface(recolored_surface);

                    const SDL_Color& color_player_skin = PLAYER_COLORS[player_color].skin_color;
                    const SDL_Color& color_player_clothes = PLAYER_COLORS[player_color].clothes_color;
                    uint32_t replace_skin_pixel = SDL_MapRGBA(recolored_surface->format, color_player_skin.r, color_player_skin.g, color_player_skin.b, color_player_skin.a);
                    uint32_t replace_clothes_pixel = SDL_MapRGBA(recolored_surface->format, color_player_clothes.r, color_player_clothes.g, color_player_clothes.b, color_player_clothes.a);

                    for (uint32_t pixel_index = 0; pixel_index < recolored_surface->w * recolored_surface->h; pixel_index++) {
                        if (sprite_pixels[pixel_index] == skin_reference_pixel) {
                            recolor_pixels[pixel_index] = replace_skin_pixel;
                        } else if (sprite_pixels[pixel_index] == clothes_reference_pixel) {
                            recolor_pixels[pixel_index] = replace_clothes_pixel;
                        } else {
                            recolor_pixels[pixel_index] = sprite_pixels[pixel_index];
                        }
                        if (sprite_params_it->second.strategy == SPRITE_IMPORT_RECOLOR_AND_LOW_TRANSPARENCY) {
                            uint8_t r, g, b, a;
                            SDL_GetRGBA(recolor_pixels[pixel_index], recolored_surface->format, &r, &g, &b, &a);
                            if (a != 0) {
                                a = 200;
                            }
                            recolor_pixels[pixel_index] = SDL_MapRGBA(recolored_surface->format, r, g, b, a);
                        }
                    }

                    sprite.colored_texture[player_color] = SDL_CreateTextureFromSurface(engine.renderer, recolored_surface);
                    if (sprite.texture == NULL) {
                        log_error("Error creating colored texture for sprite %s: %s", sprite_params_it->second.path, SDL_GetError());
                        return false;
                    }

                    SDL_UnlockSurface(recolored_surface);
                    SDL_FreeSurface(recolored_surface);
                }

                sprite.hframes = sprite_params_it->second.hframes;
                sprite.vframes = sprite_params_it->second.vframes;
                sprite.frame_size = xy(sprite_surface->w / sprite.hframes, sprite_surface->h / sprite.vframes);
                break;
            }
            case SPRITE_IMPORT_TILE_SIZE: {
                sprite.texture = SDL_CreateTextureFromSurface(engine.renderer, sprite_surface);
                if (sprite.texture == NULL) {
                    log_error("Error creating texture for sprite %s: %s", sprite_path, SDL_GetError());
                    return false;
                }
                sprite.frame_size = xy(sprite_params_it->second.hframes, sprite_params_it->second.vframes);
                sprite.hframes = sprite_surface->w / sprite.frame_size.x;
                sprite.vframes = sprite_surface->h / sprite.frame_size.y;
                break;
            }
            case SPRITE_IMPORT_TILESET: {
                // Load base texture
                SDL_Texture* base_texture = SDL_CreateTextureFromSurface(engine.renderer, sprite_surface);
                if (base_texture == NULL) {
                    log_error("Error creating texture from sprite: %s: %s", sprite_path, SDL_GetError());
                    return false;
                }

                // Create tileset texture
                engine.tile_index = std::vector<uint16_t>(TILE_COUNT, 0);
                int hframes = 0;
                for (int tile = 0; tile < TILE_COUNT; tile++) {
                    if (TILE_DATA.at(tile).type == TILE_TYPE_SINGLE) {
                        hframes++;
                    } else {
                        hframes += 47;
                    }
                }
                sprite.texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, hframes * TILE_SIZE, TILE_SIZE);
                if (sprite.texture == NULL) {
                    log_error("Unable to create tileset texture: %s", SDL_GetError());
                    return false;
                }
                sprite.frame_size = xy(TILE_SIZE, TILE_SIZE);
                SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND);
                SDL_SetRenderTarget(engine.renderer, sprite.texture);
                int tileset_index = 0;
                SDL_Rect src_rect = (SDL_Rect) {
                    .x = 0,
                    .y = 0,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                SDL_Rect dst_rect = (SDL_Rect) {
                    .x = 0,
                    .y = 0,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };

                for (int tile = 0; tile < TILE_COUNT; tile++) {
                    const tile_data_t& tile_data = TILE_DATA.at(tile);
                    engine.tile_index[tile] = tileset_index;
                    if (tile_data.type == TILE_TYPE_SINGLE) {
                        src_rect.x = tile_data.source_x;
                        src_rect.y = tile_data.source_y;
                        dst_rect.x = tileset_index * TILE_SIZE;
                        SDL_RenderCopy(engine.renderer, base_texture, &src_rect, &dst_rect);
                        tileset_index++;
                    } else if (tile_data.type == TILE_TYPE_AUTO) {
                        xy auto_base_pos = xy(tile_data.source_x, tile_data.source_y);
                        for (uint32_t neighbors = 0; neighbors < 256; neighbors++) {
                            bool is_unique = true;
                            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                                if (direction % 2 == 1 && (DIRECTION_MASK[direction] & neighbors) == DIRECTION_MASK[direction]) {
                                    int prev_direction = direction - 1;
                                    int next_direction = (direction + 1) % DIRECTION_COUNT;
                                    if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                                        (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                                        is_unique = false;
                                        break;
                                    }
                                }
                            }
                            if (!is_unique) {
                                continue;
                            }

                            for (uint32_t edge = 0; edge < 4; edge++) {
                                xy source_pos = auto_base_pos + (autotile_edge_lookup(edge, neighbors & AUTOTILE_EDGE_MASK[edge]) * (TILE_SIZE / 2));
                                SDL_Rect subtile_src_rect = (SDL_Rect) {
                                    .x = source_pos.x,
                                    .y = source_pos.y,
                                    .w = TILE_SIZE / 2,
                                    .h = TILE_SIZE / 2
                                };
                                SDL_Rect subtile_dst_rect = (SDL_Rect) {
                                    .x = (tileset_index * TILE_SIZE) + (AUTOTILE_EDGE_OFFSETS[edge].x * (TILE_SIZE / 2)),
                                    .y = AUTOTILE_EDGE_OFFSETS[edge].y * (TILE_SIZE / 2),
                                    .w = TILE_SIZE / 2,
                                    .h = TILE_SIZE / 2
                                };

                                SDL_RenderCopy(engine.renderer, base_texture, &subtile_src_rect, &subtile_dst_rect);
                            }
                            tileset_index++;
                        } // End for each neighbor combo in water autotile
                    } // End if tile type is auto
                } // End for each tile

                SDL_SetRenderTarget(engine.renderer, NULL);
                SDL_DestroyTexture(base_texture);
                break;
            }
            case SPRITE_IMPORT_FOG_OF_WAR: {
                // Load base texture
                SDL_Texture* base_texture = SDL_CreateTextureFromSurface(engine.renderer, sprite_surface);
                if (base_texture == NULL) {
                    log_error("Error creating texture from sprite: %s: %s", sprite_path, SDL_GetError());
                    return false;
                }
                // Create tileset texture
                int hframes = 47 + 47;
                sprite.texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, hframes * TILE_SIZE, TILE_SIZE);
                if (sprite.texture == NULL) {
                    log_error("Unable to create tileset texture: %s", SDL_GetError());
                    return false;
                }
                sprite.frame_size = xy(TILE_SIZE, TILE_SIZE);
                SDL_SetTextureBlendMode(sprite.texture, SDL_BLENDMODE_BLEND);
                SDL_SetRenderTarget(engine.renderer, sprite.texture);
                int tileset_index = 0;

                // Blit autotile
                for (uint32_t neighbors = 0; neighbors < 256; neighbors++) {
                    bool is_unique = true;
                    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                        if (direction % 2 == 1 && (DIRECTION_MASK[direction] & neighbors) == DIRECTION_MASK[direction]) {
                            int prev_direction = direction - 1;
                            int next_direction = (direction + 1) % DIRECTION_COUNT;
                            if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                                (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                                is_unique = false;
                                break;
                            }
                        }
                    }
                    if (!is_unique) {
                        continue;
                    }

                    for (uint32_t edge = 0; edge < 4; edge++) {
                        xy source_pos = autotile_edge_lookup(edge, neighbors & AUTOTILE_EDGE_MASK[edge]) * (TILE_SIZE / 2);
                        SDL_Rect subtile_src_rect = (SDL_Rect) {
                            .x = source_pos.x,
                            .y = source_pos.y,
                            .w = TILE_SIZE / 2,
                            .h = TILE_SIZE / 2
                        };
                        SDL_Rect subtile_dst_rect = (SDL_Rect) {
                            .x = (tileset_index * TILE_SIZE) + (AUTOTILE_EDGE_OFFSETS[edge].x * (TILE_SIZE / 2)),
                            .y = AUTOTILE_EDGE_OFFSETS[edge].y * (TILE_SIZE / 2),
                            .w = TILE_SIZE / 2,
                            .h = TILE_SIZE / 2
                        };
                        SDL_RenderCopy(engine.renderer, base_texture, &subtile_src_rect, &subtile_dst_rect);

                        subtile_src_rect.x += 32;
                        subtile_src_rect.y = source_pos.y;
                        subtile_dst_rect.x += (TILE_SIZE * 47);
                        SDL_RenderCopy(engine.renderer, base_texture, &subtile_src_rect, &subtile_dst_rect);
                    }
                    tileset_index++;
                } // End for each neighbor combo 

                SDL_SetRenderTarget(engine.renderer, NULL);
                SDL_DestroyTexture(base_texture);
                break;
            }
        }

        GOLD_ASSERT(sprite_surface->w % sprite.frame_size.x == 0 || sprite_params_it->second.strategy >= SPRITE_IMPORT_TILESET);
        GOLD_ASSERT(sprite_surface->h % sprite.frame_size.y == 0 || sprite_params_it->second.strategy >= SPRITE_IMPORT_TILESET);
        engine.sprites.push_back(sprite);

        SDL_FreeSurface(sprite_surface);
    } // End for each sprite

    // Setup neighbors to autotile index
    {
        uint32_t unique_index = 0;
        for (uint32_t neighbors = 0; neighbors < 256; neighbors++) {
            bool is_unique = true;
            for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                if (direction % 2 == 1 && (DIRECTION_MASK[direction] & neighbors) == DIRECTION_MASK[direction]) {
                    int prev_direction = direction - 1;
                    int next_direction = (direction + 1) % DIRECTION_COUNT;
                    if ((DIRECTION_MASK[prev_direction] & neighbors) != DIRECTION_MASK[prev_direction] ||
                        (DIRECTION_MASK[next_direction] & neighbors) != DIRECTION_MASK[next_direction]) {
                        is_unique = false;
                        break;
                    }
                }
            }
            if (!is_unique) {
                continue;
            }
            engine.neighbors_to_autotile_index[neighbors] = unique_index;
            unique_index++;
        }
    }

    engine.cursors.reserve(CURSOR_COUNT);
    for (uint32_t i = 0; i < CURSOR_COUNT; i++) {
        auto it = CURSOR_PARAMS.find((Cursor)i);
        if (it == CURSOR_PARAMS.end()) {
            log_error("Cursor params not defined for cursor %u", i);
            return false;
        }

        char cursor_path[128];
        sprintf(cursor_path, "%s%s", GOLD_RESOURCE_PATH, it->second.path);
        SDL_Surface* cursor_image = IMG_Load(cursor_path);
        if (cursor_image == NULL) {
            log_error("Unable to load cursor image at path %s: %s", it->second.path, IMG_GetError());
            return false;
        }

        SDL_Cursor* cursor = SDL_CreateColorCursor(cursor_image, it->second.hot_x, it->second.hot_y);
        if (cursor == NULL) {
            log_error("Unable to create cursor %u: %s", i, SDL_GetError());
            return false;
        }

        engine.cursors.push_back(cursor);
        SDL_FreeSurface(cursor_image);
    }
    engine.current_cursor = CURSOR_DEFAULT;
    SDL_SetCursor(engine.cursors[CURSOR_DEFAULT]);

    SDL_RendererInfo renderer_info;
    SDL_GetRendererInfo(engine.renderer, &renderer_info);

    log_info("Initialized %s renderer.", renderer_info.name);

    bool renderer_supports_argb8888 = false;
    for (uint32_t i = 0; i < renderer_info.num_texture_formats; i++) {
        if (renderer_info.texture_formats[i] == SDL_PIXELFORMAT_ARGB8888) {
            renderer_supports_argb8888 = true;
        }
    }
    if (!renderer_supports_argb8888) {
        log_warn("Renderer does not support ARGB8888.");
    }
    return true;
}

void engine_destroy_renderer() {
    // Free fonts
    for (font_t font : engine.fonts) {
        for (int glyph_index = 0; glyph_index < FONT_GLYPH_COUNT; glyph_index++) {
            SDL_DestroyTexture(font.glyphs[glyph_index].texture);
        }
    }
    engine.fonts.clear();

    // Free sprites
    for (uint32_t index = 0; index < SPRITE_COUNT; index++) {
        if (SPRITE_PARAMS.at((Sprite)index).strategy == SPRITE_IMPORT_RECOLOR) {
            for (uint8_t i = 0; i < MAX_PLAYERS; i++) {
                SDL_DestroyTexture(engine.sprites[index].colored_texture[i]);
            }
        } else {
            SDL_DestroyTexture(engine.sprites[index].texture);
        }
    }
    engine.sprites.clear();
    engine.tile_index.clear();

    for (SDL_Cursor* cursor : engine.cursors) {
        SDL_FreeCursor(cursor);
    }
    engine.cursors.clear();

    if (engine.minimap_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_texture);
        engine.minimap_texture = NULL;
    }
    if (engine.minimap_tiles_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_tiles_texture);
        engine.minimap_tiles_texture = NULL;
    }

    SDL_DestroyRenderer(engine.renderer);
    log_info("Destroyed renderer.");
}

void engine_quit() {
    engine_destroy_renderer();
    SDL_DestroyWindow(engine.window);

    for (int i = 0; i < engine.sounds.size(); i++) {
        Mix_FreeChunk(engine.sounds[i]);
    }

    Mix_Quit();

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    log_info("Quit engine.");
}

void engine_set_cursor(Cursor cursor) {
    if (engine.current_cursor == cursor) {
        return;
    }
    engine.current_cursor = cursor;
    SDL_SetCursor(engine.cursors[cursor]);
}

// RENDER

xy autotile_edge_lookup(uint32_t edge, uint8_t neighbors) {
    switch (edge) {
        case 0:
            switch (neighbors) {
                case 0: return xy(0, 0);
                case 128: return xy(0, 0);
                case 1: return xy(0, 3);
                case 129: return xy(0, 3);
                case 64: return xy(1, 2);
                case 192: return xy(1, 2);
                case 65: return xy(2, 0);
                case 193: return xy(1, 3);
                default: GOLD_ASSERT(false);
            }
        case 1:
            switch (neighbors) {
                case 0: return xy(1, 0);
                case 2: return xy(1, 0);
                case 1: return xy(3, 3);
                case 3: return xy(3, 3);
                case 4: return xy(1, 2);
                case 6: return xy(1, 2);
                case 5: return xy(3, 0);
                case 7: return xy(1, 3);
                default: GOLD_ASSERT(false);
            }
        case 2:
            switch (neighbors) {
                case 0: return xy(0, 1);
                case 32: return xy(0, 1);
                case 16: return xy(0, 3);
                case 48: return xy(0, 3);
                case 64: return xy(1, 5);
                case 96: return xy(1, 5);
                case 80: return xy(2, 1);
                case 112: return xy(1, 3);
            }
        case 3:
            switch (neighbors) {
                case 0: return xy(1, 1);
                case 8: return xy(1, 1);
                case 4: return xy(1, 5);
                case 12: return xy(1, 5);
                case 16: return xy(3, 3);
                case 24: return xy(3, 3);
                case 20: return xy(3, 1);
                case 28: return xy(1, 3);
            }
        default: GOLD_ASSERT(false);
    }

    return xy(-1, -1);
}

xy render_get_text_size(Font font, const char* text) {
    xy text_size = xy(0, 0);
    uint32_t text_index = 0;
    while (text[text_index] != '\0') {
        int glyph_index = (int)text[text_index] - 32;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (int)'|' - 32;
        }

        text_size.x += engine.fonts[font].glyphs[glyph_index].advance;
        text_size.y = std::max(text_size.y, engine.fonts[font].glyphs[glyph_index].height);
        text_index++;
    }

    return text_size;
}

void render_text(Font font, const char* text, xy position, TextAnchor anchor) {
    // Don't render empty strings
    if (text[0] == '\0') {
        return;
    }

    xy text_size = render_get_text_size(font, text);

    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = 0, .h = 0 };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = position.x == RENDER_TEXT_CENTERED ? (SCREEN_WIDTH / 2) - (text_size.x / 2) : position.x,
        .y = position.y == RENDER_TEXT_CENTERED ? (SCREEN_HEIGHT / 2) - (text_size.y / 2) : position.y,
        .w = 0,
        .h = 0
    };
    int x_direction = 1;
    if (anchor == TEXT_ANCHOR_BOTTOM_LEFT || anchor == TEXT_ANCHOR_BOTTOM_RIGHT) {
        dst_rect.y -= text_size.y;
    }
    if (anchor == TEXT_ANCHOR_TOP_RIGHT || anchor == TEXT_ANCHOR_BOTTOM_RIGHT) {
        dst_rect.x -= text_size.x;
        x_direction = -1;
    }
    if (anchor == TEXT_ANCHOR_TOP_CENTER) {
        dst_rect.x -= (text_size.x / 2);
    }

    int text_index = 0;
    int dst_y = dst_rect.y;
    int dst_x = dst_rect.x;
    while (text[text_index] != 0) {
        int glyph_index = (int)text[text_index] - 32;
        if (glyph_index < 0 || glyph_index >= FONT_GLYPH_COUNT) {
            glyph_index = (int)'|' - 32;
        }

        src_rect.w = engine.fonts[font].glyphs[glyph_index].width;
        src_rect.h = engine.fonts[font].glyphs[glyph_index].height;

        dst_rect.x = dst_x + engine.fonts[font].glyphs[glyph_index].bearing_x;
        dst_rect.y = dst_y + text_size.y - engine.fonts[font].glyphs[glyph_index].bearing_y;
        dst_rect.w = src_rect.w;
        dst_rect.h = src_rect.h;

        SDL_RenderCopy(engine.renderer, engine.fonts[font].glyphs[glyph_index].texture, &src_rect, &dst_rect);
        dst_x += engine.fonts[font].glyphs[glyph_index].advance * x_direction;
        text_index++;
    }
}

int ysort_render_params_partition(std::vector<render_sprite_params_t>& params, int low, int high) {
    render_sprite_params_t pivot = params[high];
    int i = low - 1;

    for (int j = low; j <= high - 1; j++) {
        if (params[j].position.y < pivot.position.y) {
            i++;
            render_sprite_params_t temp = params[j];
            params[j] = params[i];
            params[i] = temp;
        }
    }

    render_sprite_params_t temp = params[high];
    params[high] = params[i + 1];
    params[i + 1] = temp;

    return i + 1;
}

void ysort_render_params(std::vector<render_sprite_params_t>& params, int low, int high) {
    if (low < high) {
        int partition_index = ysort_render_params_partition(params, low, high);
        ysort_render_params(params, low, partition_index - 1);
        ysort_render_params(params, partition_index + 1, high);
    }
}

void render_sprite(Sprite sprite, const xy& frame, const xy& position, uint32_t options, uint8_t recolor_id) {
    GOLD_ASSERT(frame.x < engine.sprites[sprite].hframes && frame.y < engine.sprites[sprite].vframes);

    bool flip_h = (options & RENDER_SPRITE_FLIP_H) == RENDER_SPRITE_FLIP_H;
    bool centered = (options & RENDER_SPRITE_CENTERED) == RENDER_SPRITE_CENTERED;
    bool cull = !((options & RENDER_SPRITE_NO_CULL) == RENDER_SPRITE_NO_CULL);

    SDL_Rect src_rect = (SDL_Rect) {
        .x = frame.x * engine.sprites[sprite].frame_size.x, 
        .y = frame.y * engine.sprites[sprite].frame_size.y, 
        .w = engine.sprites[sprite].frame_size.x, 
        .h = engine.sprites[sprite].frame_size.y 
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = position.x, .y = position.y,
        .w = src_rect.w, .h = src_rect.h
    };
    if (cull) {
        if (dst_rect.x + dst_rect.w < 0 || dst_rect.x > SCREEN_WIDTH || dst_rect.y + dst_rect.h < 0 || dst_rect.y > SCREEN_HEIGHT) {
            return;
        }
    }
    if (centered) {
        dst_rect.x -= (dst_rect.w / 2);
        dst_rect.y -= (dst_rect.h / 2);
    }

    SDL_Texture* texture;
    if (recolor_id == RECOLOR_NONE) {
        texture = engine.sprites[sprite].texture;
    } else {
        texture = engine.sprites[sprite].colored_texture[recolor_id];
    }
    SDL_RenderCopyEx(engine.renderer, texture, &src_rect, &dst_rect, 0.0, NULL, flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void render_ninepatch(Sprite sprite, const SDL_Rect& rect, int patch_margin) {
    GOLD_ASSERT(rect.w > patch_margin * 2 && rect.h > patch_margin * 2);

    SDL_Rect src_rect = (SDL_Rect) {
        .x = 0,
        .y = 0,
        .w = patch_margin,
        .h = patch_margin
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = rect.x, 
        .y = rect.y,
        .w = patch_margin,
        .h = patch_margin
    };

    // Top left
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Top right
    src_rect.x = patch_margin * 2;
    dst_rect.x = rect.x + rect.w - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Bottom right
    src_rect.y = patch_margin * 2;
    dst_rect.y = rect.y + rect.h - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);
    
    // Bottom left
    src_rect.x = 0;
    dst_rect.x = rect.x;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Top edge
    src_rect.x = patch_margin;
    src_rect.y = 0;
    dst_rect.x = rect.x + patch_margin;
    dst_rect.y = rect.y;
    dst_rect.w = rect.w - (patch_margin * 2);
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Bottom edge
    src_rect.y = patch_margin * 2;
    dst_rect.y = rect.y + rect.h - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Left edge
    src_rect.x = 0;
    src_rect.y = patch_margin;
    dst_rect.x = rect.x;
    dst_rect.y = rect.y + patch_margin;
    dst_rect.w = patch_margin;
    dst_rect.h = rect.h - (patch_margin * 2);
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Right edge
    src_rect.x = patch_margin * 2;
    dst_rect.x = rect.x + rect.w - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Center
    src_rect.x = patch_margin;
    src_rect.y = patch_margin;
    dst_rect.x = rect.x + patch_margin;
    dst_rect.y = rect.y + patch_margin;
    dst_rect.w = rect.w - (patch_margin * 2);
    dst_rect.h = rect.h - (patch_margin * 2);
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);
}

void render_text_with_text_frame(Sprite sprite, Font font, const char* text, xy position, xy text_offset, bool center_text) {
    int frame_size = engine.sprites[sprite].frame_size.x;
    xy text_size = render_get_text_size(font, text);
    int frame_width = (text_size.x / frame_size) + 1;
    if (text_size.x % frame_size != 0) {
        frame_width++;
    }
    for (int frame_x = 0; frame_x < frame_width; frame_x++) {
        int x_frame = 1;
        if (frame_x == 0) {
            x_frame = 0;
        } else if (frame_x == frame_width - 1) {
            x_frame = 2;
        }
        render_sprite(sprite, xy(x_frame, 0), position + xy(frame_x * frame_size, 0), RENDER_SPRITE_NO_CULL);
    }

    xy text_position_offset = center_text ? xy(((frame_width * frame_size) / 2) - (text_size.x / 2), 0) : xy(0, 0); 
    render_text(font, text, position + text_position_offset + text_offset);
}

SDL_Rect get_text_with_text_frame_rect(Sprite sprite, Font font, const char* text, xy position) {
    int frame_size = engine.sprites[sprite].frame_size.x;
    xy text_size = render_get_text_size(font, text);
    int frame_width = (text_size.x / frame_size) + 1;
    if (text_size.x % frame_size != 0) {
        frame_width++;
    }
    return (SDL_Rect) { 
        .x = position.x, .y = position.y, 
        .w = frame_width * engine.sprites[sprite].frame_size.x, 
        .h = engine.sprites[sprite].frame_size.y 
    };
}

// SOUND

void sound_play(Sound sound) {
    int variant = SOUND_PARAMS.at(sound).variants == 1 ? 0 : lcg_rand() % SOUND_PARAMS.at(sound).variants;
    Mix_PlayChannel(-1, engine.sounds[engine.sound_index[sound] + variant], 0);
}
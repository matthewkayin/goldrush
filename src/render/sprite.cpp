#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<Tileset, TilesetParams> TILESET_PARAMS = {
    { TILESET_ARIZONA, (TilesetParams) { .path = "map/arizona_tiles.png" }},
    { TILESET_KLONDIKE, (TilesetParams) { .path = "map/klondike_tiles.png" }},
    { TILESET_FOG, (TilesetParams) { .path = "map/fog_of_war.png" }}
};

static const std::unordered_map<SpriteName, SpriteParams> SPRITE_PARAMS = {
    { SPRITE_TILE_NULL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_SAND1, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 16,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND_WATER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_SNOW1, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_KLONDIKE,
            .type = TILE_TYPE_SINGLE,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SNOW2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_KLONDIKE,
            .type = TILE_TYPE_SINGLE,
            .source_x = 16,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SNOW3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_KLONDIKE,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SNOW_WATER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_KLONDIKE,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_NW_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_NE_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_SW_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SE_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_NORTH_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_WEST_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_EAST_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SE_FRONT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_FRONT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SW_FRONT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NW_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_NE_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_SW_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SE_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_LEFT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_RIGHT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 128,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_LEFT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_RIGHT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 128,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_LEFT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_RIGHT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 176,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_TOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_BOTTOM, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_TOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_BOTTOM, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 32
        }
    }},
    { SPRITE_DECORATION_ARIZONA, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "map/arizona_decorations.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_DECORATION_KLONDIKE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "map/klondike_decorations.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_FOG_HIDDEN, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_FOG,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_FOG_EXPLORED, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_FOG,
            .type = TILE_TYPE_AUTO,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_UI_MINIMAP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/minimap.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_WANTED_SIGN, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/wanted_sign.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_FRAME_BOLTS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/frame_bolts.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_FRAME, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/frame.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_FRAME_SMALL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/frame_small.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_BUTTON_REFRESH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/menu_refresh.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_ARROW, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/menu_next.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_BURGER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/menu_burger.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_PROFILE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/menu_profile.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_REPLAY_PAUSE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/replay_pause.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_REPLAY_PLAY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/replay_play.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
#ifdef GOLD_DEBUG
    { SPRITE_UI_EDITOR_PLUS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/editor_plus.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_EDITOR_EDIT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/editor_edit.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_EDITOR_TRASH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/editor_trash.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
#endif
    { SPRITE_UI_GOLD_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/icon_gold.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_HOUSE_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/icon_house.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MINER_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "ui/icon_miner.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_ENERGY_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/icon_energy.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_DROPDOWN, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/options_dropdown.png",
            .hframes = 1,
            .vframes = 5
        }
    }},
    { SPRITE_UI_DROPDOWN_MINI, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/dropdown_mini.png",
            .hframes = 1,
            .vframes = 5
        }
    }},
    { SPRITE_UI_TEAM_PICKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/team_picker.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MENU_BUTTON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/parchment_buttons.png",
            .hframes = 3,
            .vframes = 2
        }
    }},
    { SPRITE_UI_TEXT_FRAME, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/text_frame.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_CLOUDS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/menu_clouds.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_SWATCH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_SWATCH,
    }},
    { SPRITE_UI_TITLE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/title.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MOVE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/move.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_UI_CONTROL_GROUP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/control_group_frame.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_ICON_BUTTON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/button.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_TOOLTIP_FRAME, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/tooltip_frame.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_STAT_ICON_DETECTION, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/icon_detection.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_STAT_ICON_BLEED, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui/icon_bleed.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/attack.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_STOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/stop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DEFEND, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/defend.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUILD, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/build.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUILD2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/build2.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_REPAIR, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/repair.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_UNLOAD, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/unload.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CANCEL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/cancel.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_EXPLODE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/explode.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BOMB, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/bomb.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_HALL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/hall.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_HOUSE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/house.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SALOON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/saloon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUNKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/bunker.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SMITH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/smith.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_COOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/coop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BARRACKS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/barracks.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SHERIFFS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/sheriffs.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_MINER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/miner.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/wagon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAR_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/war_wagon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_COWBOY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/cowboy.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BANDIT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/bandit.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SAPPER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/sapper.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_PYRO, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/pyro.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SOLDIER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/soldier.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CANNON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/cannon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_JOCKEY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/jockey.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DETECTIVE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/detective.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BALLOON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/balloon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_LANDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/landmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_GOLDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/goldmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAGON_ARMOR, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/wagon_armor.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_EXPLOSIVES, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/tnt.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WORKSHOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/workshop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BAYONETS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/bayonets.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_MOLOTOV, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/molotov.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CAMO, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/camo.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DECAMO, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/camo_off.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_PRIVATE_EYE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/private_eye.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_STAKEOUT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/stakeout.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SERRATED_KNIVES, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/serrated_knives.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_FAN_HAMMER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/fan_hammer.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_TAILWIND, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "icon_button/tailwind.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_LANDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/landmine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_LANDMINE_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/landmine_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_UNIT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/1x1.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_UNIT_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/1x1_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/wagon.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_WAGON_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/wagon_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/2x2.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE2_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/2x2_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/3x3.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE3_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/3x3_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE4, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/4x4.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE4_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/4x4_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_GOLDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring/goldmine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_GOLDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/goldmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_wagon.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_WAR_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_war_wagon.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_MINER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_miner.png",
            .hframes = 15,
            .vframes = 6
        }
    }},
    { SPRITE_UNIT_COWBOY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_cowboy.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_SAPPER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_sapper.png",
            .hframes = 15,
            .vframes = 6 
        }
    }},
    { SPRITE_UNIT_PYRO, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_pyro.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_JOCKEY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_jockey.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_SOLDIER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_soldier.png",
            .hframes = 26,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_CANNON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_cannon.png",
            .hframes = 21,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_DETECTIVE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_spy.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_DETECTIVE_INVISIBLE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR_AND_LOW_ALPHA,
        .sheet = {
            .path = "entity/unit_spy.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_BALLOON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_balloon.png",
            .hframes = 5,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_BALLOON_STEAM, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/unit_balloon_steam.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_BALLOON_SHADOW, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/unit_balloon_shadow.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_BANDIT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_bandit.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_MINER_BUILDING, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/unit_miner_building.png",
            .hframes = 2,
            .vframes = 3
        }
    }},
    { SPRITE_BUILDING_HALL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_hall.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_HOUSE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_house.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SALOON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_saloon.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_BUNKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_bunker.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SMITH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_smith.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SMITH_ANIMATION, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_smith_animation.png",
            .hframes = 9,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_WORKSHOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_workshop.png",
            .hframes = 19,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_COOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_coop.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_BARRACKS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_barracks.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SHERIFFS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_sheriffs.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_LANDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "entity/building_mine.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_BUNKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_destroyed_bunker.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_MINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_destroyed_mine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_destroyed2x2.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_destroyed3x3.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_4, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_destroyed4x4.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_RALLY_FLAG, (SpriteParams) {
        .strategy = SPRITE_IMPORT_PLAYER_COLOR,
        .sheet = {
            .path = "rally_flag.png",
            .hframes = 6,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_SPARKS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle/sparks.png",
            .hframes = 4,
            .vframes = 3
        }
    }},
    { SPRITE_PARTICLE_BUNKER_FIRE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle/bunker_cowboy.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_EXPLOSION, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle/explosion.png",
            .hframes = 6,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_CANNON_EXPLOSION, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle/cannon_explosion.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_FIRE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle/fire.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_BLEED, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle/bleed.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_PROJECTILE_MOLOTOV, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "projectile_molotov.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_WORKSHOP_STEAM, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "entity/building_workshop_steam.png",
            .hframes = 5,
            .vframes = 1
        }
    }}
};

const SpriteParams& render_get_sprite_params(SpriteName sprite) {
    return SPRITE_PARAMS.at(sprite);
}

const TilesetParams& render_get_tileset_params(Tileset tileset) {
    return TILESET_PARAMS.at(tileset);
}
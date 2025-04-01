#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<Tileset, TilesetParams> TILESET_PARAMS = {
    { TILESET_ARIZONA, (TilesetParams) { .path = "tileset_arizona.png" }},
    { TILESET_FOG, (TilesetParams) { .path = "fog_of_war.png" }}
};

static const std::unordered_map<SpriteName, SpriteParams> SPRITE_PARAMS = {
    { SPRITE_TILE_NULL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_SAND1, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 16,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WATER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_NW_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_NE_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_SW_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SE_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_NORTH_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_WEST_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_EAST_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_EDGE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SE_FRONT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_FRONT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SW_FRONT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NW_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_NE_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_SW_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SE_INNER_CORNER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_LEFT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_RIGHT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 128,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_LEFT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_RIGHT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 128,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_LEFT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_RIGHT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 176,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_TOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_BOTTOM, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_TOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_CENTER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_BOTTOM, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 32
        }
    }},
    { SPRITE_DECORATION, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "tile_decorations.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_FOG_HIDDEN, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_FOG,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_FOG_EXPLORED, (SpriteParams) {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = (SpriteParamsTile) {
            .tileset = TILESET_FOG,
            .type = TILE_TYPE_AUTO,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_UI_MINIMAP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_minimap.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_PANEL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_frame_buttons.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BOTTOM_PANEL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_frame_bottom.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_FRAME, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_frame.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_FRAME_SMALL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_frame_small.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_BUTTON_REFRESH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "menu_refresh.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_ARROW, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "menu_next.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_BURGER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_menu_button.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_GOLD_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_gold.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_HOUSE_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_house.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MINER_ICON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_miner.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_DROPDOWN, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_options_dropdown.png",
            .hframes = 1,
            .vframes = 5
        }
    }},
    { SPRITE_UI_DROPDOWN_MINI, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_dropdown_mini.png",
            .hframes = 1,
            .vframes = 5
        }
    }},
    { SPRITE_UI_TEAM_PICKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_team_picker.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MENU_BUTTON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_parchment_buttons.png",
            .hframes = 3,
            .vframes = 2
        }
    }},
    { SPRITE_UI_TEXT_FRAME, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_text_frame.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_CLOUDS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "menu_clouds.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_SWATCH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_swatch.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_UI_TITLE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_title.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MOVE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_move.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_UI_ICON_BUTTON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_TOOLTIP_FRAME, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_tooltip_frame.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_BUTTON_ICON_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_attack.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_STOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_stop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DEFEND, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_defend.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUILD, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_build.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUILD2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_build2.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_REPAIR, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_repair.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CANCEL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_cancel.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_EXPLODE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_explode.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BOMB, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_bomb.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_HALL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_hall.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_HOUSE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_house.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SALOON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_saloon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUNKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_bunker.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SMITH, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_smith.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_COOP, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_coop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BARRACKS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_barracks.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SHERIFFS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_sheriffs.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_MINER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_miner.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_wagon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAR_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_war_wagon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_COWBOY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_cowboy.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BANDIT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_bandit.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SAPPER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_sapper.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_TINKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_tinker.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SOLDIER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_soldier.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CANNON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_cannon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_JOCKEY, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_jockey.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DETECTIVE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_detective.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_LANDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_landmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_GOLDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_goldmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAGON_ARMOR, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_wagon_armor.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_EXPLOSIVES, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_tnt.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BAYONETS, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "ui_button_icon_bayonets.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_LANDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_mine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_LANDMINE_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_mine_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_UNIT, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_UNIT_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_wagon.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_WAGON_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_wagon_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_building2x2.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE2_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_building2x2_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_building3x3.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE3_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_building3x3_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE4, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_building4x4.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE4_ATTACK, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_building4x4_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_GOLDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "select_ring_gold_mine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_GOLDMINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "gold_mine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_WAGON, (SpriteParams) {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = (SpriteParamsSheet) {
            .path = "unit_wagon.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_MINER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = (SpriteParamsSheet) {
            .path = "unit_miner.png",
            .hframes = 15,
            .vframes = 6
        }
    }},
    { SPRITE_MINER_BUILDING, (SpriteParams) {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = (SpriteParamsSheet) {
            .path = "unit_miner_building.png",
            .hframes = 2,
            .vframes = 3
        }
    }},
    { SPRITE_BUILDING_HALL, (SpriteParams) {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = (SpriteParamsSheet) {
            .path = "building_hall.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_BUNKER, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "building_destroyed_bunker.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_MINE, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "building_destroyed_mine.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_2, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "building_destroyed2x2.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_3, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "building_destroyed3x3.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_4, (SpriteParams) {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = (SpriteParamsSheet) {
            .path = "building_destroyed4x4.png",
            .hframes = 4,
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
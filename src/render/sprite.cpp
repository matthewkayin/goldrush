#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<Tileset, TilesetParams> TILESET_PARAMS = {
    { TILESET_ARIZONA, { .path = "tileset_arizona.png" }},
    { TILESET_FOG, { .path = "fog_of_war.png" }}
};

static const std::unordered_map<SpriteName, SpriteParams> SPRITE_PARAMS = {
    { SPRITE_TILE_NULL, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_SAND1, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND2, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 16,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_SAND3, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WATER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_NW_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_NE_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_SW_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SE_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_NORTH_EDGE, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_WEST_EDGE, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_EAST_EDGE, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_EDGE, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SE_FRONT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 80,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_FRONT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 64,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SW_FRONT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 48,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NW_INNER_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_NE_INNER_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_SW_INNER_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SE_INNER_CORNER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_LEFT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_CENTER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_RIGHT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 128,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_LEFT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 96,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_CENTER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 112,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_SOUTH_STAIR_FRONT_RIGHT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 128,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_LEFT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_CENTER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_NORTH_STAIR_RIGHT, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 176,
            .source_y = 48
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_TOP, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_CENTER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_EAST_STAIR_BOTTOM, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 160,
            .source_y = 32
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_TOP, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 0
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_CENTER, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 16
        }
    }},
    { SPRITE_TILE_WALL_WEST_STAIR_BOTTOM, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_ARIZONA,
            .type = TILE_TYPE_SINGLE,
            .source_x = 144,
            .source_y = 32
        }
    }},
    { SPRITE_DECORATION, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "tile_decorations.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_FOG_HIDDEN, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_FOG,
            .type = TILE_TYPE_AUTO,
            .source_x = 0,
            .source_y = 0
        }
    }},
    { SPRITE_FOG_EXPLORED, {
        .strategy = SPRITE_IMPORT_TILE,
        .tile = {
            .tileset = TILESET_FOG,
            .type = TILE_TYPE_AUTO,
            .source_x = 32,
            .source_y = 0
        }
    }},
    { SPRITE_UI_MINIMAP, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_minimap.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_WANTED_SIGN, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_wanted_sign.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_FRAME_BOLTS, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_frame_bolts.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_FRAME, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_frame.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_FRAME_SMALL, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_frame_small.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_BUTTON_REFRESH, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "menu_refresh.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_ARROW, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "menu_next.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_BURGER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_menu_button.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_BUTTON_PROFILE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "menu_profile.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_REPLAY_PAUSE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_replay_pause.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_REPLAY_PLAY, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_replay_play.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_GOLD_ICON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_gold.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_HOUSE_ICON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_house.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MINER_ICON, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "ui_miner.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_ENERGY_ICON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_energy.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_DROPDOWN, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_options_dropdown.png",
            .hframes = 1,
            .vframes = 5
        }
    }},
    { SPRITE_UI_DROPDOWN_MINI, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_dropdown_mini.png",
            .hframes = 1,
            .vframes = 5
        }
    }},
    { SPRITE_UI_TEAM_PICKER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_team_picker.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MENU_BUTTON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_parchment_buttons.png",
            .hframes = 3,
            .vframes = 2
        }
    }},
    { SPRITE_UI_TEXT_FRAME, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_text_frame.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_CLOUDS, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "menu_clouds.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"
    { SPRITE_UI_SWATCH, {
        .strategy = SPRITE_IMPORT_SWATCH,
    }},
    #pragma clang diagnostic pop
    { SPRITE_UI_TITLE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_title.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_MOVE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_move.png",
            .hframes = 5,
            .vframes = 1
        }
    }},
    { SPRITE_UI_CONTROL_GROUP, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_control_group_frame.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_ICON_BUTTON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UI_TOOLTIP_FRAME, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_tooltip_frame.png",
            .hframes = 3,
            .vframes = 3
        }
    }},
    { SPRITE_UI_STAT_ICON_DETECTION, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_stat_detection.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UI_STAT_ICON_BLEED, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_stat_bleed.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_attack.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_STOP, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_stop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DEFEND, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_defend.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUILD, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_build.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUILD2, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_build2.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_REPAIR, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_repair.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_UNLOAD, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_unload.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CANCEL, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_cancel.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_EXPLODE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_explode.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BOMB, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_bomb.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_HALL, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_hall.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_HOUSE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_house.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SALOON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_saloon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BUNKER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_bunker.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SMITH, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_smith.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_COOP, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_coop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BARRACKS, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_barracks.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SHERIFFS, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_sheriffs.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_MINER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_miner.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAGON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_wagon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAR_WAGON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_war_wagon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_COWBOY, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_cowboy.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BANDIT, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_bandit.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SAPPER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_sapper.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_PYRO, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_pyro.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SOLDIER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_soldier.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CANNON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_cannon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_JOCKEY, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_jockey.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DETECTIVE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_detective.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BALLOON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_balloon.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_LANDMINE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_landmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_GOLDMINE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_goldmine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WAGON_ARMOR, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_wagon_armor.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_EXPLOSIVES, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_tnt.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_WORKSHOP, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_workshop.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_BAYONETS, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_bayonets.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_MOLOTOV, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_molotov.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_CAMO, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_camo.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_DECAMO, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_decamo.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_PRIVATE_EYE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_private_eye.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_STAKEOUT, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_stakeout.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_SERRATED_KNIVES, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_serrated_knives.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_FAN_HAMMER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_fan_hammer.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_BUTTON_ICON_TAILWIND, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "ui_button_icon_tailwind.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_LANDMINE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_mine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_LANDMINE_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_mine_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_UNIT, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_UNIT_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_WAGON, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_wagon.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_WAGON_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_wagon_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE2, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_building2x2.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE2_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_building2x2_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE3, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_building3x3.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE3_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_building3x3_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE4, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_building4x4.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_BUILDING_SIZE4_ATTACK, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_building4x4_attack.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_SELECT_RING_GOLDMINE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "select_ring_gold_mine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_GOLDMINE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "gold_mine.png",
            .hframes = 3,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_WAGON, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_wagon.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_WAR_WAGON, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_war_wagon.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_MINER, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_miner.png",
            .hframes = 15,
            .vframes = 6
        }
    }},
    { SPRITE_UNIT_COWBOY, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_cowboy.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_SAPPER, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_sapper.png",
            .hframes = 15,
            .vframes = 6 
        }
    }},
    { SPRITE_UNIT_PYRO, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_pyro.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_JOCKEY, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_jockey.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_SOLDIER, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_soldier.png",
            .hframes = 26,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_CANNON, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_cannon.png",
            .hframes = 21,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_DETECTIVE, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_spy.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_DETECTIVE_INVISIBLE, {
        .strategy = SPRITE_IMPORT_RECOLOR_AND_LOW_ALPHA,
        .sheet = {
            .path = "unit_spy.png",
            .hframes = 15,
            .vframes = 3 
        }
    }},
    { SPRITE_UNIT_BALLOON, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_balloon.png",
            .hframes = 5,
            .vframes = 3
        }
    }},
    { SPRITE_UNIT_BALLOON_STEAM, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "unit_balloon_steam.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_BALLOON_SHADOW, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "unit_balloon_shadow.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_UNIT_BANDIT, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_bandit.png",
            .hframes = 15,
            .vframes = 3
        }
    }},
    { SPRITE_MINER_BUILDING, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "unit_miner_building.png",
            .hframes = 2,
            .vframes = 3
        }
    }},
    { SPRITE_BUILDING_HALL, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_hall.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_HOUSE, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_house.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SALOON, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_saloon.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_BUNKER, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_bunker.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SMITH, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_smith.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SMITH_ANIMATION, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_smith_animation.png",
            .hframes = 9,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_WORKSHOP, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_workshop.png",
            .hframes = 19,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_COOP, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_coop.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_BARRACKS, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_barracks.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_SHERIFFS, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_sheriffs.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_LANDMINE, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "building_mine.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_BUNKER, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_destroyed_bunker.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_MINE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_destroyed_mine.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_2, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_destroyed2x2.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_3, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_destroyed3x3.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_BUILDING_DESTROYED_4, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_destroyed4x4.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_RALLY_FLAG, {
        .strategy = SPRITE_IMPORT_RECOLOR,
        .sheet = {
            .path = "rally_flag.png",
            .hframes = 6,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_SPARKS, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle_sparks.png",
            .hframes = 4,
            .vframes = 3
        }
    }},
    { SPRITE_PARTICLE_BUNKER_FIRE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle_bunker_cowboy.png",
            .hframes = 2,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_EXPLOSION, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle_explosion.png",
            .hframes = 6,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_CANNON_EXPLOSION, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle_cannon_explosion.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_FIRE, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle_fire.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_PARTICLE_BLEED, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "particle_bleed.png",
            .hframes = 4,
            .vframes = 1
        }
    }},
    { SPRITE_PROJECTILE_MOLOTOV, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "projectile_molotov.png",
            .hframes = 1,
            .vframes = 1
        }
    }},
    { SPRITE_WORKSHOP_STEAM, {
        .strategy = SPRITE_IMPORT_DEFAULT,
        .sheet = {
            .path = "building_workshop_steam.png",
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
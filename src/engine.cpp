#include "engine.h"

#include "logger.h"
#include "asserts.h"
#include <unordered_map>

struct font_params_t {
    const char* path;
    int size;
};

static const std::unordered_map<uint32_t, font_params_t> font_params = {
    { FONT_HACK, (font_params_t) {
        .path = "font/hack.ttf",
        .size = 10
    }},
    { FONT_WESTERN8, (font_params_t) {
        .path = "font/western.ttf",
        .size = 8
    }},
    { FONT_WESTERN16, (font_params_t) {
        .path = "font/western.ttf",
        .size = 16
    }},
    { FONT_WESTERN32, (font_params_t) {
        .path = "font/western.ttf",
        .size = 32
    }},
    { FONT_M3X6, (font_params_t) {
        .path = "font/m3x6.ttf",
        .size = 16
    }},
};

enum SpriteImportStrategy {
    SPRITE_IMPORT_DEFAULT,
    SPRITE_IMPORT_RECOLOR,
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
        .vframes = 3,
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
        .hframes = 2,
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
    { SPRITE_UNIT_BANDIT, (sprite_params_t) {
        .path = "sprite/unit_bandit.png",
        .hframes = 15,
        .vframes = 3,
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
    { SPRITE_BUILDING_MINE, (sprite_params_t) {
        .path = "sprite/building_mine.png",
        .hframes = 3,
        .vframes = 1,
        .strategy = SPRITE_IMPORT_DEFAULT
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
    { SPRITE_BUILDING_DESTROYED_BUNKER, (sprite_params_t) {
        .path = "sprite/building_destroyed_bunker.png",
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

engine_t engine;

bool engine_init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
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

    engine.window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
    if (engine.window == NULL) {
        log_error("Error creating window: %s", SDL_GetError());
        return false;
    }

    if (!engine_init_renderer()) {
        return false;
    }

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
        TTF_Font* font = TTF_OpenFont(font_path, font_params_it->second.size);
        if (font == NULL) {
            log_error("Error loading font %s: %s", font_params.at(i).path, TTF_GetError());
            return false;
        }
        engine.fonts.push_back(font);
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
            case SPRITE_IMPORT_RECOLOR: {
                // Re-color texture creation
                uint32_t* sprite_pixels = (uint32_t*)sprite_surface->pixels;
                uint32_t reference_pixel = SDL_MapRGBA(sprite_surface->format, RECOLOR_REF.r, RECOLOR_REF.g, RECOLOR_REF.b, RECOLOR_REF.a);

                for (uint32_t player_color = 0; player_color < MAX_PLAYERS; player_color++) {
                    SDL_Surface* recolored_surface = SDL_CreateRGBSurfaceWithFormat(0, sprite_surface->w, sprite_surface->h, 32, sprite_surface->format->format);
                    uint32_t* recolor_pixels = (uint32_t*)recolored_surface->pixels;
                    SDL_LockSurface(recolored_surface);

                    const SDL_Color& color_player = PLAYER_COLORS[player_color];
                    uint32_t replace_pixel = SDL_MapRGBA(recolored_surface->format, color_player.r, color_player.g, color_player.b, color_player.a);

                    for (uint32_t pixel_index = 0; pixel_index < recolored_surface->w * recolored_surface->h; pixel_index++) {
                        recolor_pixels[pixel_index] = sprite_pixels[pixel_index] == reference_pixel ? replace_pixel : sprite_pixels[pixel_index];
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

    log_info("Initialized renderer.");
    return true;
}

void engine_destroy_renderer() {
    // Free fonts
    for (TTF_Font* font : engine.fonts) {
        TTF_CloseFont(font);
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

    SDL_DestroyRenderer(engine.renderer);
    log_info("Destroyed renderer.");
}

void engine_quit() {
    engine_destroy_renderer();
    SDL_DestroyWindow(engine.window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    log_info("Quit engine.");
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

void render_text(Font font, const char* text, SDL_Color color, xy position, TextAnchor anchor) {
    // Don't render empty strings
    if (text[0] == '\0') {
        return;
    }

    TTF_Font* font_data = engine.fonts[font];
    SDL_Surface* text_surface = TTF_RenderText_Solid(font_data, text, color);
    if (text_surface == NULL) {
        log_error("Unable to render text to surface. SDL Error: %s", TTF_GetError());
        return;
    }

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(engine.renderer, text_surface);
    if (text_texture == NULL) {
        log_error("Unable to create text from text surface. SDL Error: %s", SDL_GetError());
        return;
    }

    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = text_surface->w, .h = text_surface->h };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = position.x == RENDER_TEXT_CENTERED ? (SCREEN_WIDTH / 2) - (text_surface->w / 2) : position.x,
        .y = position.y == RENDER_TEXT_CENTERED ? (SCREEN_HEIGHT / 2) - (text_surface->h / 2) : position.y,
        .w = text_surface->w,
        .h = text_surface->h
    };
    if (anchor == TEXT_ANCHOR_BOTTOM_LEFT || anchor == TEXT_ANCHOR_BOTTOM_RIGHT) {
        dst_rect.y -= dst_rect.h;
    }
    if (anchor == TEXT_ANCHOR_TOP_RIGHT || anchor == TEXT_ANCHOR_BOTTOM_RIGHT) {
        dst_rect.x -= dst_rect.w;
    }
    if (anchor == TEXT_ANCHOR_TOP_CENTER) {
        dst_rect.x -= (dst_rect.w / 2);
    }
    SDL_RenderCopy(engine.renderer, text_texture, &src_rect, &dst_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
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
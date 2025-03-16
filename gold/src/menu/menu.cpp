#include "menu.h"

#include "defines.h"
#include "core/cursor.h"
#include "core/logger.h"
#include "render/render.h"
#include <cstdio>

static const int WAGON_X_DEFAULT = 380;
static const int WAGON_X_LOBBY = 480;
static const int PARALLAX_TIMER_DURATION = 2;

static const int MENU_TILE_WIDTH = (SCREEN_WIDTH / TILE_SIZE) * 2;
static const int MENU_TILE_HEIGHT = 2;
static const ivec2 MENU_DECORATION_COORDS[3] = { 
    ivec2(680, -8), 
    ivec2(920, 30), 
    ivec2(1250, -8) 
};

static const int CLOUD_COUNT = 6;
static const ivec2 CLOUD_COORDS[CLOUD_COUNT] = { 
    ivec2(640, 16), ivec2(950, 64), 
    ivec2(1250, 48), ivec2(-30, 48), 
    ivec2(320, 32), ivec2(1600, 32) 
};
static const int CLOUD_FRAME_X[CLOUD_COUNT] = { 0, 1, 2, 2, 1, 1};

void menu_render_decoration(const menu_state_t& state, int index);

menu_state_t menu_init() {
    menu_state_t state;

    state.mode = MENU_MODE_MAIN;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;

    return state;
}

void menu_update(menu_state_t& state) {
    cursor_set(CURSOR_DEFAULT);

    animation_update(state.wagon_animation);
    state.parallax_timer--;
    state.parallax_x = (state.parallax_x + 1) % (SCREEN_WIDTH * 2);
    if (state.parallax_timer == 0) {
        state.parallax_cloud_x = (state.parallax_cloud_x + 1) % (SCREEN_WIDTH * 2);
        if (state.parallax_x == 0) {
            state.parallax_cactus_offset = (state.parallax_cactus_offset + 1) % 5;
        }
        state.parallax_timer = PARALLAX_TIMER_DURATION;
    }
    int expected_wagon_x = state.mode == MENU_MODE_LOBBY ? WAGON_X_LOBBY : WAGON_X_DEFAULT;
    if (state.wagon_x < expected_wagon_x) {
        state.wagon_x++;
    } else if (state.wagon_x > expected_wagon_x) {
        state.wagon_x--;
    }
}

void menu_render(const menu_state_t& state) {
    // Sky background
    const sprite_info_t& sky_sprite_info = resource_get_sprite_info(SPRITE_UI_SKY);
    rect_t sky_src_rect = (rect_t) { 
        .x = sky_sprite_info.atlas_x, 
        .y = sky_sprite_info.atlas_y, 
        .w = sky_sprite_info.frame_width, 
        .h = sky_sprite_info.frame_height 
    };
    render_atlas(sky_sprite_info.atlas, sky_src_rect, (rect_t) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT }, RENDER_SPRITE_NO_CULL);

    // Tiles
    for (int y = 0; y < MENU_TILE_HEIGHT; y++) {
        for (int x = 0; x < MENU_TILE_WIDTH; x++) {
            sprite_name sprite = (x + y) % 10 == 0 ? SPRITE_TILE_SAND2 : SPRITE_TILE_SAND;
            const sprite_info_t& sprite_info = resource_get_sprite_info(sprite);
            rect_t src_rect = (rect_t) {
                .x = sprite_info.atlas_x,
                .y = sprite_info.atlas_y,
                .w = sprite_info.frame_width,
                .h = sprite_info.frame_height
            };
            rect_t dst_rect = (rect_t) {
                .x = (x * TILE_SIZE * 2) - state.parallax_x,
                .y = SCREEN_HEIGHT - ((MENU_TILE_HEIGHT - y) * TILE_SIZE * 2),
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            render_atlas(sprite_info.atlas, src_rect, dst_rect, 0);
        }
    }

    menu_render_decoration(state, 0);
    menu_render_decoration(state, 2);

    // Wagon animation
    const sprite_info_t& sprite_wagon_info = resource_get_sprite_info(SPRITE_UNIT_WAGON);
    rect_t wagon_src_rect = (rect_t) {
        .x = sprite_wagon_info.atlas_x + (sprite_wagon_info.frame_width * state.wagon_animation.frame.x),
        .y = sprite_wagon_info.atlas_y + (sprite_wagon_info.frame_height * 2),
        .w = sprite_wagon_info.frame_width,
        .h = sprite_wagon_info.frame_height
    };
    rect_t wagon_dst_rect = (rect_t) {
        .x = state.wagon_x,
        .y = SCREEN_HEIGHT - 88,
        .w = wagon_src_rect.w * 2,
        .h = wagon_src_rect.h * 2
    };
    render_atlas(sprite_wagon_info.atlas, wagon_src_rect, wagon_dst_rect, RENDER_SPRITE_NO_CULL);

    menu_render_decoration(state, 1);

    // Render clouds
    const sprite_info_t& cloud_sprite_info = resource_get_sprite_info(SPRITE_UI_CLOUDS);
    for (int index = 0; index < CLOUD_COUNT; index++) {
        rect_t src_rect = (rect_t) {
            .x = cloud_sprite_info.atlas_x + (CLOUD_FRAME_X[index] * cloud_sprite_info.frame_width),
            .y = cloud_sprite_info.atlas_y,
            .w = cloud_sprite_info.frame_width,
            .h = cloud_sprite_info.frame_height
        };
        rect_t dst_rect = (rect_t) {
            .x = CLOUD_COORDS[index].x - state.parallax_cloud_x,
            .y = CLOUD_COORDS[index].y,
            .w = src_rect.w * 2,
            .h = src_rect.h * 2
        };
        render_atlas(cloud_sprite_info.atlas, src_rect, dst_rect, 0);
    }

    char version_text[32];
    sprintf(version_text, "Version %s", APP_VERSION);
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_BLACK, version_text);
    render_text(FONT_WESTERN8_BLACK, version_text, ivec2(4, SCREEN_HEIGHT - text_size.y - 4), RENDER_COLOR_BLACK);
}

void menu_render_decoration(const menu_state_t& state, int index) {
    int cactus_index = ((index * 2) + state.parallax_cactus_offset) % 5;
    if (cactus_index == 2) {
        cactus_index = 0;
    }
    sprite_name sprite = (sprite_name)(SPRITE_TILE_DECORATION0 + cactus_index);
    const sprite_info_t& sprite_info = resource_get_sprite_info(sprite);
    rect_t src_rect = (rect_t) {
        .x = sprite_info.atlas_x,
        .y = sprite_info.atlas_y,
        .w = sprite_info.frame_width,
        .h = sprite_info.frame_height
    };
    rect_t dst_rect = (rect_t) {
        .x = MENU_DECORATION_COORDS[index].x - state.parallax_x,
        .y = SCREEN_HEIGHT - (MENU_TILE_HEIGHT * TILE_SIZE * 2) + MENU_DECORATION_COORDS[index].y,
        .w = TILE_SIZE * 2,
        .h = TILE_SIZE * 2
    };
    render_atlas(sprite_info.atlas, src_rect, dst_rect, 0);
}
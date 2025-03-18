#include "menu.h"

#include "defines.h"
#include "core/cursor.h"
#include "core/logger.h"
#include "core/input.h"
#include "render/render.h"
#include <cstdio>
#include <vector>

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

static const int TEXT_INPUT_BLINK_DURATION = 30;
static const int BUTTON_X = 44;
static const int BUTTON_Y = 128;
static const int BUTTON_Y_DIST = 25;

void menu_set_mode(menu_state_t& state, menu_mode mode);
menu_item_t menu_textbox_create(menu_item_name name, const char* prompt, rect_t rect);
menu_item_t menu_button_create(menu_item_name name, const char* text, ivec2 position);
void menu_handle_item_press(menu_state_t& state, menu_item_t& item);
void menu_render_decoration(const menu_state_t& state, int index);
void menu_render_button(const menu_item_t& button, bool hovered);
void menu_render_textbox(const menu_item_t& textbox, const char* value, bool show_cursor);

menu_state_t menu_init() {
    menu_state_t state;

    state.wagon_animation = animation_create(ANIMATION_UNIT_MOVE_SLOW);
    state.wagon_x = WAGON_X_DEFAULT;
    state.parallax_x = 0;
    state.parallax_cloud_x = 0;
    state.parallax_timer = PARALLAX_TIMER_DURATION;
    state.parallax_cactus_offset = 0;
    state.text_input_cursor_blink_timer = TEXT_INPUT_BLINK_DURATION;
    state.text_input_show_cursor = false;

    memset(state.username, 0, sizeof(state.username));
    state.username_length = 0;

    state.mode = MENU_MODE_MAIN;
    menu_set_mode(state, state.mode);

    return state;
}

void menu_set_mode(menu_state_t& state, menu_mode mode) {
    // Stop text input if leaving mode with a textbox
    if (state.mode == MENU_MODE_USERNAME) {
        input_stop_text_input();
    }

    // If entering a mode with a textbox, find the textbox and start input on it
    state.mode = mode;
    if (mode == MENU_MODE_USERNAME) {
        input_start_text_input(state.username, &state.username_length, MAX_USERNAME_LENGTH);
    }

    state.menu_items.clear();
    switch (state.mode) {
        case MENU_MODE_MAIN: {
            state.menu_items.push_back(menu_button_create(MENU_ITEM_MAIN_BUTTON_PLAY, "Play", ivec2(BUTTON_X, BUTTON_Y)));
            state.menu_items.push_back(menu_button_create(MENU_ITEM_MAIN_BUTTON_EXIT, "Exit", ivec2(BUTTON_X, BUTTON_Y + BUTTON_Y_DIST)));
            break;
        }
        case MENU_MODE_USERNAME: {
            state.menu_items.push_back(menu_textbox_create(MENU_ITEM_USERNAME_TEXTBOX, "Username: ", (rect_t) { .x = BUTTON_X - 4, .y = BUTTON_Y, .w = 256, .h = 24 }));
            state.menu_items.push_back(menu_button_create(MENU_ITEM_USERNAME_BUTTON_BACK, "Back", ivec2(BUTTON_X, BUTTON_Y + BUTTON_Y_DIST + 4)));
            state.menu_items.push_back(menu_button_create(MENU_ITEM_USERNAME_BUTTON_OK, "OK", ivec2(BUTTON_X + state.menu_items[1].rect.w + 4, BUTTON_Y + BUTTON_Y_DIST + 4)));
        }
    }

}

menu_item_t menu_textbox_create(menu_item_name name, const char* prompt, rect_t rect) {
    menu_item_t item;
    item.type = MENU_ITEM_TEXTBOX;
    item.name = name;
    item.rect = rect;
    item.textbox.prompt = prompt;

    return item;
}

menu_item_t menu_button_create(menu_item_name name, const char* text, ivec2 position) {
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, text);
    if (text_size.x % 8 != 0) {
        text_size.x = ((text_size.x / 8) + 1) * 8;
    }
    text_size.x += 16;

    return (menu_item_t) {
        .type = MENU_ITEM_BUTTON,
        .name = name,
        .rect = (rect_t) { 
            .x = position.x, .y = position.y,
            .w = text_size.x, .h = 21
        },
        .button = (menu_button_t) {
            .text = text,
        }
    };
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

    state.text_input_cursor_blink_timer--;
    if (state.text_input_cursor_blink_timer == 0) {
        state.text_input_cursor_blink_timer = TEXT_INPUT_BLINK_DURATION;
        state.text_input_show_cursor = !state.text_input_show_cursor;
    }

    if (input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
        if (input_is_text_input_active()) {
            input_stop_text_input();
        }
        for (menu_item_t& item : state.menu_items) {
            if (rect_has_point(item.rect, input_get_mouse_position())) {
                menu_handle_item_press(state, item);
                break;
            } 
        }
    }
}

void menu_handle_item_press(menu_state_t& state, menu_item_t& item) {
    switch (item.name) {
        case MENU_ITEM_MAIN_BUTTON_PLAY: {
            menu_set_mode(state, MENU_MODE_USERNAME);
            return;
        }
        case MENU_ITEM_MAIN_BUTTON_EXIT: {
            menu_set_mode(state, MENU_MODE_EXIT);
            return;
        }
        case MENU_ITEM_USERNAME_BUTTON_BACK: {
            menu_set_mode(state, MENU_MODE_MAIN);
            return;
        }
        case MENU_ITEM_USERNAME_TEXTBOX: {
            input_start_text_input(state.username, &state.username_length, MAX_USERNAME_LENGTH);
            return;
        }
        case MENU_ITEM_USERNAME_BUTTON_OK: {
            // TODO validate username
            menu_set_mode(state, MENU_MODE_MATCHLIST);
            return;
        }
        default:
            return;
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

    // Title text
    if (state.mode != MENU_MODE_LOBBY && state.mode != MENU_MODE_MATCHLIST) {
        render_text(FONT_WESTERN32_OFFBLACK, "GOLD RUSH", ivec2(24, 24));
    }

    // Render menu items
    for (const menu_item_t& item : state.menu_items) {
        switch (item.type) {
            case MENU_ITEM_BUTTON:
                menu_render_button(item, rect_has_point(item.rect, input_get_mouse_position()));
                break;
            case MENU_ITEM_TEXTBOX:
                if (state.mode == MENU_MODE_USERNAME) {
                    menu_render_textbox(item, state.username, state.text_input_show_cursor);
                }
                break;
        }
    }

    // Render version
    char version_text[32];
    sprintf(version_text, "Version %s", APP_VERSION);
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, version_text);
    render_text(FONT_WESTERN8_OFFBLACK, version_text, ivec2(4, SCREEN_HEIGHT - text_size.y - 4));
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

void menu_render_button(const menu_item_t& button, bool hovered) {
    // Draw button parchment
    int frame_count = button.rect.w / 8;
    for (int frame = 0; frame < frame_count; frame++) {
        // Determine hframe
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        render_sprite(SPRITE_UI_MENU_BUTTON, ivec2(hframe, (int)hovered), ivec2(button.rect.x + (frame * 8), button.rect.y), RENDER_SPRITE_NO_CULL, 0);
    }

    render_text(hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, button.button.text, ivec2(button.rect.x + 5, button.rect.y + 3 - (int)hovered));
}

void menu_render_textbox(const menu_item_t& textbox, const char* value, bool show_cursor) {
    render_ninepatch(SPRITE_UI_FRAME_SMALL, textbox.rect);
    render_text(FONT_HACK_GOLD, textbox.textbox.prompt, ivec2(textbox.rect.x + 5, textbox.rect.y + 6));
    ivec2 prompt_size = render_get_text_size(FONT_HACK_GOLD, textbox.textbox.prompt);
    render_text(FONT_HACK_GOLD, value, ivec2(textbox.rect.x + prompt_size.x, textbox.rect.y + 6));
    if (show_cursor && input_is_text_input_active()) {
        int value_text_width = render_get_text_size(FONT_HACK_GOLD, value).x;
        ivec2 cursor_pos = ivec2(textbox.rect.x + prompt_size.x + value_text_width - 4, textbox.rect.y + 6);
        render_text(FONT_HACK_GOLD, "|", cursor_pos);
    }
}
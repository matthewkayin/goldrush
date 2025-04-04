#include "ui.h"

#include "render/render.h"
#include "core/input.h"
#include "core/sound.h"
#include "core/logger.h"
#include <vector>
#include <algorithm>

static const uint32_t UI_TEXT_INPUT_BLINK_DURATION = 30;
static const int UI_DROPDOWN_NONE = -1;

enum UiRenderType {
    UI_RENDER_TEXT,
    UI_RENDER_SPRITE,
    UI_RENDER_NINEPATCH
};

struct UiRenderText {
    FontName font;
    char text[UI_RENDER_TEXT_BUFFER_SIZE];
    ivec2 position;
};

struct UiRenderSprite {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    bool flip_h;
};

struct UiRenderNinepatch {
    SpriteName sprite;
    Rect rect;
};

struct UiRender {
    UiRenderType type;
    union {
        UiRenderText text;
        UiRenderSprite sprite;
        UiRenderNinepatch ninepatch;
    };
};

enum UiContainerType {
    UI_CONTAINER_ROW,
    UI_CONTAINER_COLUMN,
    UI_CONTAINER_ELEMENT_POSITION,
    UI_CONTAINER_ELEMENT_SIZE
};

struct UiContainer {
    UiContainerType type;
    ivec2 origin;
    int spacing;
    ivec2 size;
};

struct UiState {
    uint32_t text_input_cursor_blink_timer;
    bool text_input_show_cursor;
    std::vector<UiContainer> container_stack;
    std::vector<UiRender> render_queue[UI_Z_INDEX_COUNT]; 
    int dropdown_open;
    int dropdown_open_future;
};
static UiState state;
static bool initialized = false;

ivec2 ui_get_container_origin();
void ui_update_container(ivec2 size);
void ui_queue_text(FontName font, const char* text, ivec2 position, int z_index);
void ui_queue_sprite(SpriteName sprite, ivec2 frame, ivec2 position, int z_index, bool flip_h = false);
void ui_queue_ninepatch(SpriteName sprite, Rect rect, int z_index);

void ui_begin() {
    if (!initialized) {
        state.text_input_cursor_blink_timer = UI_TEXT_INPUT_BLINK_DURATION;
        state.text_input_show_cursor = false;
        state.dropdown_open = UI_DROPDOWN_NONE;
        state.dropdown_open_future = UI_DROPDOWN_NONE;
        initialized = true;
    }

    state.container_stack.clear();
    for (int z_index = 0; z_index < UI_Z_INDEX_COUNT; z_index++) {
        state.render_queue[z_index].clear();
    }

    // Update of timers and such can go here since ui_begin() is called every frame
    state.text_input_cursor_blink_timer--;
    if (state.text_input_cursor_blink_timer == 0) {
        state.text_input_show_cursor = !state.text_input_show_cursor;
    }

    state.dropdown_open = state.dropdown_open_future;
}

void ui_element_position(ivec2 position) {
    state.container_stack.push_back((UiContainer) {
        .type = UI_CONTAINER_ELEMENT_POSITION,
        .origin = position,
        .spacing = 0,
        .size = ivec2(0, 0)
    });
}

void ui_element_size(ivec2 size) {
    state.container_stack.push_back((UiContainer) {
        .type = UI_CONTAINER_ELEMENT_SIZE,
        .origin = ivec2(0, 0),
        .spacing = 0,
        .size = size
    });
}

void ui_begin_row(ivec2 position, int spacing) {
    state.container_stack.push_back((UiContainer) {
        .type = UI_CONTAINER_ROW,
        .origin = position,
        .spacing = spacing,
        .size = ivec2(0, 0)
    });
}

void ui_begin_column(ivec2 position, int spacing) {
    state.container_stack.push_back((UiContainer) {
        .type = UI_CONTAINER_COLUMN,
        .origin = position,
        .spacing = spacing,
        .size = ivec2(0, 0)
    });
}

void ui_end_container() {
    ivec2 container_size = state.container_stack.back().size;
    state.container_stack.pop_back();
    ui_update_container(container_size);
}

bool ui_button(const char* text) {
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, text);
    if (text_size.x % 8 != 0) {
        text_size.x = ((text_size.x / 8) + 1) * 8;
    }
    text_size.x += 16;

    ivec2 origin = ui_get_container_origin();
    Rect button_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = text_size.x, .h = 21
    };
    ui_update_container(ivec2(button_rect.w, button_rect.h));

    bool hovered = button_rect.has_point(input_get_mouse_position());

    int frame_count = button_rect.w / 8;
    for (int frame = 0; frame < frame_count; frame++) {
        // Determine hframe
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        ui_queue_sprite(SPRITE_UI_MENU_BUTTON, ivec2(hframe, (int)hovered), ivec2(button_rect.x + (frame * 8), button_rect.y - (int)hovered), 0);
    }

    ui_queue_text(hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, text, ivec2(button_rect.x + 5, button_rect.y + 3 - (int)hovered), 0);

    bool clicked = state.dropdown_open == UI_DROPDOWN_NONE && hovered && input_is_action_just_pressed(INPUT_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

bool ui_sprite_button(SpriteName sprite, bool disabled, bool flip_h) {
    ivec2 origin = ui_get_container_origin();
    const SpriteInfo& sprite_info = render_get_sprite_info(sprite);
    ui_update_container(ivec2(sprite_info.frame_width, sprite_info.frame_height));

    Rect sprite_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = sprite_info.frame_width, .h = sprite_info.frame_height
    };
    int hframe = 0;
    if (disabled) {
        hframe = 2;
    } else if (sprite_rect.has_point(input_get_mouse_position())) {
        hframe = 1;
    }

    ui_queue_sprite(sprite, ivec2(hframe, 0), origin, 0, flip_h);

    bool clicked = state.dropdown_open == UI_DROPDOWN_NONE && !disabled && input_is_action_just_pressed(INPUT_LEFT_CLICK) && sprite_rect.has_point(input_get_mouse_position());
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

void ui_text(FontName font, const char* text) {
    ivec2 text_size = render_get_text_size(font, text);
    ivec2 origin = ui_get_container_origin();
    ui_update_container(text_size);
    ui_queue_text(font, text, origin, 0);
}

ivec2 ui_text_frame_size(const char* text) {
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
    ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, text);
    int frame_count = (text_size.x / sprite_info.frame_width) + 1;
    if (text_size.x % sprite_info.frame_width != 0) {
        frame_count++;
    }

    return ivec2(sprite_info.frame_width * frame_count, sprite_info.frame_height);
}

bool ui_text_frame(const char* text, bool disabled) {
    ivec2 origin = ui_get_container_origin();

    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
    ivec2 text_size = render_get_text_size(FONT_HACK_OFFBLACK, text);
    int frame_count = (text_size.x / sprite_info.frame_width) + 1;
    if (text_size.x % sprite_info.frame_width != 0) {
        frame_count++;
    }
    Rect rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = frame_count * sprite_info.frame_width, .h = sprite_info.frame_height
    };
    ui_update_container(ivec2(rect.w, rect.h));
    bool hovered = !disabled && rect.has_point(input_get_mouse_position());

    for (int frame = 0; frame < frame_count; frame++) {
        int hframe = 1;
        if (frame == 0) {
            hframe = 0;
        } else if (frame == frame_count - 1) {
            hframe = 2;
        }
        ui_queue_sprite(SPRITE_UI_TEXT_FRAME, ivec2(hframe, 0), ivec2(origin.x + (frame * sprite_info.frame_width), origin.y - (int)hovered), 0);
    }

    ui_queue_text(hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, text, ivec2(rect.x + (rect.w / 2) - (text_size.x / 2), rect.y + 1 -(int)hovered), 0);

    bool clicked = state.dropdown_open == UI_DROPDOWN_NONE && hovered && input_is_action_just_pressed(INPUT_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

void ui_frame(ivec2 size) {
    ivec2 origin = ui_get_container_origin();
    ui_update_container(size);
    Rect frame_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };
    ui_queue_ninepatch(SPRITE_UI_FRAME, frame_rect, 0);
}

void ui_frame_rect(Rect rect) {
    ui_element_position(ivec2(rect.x, rect.y));
    ui_frame(ivec2(rect.w, rect.h));
}

void ui_text_input(const char* prompt, ivec2 size, std::string* value, size_t max_length) {
    ivec2 origin = ui_get_container_origin();
    ui_update_container(size);

    Rect text_input_rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };
    ui_queue_ninepatch(SPRITE_UI_FRAME_SMALL, text_input_rect, 0);
    ui_queue_text(FONT_HACK_GOLD, prompt, ivec2(text_input_rect.x + 5, text_input_rect.y + 6), 0);
    ivec2 prompt_size = render_get_text_size(FONT_HACK_GOLD, prompt);
    ui_queue_text(FONT_HACK_GOLD, value->c_str(), ivec2(text_input_rect.x + prompt_size.x, text_input_rect.y + 6), 0);

    if (state.text_input_show_cursor && input_is_text_input_active()) {
        int value_text_width = render_get_text_size(FONT_HACK_GOLD, value->c_str()).x;
        ivec2 cursor_pos = ivec2(text_input_rect.x + prompt_size.x + value_text_width - 2, text_input_rect.y + 5);
        ui_queue_text(FONT_HACK_GOLD, "|", cursor_pos, 0);
    }

    if (input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
        if (text_input_rect.has_point(input_get_mouse_position())) {
            sound_play(SOUND_UI_CLICK);
            input_start_text_input(value, max_length);
        } else if (input_is_text_input_active()) {
            input_stop_text_input();
        }
    } 
}

bool ui_team_picker(char value, bool disabled) {
    ivec2 origin = ui_get_container_origin();
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_TEAM_PICKER);
    ivec2 size = ivec2(sprite_info.frame_width, sprite_info.frame_height);
    ui_update_container(size);

    Rect rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };

    bool is_hovered = !disabled && rect.has_point(input_get_mouse_position());

    ui_queue_sprite(SPRITE_UI_TEAM_PICKER, ivec2((int)is_hovered, 0), origin, 0);
    char text[2] = { value, '\0' };
    ui_queue_text(is_hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, text, ivec2(rect.x + 5, rect.y + 2), 0);

    bool clicked = state.dropdown_open == UI_DROPDOWN_NONE && is_hovered && input_is_action_just_pressed(INPUT_LEFT_CLICK);
    if (clicked) {
        sound_play(SOUND_UI_CLICK);
    }
    return clicked;
}

bool ui_dropdown(int dropdown_id, uint32_t* selected_item, const char** items, size_t item_count, bool disabled) {
    ivec2 origin = ui_get_container_origin();
    const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    ivec2 size = ivec2(sprite_info.frame_width, sprite_info.frame_height);
    ui_update_container(size);

    Rect rect = (Rect) {
        .x = origin.x, .y = origin.y,
        .w = size.x, .h = size.y
    };

    bool hovered = rect.has_point(input_get_mouse_position());

    int vframe = 0;
    if (disabled) {
        vframe = 3;
    } else if (state.dropdown_open == dropdown_id) {
        vframe = 2;
    } else if (state.dropdown_open == UI_DROPDOWN_NONE && hovered) {
        vframe = 1;
    }
    ui_queue_sprite(SPRITE_UI_DROPDOWN_MINI, ivec2(0, vframe), origin, 0);
    ui_queue_text(vframe == 1 ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, items[*selected_item], ivec2(origin.x + 5, origin.y + 2), 0);

    if (state.dropdown_open == dropdown_id) {
        // Render all the dropdown items
        int item_hovered = -1;
        for (int index = 0; index < item_count; index++) {
            Rect item_rect = (Rect) {
                .x = origin.x, .y = origin.y + (size.y * (index + 1)),
                .w = size.x, .h = size.y
            };
            bool item_is_hovered = item_rect.has_point(input_get_mouse_position());
            ui_queue_sprite(SPRITE_UI_DROPDOWN_MINI, ivec2(0, 3 + (int)item_is_hovered), ivec2(item_rect.x, item_rect.y), 1);
            ui_queue_text(item_is_hovered ? FONT_HACK_WHITE : FONT_HACK_OFFBLACK, items[index], ivec2(item_rect.x + 5, item_rect.y + 2), 1);

            if (item_is_hovered) {
                item_hovered = index;
            }
        }

        // If the user left clicked, close the dropdown
        if (input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
            state.dropdown_open_future = UI_DROPDOWN_NONE;
            // If they happened to be selecting an item, update the value
            if (item_hovered != -1) {
                *selected_item = item_hovered;
            } 
            sound_play(SOUND_UI_CLICK);
            // Return true if the item was changed
            return item_hovered != -1;
        }
    } else if (state.dropdown_open == UI_DROPDOWN_NONE && !disabled && hovered && input_is_action_just_pressed(INPUT_LEFT_CLICK)) {
        state.dropdown_open_future = dropdown_id;
        sound_play(SOUND_UI_CLICK);
    } 

    return false;
}

void ui_render() {
    for (int z_index = 0; z_index < UI_Z_INDEX_COUNT; z_index++) {
        for (const UiRender& render : state.render_queue[z_index]) {
            switch (render.type) {
                case UI_RENDER_TEXT: {
                    render_text(render.text.font, render.text.text, render.text.position);
                    break;
                }
                case UI_RENDER_SPRITE: {
                    uint32_t sprite_options = RENDER_SPRITE_NO_CULL;
                    if (render.sprite.flip_h) {
                        sprite_options |= RENDER_SPRITE_FLIP_H;
                    }
                    render_sprite_frame(render.sprite.sprite, render.sprite.frame, render.sprite.position, sprite_options, 0);
                    break;
                }
                case UI_RENDER_NINEPATCH: {
                    render_ninepatch(render.ninepatch.sprite, render.ninepatch.rect);
                    break;
                }
            }
        }
    }
}

ivec2 ui_get_container_origin() {
    ivec2 origin = ivec2(0, 0);
    for (const UiContainer& container : state.container_stack) {
        origin += container.origin;
    }

    return origin;
}

void ui_update_container(ivec2 size) {
    if (!state.container_stack.empty()) {
        UiContainer& container = state.container_stack.back();
        switch (container.type) {
            case UI_CONTAINER_ROW: {
                container.origin.x += size.x + container.spacing;
                container.size.x += size.x + container.spacing;
                container.size.y = std::max(container.size.y, size.y);
                break;
            }
            case UI_CONTAINER_COLUMN: {
                container.origin.y += size.y + container.spacing;
                container.size.y += size.y + container.spacing;
                container.size.x = std::max(container.size.x, size.x);
                break;
            }
            case UI_CONTAINER_ELEMENT_POSITION: {
                ui_end_container();
                break;
            }
            case UI_CONTAINER_ELEMENT_SIZE: {
                ui_end_container();
                break;
            }
        }
    }
}

void ui_queue_text(FontName font, const char* text, ivec2 position, int z_index) {
    UiRender render;
    render.type = UI_RENDER_TEXT;
    render.text.font = font;
    render.text.position = position;
    strcpy(render.text.text, text);
    state.render_queue[z_index].push_back(render);
}

void ui_queue_sprite(SpriteName sprite, ivec2 frame, ivec2 position, int z_index, bool flip_h) {
    state.render_queue[z_index].push_back((UiRender) {
        .type = UI_RENDER_SPRITE,
        .sprite = (UiRenderSprite) {
            .sprite = sprite,
            .frame = frame,
            .position = position,
            .flip_h = flip_h
        }
    });
}

void ui_queue_ninepatch(SpriteName sprite, Rect rect, int z_index) {
    state.render_queue[z_index].push_back((UiRender) {
        .type = UI_RENDER_NINEPATCH,
        .ninepatch = (UiRenderNinepatch) {
            .sprite = sprite,
            .rect = rect
        }
    });
}
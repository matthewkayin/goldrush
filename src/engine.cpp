#include "engine.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <unordered_map>
#include <vector>
#include <string>

#ifdef GOLD_DEBUG
    #define RESOURCE_BASE_PATH "../res/"
#else
    #define RESOURCE_BASE_PATH "./res/"
#endif

static const uint8_t INPUT_MOUSE_BUTTON_COUNT = 3;
uint8_t MOUSE_BUTTON_LEFT = SDL_BUTTON_LEFT;
uint8_t MOUSE_BUTTON_RIGHT = SDL_BUTTON_RIGHT;

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
        .size = 24 
    }}
};

struct sprite_params_t {
    const char* path;
    int h_frames;
    int v_frames;
};

struct sprite_t {
    SDL_Texture* texture;
    ivec2 frame_size;
    int h_frames;
    int v_frames;
};

static const std::unordered_map<uint32_t, sprite_params_t> sprite_params = {
    { SPRITE_TILES, (sprite_params_t) {
        .path = "sprite/tiles.png",
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
    { SPRITE_UI_MINIMAP, (sprite_params_t) {
        .path = "sprite/ui_minimap.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_SELECT_RING, (sprite_params_t) {
        .path = "sprite/select_ring.png",
        .h_frames = 1,
        .v_frames = 1
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "sprite/unit_miner.png",
        .h_frames = 8,
        .v_frames = 4
    }}  
};

SDL_Rect rect_to_sdl(const rect_t& r) {
    return (SDL_Rect) { .x = r.position.x, .y = r.position.y, .w = r.size.x, .h = r.size.y };
}

SDL_Color color_to_sdl(const color_t& color) {
    return (SDL_Color) { .r = color.r, .g = color.g, .b = color.b, .a = color.a };
}

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;

    bool is_running = false;
    bool is_fullscreen = false;

    // Input state
    bool mouse_button_state[INPUT_MOUSE_BUTTON_COUNT];
    bool mouse_button_previous_state[INPUT_MOUSE_BUTTON_COUNT];
    ivec2 mouse_position;
    std::string input_text;
    size_t input_length_limit;

    std::vector<TTF_Font*> fonts;
    std::vector<sprite_t> sprites;
    SDL_Texture* minimap_texture;
};
static engine_t engine;

bool engine_init(ivec2 window_size) {
    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        log_error("SDL failed to initialize: %s", SDL_GetError());
        return false;
    }

    // Create window
    engine.window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_size.x, window_size.y, SDL_WINDOW_SHOWN);
    if (engine.window == NULL) {
        log_error("Error creating window: %s", SDL_GetError());
        return false;
    }

    // Create renderer
    engine.renderer = SDL_CreateRenderer(engine.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (engine.renderer == NULL) {
        log_error("Error creating renderer: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(engine.renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

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

    // Init input
    memset(engine.mouse_button_state, 0, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));
    memset(engine.mouse_button_previous_state, 0, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));
    engine.mouse_position = ivec2(0, 0);

    // Load fonts
    std::string resource_base_path = std::string(RESOURCE_BASE_PATH);

    engine.fonts.reserve(FONT_COUNT);
    for (uint32_t i = 0; i < FONT_COUNT; i++) {
        // Get the font params
        auto font_params_it = font_params.find(i);
        if (font_params_it == font_params.end()) {
            log_error("Font params not defined for font id %u", i);
            return false;
        }

        // Load the font
        TTF_Font* font = TTF_OpenFont((resource_base_path + std::string(font_params_it->second.path)).c_str(), font_params_it->second.size);
        if (font == NULL) {
            log_error("Error loading font %s: %s", font_params.at(i).path, TTF_GetError());
            return false;
        }
        engine.fonts.push_back(font);
    }

    engine.sprites.reserve(SPRITE_COUNT);
    for (uint32_t i = 0; i < SPRITE_COUNT; i++) {
        // Get the sprite params
        auto sprite_params_it = sprite_params.find(i);
        if (sprite_params_it == sprite_params.end()) {
            log_error("Sprite params not defined for sprite id %u", i);
            return false;
        }

        // Load the sprite
        sprite_t sprite;
        SDL_Surface* sprite_surface = IMG_Load((resource_base_path + std::string(sprite_params_it->second.path)).c_str());
        if (sprite_surface == NULL) {
            log_error("Error loading sprite %s: %s", sprite_params_it->second.path, IMG_GetError());
            return false;
        }

        sprite.texture = SDL_CreateTextureFromSurface(engine.renderer, sprite_surface);
        if (sprite.texture == NULL) {
            log_error("Error creating texture for sprite %s: %s", sprite_params_it->second.path, SDL_GetError());
            return false;
        }

        if (i == SPRITE_TILES) {
            sprite.frame_size = ivec2(TILE_SIZE, TILE_SIZE);
            sprite.h_frames = sprite_surface->w / sprite.frame_size.x;
            sprite.v_frames = sprite_surface->h / sprite.frame_size.y;
        } else {
            sprite.h_frames = sprite_params_it->second.h_frames;
            sprite.v_frames = sprite_params_it->second.v_frames;
            sprite.frame_size = ivec2(sprite_surface->w / sprite.h_frames, sprite_surface->h / sprite.v_frames);
        }
        GOLD_ASSERT(sprite_surface->w % sprite.frame_size.x == 0);
        GOLD_ASSERT(sprite_surface->h % sprite.frame_size.y == 0);
        engine.sprites.push_back(sprite);

        SDL_FreeSurface(sprite_surface);
    }

    engine.minimap_texture = NULL;
    SDL_StopTextInput();
    engine.is_running = true;

    log_info("%s initialized.", APP_NAME);
    return true;
}

void engine_quit() {
    for (TTF_Font* font : engine.fonts) {
        TTF_CloseFont(font);
    }
    for (sprite_t sprite : engine.sprites) {
        SDL_DestroyTexture(sprite.texture);
    }
    if (engine.minimap_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_texture);
    }

    SDL_DestroyRenderer(engine.renderer);
    SDL_DestroyWindow(engine.window);

    TTF_Quit();
    SDL_Quit();

    log_info("Application quit gracefully.");
    logger_quit();
}

bool engine_is_running() {
    return engine.is_running;
}

void engine_set_running(bool value) {
    engine.is_running = value;
}

// SPRITE

ivec2 sprite_get_frame_size(Sprite sprite) {
    return engine.sprites[sprite].frame_size;
}

// INPUT

void input_pump_events() {
    memcpy(engine.mouse_button_previous_state, engine.mouse_button_state, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle quit
        if (event.type == SDL_QUIT) {
            engine.is_running = false;
        }
        // Toggle fullscreen
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
            engine.is_fullscreen = !engine.is_fullscreen;
            if (engine.is_fullscreen) {
                SDL_SetWindowFullscreen(engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                SDL_SetWindowGrab(engine.window, SDL_TRUE);
            } else {
                SDL_SetWindowFullscreen(engine.window, 0);
            }
            break;
        }
        // Capture mouse
        if (SDL_GetWindowGrab(engine.window) == SDL_FALSE) {
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_SetWindowGrab(engine.window, SDL_TRUE);
            }
            // If the mouse is not captured, don't handle any other input
            break;
        }
        // Release mouse
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            SDL_SetWindowGrab(engine.window, SDL_FALSE);
            break;
        }
        // Update mouse position
        if (event.type == SDL_MOUSEMOTION) {
            engine.mouse_position = ivec2(event.motion.x, event.motion.y);
            break;
        }
        // Handle mouse button
        if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)  {
            if (event.button.button < INPUT_MOUSE_BUTTON_COUNT + 1) {
                engine.mouse_button_state[event.button.button - 1] = event.type == SDL_MOUSEBUTTONDOWN;
            }
            break;
        }
        // Text input
        if (event.type == SDL_TEXTINPUT) {
            engine.input_text += std::string(event.text.text);
            if (engine.input_text.length() > engine.input_length_limit) {
                engine.input_text = engine.input_text.substr(0, engine.input_length_limit);
            }
            break;
        }
        if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
            if (engine.input_text.length() > 0) {
                engine.input_text.pop_back();
            }
            break;
        }
    }
}

bool input_is_mouse_button_pressed(uint8_t button) {
    GOLD_ASSERT(button - 1 < INPUT_MOUSE_BUTTON_COUNT);
    return engine.mouse_button_state[button - 1];
}

bool input_is_mouse_button_just_pressed(uint8_t button) {
    GOLD_ASSERT(button - 1 < INPUT_MOUSE_BUTTON_COUNT);
    return engine.mouse_button_state[button - 1] && !engine.mouse_button_previous_state[button - 1];
}

bool input_is_mouse_button_just_released(uint8_t button) {
    GOLD_ASSERT(button - 1 < INPUT_MOUSE_BUTTON_COUNT);
    return !engine.mouse_button_state[button - 1] && engine.mouse_button_previous_state[button - 1];
}

ivec2 input_get_mouse_position() {
    return engine.mouse_position;
}

void input_start_text_input(const rect_t& text_input_rect, size_t input_length_limit) {
    engine.input_length_limit = input_length_limit;
    SDL_Rect sdl_text_input_rect = rect_to_sdl(text_input_rect);
    SDL_SetTextInputRect(&sdl_text_input_rect);
    SDL_StartTextInput();
}

void input_stop_text_input() {
    SDL_StopTextInput();
}

bool input_is_text_input_active() {
    return SDL_IsTextInputActive() == SDL_TRUE;
}

void input_set_text_input_value(const char* value) {
    engine.input_text = std::string(value);
}

const char* input_get_text_input_value() {
    return engine.input_text.c_str();
}

size_t input_get_text_input_length() {
    return engine.input_text.length();
}

// RENDER

void render_clear() {
    SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
    SDL_RenderClear(engine.renderer);
}

void render_present() {
    SDL_RenderPresent(engine.renderer);
}

void render_text(Font font, const char* text, color_t color, ivec2 position, TextAnchor anchor) {
    // Don't render empty strings
    if (text[0] == '\0') {
        return;
    }

    TTF_Font* font_data = engine.fonts[font];
    SDL_Surface* text_surface = TTF_RenderText_Solid(font_data, text, color_to_sdl(color));
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
    if (anchor == TEXT_ANCHOR_BOTTOM_LEFT) {
        dst_rect.y -= dst_rect.h;
    }
    SDL_RenderCopy(engine.renderer, text_texture, &src_rect, &dst_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}

void render_rect(rect_t r, color_t color, bool fill) {
    SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
    SDL_Rect sdl_rect = rect_to_sdl(r);
    if (fill) {
        SDL_RenderFillRect(engine.renderer, &sdl_rect);
    } else {
        SDL_RenderDrawRect(engine.renderer, &sdl_rect);
    }
}

void render_line(ivec2 start, ivec2 end, color_t color) {
    SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(engine.renderer, start.x, start.y, end.x, end.y);
}

void render_map(ivec2 camera_offset, int* tiles, int map_width, int map_height) {
    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    SDL_Rect dst_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    ivec2 base_pos = ivec2(-(camera_offset.x % TILE_SIZE), -(camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(camera_offset.x / TILE_SIZE, camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }
    rect_t screen_rect = rect_t(ivec2(0, 0), ivec2(SCREEN_WIDTH, SCREEN_HEIGHT));

    for (int y = 0; y < max_visible_tiles.y; y++) {
        for (int x = 0; x < max_visible_tiles.x; x++) {
            int map_index = (base_coords.x + x) + ((base_coords.y + y) * map_width);
            src_rect.x = (tiles[map_index] % engine.sprites[SPRITE_TILES].h_frames) * TILE_SIZE;
            src_rect.y = (tiles[map_index] / engine.sprites[SPRITE_TILES].h_frames) * TILE_SIZE;

            dst_rect.x = base_pos.x + (x * TILE_SIZE);
            dst_rect.y = base_pos.y + (y * TILE_SIZE);

            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILES].texture, &src_rect, &dst_rect);
        }
    }
}

void render_sprite(ivec2 camera_offset, Sprite sprite, const ivec2& frame, const vec2& position, bool centered) {
    SDL_Rect src_rect = (SDL_Rect) { 
        .x = frame.x * engine.sprites[sprite].frame_size.x, .y = frame.y * engine.sprites[sprite].frame_size.y, 
        .w = engine.sprites[sprite].frame_size.x, .h = engine.sprites[sprite].frame_size.y };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = position.x.integer_part() - camera_offset.x, .y = position.y.integer_part() - camera_offset.y,
        .w = src_rect.w, .h = src_rect.h
    };
    if (dst_rect.x + dst_rect.w < 0 || dst_rect.x > SCREEN_WIDTH || dst_rect.y + dst_rect.h > SCREEN_HEIGHT || dst_rect.y < 0) {
        return;
    }
    if (centered) {
        dst_rect.x -= (dst_rect.w / 2);
        dst_rect.y -= (dst_rect.h / 2);
    }
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);
}

void render_ui_frame(ivec2 position, ivec2 size) {
    sprite_t& sprite = engine.sprites[SPRITE_UI_FRAME];
    GOLD_ASSERT(size.x % sprite.frame_size.x == 0 && size.y % sprite.frame_size.y == 0);

    SDL_Rect src_rect = (SDL_Rect) {
        .x = 0, .y = 0, .w = sprite.frame_size.x, .h = sprite.frame_size.y
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = 0, .y = 0, .w = src_rect.w, .h = src_rect.h
    };

    for (int y = position.y; y < position.y + size.y; y += sprite.frame_size.y) {
        for (int x = position.x; x < position.x + size.x; x += sprite.frame_size.x) {
            if (x == position.x) {
                src_rect.x = 0;
            } else if (x == position.x + size.x - sprite.frame_size.x) {
                src_rect.x = 2 * sprite.frame_size.x;
            } else {
                src_rect.x = sprite.frame_size.x;
            }
            if (y == position.y) {
                src_rect.y = 0;
            } else if (y == position.y + size.y - sprite.frame_size.y) {
                src_rect.y = 2 * sprite.frame_size.y;
            } else {
                src_rect.y = sprite.frame_size.y;
            }
            dst_rect.x = x;
            dst_rect.y = y;

            SDL_RenderCopy(engine.renderer, sprite.texture, &src_rect, &dst_rect);
        }
    }
}

void render_create_minimap_texture(int map_width, int map_height) {
    engine.minimap_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, map_width, map_height);
}

void render_minimap(ivec2 camera_offset, int* tiles, int* cells, int map_width, int map_height, ivec2 position, ivec2 size) {
    SDL_SetRenderTarget(engine.renderer, engine.minimap_texture);

    SDL_SetRenderDrawColor(engine.renderer, COLOR_SAND.r, COLOR_SAND.g, COLOR_SAND.b, COLOR_SAND.a);
    SDL_RenderClear(engine.renderer);

    SDL_Rect camera_rect = (SDL_Rect) { .x = camera_offset.x / TILE_SIZE, .y = camera_offset.y / TILE_SIZE, .w = SCREEN_WIDTH / TILE_SIZE, .h = (SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE };
    SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(engine.renderer, &camera_rect);

    SDL_SetRenderTarget(engine.renderer, NULL);

    // Render the minimap texture
    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = map_width, .h = map_height };
    SDL_Rect dst_rect = (SDL_Rect) { .x = position.x, .y = position.y, .w = size.x, .h = size.y };
    SDL_RenderCopy(engine.renderer, engine.minimap_texture, &src_rect, &dst_rect);
}
#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include "util.h"
#include "menu.h"
#include "match/match.h"
#include "network.h"
#include "platform.h"
#include "input.h"
#include "container.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef GOLD_DEBUG
    #define RESOURCE_BASE_PATH "../res/"
#else
    #define RESOURCE_BASE_PATH "./res/"
#endif

const double UPDATE_TIME = 1.0 / 60.0;
const uint8_t INPUT_MOUSE_BUTTON_COUNT = 3;
enum TextAnchor {
    TEXT_ANCHOR_TOP_LEFT,
    TEXT_ANCHOR_BOTTOM_LEFT
};
const int RENDER_TEXT_CENTERED = -1;

const SDL_Color COLOR_BLACK = (SDL_Color) { .r = 0, .g = 0, .b = 0, .a = 255 };
const SDL_Color COLOR_WHITE = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 };
const SDL_Color COLOR_SAND = (SDL_Color) { .r = 226, .g = 179, .b = 152, .a = 255 };
const SDL_Color COLOR_SAND_DARK = (SDL_Color) { .r = 204, .g = 162, .b = 139, .a = 255 };
const SDL_Color COLOR_RED = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 };
const SDL_Color COLOR_GREEN = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 };

// FONT

enum Font {
    FONT_HACK,
    FONT_WESTERN8,
    FONT_WESTERN16,
    FONT_WESTERN32,
    FONT_COUNT
};

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

// SPRITE

enum Sprite {
    SPRITE_TILES,
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
    SPRITE_MINER_BUILDING,
    SPRITE_UNIT_MINER,
    SPRITE_BUILDING_HOUSE,
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
        .h_frames = BUTTON_ICON_COUNT,
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
    { SPRITE_MINER_BUILDING, (sprite_params_t) {
        .path = "sprite/unit_miner_building.png",
        .h_frames = 2,
        .v_frames = 1
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "sprite/unit_miner.png",
        .h_frames = 8,
        .v_frames = 4
    }},
    { SPRITE_BUILDING_HOUSE, (sprite_params_t) {
        .path = "sprite/building_house.png",
        .h_frames = 4,
        .v_frames = 1
    }}  
};

struct sprite_t {
    SDL_Texture* texture;
    ivec2 frame_size;
    int h_frames;
    int v_frames;
};

// CURSOR

struct cursor_params_t {    
    const char* path;
    int hot_x;
    int hot_y;
};

const std::unordered_map<uint32_t, cursor_params_t> cursor_params = {
    { CURSOR_DEFAULT, (cursor_params_t) {
        .path = "sprite/ui_cursor.png",
        .hot_x = 0,
        .hot_y = 0
    }}
};

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

    // Resources
    std::vector<TTF_Font*> fonts;
    std::vector<sprite_t> sprites;
    std::vector<SDL_Cursor*> cursors;
    SDL_Texture* minimap_texture;

};
static engine_t engine;

SDL_Rect rect_to_sdl(const rect_t& r) {
    return (SDL_Rect) { .x = r.position.x, .y = r.position.y, .w = r.size.x, .h = r.size.y };
}

bool engine_init(ivec2 window_size);
void engine_quit();
void render_text(Font font, const char* text, SDL_Color color, ivec2 position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);
void render_menu(const menu_t& menu);
void render_match(const match_t& match);

enum Mode {
    MODE_MENU,
    MODE_MATCH
};

int main(int argc, char** argv) {
    std::string logfile_path = "goldrush.log";
    ivec2 window_size = ivec2(SCREEN_WIDTH, SCREEN_HEIGHT);
    for (int argn = 1; argn < argc; argn++) {
        std::string arg = std::string(argv[argn]);
        if (arg == "--logfile" && argn + 1 < argc) {
            argn++;
            logfile_path = std::string(argv[argn]);
        } else if (arg == "--resolution" && argn + 1 < argc) {
            argn++;
            std::string resolution_string = std::string(argv[argn]);

            size_t x_index = resolution_string.find('x');
            if (x_index == std::string::npos) {
                continue;
            }

            bool is_numeric = true;
            std::string width_string = resolution_string.substr(0, x_index);
            for (char c : width_string) {
                if (c < '0' | c > '9') {
                    is_numeric = false;
                }
            }
            std::string height_string = resolution_string.substr(x_index + 1);
            for (char c : height_string) {
                if (c < '0' | c > '9') {
                    is_numeric = false;
                }
            }
            if (!is_numeric) {
                continue;
            }

            window_size = ivec2(std::stoi(width_string.c_str()), std::stoi(height_string.c_str()));
        }
    }

    logger_init(logfile_path.c_str());

    if (!engine_init(window_size)) {
        log_info("Closing logger...");
        logger_quit();
        return 1;
    }
    if (!network_init()) {
        log_info("Closing engine...");
        engine_quit();
        log_info("Closing logger...");
        logger_quit();
        return 1;
    }

    Mode mode = MODE_MENU;
    menu_t menu;
    match_t match;

    double last_time = platform_get_absolute_time();
    double last_second = last_time;
    double update_accumulator = 0.0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    while (engine.is_running) {
        // TIMEKEEP
        double current_time = platform_get_absolute_time();
        update_accumulator += current_time - last_time;
        last_time = current_time;

        if (current_time - last_second >= 1.0) {
            fps = frames;
            frames = 0;
            ups = updates;
            updates = 0;
            last_second += 1.0;
        }
        frames++;

        // INPUT
        memcpy(engine.mouse_button_previous_state, engine.mouse_button_state, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Handle quit
            if (event.type == SDL_QUIT) {
                engine.is_running = false;
            }
            // Toggle fullscreen
            if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_F11 || event.key.keysym.sym == SDLK_f)) {
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
                if (event.button.button - 1 < INPUT_MOUSE_BUTTON_COUNT) {
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
        } // End while SDL_PollEvent()

        // UPDATE
        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;

            // UPDATE
            switch (mode) {
                case MODE_MENU: {
                    menu.update();
                    if (menu.get_mode() == MENU_MODE_MATCH_START) {
                        match.init();
                        mode = MODE_MATCH;
                    }
                    break;
                }
                case MODE_MATCH: {
                    match.update();
                    break;
                }
            }
        }

        // RENDER
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderClear(engine.renderer);

        switch (mode) {
            case MODE_MENU:
                render_menu(menu);
                break;
            case MODE_MATCH:
                render_match(match);
                break;
        }

        char fps_text[16];
        sprintf(fps_text, "FPS: %u", fps);
        char ups_text[16];
        sprintf(ups_text, "UPS: %u", ups);
        render_text(FONT_HACK, fps_text, COLOR_BLACK, ivec2(0, 0));
        render_text(FONT_HACK, ups_text, COLOR_BLACK, ivec2(0, 12));

        SDL_RenderPresent(engine.renderer);
    } // End while running

    network_quit();
    engine_quit();
    logger_quit();

    return 0;
}

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

    engine.cursors.reserve(CURSOR_COUNT);
    for (uint32_t i = 0; i < CURSOR_COUNT; i++) {
        auto it = cursor_params.find(i);
        if (it == cursor_params.end()) {
            log_error("Cursor params not defined for cursor %u", i);
            return false;
        }

        SDL_Surface* cursor_image = IMG_Load((resource_base_path + std::string(it->second.path)).c_str());
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

    SDL_SetCursor(engine.cursors[CURSOR_DEFAULT]);

    engine.minimap_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 128, 128);
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
    for (SDL_Cursor* cursor : engine.cursors) {
        SDL_FreeCursor(cursor);
    }
    SDL_DestroyTexture(engine.minimap_texture);

    SDL_DestroyRenderer(engine.renderer);
    SDL_DestroyWindow(engine.window);

    TTF_Quit();
    SDL_Quit();

    log_info("Application quit gracefully.");
    logger_quit();
}

// INPUT

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

void render_text(Font font, const char* text, SDL_Color color, ivec2 position, TextAnchor anchor) {
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
    if (anchor == TEXT_ANCHOR_BOTTOM_LEFT) {
        dst_rect.y -= dst_rect.h;
    }
    SDL_RenderCopy(engine.renderer, text_texture, &src_rect, &dst_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}

void render_menu(const menu_t& menu) {
    if (menu.mode == MENU_MODE_MATCH_START) {
        return;
    }

    SDL_SetRenderDrawColor(engine.renderer, COLOR_SAND.r, COLOR_SAND.g, COLOR_SAND.b, COLOR_SAND.a);
    SDL_Rect background_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_RenderFillRect(engine.renderer, &background_rect);

    if (menu.mode != MENU_MODE_LOBBY) {
        render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_BLACK, ivec2(RENDER_TEXT_CENTERED, 24));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_BLACK, ivec2(4, SCREEN_HEIGHT - 14));

    if (menu.status_timer > 0) {
        render_text(FONT_WESTERN8, menu.status_text.c_str(), COLOR_RED, ivec2(RENDER_TEXT_CENTERED, TEXT_INPUT_RECT.position.y - 38));
    }

    if (menu.mode == MENU_MODE_MAIN || menu.mode == MENU_MODE_JOIN_IP) {
        const char* prompt = menu.mode == MENU_MODE_MAIN ? "USERNAME" : "IP ADDRESS";
        render_text(FONT_WESTERN8, prompt, COLOR_BLACK, TEXT_INPUT_RECT.position + ivec2(1, -13));
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_Rect text_input_rect = rect_to_sdl(TEXT_INPUT_RECT);
        SDL_RenderDrawRect(engine.renderer, &text_input_rect);

        render_text(FONT_WESTERN16, input_get_text_input_value(), COLOR_BLACK, TEXT_INPUT_RECT.position + ivec2(2, TEXT_INPUT_RECT.size.y - 4), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (menu.mode == MENU_MODE_JOIN_CONNECTING) {
        render_text(FONT_WESTERN16, "Connecting...", COLOR_BLACK, ivec2(RENDER_TEXT_CENTERED, RENDER_TEXT_CENTERED));
    }

    if (menu.mode == MENU_MODE_LOBBY) {
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_Rect playerlist_rect = rect_to_sdl(PLAYERLIST_RECT);
        SDL_RenderDrawRect(engine.renderer, &playerlist_rect);

        uint32_t player_index = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            std::string player_name_text = std::string(player.name);
            if (player.status == PLAYER_STATUS_HOST) {
                player_name_text += ": HOST";
            } else if (player.status == PLAYER_STATUS_NOT_READY) {
                player_name_text += ": NOT READY";
            } else if (player.status == PLAYER_STATUS_READY) {
                player_name_text += ": READY";
            }

            int line_y = 16 * (player_index + 1);
            render_text(FONT_WESTERN8, player_name_text.c_str(), COLOR_BLACK, PLAYERLIST_RECT.position + ivec2(4, line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(engine.renderer, PLAYERLIST_RECT.position.x, PLAYERLIST_RECT.position.y + line_y, PLAYERLIST_RECT.position.x + PLAYERLIST_RECT.size.x - 1, PLAYERLIST_RECT.position.y + line_y);
            player_index++;
        }

        if (network_is_server()) {
            ivec2 side_text_pos = PLAYERLIST_RECT.position + ivec2(PLAYERLIST_RECT.size.x + 2, 0);
            render_text(FONT_WESTERN8, "You are the host.", COLOR_BLACK, side_text_pos);
        }
    }

    for (auto it : menu.buttons) {
        SDL_Color button_color = menu.button_hovered == it.first ? COLOR_WHITE : COLOR_BLACK;
        render_text(FONT_WESTERN16, it.second.text, button_color, it.second.rect.position + ivec2(4, 4));
        SDL_SetRenderDrawColor(engine.renderer, button_color.r, button_color.g, button_color.b, button_color.a);
        SDL_Rect button_rect = rect_to_sdl(it.second.rect);
        SDL_RenderDrawRect(engine.renderer, &button_rect);
    }
}

struct render_sprite_params_t {
    Sprite sprite;
    ivec2 position;
    ivec2 frame;
    uint32_t options;
};
const uint32_t RENDER_SPRITE_FLIP_H = 1;
const uint32_t RENDER_SPRITE_CENTERED = 1 << 1;
const uint32_t RENDER_SPRITE_NO_CULL = 1 << 2;

void render_sprite(Sprite sprite, const ivec2& frame, const ivec2& position, uint32_t options = 0) {
    GOLD_ASSERT(frame.x < engine.sprites[sprite].h_frames && frame.y < engine.sprites[sprite].v_frames);

    bool flip_h = (options & RENDER_SPRITE_FLIP_H) == RENDER_SPRITE_FLIP_H;
    bool centered = (options & RENDER_SPRITE_CENTERED) == RENDER_SPRITE_CENTERED;
    bool cull = !((options & RENDER_SPRITE_NO_CULL) == RENDER_SPRITE_NO_CULL);

    SDL_Rect src_rect = (SDL_Rect) { 
        .x = frame.x * engine.sprites[sprite].frame_size.x, .y = frame.y * engine.sprites[sprite].frame_size.y, 
        .w = engine.sprites[sprite].frame_size.x, .h = engine.sprites[sprite].frame_size.y };
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

    SDL_RenderCopyEx(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect, 0.0, NULL, flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

int ysort_partition(render_sprite_params_t* params, int low, int high) {
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

void ysort(render_sprite_params_t* params, int low, int high) {
    if (low < high) {
        int partition_index = ysort_partition(params, low, high);
        ysort(params, low, partition_index - 1);
        ysort(params, partition_index + 1, high);
    }
}

void render_match(const match_t& match) {
    uint8_t current_player_id = network_get_player_id();

    // Render map
    SDL_Rect tile_src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    SDL_Rect tile_dst_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    ivec2 base_pos = ivec2(-(match.camera_offset.x % TILE_SIZE), -(match.camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(match.camera_offset.x / TILE_SIZE, match.camera_offset.y / TILE_SIZE);
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
            int map_index = (base_coords.x + x) + ((base_coords.y + y) * match.map_width);
            tile_src_rect.x = (match.map_tiles[map_index] % engine.sprites[SPRITE_TILES].h_frames) * TILE_SIZE;
            tile_src_rect.y = (match.map_tiles[map_index] / engine.sprites[SPRITE_TILES].h_frames) * TILE_SIZE;

            tile_dst_rect.x = base_pos.x + (x * TILE_SIZE);
            tile_dst_rect.y = base_pos.y + (y * TILE_SIZE);

            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILES].texture, &tile_src_rect, &tile_dst_rect);
        }
    }

    // Select rings and healthbars
    static const int HEALTHBAR_HEIGHT = 4;
    static const int HEALTHBAR_PADDING = 2;
    static const int BUILDING_HEALTHBAR_PADDING = 5;
    if (match.is_selecting_building) {
        uint32_t building_index = match.buildings[current_player_id].get_index_of(match.selected_building_id);
        if (building_index != id_array<building_t>::INDEX_INVALID) {
            const building_t& building = match.buildings[current_player_id][building_index];
            rect_t building_rect = match.building_get_rect(building);
            render_sprite(SPRITE_SELECT_RING_HOUSE, ivec2(0, 0), building_rect.position + (building_rect.size / 2) - match.camera_offset, RENDER_SPRITE_CENTERED);

            // Determine healthbar rect
            ivec2 building_render_pos = building_rect.position - match.camera_offset;
            SDL_Rect healthbar_rect = (SDL_Rect) { .x = building_render_pos.x, .y = building_render_pos.y + building_rect.size.y + BUILDING_HEALTHBAR_PADDING, .w = building_rect.size.x, .h = HEALTHBAR_HEIGHT };
            SDL_Rect healthbar_subrect = healthbar_rect;
            healthbar_subrect.w = (healthbar_rect.w * building.health) / building_data.at(building.type).max_health;

            // Cull the healthbar
            if (!(healthbar_rect.x + healthbar_rect.w < 0 || healthbar_rect.y + healthbar_rect.h < 0 || healthbar_rect.x >= SCREEN_WIDTH || healthbar_rect.y >= SCREEN_HEIGHT)) {
                // Render the healthbar
                SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
                SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
                SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
                SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
            }
        }
    } else {
        for (const unit_t& unit : match.units[current_player_id]) {
            if (unit.is_selected) {
                render_sprite(SPRITE_SELECT_RING, ivec2(0, 0), unit.position.to_ivec2() - match.camera_offset, RENDER_SPRITE_CENTERED);

                // Determine healthbar rect
                ivec2 unit_render_pos = unit.position.to_ivec2() - match.camera_offset;
                ivec2 unit_render_size = engine.sprites[SPRITE_UNIT_MINER].frame_size;
                SDL_Rect healthbar_rect = (SDL_Rect) { .x = unit_render_pos.x - (unit_render_size.x / 2), .y = unit_render_pos.y + (unit_render_size.y / 2) + HEALTHBAR_PADDING, .w = unit_render_size.x, .h = HEALTHBAR_HEIGHT };
                SDL_Rect healthbar_subrect = healthbar_rect;
                healthbar_subrect.w = (healthbar_rect.w * unit.health) / unit_data.at(unit.type).max_health;

                // Cull the healthbar
                if (healthbar_rect.x + healthbar_rect.w < 0 || healthbar_rect.y + healthbar_rect.h < 0 || healthbar_rect.x >= SCREEN_WIDTH || healthbar_rect.y >= SCREEN_HEIGHT ) {
                    continue;
                }

                // Render the healthbar
                SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
                SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
                SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
                SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
            }
        }
    }

    // UI move
    if (match.ui_move_animation.is_playing) {
        render_sprite(SPRITE_UI_MOVE, match.ui_move_animation.frame, match.ui_move_position - match.camera_offset, RENDER_SPRITE_CENTERED);
    }

    // Y-Sort
    static render_sprite_params_t ysorted[(MAX_UNITS + MAX_BUILDINGS) * MAX_PLAYERS];
    uint32_t ysorted_count = 0;

    // Buildings
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (const building_t& building : match.buildings[player_id]) {
            int sprite = SPRITE_BUILDING_HOUSE + building.type;
            GOLD_ASSERT(sprite < SPRITE_COUNT);

            // Cull the building sprite
            ivec2 building_render_pos = (building.cell * TILE_SIZE) - match.camera_offset;
            ivec2 building_render_size = engine.sprites[sprite].frame_size;
            if (building_render_pos.x + building_render_size.x < 0 || building_render_pos.x > SCREEN_WIDTH || building_render_pos.y + building_render_size.y < 0 || building_render_pos.y > SCREEN_HEIGHT) {
                continue;
            }

            int hframe = building.is_finished ? 3 : ((3 * building.health) / building_data.at(building.type).max_health);
            ysorted[ysorted_count] = (render_sprite_params_t) {
                .sprite = (Sprite)sprite,
                .position = building_render_pos,
                .frame = ivec2(hframe, 0),
                .options = RENDER_SPRITE_NO_CULL
            };
            ysorted_count++;
        }
    }

    // Units
    for (uint32_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (const unit_t& unit : match.units[player_id]) {
            ivec2 unit_render_pos = unit.position.to_ivec2() - match.camera_offset;
            ivec2 unit_render_size = engine.sprites[SPRITE_UNIT_MINER].frame_size;

            // Cull the unit sprite
            if (unit_render_pos.x + unit_render_size.x < 0 || unit_render_pos.x > SCREEN_WIDTH || unit_render_pos.y + unit_render_size.y < 0 || unit_render_pos.y > SCREEN_HEIGHT) {
                continue;
            }

            ysorted[ysorted_count] = (render_sprite_params_t) { 
                .sprite = SPRITE_UNIT_MINER, 
                .position = unit_render_pos, 
                .frame = unit.animation.frame ,
                .options = RENDER_SPRITE_CENTERED | RENDER_SPRITE_NO_CULL
            };
            if (unit.mode == UNIT_MODE_BUILD) {
                const building_t& building = match.buildings[player_id][match.buildings->get_index_of(unit.building_id)];
                const building_data_t& data = building_data.at(building.type);
                int hframe = ((3 * building.health) / data.max_health);
                ysorted[ysorted_count].sprite = SPRITE_MINER_BUILDING;

                ivec2 building_position = (building.cell * TILE_SIZE) - match.camera_offset;
                ysorted[ysorted_count].position = building_position + data.builder_positions(hframe);

                ysorted[ysorted_count].options &= ~RENDER_SPRITE_CENTERED;
                if (hframe == 1) {
                    ysorted[ysorted_count].options |= RENDER_SPRITE_FLIP_H;
                }
            }
            ysorted_count++;
        }
    }
    ysort(ysorted, 0, ysorted_count - 1);
    for (uint32_t i = 0; i < ysorted_count; i++) {
        const render_sprite_params_t& params= ysorted[i];
        render_sprite(params.sprite, params.frame, params.position, params.options);
    }

    // Building placement
    if (match.ui_mode == UI_MODE_BUILDING_PLACE && match.ui_building_cell.x != -1) {
        int sprite = SPRITE_BUILDING_HOUSE + match.ui_building_type;
        GOLD_ASSERT(sprite < SPRITE_COUNT);
        render_sprite((Sprite)(sprite), ivec2(3, 0), (match.ui_building_cell * TILE_SIZE) - match.camera_offset);

        auto it = building_data.find(match.ui_building_type);
        bool is_placement_out_of_bounds = match.ui_building_cell.x + it->second.cell_width > match.map_width || 
                                          match.ui_building_cell.y + it->second.cell_height > match.map_height;
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
        for (int y = match.ui_building_cell.y; y < match.ui_building_cell.y + it->second.cell_height; y++) {
            for (int x = match.ui_building_cell.x; x < match.ui_building_cell.x + it->second.cell_width; x++) {
                bool is_cell_green;
                if (is_placement_out_of_bounds) {
                    is_cell_green = false;
                } else {
                    is_cell_green = !match.cell_is_blocked(ivec2(x, y));
                }

                SDL_Color cell_color = is_cell_green ? COLOR_GREEN : COLOR_RED;
                SDL_SetRenderDrawColor(engine.renderer, cell_color.r, cell_color.g, cell_color.b, 128);
                SDL_Rect cell_rect = (SDL_Rect) { .x = (x * TILE_SIZE) - match.camera_offset.x, .y = (y * TILE_SIZE) - match.camera_offset.y, .w = TILE_SIZE, .h = TILE_SIZE };
                SDL_RenderFillRect(engine.renderer, &cell_rect);
            }
        }
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);
    }

    // Select rect
    if (match.ui_mode == UI_MODE_SELECTING) {
        SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
        SDL_Rect select_rect = (SDL_Rect) { .x = match.select_rect.position.x - match.camera_offset.x, .y = match.select_rect.position.y - match.camera_offset.y, .w = match.select_rect.size.x, .h = match.select_rect.size.y };
        SDL_RenderDrawRect(engine.renderer, &select_rect);
    }

    // UI frames
    render_sprite(SPRITE_UI_MINIMAP, ivec2(0, 0), ivec2(0, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_MINIMAP].frame_size.y));
    render_sprite(SPRITE_UI_FRAME_BOTTOM, ivec2(0, 0), ivec2(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x, SCREEN_HEIGHT - UI_HEIGHT));
    render_sprite(SPRITE_UI_FRAME_BUTTONS, ivec2(0, 0), ivec2(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x + engine.sprites[SPRITE_UI_FRAME_BOTTOM].frame_size.x, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y));

    // UI Buttons
    for (int i = 0; i < 6; i++) {
        if (!match.ui_buttons[i].enabled) {
            continue;
        }

        bool is_button_hovered = match.ui_button_hovered == i;
        bool is_button_pressed = is_button_hovered && input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
        ivec2 offset = is_button_pressed ? ivec2(1, 1) : (is_button_hovered ? ivec2(0, -1) : ivec2(0, 0));
        render_sprite(SPRITE_UI_BUTTON, ivec2(is_button_hovered && !is_button_pressed ? 1 : 0, 0), match.ui_buttons[i].rect.position + offset);
        render_sprite(SPRITE_UI_BUTTON_ICON, ivec2(match.ui_buttons[i].icon, is_button_hovered && !is_button_pressed ? 1 : 0), match.ui_buttons[i].rect.position + offset);
    }

    // UI Status message
    if (!match.ui_status_timer.is_stopped) {
        render_text(FONT_HACK, match.ui_status_message.c_str(), COLOR_WHITE, ivec2(RENDER_TEXT_CENTERED, SCREEN_HEIGHT - 128));
    }

    // Resource counters
    char gold_text[8];
    sprintf(gold_text, "%u", match.player_gold[current_player_id]);
    render_text(FONT_WESTERN8, gold_text, COLOR_BLACK, ivec2(SCREEN_WIDTH - 64 + 18, 4));
    render_sprite(SPRITE_UI_GOLD, ivec2(0, 0), ivec2(SCREEN_WIDTH - 64, 2));

    // Render minimap
    SDL_SetRenderTarget(engine.renderer, engine.minimap_texture);

    SDL_SetRenderDrawColor(engine.renderer, COLOR_SAND_DARK.r, COLOR_SAND_DARK.g, COLOR_SAND_DARK.b, COLOR_SAND_DARK.a);
    SDL_RenderClear(engine.renderer);

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        SDL_SetRenderDrawColor(engine.renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, COLOR_GREEN.a);
        for (const building_t& building : match.buildings[player_id]) {
            SDL_Rect building_rect = (SDL_Rect) { 
                .x = (building.cell.x * MINIMAP_RECT.size.x) / match.map_width,
                .y = (building.cell.y * MINIMAP_RECT.size.y) / match.map_height, 
                .w = 2 * building_data.at(building.type).cell_width,
                .h = 2 * building_data.at(building.type).cell_height
            };
            SDL_RenderFillRect(engine.renderer, &building_rect);
        }
        for (const unit_t& unit : match.units[player_id]) {
            SDL_Rect unit_rect = (SDL_Rect) { .x = (unit.cell.x * MINIMAP_RECT.size.x) / match.map_width, .y = (unit.cell.y * MINIMAP_RECT.size.y) / match.map_height, .w = 2, .h = 2 };
            SDL_RenderFillRect(engine.renderer, &unit_rect);
        }
    } 

    SDL_Rect camera_rect = (SDL_Rect) { 
        .x = ((match.camera_offset.x / TILE_SIZE) * MINIMAP_RECT.size.x) / match.map_width, 
        .y = ((match.camera_offset.y / TILE_SIZE) * MINIMAP_RECT.size.y) / match.map_height, 
        .w = ((SCREEN_WIDTH / TILE_SIZE) * MINIMAP_RECT.size.x) / match.map_width, 
        .h = 1 + (((SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE) * MINIMAP_RECT.size.y) / match.map_height };
    SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(engine.renderer, &camera_rect);

    SDL_SetRenderTarget(engine.renderer, NULL);

    // Render the minimap texture
    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = MINIMAP_RECT.size.x, .h = MINIMAP_RECT.size.y };
    SDL_Rect dst_rect = (SDL_Rect) { .x = MINIMAP_RECT.position.x, .y = MINIMAP_RECT.position.y, .w = MINIMAP_RECT.size.x, .h = MINIMAP_RECT.size.y };
    SDL_RenderCopy(engine.renderer, engine.minimap_texture, &src_rect, &dst_rect);
}
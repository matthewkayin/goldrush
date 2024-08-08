#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include "util.h"
#include "menu.h"
#include "match.h"
#include "sprite.h"
#include "network.h"
#include "platform.h"
#include "input.h"
#include "container.h"
#include "lcg.h"
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
const SDL_Color COLOR_OFFBLACK = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 };
const SDL_Color COLOR_WHITE = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 };
const SDL_Color COLOR_SAND = (SDL_Color) { .r = 226, .g = 179, .b = 152, .a = 255 };
const SDL_Color COLOR_SAND_DARK = (SDL_Color) { .r = 204, .g = 162, .b = 139, .a = 255 };
const SDL_Color COLOR_RED = (SDL_Color) { .r = 186, .g = 97, .b = 95, .a = 255 };
const SDL_Color COLOR_GREEN = (SDL_Color) { .r = 123, .g = 174, .b = 121, .a = 255 };
const SDL_Color COLOR_GOLD = (SDL_Color) { .r = 238, .g = 209, .b = 158, .a = 255 };

enum RecolorName {
    RECOLOR_BLUE,
    RECOLOR_RED,
    RECOLOR_GREEN,
    RECOLOR_ORANGE,
    RECOLOR_TURQUOISE,
    RECOLOR_PURPLE,
    RECOLOR_BLACK,
    RECOLOR_WHITE,
    RECOLOR_NONE
};
const std::unordered_map<uint32_t, SDL_Color> COLOR_PLAYER = {
    { RECOLOR_BLUE, (SDL_Color) { .r = 64, .g = 74, .b = 94, .a = 255 } },
    { RECOLOR_RED, (SDL_Color) { .r = 158, .g = 83, .b = 82, .a = 255 } },
    { RECOLOR_GREEN, (SDL_Color) { .r = 63, .g = 112, .b = 95, .a = 255 } },
    { RECOLOR_ORANGE, (SDL_Color) { .r = 191, .g = 136, .b = 112, .a = 255 } },
    { RECOLOR_TURQUOISE, (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 } },
    { RECOLOR_PURPLE, (SDL_Color) { .r = 91, .g = 75, .b = 97, .a = 255 } },
    { RECOLOR_BLACK, (SDL_Color) { .r = 61, .g = 57, .b = 69, .a = 255 } },
    { RECOLOR_WHITE, (SDL_Color) { .r = 196, .g = 194, .b = 173, .a = 255 } }
};
const SDL_Color RECOLOR_REF = (SDL_Color) { .r = 255, .g = 0, .b = 255, .a = 255 };

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

struct sprite_t {
    union {
        SDL_Texture* texture;
        SDL_Texture* colored_texture[MAX_PLAYERS];
    };
    xy frame_size;
    int hframes;
    int vframes;
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
    }},
    { CURSOR_TARGET, (cursor_params_t) {
        .path = "sprite/ui_cursor_target.png",
        .hot_x = 9,
        .hot_y = 9
    }}
};

// Hotkeys

const std::unordered_map<SDL_Keycode, Key> keymap = {
    { SDLK_1, KEY_1 },
    { SDLK_2, KEY_2 },
    { SDLK_3, KEY_3 },
    { SDLK_4, KEY_4 },
    { SDLK_5, KEY_5 },
    { SDLK_6, KEY_6 },
    { SDLK_7, KEY_7 },
    { SDLK_8, KEY_8 },
    { SDLK_9, KEY_9 },
    { SDLK_LCTRL, KEY_CTRL },
    { SDLK_LSHIFT, KEY_SHIFT }
};
const std::unordered_map<UiButton, SDL_Keycode> hotkey_keymap = {
    { UI_BUTTON_MOVE, SDLK_v },
    { UI_BUTTON_STOP, SDLK_s },
    { UI_BUTTON_ATTACK, SDLK_a },
    { UI_BUTTON_BUILD, SDLK_b },
    { UI_BUTTON_CANCEL, SDLK_ESCAPE },
    { UI_BUTTON_BUILD_HOUSE, SDLK_e },
    { UI_BUTTON_BUILD_CAMP, SDLK_c },
    { UI_BUTTON_BUILD_SALOON, SDLK_s },
    { UI_BUTTON_UNIT_MINER, SDLK_r }
};
std::unordered_map<SDL_Keycode, std::vector<UiButton>> hotkeys;

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool is_running = false;
    bool is_fullscreen = false;
    Cursor current_cursor;

    // Input state
    bool mouse_button_state[INPUT_MOUSE_BUTTON_COUNT];
    bool mouse_button_previous_state[INPUT_MOUSE_BUTTON_COUNT];
    xy mouse_position;
    bool hotkey_state[UI_BUTTON_COUNT];
    bool hotkey_state_previous[UI_BUTTON_COUNT];
    bool key_state[KEY_COUNT];
    bool key_state_previous[KEY_COUNT];
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

bool engine_init(xy window_size);
void engine_quit();
void render_text(Font font, const char* text, SDL_Color color, xy position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);
void render_menu(const menu_state_t& menu);
void render_match(const match_state_t& state);

enum Mode {
    MODE_MENU,
    MODE_MATCH
};

int main(int argc, char** argv) {
    std::string logfile_path = "goldrush.log";
    xy window_size = xy(SCREEN_WIDTH, SCREEN_HEIGHT);
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

            window_size = xy(std::stoi(width_string.c_str()), std::stoi(height_string.c_str()));
        }
    }

    logger_init(logfile_path.c_str());
    log_info("opened with window size %vi", &window_size);

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
    menu_state_t menu_state = menu_init();
    match_state_t match_state;

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
        memcpy(engine.hotkey_state_previous, engine.hotkey_state, UI_BUTTON_COUNT * sizeof(bool));
        memcpy(engine.key_state_previous, engine.key_state, KEY_COUNT * sizeof(bool));

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
                    log_info("catch mouse.");
                    SDL_SetWindowGrab(engine.window, SDL_TRUE);
                }
                // If the mouse is not captured, don't handle any other input
                break;
            }
            // Release mouse
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
                log_info("release mouse.");
                SDL_SetWindowGrab(engine.window, SDL_FALSE);
                break;
            }
            // Update mouse position
            if (event.type == SDL_MOUSEMOTION) {
                engine.mouse_position = xy(event.motion.x, event.motion.y);
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
            if (mode == MODE_MATCH && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)) {
                auto keymap_it = keymap.find(event.key.keysym.sym);
                if (keymap_it != keymap.end()) {
                    engine.key_state[keymap_it->second] = event.type == SDL_KEYDOWN;
                    break;
                }

                auto hotkey_it = hotkeys.find(event.key.keysym.sym);
                if (hotkey_it == hotkeys.end()) {
                    continue;
                }
                for (UiButton button : hotkey_it->second) {
                    engine.hotkey_state[button] = event.type == SDL_KEYDOWN;
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
                    menu_update(menu_state);
                    if (menu_state.mode == MENU_MODE_MATCH_START) {
                        match_state = match_init();
                        mode = MODE_MATCH;
                    }
                    break;
                }
                case MODE_MATCH: {
                    match_update(match_state);
                    break;
                }
            }
        }

        // RENDER
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderClear(engine.renderer);

        switch (mode) {
            case MODE_MENU:
                render_menu(menu_state);
                break;
            case MODE_MATCH:
                render_match(match_state);
                break;
        }

        char fps_text[16];
        sprintf(fps_text, "FPS: %u", fps);
        char ups_text[16];
        sprintf(ups_text, "UPS: %u", ups);
        render_text(FONT_HACK, fps_text, COLOR_WHITE, xy(0, 0));
        render_text(FONT_HACK, ups_text, COLOR_WHITE, xy(0, 12));

        SDL_RenderPresent(engine.renderer);
    } // End while running

    network_quit();
    engine_quit();
    logger_quit();

    return 0;
}

bool engine_init(xy window_size) {
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
    engine.mouse_position = xy(0, 0);
    memset(engine.hotkey_state, 0, UI_BUTTON_COUNT * sizeof(bool));
    memset(engine.hotkey_state_previous, 0, UI_BUTTON_COUNT * sizeof(bool));
    memset(engine.key_state, 0, KEY_COUNT * sizeof(bool));
    memset(engine.key_state_previous, 0, KEY_COUNT * sizeof(bool));

    // Init hotkey sets
    for (int buttonset = 0; buttonset < UI_BUTTONSET_COUNT; buttonset++) {
        for (int button_index = 0; button_index < 6; button_index++) {
            UiButton button = UI_BUTTONS.at((UiButtonset)buttonset)[button_index];
            if (button == UI_BUTTON_NONE) {
                continue;
            }

            SDL_Keycode key = hotkey_keymap.at(button);
            auto it = hotkeys.find(key);
            if (it == hotkeys.end()) {
                hotkeys[key] = std::vector<UiButton>();
            }
            hotkeys[key].push_back(button);
        }
    }

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
        auto sprite_params_it = SPRITE_PARAMS.find(i);
        if (sprite_params_it == SPRITE_PARAMS.end()) {
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

        if (sprite_params_it->second.recolor) {
            // Re-color texture creation
            uint32_t* sprite_pixels = (uint32_t*)sprite_surface->pixels;
            uint32_t reference_pixel = SDL_MapRGBA(sprite_surface->format, RECOLOR_REF.r, RECOLOR_REF.g, RECOLOR_REF.b, RECOLOR_REF.a);

            for (uint32_t player_color = 0; player_color < MAX_PLAYERS; player_color++) {
                SDL_Surface* recolored_surface = SDL_CreateRGBSurfaceWithFormat(0, sprite_surface->w, sprite_surface->h, 32, sprite_surface->format->format);
                uint32_t* recolor_pixels = (uint32_t*)recolored_surface->pixels;
                SDL_LockSurface(recolored_surface);

                const SDL_Color& color_player = COLOR_PLAYER.at(player_color);
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
        } else {
            // Non-recolor texture creation
            sprite.texture = SDL_CreateTextureFromSurface(engine.renderer, sprite_surface);
            if (sprite.texture == NULL) {
                log_error("Error creating texture for sprite %s: %s", sprite_params_it->second.path, SDL_GetError());
                return false;
            }
        }

        if (sprite_params_it->second.hframes == -1) {
            sprite.frame_size = xy(TILE_SIZE, TILE_SIZE);
            sprite.hframes = sprite_surface->w / sprite.frame_size.x;
            sprite.vframes = sprite_surface->h / sprite.frame_size.y;
        } else {
            sprite.hframes = sprite_params_it->second.hframes;
            sprite.vframes = sprite_params_it->second.vframes;
            sprite.frame_size = xy(sprite_surface->w / sprite.hframes, sprite_surface->h / sprite.vframes);
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

    engine.current_cursor = CURSOR_DEFAULT;
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

xy input_get_mouse_position() {
    return engine.mouse_position;
}

bool input_is_ui_hotkey_just_pressed(UiButton button) {
    return engine.hotkey_state[button] && !engine.hotkey_state_previous[button];
}

bool input_is_key_just_pressed(Key key) {
    return engine.key_state[key] && !engine.key_state_previous[key];
}

bool input_is_key_pressed(Key key) {
    return engine.key_state[key];
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
    if (anchor == TEXT_ANCHOR_BOTTOM_LEFT) {
        dst_rect.y -= dst_rect.h;
    }
    SDL_RenderCopy(engine.renderer, text_texture, &src_rect, &dst_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
}

void render_menu(const menu_state_t& menu) {
    if (menu.mode == MENU_MODE_MATCH_START) {
        return;
    }

    SDL_SetRenderDrawColor(engine.renderer, COLOR_SAND.r, COLOR_SAND.g, COLOR_SAND.b, COLOR_SAND.a);
    SDL_Rect background_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_RenderFillRect(engine.renderer, &background_rect);

    if (menu.mode != MENU_MODE_LOBBY) {
        render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_BLACK, xy(RENDER_TEXT_CENTERED, 24));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_BLACK, xy(4, SCREEN_HEIGHT - 14));

    if (menu.status_timer > 0) {
        render_text(FONT_WESTERN8, menu.status_text.c_str(), COLOR_RED, xy(RENDER_TEXT_CENTERED, TEXT_INPUT_RECT.position.y - 38));
    }

    if (menu.mode == MENU_MODE_MAIN || menu.mode == MENU_MODE_JOIN_IP) {
        const char* prompt = menu.mode == MENU_MODE_MAIN ? "USERNAME" : "IP ADDRESS";
        render_text(FONT_WESTERN8, prompt, COLOR_BLACK, TEXT_INPUT_RECT.position + xy(1, -13));
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_Rect text_input_rect = rect_to_sdl(TEXT_INPUT_RECT);
        SDL_RenderDrawRect(engine.renderer, &text_input_rect);

        render_text(FONT_WESTERN16, input_get_text_input_value(), COLOR_BLACK, TEXT_INPUT_RECT.position + xy(2, TEXT_INPUT_RECT.size.y - 4), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (menu.mode == MENU_MODE_JOIN_CONNECTING) {
        render_text(FONT_WESTERN16, "Connecting...", COLOR_BLACK, xy(RENDER_TEXT_CENTERED, RENDER_TEXT_CENTERED));
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
            render_text(FONT_WESTERN8, player_name_text.c_str(), COLOR_BLACK, PLAYERLIST_RECT.position + xy(4, line_y - 2), TEXT_ANCHOR_BOTTOM_LEFT);
            SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
            SDL_RenderDrawLine(engine.renderer, PLAYERLIST_RECT.position.x, PLAYERLIST_RECT.position.y + line_y, PLAYERLIST_RECT.position.x + PLAYERLIST_RECT.size.x - 1, PLAYERLIST_RECT.position.y + line_y);
            player_index++;
        }

        if (network_is_server()) {
            xy side_text_pos = PLAYERLIST_RECT.position + xy(PLAYERLIST_RECT.size.x + 2, 0);
            render_text(FONT_WESTERN8, "You are the host.", COLOR_BLACK, side_text_pos);
        }
    }

    for (auto it : menu.buttons) {
        SDL_Color button_color = menu.button_hovered == it.first ? COLOR_WHITE : COLOR_BLACK;
        render_text(FONT_WESTERN16, it.second.text, button_color, it.second.rect.position + xy(4, 4));
        SDL_SetRenderDrawColor(engine.renderer, button_color.r, button_color.g, button_color.b, button_color.a);
        SDL_Rect button_rect = rect_to_sdl(it.second.rect);
        SDL_RenderDrawRect(engine.renderer, &button_rect);
    }
}

struct render_sprite_params_t {
    Sprite sprite;
    xy position;
    xy frame;
    uint32_t options;
    RecolorName recolor_name;
};
const uint32_t RENDER_SPRITE_FLIP_H = 1;
const uint32_t RENDER_SPRITE_CENTERED = 1 << 1;
const uint32_t RENDER_SPRITE_NO_CULL = 1 << 2;

void render_sprite(Sprite sprite, const xy& frame, const xy& position, uint32_t options = 0, RecolorName recolor_name = RECOLOR_NONE) {
    GOLD_ASSERT(frame.x < engine.sprites[sprite].hframes && frame.y < engine.sprites[sprite].vframes);

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

    SDL_Texture* texture;
    if (recolor_name == RECOLOR_NONE) {
        GOLD_ASSERT(SPRITE_PARAMS.at(sprite).recolor == false);
        texture = engine.sprites[sprite].texture;
    } else {
        GOLD_ASSERT(SPRITE_PARAMS.at(sprite).recolor == true);
        texture = engine.sprites[sprite].colored_texture[recolor_name];
    }
    SDL_RenderCopyEx(engine.renderer, texture, &src_rect, &dst_rect, 0.0, NULL, flip_h ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
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

bool is_cell_revealed(const match_state_t& state, xy cell, xy size) {
    for (int x = cell.x; x < cell.x + size.x; x++) {
        for (int y = cell.y; y < cell.y + size.y; y++) {
            if (state.map_fog[x + (state.map_width * y)] == FOG_REVEALED) {
                return true;
            }
        }
    }

    return false;
}

void render_match(const match_state_t& state) {
    Cursor expected_cursor = CURSOR_DEFAULT;
    if (state.ui_mode == UI_MODE_ATTACK_MOVE && (!ui_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position()))) {
        expected_cursor = CURSOR_TARGET;
    }
    if (engine.current_cursor != expected_cursor) {
        engine.current_cursor = expected_cursor;
        SDL_SetCursor(engine.cursors[engine.current_cursor]);
    }

    std::vector<render_sprite_params_t> ysorted;

    // Render map
    SDL_Rect tile_src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    SDL_Rect tile_dst_rect = (SDL_Rect) { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
    xy base_pos = xy(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
    xy base_coords = xy(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
    xy max_visible_tiles = xy(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }
    rect_t screen_rect = rect_t(xy(0, 0), xy(SCREEN_WIDTH, SCREEN_HEIGHT));

    SDL_SetRenderDrawColor(engine.renderer, 255, 0, 0, 255);
    for (int y = 0; y < max_visible_tiles.y; y++) {
        for (int x = 0; x < max_visible_tiles.x; x++) {
            int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.map_width);
            tile_src_rect.x = (state.map_tiles[map_index] % engine.sprites[SPRITE_TILES].hframes) * TILE_SIZE;
            tile_src_rect.y = (state.map_tiles[map_index] / engine.sprites[SPRITE_TILES].hframes) * TILE_SIZE;

            tile_dst_rect.x = base_pos.x + (x * TILE_SIZE);
            tile_dst_rect.y = base_pos.y + (y * TILE_SIZE);

            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILES].texture, &tile_src_rect, &tile_dst_rect);

            // Render gold
            uint32_t map_cell = map_get_cell(state, xy(base_coords.x + x, base_coords.y + y)).type;
            if (map_cell >= CELL_GOLD1 && map_cell <= CELL_GOLD3) {
                ysorted.push_back((render_sprite_params_t) {
                    .sprite = SPRITE_TILE_GOLD,
                    .position = xy(tile_dst_rect.x, tile_dst_rect.y),
                    .frame = xy(map_cell - CELL_GOLD1, 0),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_name = RECOLOR_NONE
                });
            }
        }
    }

    // Select rings and healthbars
    static const int HEALTHBAR_HEIGHT = 4;
    static const int HEALTHBAR_PADDING = 2;
    static const int BUILDING_HEALTHBAR_PADDING = 5;
    if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        uint32_t building_index = state.buildings.get_index_of(state.selection.ids[0]);
        if (building_index != INDEX_INVALID) {
            const building_t& building = state.buildings[building_index];
            rect_t building_rect = building_get_rect(building);
            render_sprite(building_get_select_ring(building.type), xy(0, 0), building_rect.position + (building_rect.size / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);

            // Determine healthbar rect
            xy building_render_pos = building_rect.position - state.camera_offset;
            SDL_Rect healthbar_rect = (SDL_Rect) { 
                .x = building_render_pos.x, 
                .y = building_render_pos.y + building_rect.size.y + BUILDING_HEALTHBAR_PADDING, 
                .w = building_rect.size.x, 
                .h = HEALTHBAR_HEIGHT };
            SDL_Rect healthbar_subrect = healthbar_rect;
            healthbar_subrect.w = (healthbar_rect.w * building.health) / 
                                    BUILDING_DATA.at(building.type).max_health;

            // Cull the healthbar
            if (!(healthbar_rect.x + healthbar_rect.w < 0 || 
                  healthbar_rect.y + healthbar_rect.h < 0 || 
                  healthbar_rect.x >= SCREEN_WIDTH || 
                  healthbar_rect.y >= SCREEN_HEIGHT)) {
                // Render the healthbar
                SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
                SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
                SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
                SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
                SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
            }
        }
    } else {
        for (entity_id unit_id : state.selection.ids) {
            uint32_t index = state.units.get_index_of(unit_id);
            if (index == INDEX_INVALID) {
                continue;
            }
            const unit_t& unit = state.units[index];

            xy unit_render_pos = unit.position.to_xy() - state.camera_offset;
            render_sprite(SPRITE_SELECT_RING, xy(0, 0), unit_render_pos, RENDER_SPRITE_CENTERED);

            // Determine healthbar rect
            xy unit_render_size = engine.sprites[SPRITE_UNIT_MINER].frame_size;
            SDL_Rect healthbar_rect = (SDL_Rect) { .x = unit_render_pos.x - (unit_render_size.x / 2), .y = unit_render_pos.y + (unit_render_size.y / 2) + HEALTHBAR_PADDING, .w = unit_render_size.x, .h = HEALTHBAR_HEIGHT };
            SDL_Rect healthbar_subrect = healthbar_rect;
            healthbar_subrect.w = (healthbar_rect.w * unit.health) / UNIT_DATA.at(unit.type).max_health;

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

    // UI move animation
    if (animation_is_playing(state.ui_move_animation) && map_get_fog(state, state.ui_move_position / TILE_SIZE) != FOG_HIDDEN) {
        if (state.ui_move_cell.type == CELL_EMPTY) {
            render_sprite(SPRITE_UI_MOVE, state.ui_move_animation.frame, state.ui_move_position - state.camera_offset, RENDER_SPRITE_CENTERED);
        } else if (state.ui_move_animation.frame.x % 2 == 0) {
            entity_id id = (entity_id)(state.ui_move_cell.value);
            switch (state.ui_move_cell.type) {
                case CELL_UNIT: {
                    uint32_t unit_index = state.units.get_index_of(id);
                    if (unit_index != INDEX_INVALID) {
                        Sprite sprite = state.units[unit_index].player_id == network_get_player_id() ? SPRITE_SELECT_RING : SPRITE_SELECT_RING_ATTACK;
                        render_sprite(sprite, xy(0, 0), state.units[unit_index].position.to_xy() - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                    break;
                }
                case CELL_BUILDING: {
                    uint32_t building_index = state.buildings.get_index_of(id);
                    if (building_index != INDEX_INVALID) {
                        rect_t building_rect = building_get_rect(state.buildings[building_index]);
                        Sprite sprite = building_get_select_ring(state.buildings[building_index].type);
                        if (state.buildings[building_index].player_id != network_get_player_id()) {
                            sprite = (Sprite)(sprite + 1);
                        }
                        render_sprite(sprite, xy(0, 0), building_rect.position + (building_rect.size / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                    break;
                }
                case CELL_GOLD1:
                case CELL_GOLD2:
                case CELL_GOLD3: {
                    render_sprite(SPRITE_SELECT_RING_GOLD, xy(0, 0), state.ui_move_position - state.camera_offset, RENDER_SPRITE_CENTERED);
                    break;
                }
                default:
                    break;
            }
        }
    }
    
    // Buildings
    for (const building_t& building : state.buildings) {
        int sprite = SPRITE_BUILDING_HOUSE + (building.type - BUILDING_HOUSE);
        GOLD_ASSERT(sprite < SPRITE_COUNT);

        // Cull the building sprite
        xy building_render_pos = (building.cell * TILE_SIZE) - state.camera_offset;
        xy building_render_size = engine.sprites[sprite].frame_size;
        if (building_render_pos.x + building_render_size.x < 0 || building_render_pos.x > SCREEN_WIDTH || building_render_pos.y + building_render_size.y < 0 || building_render_pos.y > SCREEN_HEIGHT) {
            continue;
        }

        int hframe = building.is_finished ? 3 : ((3 * building.health) / BUILDING_DATA.at(building.type).max_health);
        ysorted.push_back((render_sprite_params_t) {
            .sprite = (Sprite)sprite,
            .position = building_render_pos,
            .frame = xy(hframe, 0),
            .options = RENDER_SPRITE_NO_CULL,
            .recolor_name = (RecolorName)building.player_id
        });
    }

    // Units
    for (const unit_t& unit : state.units) {
        xy unit_render_pos = unit.position.to_xy() - state.camera_offset;
        xy unit_render_size = engine.sprites[SPRITE_UNIT_MINER].frame_size;

        // Cull the unit sprite
        if (unit_render_pos.x + unit_render_size.x < 0 || unit_render_pos.x > SCREEN_WIDTH || unit_render_pos.y + unit_render_size.y < 0 || unit_render_pos.y > SCREEN_HEIGHT) {
            continue;
        }
        if (unit.player_id != network_get_player_id() && !is_cell_revealed(state, unit.cell, xy(1, 1))) {
            continue;
        }

        render_sprite_params_t unit_params = (render_sprite_params_t) { 
            .sprite = UNIT_DATA.at(unit.type).sprite, 
            .position = unit_render_pos, 
            .frame = unit.animation.frame,
            .options = RENDER_SPRITE_CENTERED | RENDER_SPRITE_NO_CULL,
            .recolor_name = (RecolorName)unit.player_id
        };
        if (unit.mode == UNIT_MODE_BUILD) {
            const building_t& building = state.buildings[state.buildings.get_index_of(unit.target.build.building_id)];
            const building_data_t& data = BUILDING_DATA.at(building.type);
            int hframe = ((3 * building.health) / data.max_health);
            unit_params.sprite = SPRITE_MINER_BUILDING;

            xy building_position = (building.cell * TILE_SIZE) - state.camera_offset;
            unit_params.position = building_position + xy(data.builder_positions_x[hframe], 
                                                          data.builder_positions_y[hframe]);

            unit_params.options &= ~RENDER_SPRITE_CENTERED;
            if (data.builder_flip_h[hframe]) {
                unit_params.options |= RENDER_SPRITE_FLIP_H;
            }
        }
        ysorted.push_back(unit_params);
    }

    // End Ysort
    ysort(&ysorted[0], 0, ysorted.size() - 1);
    for (uint32_t i = 0; i < ysorted.size(); i++) {
        const render_sprite_params_t& params= ysorted[i];
        render_sprite(params.sprite, params.frame, params.position, params.options, params.recolor_name);
    }

    // Fog of War
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < max_visible_tiles.y; y++) {
        for (int x = 0; x < max_visible_tiles.x; x++) {
            xy fog_cell = base_coords + xy(x, y);
            Fog fog_value = state.map_fog[fog_cell.x + (fog_cell.y * state.map_width)];
            if (fog_value == FOG_REVEALED) {
                continue;
            }

            tile_dst_rect.x = base_pos.x + x * TILE_SIZE;
            tile_dst_rect.y = base_pos.y + y * TILE_SIZE;
            SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, fog_value == FOG_HIDDEN ? 255 : 128);
            SDL_RenderFillRect(engine.renderer, &tile_dst_rect);
        }
    }
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);

    // UI move animation (for blind moves)
    if (animation_is_playing(state.ui_move_animation) && map_get_fog(state, state.ui_move_position / TILE_SIZE) == FOG_HIDDEN) {
        render_sprite(SPRITE_UI_MOVE, state.ui_move_animation.frame, state.ui_move_position - state.camera_offset, RENDER_SPRITE_CENTERED);
    }

    // Building placement
    if (state.ui_mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui()) {
        int sprite = SPRITE_BUILDING_HOUSE + (state.ui_building_type - BUILDING_HOUSE);
        GOLD_ASSERT(sprite < SPRITE_COUNT);
        render_sprite((Sprite)(sprite), xy(3, 0), (ui_get_building_cell(state) * TILE_SIZE) - state.camera_offset, 0, (RecolorName)network_get_player_id());

        const building_data_t& data = BUILDING_DATA.at(state.ui_building_type);
        bool is_placement_out_of_bounds = ui_get_building_cell(state).x + data.cell_width > state.map_width || 
                                          ui_get_building_cell(state).y + data.cell_height > state.map_height;
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
        for (int y = ui_get_building_cell(state).y; y < ui_get_building_cell(state).y + data.cell_height; y++) {
            for (int x = ui_get_building_cell(state).x; x < ui_get_building_cell(state).x + data.cell_width; x++) {
                bool is_cell_green;
                if (is_placement_out_of_bounds || state.map_fog[x + (state.map_width * y)] == FOG_HIDDEN) {
                    is_cell_green = false;
                } else {
                    is_cell_green = !map_is_cell_blocked(state, xy(x, y));
                }

                SDL_Color cell_color = is_cell_green ? COLOR_GREEN : COLOR_RED;
                SDL_SetRenderDrawColor(engine.renderer, cell_color.r, cell_color.g, cell_color.b, 128);
                SDL_Rect cell_rect = (SDL_Rect) { 
                    .x = (x * TILE_SIZE) - state.camera_offset.x, 
                    .y = (y * TILE_SIZE) - state.camera_offset.y, 
                    .w = TILE_SIZE, 
                    .h = TILE_SIZE };
                SDL_RenderFillRect(engine.renderer, &cell_rect);
            }
        }
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);
    }

    // Select rect
    if (state.ui_mode == UI_MODE_SELECTING) {
        SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
        SDL_Rect select_rect = (SDL_Rect) { 
            .x = state.select_rect.position.x - state.camera_offset.x, 
            .y = state.select_rect.position.y - state.camera_offset.y, 
            .w = state.select_rect.size.x, 
            .h = state.select_rect.size.y 
        };
        SDL_RenderDrawRect(engine.renderer, &select_rect);
    }

    // UI frames
    render_sprite(SPRITE_UI_MINIMAP, xy(0, 0), xy(0, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_MINIMAP].frame_size.y));
    render_sprite(SPRITE_UI_FRAME_BOTTOM, xy(0, 0), xy(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x, SCREEN_HEIGHT - UI_HEIGHT));
    render_sprite(SPRITE_UI_FRAME_BUTTONS, xy(0, 0), xy(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x + engine.sprites[SPRITE_UI_FRAME_BOTTOM].frame_size.x, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y));

    // UI Buttons
    for (int i = 0; i < 6; i++) {
        UiButton ui_button = ui_get_ui_button(state, i);
        if (ui_button == UI_BUTTON_NONE) {
            continue;
        }

        bool is_button_hovered = ui_get_ui_button_hovered(state) == i;
        bool is_button_pressed = is_button_hovered && input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
        xy offset = is_button_pressed ? xy(1, 1) : (is_button_hovered ? xy(0, -1) : xy(0, 0));
        render_sprite(SPRITE_UI_BUTTON, xy(is_button_hovered && !is_button_pressed ? 1 : 0, 0), ui_get_ui_button_rect(i).position + offset);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(ui_button - 1, is_button_hovered && !is_button_pressed ? 1 : 0), ui_get_ui_button_rect(i).position + offset);
    }

    // UI Status message
    if (state.ui_status_timer != 0) {
        render_text(FONT_HACK, state.ui_status_message.c_str(), COLOR_WHITE, xy(RENDER_TEXT_CENTERED, SCREEN_HEIGHT - 128));
    }

    // Resource counters
    char gold_text[8];
    sprintf(gold_text, "%u", state.player_gold[network_get_player_id()]);
    render_text(FONT_WESTERN8, gold_text, COLOR_WHITE, xy(SCREEN_WIDTH - 172 + 18, 4));
    render_sprite(SPRITE_UI_GOLD, xy(0, 0), xy(SCREEN_WIDTH - 172, 2));

    char population_text[8];
    sprintf(population_text, "%u/%u", match_get_player_population(state, network_get_player_id()), match_get_player_max_population(state, network_get_player_id()));
    render_text(FONT_WESTERN8, population_text, COLOR_WHITE, xy(SCREEN_WIDTH - 88 + 22, 4));
    render_sprite(SPRITE_UI_HOUSE, xy(0, 0), xy(SCREEN_WIDTH - 88, 0));

    // Render minimap
    SDL_SetRenderTarget(engine.renderer, engine.minimap_texture);

    SDL_SetRenderDrawColor(engine.renderer, COLOR_SAND_DARK.r, COLOR_SAND_DARK.g, COLOR_SAND_DARK.b, COLOR_SAND_DARK.a);
    SDL_RenderClear(engine.renderer);

    SDL_SetRenderDrawColor(engine.renderer, COLOR_GOLD.r, COLOR_GOLD.g, COLOR_GOLD.b, COLOR_GOLD.a);
    for (int x = 0; x < state.map_width; x++) {
        for (int y = 0; y < state.map_height; y++) {
            if (map_is_cell_gold(state, xy(x, y))) {
                SDL_Rect gold_rect = (SDL_Rect) {
                    .x = (x * MINIMAP_RECT.size.x) / (int)state.map_width,
                    .y = (y * MINIMAP_RECT.size.y) / (int)state.map_height,
                    .w = 2,
                    .h = 2
                };
                SDL_RenderFillRect(engine.renderer, &gold_rect);
            }
        }
    }

    SDL_SetRenderDrawColor(engine.renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, COLOR_GREEN.a);
    for (const building_t& building : state.buildings) {
        SDL_Rect building_rect = (SDL_Rect) { 
            .x = (building.cell.x * MINIMAP_RECT.size.x) / (int)state.map_width,
            .y = (building.cell.y * MINIMAP_RECT.size.y) / (int)state.map_height, 
            .w = 2 * BUILDING_DATA.at(building.type).cell_width,
            .h = 2 * BUILDING_DATA.at(building.type).cell_height
        };
        SDL_RenderFillRect(engine.renderer, &building_rect);
    }

    for (const unit_t& unit : state.units) {
        if (unit.player_id != network_get_player_id() && !is_cell_revealed(state, unit.cell, xy(1, 1))) {
            continue;
        }

        SDL_Rect unit_rect = (SDL_Rect) { 
            .x = (unit.cell.x * MINIMAP_RECT.size.x) / (int)state.map_width, 
            .y = (unit.cell.y * MINIMAP_RECT.size.y) / (int)state.map_height, 
            .w = 2, 
            .h = 2 };
        SDL_RenderFillRect(engine.renderer, &unit_rect);
    }

    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect fog_rect = (SDL_Rect) { .x = 0, .y = 0, .w = 2, .h = 2 };
    for (int x = 0; x < state.map_width; x++) {
        for (int y = 0; y < state.map_height; y++) {
            Fog fog_value = state.map_fog[x + (y * state.map_width)];
            if (fog_value == FOG_REVEALED) {
                continue;
            }

            fog_rect.x = (x * MINIMAP_RECT.size.x) / (int)state.map_width;
            fog_rect.y = (y * MINIMAP_RECT.size.y) / (int)state.map_height;
            SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, fog_value == FOG_HIDDEN ? 255 : 128);
            SDL_RenderFillRect(engine.renderer, &fog_rect);
        }
    }
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);

    SDL_Rect camera_rect = (SDL_Rect) { 
        .x = ((state.camera_offset.x / TILE_SIZE) * MINIMAP_RECT.size.x) / (int)state.map_width, 
        .y = ((state.camera_offset.y / TILE_SIZE) * MINIMAP_RECT.size.y) / (int)state.map_height, 
        .w = ((SCREEN_WIDTH / TILE_SIZE) * MINIMAP_RECT.size.x) / (int)state.map_width, 
        .h = 1 + (((SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE) * MINIMAP_RECT.size.y) / (int)state.map_height };
    SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(engine.renderer, &camera_rect);

    SDL_SetRenderTarget(engine.renderer, NULL);

    // Render the minimap texture
    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = MINIMAP_RECT.size.x, .h = MINIMAP_RECT.size.y };
    SDL_Rect dst_rect = (SDL_Rect) { .x = MINIMAP_RECT.position.x, .y = MINIMAP_RECT.position.y, .w = MINIMAP_RECT.size.x, .h = MINIMAP_RECT.size.y };
    SDL_RenderCopy(engine.renderer, engine.minimap_texture, &src_rect, &dst_rect);
}
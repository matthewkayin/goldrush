#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include "util.h"
#include "menu.h"
#include "options.h"
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
#include <filesystem>

#ifdef PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

const double UPDATE_TIME = 1.0 / 60;
const uint8_t INPUT_MOUSE_BUTTON_COUNT = 3;
enum TextAnchor {
    TEXT_ANCHOR_TOP_LEFT,
    TEXT_ANCHOR_BOTTOM_LEFT,
    TEXT_ANCHOR_TOP_RIGHT,
    TEXT_ANCHOR_BOTTOM_RIGHT,
    TEXT_ANCHOR_TOP_CENTER
};
const int RENDER_TEXT_CENTERED = -1;

const SDL_Color COLOR_BLACK = (SDL_Color) { .r = 0, .g = 0, .b = 0, .a = 255 };
const SDL_Color COLOR_OFFBLACK = (SDL_Color) { .r = 40, .g = 37, .b = 45, .a = 255 };
const SDL_Color COLOR_DARKBLACK = (SDL_Color) { .r = 16, .g = 15, .b = 18, .a = 255 };
const SDL_Color COLOR_WHITE = (SDL_Color) { .r = 255, .g = 255, .b = 255, .a = 255 };
const SDL_Color COLOR_SKY_BLUE = (SDL_Color) { .r = 92, .g = 132, .b = 153, .a = 255 };
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
    FONT_M3X6,
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
        .size = 32
    }},
    { FONT_M3X6, (font_params_t) {
        .path = "font/m3x6.ttf",
        .size = 16
    }},
};

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
    { SDLK_0, KEY_0 },
    { SDLK_LCTRL, KEY_CTRL },
    { SDLK_LSHIFT, KEY_SHIFT },
    { SDLK_SPACE, KEY_SPACE }
};
const std::unordered_map<UiButton, SDL_Keycode> hotkey_keymap = {
    { UI_BUTTON_MOVE, SDLK_v },
    { UI_BUTTON_STOP, SDLK_s },
    { UI_BUTTON_ATTACK, SDLK_a },
    { UI_BUTTON_BUILD, SDLK_b },
    { UI_BUTTON_CANCEL, SDLK_ESCAPE },
    { UI_BUTTON_UNLOAD, SDLK_d },
    { UI_BUTTON_BUILD_HOUSE, SDLK_e },
    { UI_BUTTON_BUILD_CAMP, SDLK_c },
    { UI_BUTTON_BUILD_SALOON, SDLK_s },
    { UI_BUTTON_UNIT_MINER, SDLK_r },
    { UI_BUTTON_UNIT_COWBOY, SDLK_c }
};
std::unordered_map<SDL_Keycode, std::vector<UiButton>> hotkeys;

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool is_running = false;
    bool view_fps = false;
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
    SDL_Texture* texture_water_autotile;
    SDL_Texture* minimap_texture;

};
static engine_t engine;

SDL_Rect rect_to_sdl(const rect_t& r) {
    return (SDL_Rect) { .x = r.position.x, .y = r.position.y, .w = r.size.x, .h = r.size.y };
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

bool engine_init(xy window_size);
void engine_quit();
bool engine_create_renderer();
void engine_destroy_renderer();
void engine_set_window_fullscreen(OptionDisplayValue display_value);
int sdlk_to_str(char* str, SDL_Keycode key);
void render_text(Font font, const char* text, SDL_Color color, xy position, TextAnchor anchor = TEXT_ANCHOR_TOP_LEFT);
void render_text_with_text_frame(const char* text, xy position);
void render_ninepatch(Sprite sprite, rect_t rect, int patch_margin);
void render_sprite(Sprite sprite, const xy& frame, const xy& position, uint32_t options = 0, RecolorName recolor_name = RECOLOR_NONE);
void render_menu(const menu_state_t& menu);
void render_options_menu(const option_menu_state_t& state);
void render_match(const match_state_t& state);

enum Mode {
    MODE_MENU,
    MODE_MATCH
};

int gold_main(int argc, char** argv) {
    if (!std::filesystem::exists("./logs")) {
        std::filesystem::create_directory("./logs");
    }

    xy window_size = xy(SCREEN_WIDTH, SCREEN_HEIGHT);
    char logfile_path[128];

    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    sprintf(logfile_path, "./logs/%d-%02d-%02dT%02d%02d%02d.log", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);

#ifdef GOLD_DEBUG
    // Parse Arguments
    for (int argn = 1; argn < argc; argn++) {
        std::string arg = std::string(argv[argn]);
        if (arg == "--logfile" && argn + 1 < argc) {
            argn++;
            strcpy(logfile_path, argv[argn]);
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

#endif

    logger_init(logfile_path);
    platform_clock_init();

    options_init();

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
    double update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    // Init SDL

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
        // Note that input is not processed unless we are about to update during this iteration
        if (update_accumulator >= UPDATE_TIME) {
            memcpy(engine.mouse_button_previous_state, engine.mouse_button_state, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));
            memcpy(engine.hotkey_state_previous, engine.hotkey_state, UI_BUTTON_COUNT * sizeof(bool));
            memcpy(engine.key_state_previous, engine.key_state, KEY_COUNT * sizeof(bool));

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                // Handle quit
                if (event.type == SDL_QUIT) {
                    engine.is_running = false;
                    break;
                } 
                // Capture mouse
                if (SDL_GetWindowGrab(engine.window) == SDL_FALSE) {
                    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                        SDL_SetWindowGrab(engine.window, SDL_TRUE);
                        continue;
                    }
                    // If the mouse is not captured, don't handle any other input
                    break;
                }
                // Release mouse
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB) {
                    SDL_SetWindowGrab(engine.window, SDL_FALSE);
                    break;
                }
                // Toggle fullscreen
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                    if (options[OPTION_DISPLAY] == DISPLAY_WINDOWED) {
                        options[OPTION_DISPLAY] = DISPLAY_BORDERLESS;
                    } else {
                        options[OPTION_DISPLAY] = DISPLAY_WINDOWED;
                    }
                    engine_set_window_fullscreen((OptionDisplayValue)options[OPTION_DISPLAY]);
                    continue;
                } 
                // Toggle view FPS
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3) {
                    engine.view_fps = !engine.view_fps;
                    continue;
                }
                // Update mouse position
                if (event.type == SDL_MOUSEMOTION) {
                    engine.mouse_position = xy(event.motion.x, event.motion.y);
                    continue;
                }
                // Handle mouse button
                if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)  {
                    if (event.button.button - 1 < INPUT_MOUSE_BUTTON_COUNT) {
                        engine.mouse_button_state[event.button.button - 1] = event.type == SDL_MOUSEBUTTONDOWN;
                    }
                    continue;
                }
                // Text input
                if (event.type == SDL_TEXTINPUT) {
                    engine.input_text += std::string(event.text.text);
                    if (engine.input_text.length() > engine.input_length_limit) {
                        engine.input_text = engine.input_text.substr(0, engine.input_length_limit);
                    }
                    continue;
                }
                if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
                    if (engine.input_text.length() > 0) {
                        engine.input_text.pop_back();
                    }
                    continue;
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
                    continue;
                }
            } // End while SDL_PollEvent()
        } // End if update_accumulator >= UPDATE_TIME

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
                        engine.minimap_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, match_state.map.width, match_state.map.height);
                    } else if (menu_state.mode == MENU_MODE_EXIT) {
                        engine.is_running = false;
                        break;
                    } else if (menu_state.mode == MENU_MODE_OPTIONS && menu_state.option_menu_state.mode == OPTION_MENU_REQUEST_CHANGES) {
                        if ((menu_state.option_menu_state.requested_changes & REQUEST_REINIT_RENDERER) == REQUEST_REINIT_RENDERER) {
                            engine_destroy_renderer();
                            if (!engine_create_renderer()) {
                                return 1;
                            }
                        }
                        if ((menu_state.option_menu_state.requested_changes & REQUEST_UPDATE_FULLSCREEN) == REQUEST_UPDATE_FULLSCREEN) {
                            engine_set_window_fullscreen((OptionDisplayValue)options[OPTION_DISPLAY]);
                        }
                        menu_state.option_menu_state.mode = OPTION_MENU_OPEN;
                    }
                    break;
                }
                case MODE_MATCH: {
                    match_update(match_state);
                    if (match_state.ui_mode == UI_MODE_LEAVE_MATCH) {
                        menu_state = menu_init();
                        mode = MODE_MENU;
                        SDL_DestroyTexture(engine.minimap_texture);
                        engine.minimap_texture = NULL;
                    }
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
    TTF_Quit();
    SDL_Quit();
    options_save_to_file();
    logger_quit();

    return 0;
}

#if defined PLATFORM_WIN32 && !defined GOLD_DEBUG
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return gold_main(0, NULL);
}
#else
int main(int argc, char** argv) {
    return gold_main(argc, argv);
}
#endif

bool engine_init(xy window_size) {
    // Init input
    memset(engine.mouse_button_state, 0, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));
    memset(engine.mouse_button_previous_state, 0, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));
    engine.mouse_position = xy(0, 0);
    memset(engine.hotkey_state, 0, UI_BUTTON_COUNT * sizeof(bool));
    memset(engine.hotkey_state_previous, 0, UI_BUTTON_COUNT * sizeof(bool));
    memset(engine.key_state, 0, KEY_COUNT * sizeof(bool));
    memset(engine.key_state_previous, 0, KEY_COUNT * sizeof(bool));

    // Create window
    engine.window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_size.x, window_size.y, SDL_WINDOW_SHOWN);
    if (engine.window == NULL) {
        log_error("Error creating window: %s", SDL_GetError());
        return false;
    }

    engine.minimap_texture = NULL;
    if (!engine_create_renderer()) {
        return false;
    }

    SDL_StopTextInput();
    engine.is_running = true;

#ifndef GOLD_DEBUG_MOUSE
    SDL_SetWindowGrab(engine.window, SDL_TRUE);
#endif

    engine_set_window_fullscreen((OptionDisplayValue)options[OPTION_DISPLAY]);

    log_info("%s initialized.", APP_NAME);
    return true;
}

void engine_set_window_fullscreen(OptionDisplayValue display_value) {
    if (display_value == DISPLAY_WINDOWED) {
        SDL_SetWindowFullscreen(engine.window, 0);
    } else if (display_value == DISPLAY_BORDERLESS) {
        SDL_SetWindowFullscreen(engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_SetWindowGrab(engine.window, SDL_TRUE);
    } else {
        SDL_SetWindowFullscreen(engine.window, SDL_WINDOW_FULLSCREEN);
        SDL_SetWindowGrab(engine.window, SDL_TRUE);
    }
}

bool engine_create_renderer() {
    // Create renderer
    uint32_t renderer_flags = SDL_RENDERER_ACCELERATED;
    if (options[OPTION_VSYNC] == VSYNC_ENABLED) {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }
    engine.renderer = SDL_CreateRenderer(engine.window, -1, renderer_flags);
    if (engine.renderer == NULL) {
        log_error("Error creating renderer: %s", SDL_GetError());
        return false;
    }
    SDL_RenderSetLogicalSize(engine.renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

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

    engine.texture_water_autotile = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 47 * 16, 16);
    SDL_SetRenderTarget(engine.renderer, engine.texture_water_autotile);
    uint32_t direction_mask[DIRECTION_COUNT] = {
        1,
        2,
        4,
        8,
        16,
        32,
        64, 
        128
    };
    uint32_t unique_index = 0;
    for (uint32_t i = 0; i < 256; i++) {
        bool has_direction[DIRECTION_COUNT];
        for (uint32_t direction = 0; direction < DIRECTION_COUNT; direction++) {
            has_direction[direction] = i & direction_mask[direction];
        }
        bool is_unique_combination = true;
        for (uint32_t direction = DIRECTION_NORTHEAST; direction < DIRECTION_COUNT; direction += 2) {
            if (has_direction[direction] && !(has_direction[direction - 1] && has_direction[(direction + 1) % DIRECTION_COUNT])) {
                is_unique_combination = false;
                break;
            }
        }
        if (!is_unique_combination) {
            continue;
        }

        for (int sub_x = 0; sub_x < 1; sub_x++) {
            for (int sub_y = 0; sub_y < 1; sub_y++) {

            }
        }

        unique_index++;
    }

    SDL_SetRenderTarget(engine.renderer, NULL);

    log_info("Initialized renderer.");
    return true;
}

void engine_quit() {
    SDL_DestroyWindow(engine.window);

    log_info("%s deinitialized.", APP_NAME);
}

void engine_destroy_renderer() {
    for (TTF_Font* font : engine.fonts) {
        TTF_CloseFont(font);
    }
    engine.fonts.clear();
    for (sprite_t sprite : engine.sprites) {
        SDL_DestroyTexture(sprite.texture);
    }
    engine.sprites.clear();
    for (SDL_Cursor* cursor : engine.cursors) {
        SDL_FreeCursor(cursor);
    }
    engine.cursors.clear();
    if (engine.minimap_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_texture);
    }

    SDL_DestroyTexture(engine.texture_water_autotile);

    SDL_DestroyRenderer(engine.renderer);

    log_info("Destroyed renderer.");
}

int sdlk_to_str(char* str, SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z) {
        return sprintf(str, "%c", (char)(key - 32));
    } else if (key == SDLK_ESCAPE) {
        return sprintf(str, "ESC");
    } else {
        log_error("Unhandled key %u in sdlk_to_str()", key);
        GOLD_ASSERT(false);
        return 0;
    }
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

void render_text_with_text_frame(const char* text, xy position) {
    SDL_Surface* text_surface = TTF_RenderText_Solid(engine.fonts[FONT_WESTERN8], text, COLOR_OFFBLACK);
    if (text_surface == NULL) {
        log_error("Unable to render text to surface: %s", TTF_GetError());
        return;
    }

    int frame_width = (text_surface->w / 15) + 1;
    if (text_surface->w % 15 != 0) {
        frame_width++;
    }
    for (int frame_x = 0; frame_x < frame_width; frame_x++) {
        int x_frame = 1;
        if (frame_x == 0) {
            x_frame = 0;
        } else if (frame_x == frame_width - 1) {
            x_frame = 2;
        }
        render_sprite(SPRITE_UI_TEXT_FRAME, xy(x_frame, 0), position + xy(frame_x * 15, 0), RENDER_SPRITE_NO_CULL);
    }

    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(engine.renderer, text_surface);
    if (text_texture == NULL) {
        log_error("Unable to creature texture from text surface: %s", SDL_GetError());
        return;
    }

    SDL_Rect src_rect = (SDL_Rect) { .x = 0, . y = 0, .w = text_surface->w, .h = text_surface->h };
    SDL_Rect dst_rect = (SDL_Rect) { .x = position.x + ((frame_width * 15) / 2) - (text_surface->w / 2), .y = position.y + 2, .w = src_rect.w, .h = src_rect.h };

    SDL_RenderCopy(engine.renderer, text_texture, &src_rect, &dst_rect);

    SDL_DestroyTexture(text_texture);
    SDL_FreeSurface(text_surface);
}

void render_ninepatch(Sprite sprite, rect_t rect, int patch_margin) {
    GOLD_ASSERT(rect.size.x > patch_margin * 2 && rect.size.y > patch_margin * 2);

    SDL_Rect src_rect = (SDL_Rect) {
        .x = 0,
        .y = 0,
        .w = patch_margin,
        .h = patch_margin
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = rect.position.x, 
        .y = rect.position.y,
        .w = patch_margin,
        .h = patch_margin
    };

    // Top left
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Top right
    src_rect.x = patch_margin * 2;
    dst_rect.x = rect.position.x + rect.size.x - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Bottom right
    src_rect.y = patch_margin * 2;
    dst_rect.y = rect.position.y + rect.size.y - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);
    
    // Bottom left
    src_rect.x = 0;
    dst_rect.x = rect.position.x;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Top edge
    src_rect.x = patch_margin;
    src_rect.y = 0;
    dst_rect.x = rect.position.x + patch_margin;
    dst_rect.y = rect.position.y;
    dst_rect.w = rect.size.x - (patch_margin * 2);
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Bottom edge
    src_rect.y = patch_margin * 2;
    dst_rect.y = rect.position.y + rect.size.y - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Left edge
    src_rect.x = 0;
    src_rect.y = patch_margin;
    dst_rect.x = rect.position.x;
    dst_rect.y = rect.position.y + patch_margin;
    dst_rect.w = patch_margin;
    dst_rect.h = rect.size.y - (patch_margin * 2);
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Right edge
    src_rect.x = patch_margin * 2;
    dst_rect.x = rect.position.x + rect.size.x - patch_margin;
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);

    // Center
    src_rect.x = patch_margin;
    src_rect.y = patch_margin;
    dst_rect.x = rect.position.x + patch_margin;
    dst_rect.y = rect.position.y + patch_margin;
    dst_rect.w = rect.size.x - (patch_margin * 2);
    dst_rect.h = rect.size.y - (patch_margin * 2);
    SDL_RenderCopy(engine.renderer, engine.sprites[sprite].texture, &src_rect, &dst_rect);
}

void render_menu(const menu_state_t& menu) {
    if (menu.mode == MENU_MODE_MATCH_START) {
        return;
    }

    SDL_Rect background_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_SetRenderDrawColor(engine.renderer, COLOR_SKY_BLUE.r, COLOR_SKY_BLUE.g, COLOR_SKY_BLUE.b, COLOR_SKY_BLUE.a);
    SDL_RenderFillRect(engine.renderer, &background_rect);

    // Render tiles
    static const int menu_tile_width = (SCREEN_WIDTH / TILE_SIZE) * 2;
    static const int menu_tile_height = 3;
    for (int x = 0; x < menu_tile_width; x++) {
        for (int y = 0; y < menu_tile_height; y++) {
            SDL_Rect src_rect = (SDL_Rect) {
                .x = (y == 0 || y == 2) && (x + y) % 10 == 0 ? 32 : 0,
                .y = 0,
                .w = TILE_SIZE,
                .h = TILE_SIZE
            };
            SDL_Rect dst_rect = (SDL_Rect) {
                .x = (x * TILE_SIZE * 2) - menu.parallax_x,
                .y = SCREEN_HEIGHT - ((menu_tile_height - y) * TILE_SIZE * 2),
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            if (dst_rect.x > SCREEN_WIDTH || dst_rect.x + (TILE_SIZE * 2) < 0) {
                continue;
            }
            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILES].texture, &src_rect, &dst_rect);

        }
    }
    // Render tile decorations
    static const xy tile_decoration_coords[3] = { xy(680, TILE_SIZE / 4), xy(920, 2 * (TILE_SIZE * 2)), xy(1250, TILE_SIZE / 4) };
    for (int i = 0; i < 3; i++) {
            SDL_Rect src_rect = (SDL_Rect) {
                .x = (((i * 2) + menu.parallax_cactus_offset) % 5) * TILE_SIZE,
                .y = 0,
                .w = TILE_SIZE,
                .h = TILE_SIZE
            };
            SDL_Rect dst_rect = (SDL_Rect) {
                .x = tile_decoration_coords[i].x - menu.parallax_x,
                .y = SCREEN_HEIGHT - (menu_tile_height * TILE_SIZE * 2) + tile_decoration_coords[i].y,
                .w = TILE_SIZE * 2,
                .h = TILE_SIZE * 2
            };
            if (dst_rect.x > SCREEN_WIDTH || dst_rect.x + (TILE_SIZE * 2) < 0) {
                continue;
            }
            SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILE_DECORATION].texture, &src_rect, &dst_rect);
    }
    // Render wagon animation
    SDL_Rect src_rect = (SDL_Rect) {
        .x = engine.sprites[SPRITE_UNIT_WAGON].frame_size.x * menu.wagon_animation.frame.x,
        .y = engine.sprites[SPRITE_UNIT_WAGON].frame_size.y * 2,
        .w = engine.sprites[SPRITE_UNIT_WAGON].frame_size.x,
        .h = engine.sprites[SPRITE_UNIT_WAGON].frame_size.y
    };
    SDL_Rect dst_rect = (SDL_Rect) {
        .x = 380,
        .y = SCREEN_HEIGHT - (6 * TILE_SIZE),
        .w = src_rect.w * 2,
        .h = src_rect.h * 2
    };
    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_UNIT_WAGON].colored_texture[RECOLOR_BLUE], &src_rect, &dst_rect);

    // Render clouds
    static const int cloud_count = 6;
    static const xy cloud_coords[cloud_count] = { xy(640, 16), xy(950, 64), xy(1250, 48), xy(-30, 48), xy(320, 32), xy(1600, 32) };
    static const int cloud_frame_x[cloud_count] = { 0, 1, 2, 2, 1, 1};
    for (int i = 0; i < cloud_count; i++) {
        SDL_Rect src_rect = (SDL_Rect) {
            .x = cloud_frame_x[i] * engine.sprites[SPRITE_MENU_CLOUDS].frame_size.x,
            .y = 0,
            .w = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.x,
            .h = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.y,
        };
        SDL_Rect dst_rect = (SDL_Rect) {
            .x = cloud_coords[i].x - menu.parallax_cloud_x,
            .y = cloud_coords[i].y,
            .w = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.x * 2,
            .h = engine.sprites[SPRITE_MENU_CLOUDS].frame_size.y * 2,
        };
        if (dst_rect.x > SCREEN_WIDTH || dst_rect.x + dst_rect.w < 0) {
            continue;
        }
        SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_MENU_CLOUDS].texture, &src_rect, &dst_rect);
    }

    // Render title
    if (menu.mode != MENU_MODE_LOBBY) {
        render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_OFFBLACK, xy(24, 24));
    }

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_OFFBLACK, xy(4, SCREEN_HEIGHT - 14));

    if (menu.status_timer > 0) {
        render_text(FONT_WESTERN8, menu.status_text.c_str(), COLOR_RED, xy(48, TEXT_INPUT_RECT.position.y - 38));
    }

    if (menu.mode == MENU_MODE_PLAY || menu.mode == MENU_MODE_JOIN_IP) {
        const char* prompt = menu.mode == MENU_MODE_PLAY ? "USERNAME" : "IP ADDRESS";
        render_text(FONT_WESTERN8, prompt, COLOR_OFFBLACK, TEXT_INPUT_RECT.position + xy(1, -13));
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_Rect text_input_rect = rect_to_sdl(TEXT_INPUT_RECT);
        SDL_RenderDrawRect(engine.renderer, &text_input_rect);

        render_text(FONT_WESTERN16, input_get_text_input_value(), COLOR_OFFBLACK, TEXT_INPUT_RECT.position + xy(2, TEXT_INPUT_RECT.size.y - 4), TEXT_ANCHOR_BOTTOM_LEFT);
    }

    if (menu.mode == MENU_MODE_JOIN_CONNECTING) {
        render_text(FONT_WESTERN16, "Connecting...", COLOR_OFFBLACK, xy(48, BUTTON_Y - 42));
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
        SDL_Color button_color = menu.button_hovered == it.first ? COLOR_WHITE : COLOR_OFFBLACK;
        render_text(FONT_WESTERN16, it.second.text, button_color, it.second.rect.position + xy(4, 4));
        SDL_SetRenderDrawColor(engine.renderer, button_color.r, button_color.g, button_color.b, button_color.a);
        SDL_Rect button_rect = rect_to_sdl(it.second.rect);
        SDL_RenderDrawRect(engine.renderer, &button_rect);
    }

    if (menu.mode == MENU_MODE_OPTIONS) {
        render_options_menu(menu.option_menu_state);
    }
}

void render_options_menu(const option_menu_state_t& state) {
    SDL_Rect screen_rect = (SDL_Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 128);
    SDL_RenderFillRect(engine.renderer, &screen_rect);
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);
    render_ninepatch(SPRITE_UI_FRAME, OPTIONS_FRAME_RECT, 16);
    render_sprite(SPRITE_UI_PARCHMENT_BUTTONS, xy(1, state.hover == OPTION_HOVER_APPLY ? 1 : 0), APPLY_BUTTON_POSITION + (state.hover == OPTION_HOVER_APPLY ? xy(0, -1) : xy(0, 0)), RENDER_SPRITE_NO_CULL);
    render_sprite(SPRITE_UI_PARCHMENT_BUTTONS, xy(0, state.hover == OPTION_HOVER_BACK ? 1 : 0), BACK_BUTTON_POSITION + (state.hover == OPTION_HOVER_BACK ? xy(0, -1) : xy(0, 0)), RENDER_SPRITE_NO_CULL);

    for (int i = 0; i < OPTION_COUNT; i++) {
        xy dropdown_position = OPTIONS_DROPDOWN_POSITION + xy(0, i * (OPTIONS_DROPDOWN_SIZE.y + OPTIONS_DROPDOWN_PADDING));
        xy dropdown_offset = xy(0, 0);
        int dropdown_y_frame = 0;
        if (state.hover == OPTION_HOVER_DROPDOWN && state.hover_subindex == i) {
            dropdown_y_frame = 1;
            dropdown_offset.y = -1;
        } else if (state.mode == OPTION_MENU_DROPDOWN && state.dropdown_chosen == (Option)i) {
            dropdown_y_frame = 2;
        }
        render_sprite(SPRITE_UI_OPTIONS_DROPDOWN, xy(0, dropdown_y_frame), dropdown_position + dropdown_offset, RENDER_SPRITE_NO_CULL);
        render_text(FONT_WESTERN8, option_value_string((Option)i, state.pending_changes.at((Option)i)), dropdown_y_frame == 1 ? COLOR_WHITE : COLOR_OFFBLACK, dropdown_position + dropdown_offset + xy(5, 5));
        render_text(FONT_WESTERN8, option_string((Option)i), COLOR_GOLD, dropdown_position + xy(-12, 5), TEXT_ANCHOR_TOP_RIGHT);
    }

    if (state.mode == OPTION_MENU_DROPDOWN) {
        for (int i = 0; i < OPTION_DATA.at(state.dropdown_chosen).max_value; i++) {
            xy dropdown_item_position = OPTIONS_DROPDOWN_POSITION + 
                xy(0, (int)state.dropdown_chosen * (OPTIONS_DROPDOWN_SIZE.y + OPTIONS_DROPDOWN_PADDING)) +
                xy(0, (i + 1) * OPTIONS_DROPDOWN_SIZE.y);
            bool is_hovered = state.hover == OPTION_HOVER_DROPDOWN_ITEM && state.hover_subindex == i;
            render_sprite(SPRITE_UI_OPTIONS_DROPDOWN, xy(0, is_hovered ? 4 : 3), dropdown_item_position, RENDER_SPRITE_NO_CULL);
            render_text(FONT_WESTERN8, option_value_string(state.dropdown_chosen, i), is_hovered ? COLOR_WHITE : COLOR_OFFBLACK, dropdown_item_position + xy(5, 5));
        }
    }
}

void render_sprite(Sprite sprite, const xy& frame, const xy& position, uint32_t options, RecolorName recolor_name) {
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
            int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.map.width);
            tile_src_rect.x = ((int)state.map.tiles[map_index].base % engine.sprites[SPRITE_TILES].hframes) * TILE_SIZE;
            tile_src_rect.y = ((int)state.map.tiles[map_index].base / engine.sprites[SPRITE_TILES].hframes) * TILE_SIZE;

            tile_dst_rect.x = base_pos.x + (x * TILE_SIZE);
            tile_dst_rect.y = base_pos.y + (y * TILE_SIZE);

            if (state.map.tiles[map_index].base > 2) {
                if (state.map.tiles[map_index].base == 3) {
                    tile_src_rect.x = 8;
                    tile_src_rect.y = 24;
                    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILE_WATER].texture, &tile_src_rect, &tile_dst_rect);
                } else {
                    tile_src_rect.x = state.map.tiles[map_index].base - 4;
                    tile_src_rect.y = 0;
                    SDL_RenderCopy(engine.renderer, engine.texture_water_autotile, &tile_src_rect, &tile_dst_rect);
                }
            } else {
                SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILES].texture, &tile_src_rect, &tile_dst_rect);
            }

            // Render decorations
            if (state.map.tiles[map_index].decoration != 0) {
                ysorted.push_back((render_sprite_params_t) {
                    .sprite = SPRITE_TILE_DECORATION,
                    .position = xy(tile_dst_rect.x, tile_dst_rect.y),
                    .frame = xy(state.map.tiles[map_index].decoration - 1, 0),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_name = RECOLOR_NONE
                });
            }
        }
    }

    // Render destroyed buildings
    for (auto it : state.remembered_buildings) {
        const remembered_building_t& building = it.second;
        if (building.mode == BUILDING_MODE_DESTROYED) {
            Sprite destroyed_sprite;
            if (building_cell_size(building.type) == xy(3, 3)) {
                destroyed_sprite = SPRITE_BUILDING_DESTROYED_3;
            } else {
                destroyed_sprite = SPRITE_BUILDING_DESTROYED_2;
            }
            render_sprite(destroyed_sprite, xy(0, 0), (building.cell * TILE_SIZE) - state.camera_offset);
        }
    }

    // Select rings and healthbars
    static const int HEALTHBAR_HEIGHT = 4;
    static const int HEALTHBAR_PADDING = 2;
    static const int BUILDING_HEALTHBAR_PADDING = 5;
    if (state.selection.type == SELECTION_TYPE_BUILDINGS || state.selection.type == SELECTION_TYPE_ENEMY_BUILDING) {
        uint32_t building_index = state.buildings.get_index_of(state.selection.ids[0]);
        if (building_index != INDEX_INVALID) {
            const building_t& building = state.buildings[building_index];
            rect_t building_rect = building_get_rect(building);
            render_sprite(building_get_select_ring(building.type, state.selection.type == SELECTION_TYPE_ENEMY_BUILDING), xy(0, 0), building_rect.position + (building_rect.size / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);

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
    } else if (state.selection.type == SELECTION_TYPE_UNITS || state.selection.type == SELECTION_TYPE_ENEMY_UNIT) {
        for (entity_id unit_id : state.selection.ids) {
            uint32_t index = state.units.get_index_of(unit_id);
            if (index == INDEX_INVALID) {
                continue;
            }
            const unit_t& unit = state.units[index];
            if (unit.mode == UNIT_MODE_IN_MINE) {
                continue;
            }

            xy unit_render_pos = unit.position.to_xy() - state.camera_offset;
            Sprite select_ring_sprite = unit_get_select_ring(unit.type, state.selection.type == SELECTION_TYPE_ENEMY_UNIT);
            render_sprite(select_ring_sprite, xy(0, 0), unit_render_pos, RENDER_SPRITE_CENTERED);

            // Determine healthbar rect
            xy unit_render_size = engine.sprites[UNIT_DATA.at(unit.type).sprite].frame_size;
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
    } else if (state.selection.type == SELECTION_TYPE_MINE) {
        const mine_t& mine = state.mines.get_by_id(state.selection.ids[0]);
        rect_t mine_rect = rect_t(mine.cell * TILE_SIZE, xy(MINE_SIZE * TILE_SIZE, MINE_SIZE * TILE_SIZE));
        render_sprite(SPRITE_SELECT_RING_BUILDING_3, xy(0, 0), mine_rect.position + (mine_rect.size / 2) - state.camera_offset + xy(0, -3), RENDER_SPRITE_CENTERED);
    }

    // UI move animation
    if (animation_is_playing(state.ui_move_animation) && map_get_fog(state.map, network_get_player_id(), state.ui_move_position / TILE_SIZE) != FOG_HIDDEN) {
        if (state.ui_move_cell.type == CELL_EMPTY) {
            render_sprite(SPRITE_UI_MOVE, state.ui_move_animation.frame, state.ui_move_position - state.camera_offset, RENDER_SPRITE_CENTERED);
        } else if (state.ui_move_animation.frame.x % 2 == 0) {
            entity_id id = (entity_id)(state.ui_move_cell.value);
            switch (state.ui_move_cell.type) {
                case CELL_UNIT: {
                    uint32_t unit_index = state.units.get_index_of(id);
                    if (unit_index != INDEX_INVALID && map_is_cell_rect_revealed(state.map, network_get_player_id(), rect_t(state.units[unit_index].cell, xy(1, 1)))) {
                        Sprite sprite = unit_get_select_ring(state.units[unit_index].type, state.units[unit_index].player_id != network_get_player_id());
                        render_sprite(sprite, xy(0, 0), state.units[unit_index].position.to_xy() - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                    break;
                }
                case CELL_BUILDING: {
                    uint32_t building_index = state.buildings.get_index_of(id);
                    if (building_index != INDEX_INVALID) {
                        rect_t building_rect = building_get_rect(state.buildings[building_index]);
                        Sprite sprite = building_get_select_ring(state.buildings[building_index].type, state.buildings[building_index].player_id != network_get_player_id());
                        render_sprite(sprite, xy(0, 0), building_rect.position + (building_rect.size / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                    break;
                }
                case CELL_MINE: {
                    render_sprite(SPRITE_SELECT_RING_BUILDING_3, xy(0, 0), (state.mines.get_by_id(id).cell * TILE_SIZE) + (xy(3 * TILE_SIZE, 3 * TILE_SIZE) / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    break;
                }
                default:
                    break;
            }
        }
    }

    // Rally points
    if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        const building_t& building = state.buildings.get_by_id(state.selection.ids[0]);
        if (building_is_finished(building) && building.rally_point.x != -1) {
            ysorted.push_back((render_sprite_params_t) {
                .sprite = SPRITE_RALLY_FLAG,
                .position = building.rally_point - xy(4, 15) - state.camera_offset,
                .frame = state.ui_rally_animation.frame,
                .options = 0,
                .recolor_name = (RecolorName)network_get_player_id()
            });
        }
    }

    // Mines
    for (auto it : state.remembered_mines) {
        const mine_t& mine = it.second;
        rect_t mine_render_rect = mine_get_rect(mine.cell);
        mine_render_rect.position -= state.camera_offset;
        if (mine_render_rect.position.x + mine_render_rect.size.x < 0 || mine_render_rect.position.x > SCREEN_WIDTH || mine_render_rect.position.y + mine_render_rect.size.y < 0 || mine_render_rect.position.y > SCREEN_HEIGHT) {
            continue;
        }
        int hframe = 0;
        if (mine.is_occupied) {
            hframe = 1;
        } else if (mine.gold_left == 0) {
            hframe = 2;
        }
        ysorted.push_back((render_sprite_params_t) {
            .sprite = SPRITE_BUILDING_MINE,
            .position = mine_render_rect.position,
            .frame = xy(hframe, 0),
            .options = RENDER_SPRITE_NO_CULL,
            .recolor_name = RECOLOR_NONE
        });
    }

    // Buildings
    for (auto it : state.remembered_buildings) {
        const remembered_building_t& building = it.second;
        if (building.mode == BUILDING_MODE_DESTROYED) {
            continue;
        }

        int sprite = SPRITE_BUILDING_HOUSE + (building.type - BUILDING_HOUSE);
        GOLD_ASSERT(sprite < SPRITE_COUNT);

        // Cull the building sprite
        xy building_render_pos = (building.cell * TILE_SIZE) - state.camera_offset;
        xy building_render_size = engine.sprites[sprite].frame_size;
        if (building_render_pos.x + building_render_size.x < 0 || building_render_pos.x > SCREEN_WIDTH || building_render_pos.y + building_render_size.y < 0 || building_render_pos.y > SCREEN_HEIGHT) {
            continue;
        }

        int hframe = building.mode == BUILDING_MODE_FINISHED ? 3 : ((3 * building.health) / BUILDING_DATA.at(building.type).max_health);
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
        if (unit.mode == UNIT_MODE_FERRY) {
            continue;
        }

        xy unit_render_pos = unit.position.to_xy() - state.camera_offset;
        xy unit_render_size = engine.sprites[UNIT_DATA.at(unit.type).sprite].frame_size;

        // Cull the unit sprite
        if (unit_render_pos.x + unit_render_size.x < 0 || unit_render_pos.x > SCREEN_WIDTH || unit_render_pos.y + unit_render_size.y < 0 || unit_render_pos.y > SCREEN_HEIGHT) {
            continue;
        }
        if (unit.player_id != network_get_player_id() && !map_is_cell_rect_revealed(state.map, network_get_player_id(), rect_t(unit.cell, unit_cell_size(unit.type)))) {
            continue;
        }
        if (unit.mode == UNIT_MODE_IN_MINE) {
            continue;
        }

        render_sprite_params_t unit_params = (render_sprite_params_t) { 
            .sprite = UNIT_DATA.at(unit.type).sprite, 
            .position = unit_render_pos, 
            .frame = unit.animation.frame,
            .options = RENDER_SPRITE_CENTERED | RENDER_SPRITE_NO_CULL,
            .recolor_name = (RecolorName)unit.player_id
        };
        if (unit.mode != UNIT_MODE_BUILD && unit_sprite_should_flip_h(unit)) {
            unit_params.options |= RENDER_SPRITE_FLIP_H;
        }
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
        if (unit.mode == UNIT_MODE_REPAIR) {
            unit_params.sprite = SPRITE_MINER_BUILDING;
        }
        if (unit.mode == UNIT_MODE_DEATH_FADE) {
            render_sprite(unit_params.sprite, unit_params.frame, unit_params.position, unit_params.options, unit_params.recolor_name);
        } else {
            ysorted.push_back(unit_params);
        }
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
            FogType fog = state.map.player_fog[network_get_player_id()][fog_cell.x + (fog_cell.y * state.map.width)];
            if (fog == FOG_REVEALED) {
                continue;
            }

            tile_dst_rect.x = base_pos.x + x * TILE_SIZE;
            tile_dst_rect.y = base_pos.y + y * TILE_SIZE;
            SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, fog == FOG_HIDDEN ? 255 : 128);
#ifndef GOLD_DEBUG_FOG_DISABLED
            SDL_RenderFillRect(engine.renderer, &tile_dst_rect);
#endif
        }
    }
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);

    // UI move animation (for blind moves)
    if (animation_is_playing(state.ui_move_animation) && map_get_fog(state.map, network_get_player_id(), state.ui_move_position / TILE_SIZE) == FOG_HIDDEN) {
        render_sprite(SPRITE_UI_MOVE, state.ui_move_animation.frame, state.ui_move_position - state.camera_offset, RENDER_SPRITE_CENTERED);
    }

#ifdef GOLD_DEBUG_UNIT_PATHS
    // Unit paths
    SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
    for (const unit_t& unit : state.units) {
        if (unit.path.empty()) {
            continue;
        }
        xy start = unit.position.to_xy() - state.camera_offset;
        for (uint32_t i = 0; i < unit.path.size(); i++) {
            xy end = cell_center(unit.path[i]).to_xy() - state.camera_offset;
            SDL_RenderDrawLine(engine.renderer, start.x, start.y, end.x, end.y);
            start = end;
        }
    }
#endif

    // Building placement
    if (state.ui_mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui()) {
        int sprite = SPRITE_BUILDING_HOUSE + (state.ui_building_type - BUILDING_HOUSE);
        GOLD_ASSERT(sprite < SPRITE_COUNT);
        render_sprite((Sprite)(sprite), xy(3, 0), (ui_get_building_cell(state) * TILE_SIZE) - state.camera_offset, 0, (RecolorName)network_get_player_id());

        const building_data_t& data = BUILDING_DATA.at(state.ui_building_type);
        bool is_placement_out_of_bounds = ui_get_building_cell(state).x + data.cell_size > state.map.width || 
                                          ui_get_building_cell(state).y + data.cell_size > state.map.height;
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
        for (int y = ui_get_building_cell(state).y; y < ui_get_building_cell(state).y + data.cell_size; y++) {
            for (int x = ui_get_building_cell(state).x; x < ui_get_building_cell(state).x + data.cell_size; x++) {
                bool is_cell_green = true;
                if (is_placement_out_of_bounds || map_get_fog(state.map, network_get_player_id(), xy(x, y)) == FOG_HIDDEN) {
                    is_cell_green = false;
                } else {
                    for (const mine_t& mine : state.mines) {
                        if (rect_t(xy(x, y), xy(1, 1)).intersects(mine_get_block_building_rect(mine.cell))) {
                            is_cell_green = false;
                            break;
                        }
                    }
                    xy miner_cell = state.units.get_by_id(ui_get_nearest_builder(state, ui_get_building_cell(state))).cell;
                    if (is_cell_green) {
                        is_cell_green = xy(x, y) == miner_cell || !map_is_cell_blocked(state.map, xy(x, y));
                    }
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

    // UI Control Groups
    for (uint32_t i = 0; i < 10; i++) {
        selection_mode_t control_group_mode = ui_get_mode_of_selection(state, state.control_groups[i]);
        if (control_group_mode.type == SELECTION_MODE_NONE) {
            continue;
        }
        int button_frame = 0;
        SDL_Color text_color = COLOR_OFFBLACK;
        if (state.ui_selected_control_group == i) {
            text_color = COLOR_DARKBLACK;
            button_frame = 2;
        }

        int button_icon = control_group_mode.type == SELECTION_MODE_UNIT
                            ? UI_BUTTON_UNIT_MINER + control_group_mode.unit_type
                            : UI_BUTTON_BUILD_HOUSE + (control_group_mode.building_type - BUILDING_HOUSE);
        xy render_pos = UI_FRAME_BOTTOM_POSITION + xy(4, 0) + xy((4 + engine.sprites[SPRITE_UI_CONTROL_GROUP_FRAME].frame_size.x) * i, -32);
        render_sprite(SPRITE_UI_CONTROL_GROUP_FRAME, xy(button_frame, 0), render_pos, RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(button_icon - 1, button_frame), render_pos + xy(1, 0), RENDER_SPRITE_NO_CULL);
        char control_group_number_text[4];
        sprintf(control_group_number_text, "%u", i == 9 ? 0 : i + 1);
        render_text(FONT_M3X6, control_group_number_text, text_color, render_pos + xy(3, -2));
        char control_group_count_text[4];
        sprintf(control_group_count_text, "%u", control_group_mode.count);
        render_text(FONT_M3X6, control_group_count_text, text_color, render_pos + xy(32, 30), TEXT_ANCHOR_BOTTOM_RIGHT);
    }

    // UI frames
    render_sprite(SPRITE_UI_MINIMAP, xy(0, 0), xy(0, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_MINIMAP].frame_size.y));
    render_sprite(SPRITE_UI_FRAME_BOTTOM, xy(0, 0), UI_FRAME_BOTTOM_POSITION);
    render_sprite(SPRITE_UI_FRAME_BUTTONS, xy(0, 0), xy(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x + engine.sprites[SPRITE_UI_FRAME_BOTTOM].frame_size.x, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y));

    // UI Buttons
    for (int i = 0; i < 6; i++) {
        UiButton ui_button = ui_get_ui_button(state, i);
        if (ui_button == UI_BUTTON_NONE) {
            continue;
        }

        bool is_button_hovered = ui_get_ui_button_hovered(state) == i && ui_button_requirements_met(state, ui_button);
        bool is_button_pressed = is_button_hovered && input_is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
        xy offset = is_button_pressed ? xy(1, 1) : (is_button_hovered ? xy(0, -1) : xy(0, 0));
        int button_state = 0;
        if (!ui_button_requirements_met(state, ui_button)) {
            button_state = 2;
        } else if (is_button_hovered && !is_button_pressed) {
            button_state = 1;
        }
        render_sprite(SPRITE_UI_BUTTON, xy(button_state, 0), ui_get_ui_button_rect(i).position + offset);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(ui_button - 1, button_state), ui_get_ui_button_rect(i).position + offset);
    }

    // UI Selection list
    static const xy SELECTION_LIST_TOP_LEFT = UI_FRAME_BOTTOM_POSITION + xy(12 + 16, 12);
    if (!state.selection.ids.empty()) {
        for (int i = 0; i < state.selection.ids.size(); i++) {
            const char* name;
            int selected_icon;
            int selected_health;
            int selected_max_health;
            if (state.selection.type == SELECTION_TYPE_UNITS || state.selection.type == SELECTION_TYPE_ENEMY_UNIT) {
                const unit_t& unit = state.units.get_by_id(state.selection.ids[i]);
                name = UNIT_DATA.at(unit.type).name;
                selected_icon = UI_BUTTON_UNIT_MINER + unit.type;
                selected_health = unit.health;
                selected_max_health = UNIT_DATA.at(unit.type).max_health;
            } else if (state.selection.type == SELECTION_TYPE_BUILDINGS || state.selection.type == SELECTION_TYPE_ENEMY_BUILDING) {
                const building_t& building = state.buildings.get_by_id(state.selection.ids[i]);
                name = BUILDING_DATA.at(building.type).name;
                selected_icon = UI_BUTTON_BUILD_HOUSE + (building.type - BUILDING_HOUSE);
                selected_health = building.health;
                selected_max_health = BUILDING_DATA.at(building.type).max_health;
            } else {
                name = "Gold Mine";
                selected_icon = UI_BUTTON_MINE;
                selected_health = state.mines.get_by_id(state.selection.ids[0]).gold_left;
                selected_max_health = 0;
            }

            if (state.selection.ids.size() != 1) {
                xy icon_position = SELECTION_LIST_TOP_LEFT + xy(((i % 10) * 34) - 12, (i / 10) * 34);
                render_sprite(SPRITE_UI_BUTTON, xy(0, 0), icon_position, RENDER_SPRITE_NO_CULL);
                render_sprite(SPRITE_UI_BUTTON_ICON, xy(selected_icon - 1, 0), icon_position, RENDER_SPRITE_NO_CULL);

                SDL_Rect healthbar_rect = (SDL_Rect) {
                    .x = icon_position.x + 1,
                    .y = icon_position.y + 32 - 5,
                    .w = 32 - 2,
                    .h = 4
                };
                SDL_Rect healthbar_subrect = healthbar_rect;
                healthbar_subrect.w = (healthbar_rect.w * selected_health) / selected_max_health;
                SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
                SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
                SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
                SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
                SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
            } else {
                render_text_with_text_frame(name, SELECTION_LIST_TOP_LEFT);

                render_sprite(SPRITE_UI_BUTTON, xy(0, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);
                render_sprite(SPRITE_UI_BUTTON_ICON, xy(selected_icon - 1, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);

                if (selected_max_health != 0) {
                    SDL_Rect healthbar_rect = (SDL_Rect) {
                        .x = SELECTION_LIST_TOP_LEFT.x,
                        .y = SELECTION_LIST_TOP_LEFT.y + 18 + 35,
                        .w = 64,
                        .h = 12
                    };
                    SDL_Rect healthbar_subrect = healthbar_rect;
                    healthbar_subrect.w = (healthbar_rect.w * selected_health) / selected_max_health;
                    SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
                    SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
                    SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
                    SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
                    SDL_RenderDrawRect(engine.renderer, &healthbar_rect);

                    char health_text[10];        
                    sprintf(health_text, "%i/%i", selected_health, selected_max_health);
                    SDL_Surface* health_text_surface = TTF_RenderText_Solid(engine.fonts[FONT_HACK], health_text, COLOR_WHITE);
                    if (health_text_surface == NULL) {
                        log_error("Unable to render text to surface: %s", TTF_GetError());
                        return;
                    }
                    SDL_Texture* health_text_texture = SDL_CreateTextureFromSurface(engine.renderer, health_text_surface);
                    if (health_text_texture == NULL) {
                        log_error("Unable to create text texture from surface: %s", SDL_GetError());
                        return;
                    }
                    SDL_Rect health_text_src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = health_text_surface->w, .h = health_text_surface->h };
                    SDL_Rect health_text_dst_rect = (SDL_Rect) { 
                        .x = healthbar_rect.x + (healthbar_rect.w / 2) - (health_text_surface->w / 2), 
                        .y = healthbar_rect.y + (healthbar_rect.h / 2) - (health_text_surface->h / 2),
                        .w = health_text_src_rect.w,
                        .h = health_text_src_rect.h
                    };
                    SDL_RenderCopy(engine.renderer, health_text_texture, &health_text_src_rect, &health_text_dst_rect);
                    SDL_DestroyTexture(health_text_texture);
                    SDL_FreeSurface(health_text_surface);
                // End if selected max health != 0
                } else {
                    char gold_left_str[17];
                    if (selected_health == 0) {
                        sprintf(gold_left_str, "Mine Collapsed!");
                    } else {
                        sprintf(gold_left_str, "Gold Left: %i", selected_health);
                    }
                    render_text(FONT_WESTERN8, gold_left_str, selected_health == 0 ? COLOR_RED : COLOR_GOLD, SELECTION_LIST_TOP_LEFT + xy(36, 22));
                }
            } // End if selection size is 1
        } // End for each selected unit
    } // End render selection list

    // UI Building queues
    if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        uint32_t building_index = state.buildings.get_index_of(state.selection.ids[0]);
        GOLD_ASSERT(building_index != INDEX_INVALID);
        const building_t& building = state.buildings[building_index];

        if (!building.queue.empty()) {
            int hovered_index = ui_get_building_queue_index_hovered(state);
            for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
                xy offset = building_queue_index == hovered_index ? xy(0, -1) : xy(0, 0);
                render_sprite(SPRITE_UI_BUTTON, xy(building_queue_index == hovered_index ? 1 : 0, 0), UI_BUILDING_QUEUE_POSITIONS[building_queue_index] + offset, RENDER_SPRITE_NO_CULL);
                render_sprite(SPRITE_UI_BUTTON_ICON, xy(building_queue_item_icon(building.queue[building_queue_index]) - 1, building_queue_index == hovered_index ? 1 : 0), UI_BUILDING_QUEUE_POSITIONS[building_queue_index] + offset, RENDER_SPRITE_NO_CULL);
            }

            static const SDL_Rect BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT = (SDL_Rect) {
                .x = UI_FRAME_BOTTOM_POSITION.x + BUILDING_QUEUE_TOP_LEFT.x + 32 + 4,
                .y = UI_FRAME_BOTTOM_POSITION.y + BUILDING_QUEUE_TOP_LEFT.y + 32 - 8,
                .w = 32 * 3 + (4 * 2),
                .h = 6
            };
            if (building.queue_timer == BUILDING_QUEUE_BLOCKED) {
                render_text(FONT_WESTERN8, "Build more houses.", COLOR_GOLD, xy(BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y - 12));
            } else {
                SDL_Rect building_queue_progress_bar_rect = (SDL_Rect) {
                    .x = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x,
                    .y = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y,
                    .w = (BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.w * (int)(building_queue_item_duration(building.queue[0]) - building.queue_timer)) / (int)building_queue_item_duration(building.queue[0]),
                    .h = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.h,
                };
                SDL_SetRenderDrawColor(engine.renderer, COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b, COLOR_WHITE.a);
                SDL_RenderFillRect(engine.renderer, &building_queue_progress_bar_rect);
                SDL_SetRenderDrawColor(engine.renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
                SDL_RenderDrawRect(engine.renderer, &BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT);
            }
        }
    }

    // UI Ferried units
    if (state.selection.type == SELECTION_TYPE_UNITS && state.selection.ids.size() == 1) {
        const unit_t& unit = state.units.get_by_id(state.selection.ids[0]);
        for (int i = 0; i < unit.ferried_units.size(); i++) {
            const unit_t& ferried_unit = state.units.get_by_id(unit.ferried_units[i]);
            xy icon_position = UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy((i % 4) * 34, (i / 4) * 33);
            render_sprite(SPRITE_UI_BUTTON, xy(0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(UI_BUTTON_UNIT_MINER + ferried_unit.type - 1, 0), icon_position, RENDER_SPRITE_NO_CULL);

            SDL_Rect healthbar_rect = (SDL_Rect) {
                .x = icon_position.x + 1,
                .y = icon_position.y + 32 - 5,
                .w = 32 - 2,
                .h = 4
            };
            SDL_Rect healthbar_subrect = healthbar_rect;
            healthbar_subrect.w = (healthbar_rect.w * ferried_unit.health) / UNIT_DATA.at(ferried_unit.type).max_health;
            SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;
            SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
            SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
            SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
            SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
        }
    }

    // UI Tooltip
    if (ui_get_ui_button_hovered(state) != -1) {
        uint32_t tooltip_gold_cost = 0;
        uint32_t tooltip_population_cost = 0;

        char tooltip_text[64];
        char* tooltip_text_ptr = tooltip_text;

        UiButton button = ui_get_ui_button(state, ui_get_ui_button_hovered(state));
        GOLD_ASSERT(button != UI_BUTTON_NONE && button != UI_BUTTON_MOVE);
        if (!ui_button_requirements_met(state, button)) {
            auto ui_button_requirements_it = UI_BUTTON_REQUIREMENTS.find(button);
            GOLD_ASSERT(ui_button_requirements_it != UI_BUTTON_REQUIREMENTS.end());

            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Requires ");
            switch (ui_button_requirements_it->second.type) {
                case UI_BUTTON_REQUIRES_BUILDING: {
                    tooltip_text_ptr += sprintf(tooltip_text_ptr, "%s", BUILDING_DATA.at(ui_button_requirements_it->second.building_type).name);
                    break;
                }
            }
        } else if (button == UI_BUTTON_ATTACK) {
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Attack");
        } else if (button == UI_BUTTON_STOP) {
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Stop");
        } else if (button == UI_BUTTON_BUILD) {
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Build");
        } else if (button == UI_BUTTON_CANCEL) {
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Cancel");
        } else if (button == UI_BUTTON_UNLOAD) {
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Unload");
        } else if (button >= UI_BUTTON_UNIT_MINER && button < UI_BUTTON_UNIT_MINER + UNIT_DATA.size()) {
            UnitType unit_type = (UnitType)(UNIT_MINER + (button - UI_BUTTON_UNIT_MINER));
            const unit_data_t& unit_data = UNIT_DATA.at(unit_type);
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Hire %s", unit_data.name);
            tooltip_gold_cost = unit_data.cost;
            tooltip_population_cost = unit_data.population_cost;
        } else if (button >= UI_BUTTON_BUILD_HOUSE && button < UI_BUTTON_BUILD_HOUSE + BUILDING_DATA.size()) {
            BuildingType building_type = (BuildingType)(BUILDING_HOUSE + (button - UI_BUTTON_BUILD_HOUSE));
            const building_data_t& building_data = BUILDING_DATA.at(building_type);
            tooltip_text_ptr += sprintf(tooltip_text_ptr, "Build %s", building_data.name);
            tooltip_gold_cost = building_data.cost;
        }

        if (ui_button_requirements_met(state, button)) {
            tooltip_text_ptr += sprintf(tooltip_text_ptr, " (");
            tooltip_text_ptr += sdlk_to_str(tooltip_text_ptr, hotkey_keymap.at(button));
            tooltip_text_ptr += sprintf(tooltip_text_ptr, ")");
        }

        SDL_Surface* tooltip_text_surface = TTF_RenderText_Solid(engine.fonts[FONT_WESTERN8], tooltip_text, COLOR_OFFBLACK);
        if (tooltip_text_surface == NULL) {
            log_error("Unable to create tooltip text surface: %s", TTF_GetError());
            return;
        }

        int tooltip_min_width = 10 + tooltip_text_surface->w;
        int tooltip_cell_width = tooltip_min_width / 8;
        int tooltip_cell_height = tooltip_gold_cost != 0 ? 5 : 3;
        if (tooltip_min_width % 8 != 0) {
            tooltip_cell_width++;
        }
        xy tooltip_top_left = xy(SCREEN_WIDTH - (tooltip_cell_width * 8) - 2, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y - (tooltip_cell_height * 8) - 2);
        for (int tooltip_x_index = 0; tooltip_x_index < tooltip_cell_width; tooltip_x_index++) {
            for (int tooltip_y_index = 0; tooltip_y_index < tooltip_cell_height; tooltip_y_index++) {
                xy tooltip_frame;
                if (tooltip_x_index == 0) {
                    tooltip_frame.x = 0;
                } else if (tooltip_x_index == tooltip_cell_width - 1) {
                    tooltip_frame.x = 2;
                } else {
                    tooltip_frame.x = 1;
                }
                if (tooltip_y_index == 0) {
                    tooltip_frame.y = 0;
                } else if (tooltip_y_index == tooltip_cell_height - 1) {
                    tooltip_frame.y = 2;
                } else {
                    tooltip_frame.y = 1;
                }
                render_sprite(SPRITE_UI_TOOLTIP_FRAME, tooltip_frame, tooltip_top_left + (xy(tooltip_x_index * 8, tooltip_y_index * 8)), RENDER_SPRITE_NO_CULL);
            }
        }

        SDL_Texture* tooltip_text_texture = SDL_CreateTextureFromSurface(engine.renderer, tooltip_text_surface);
        if (tooltip_text_texture == NULL) {
            log_error("Unable to create texture from tooltip text surface: %s", SDL_GetError());
            return;
        }
        SDL_Rect tooltip_text_rect = (SDL_Rect) {
            .x = tooltip_top_left.x + 5,
            .y = tooltip_top_left.y + 5,
            .w = tooltip_text_surface->w,
            .h = tooltip_text_surface->h
        };
        SDL_RenderCopy(engine.renderer, tooltip_text_texture, NULL, &tooltip_text_rect);

        SDL_DestroyTexture(tooltip_text_texture);
        SDL_FreeSurface(tooltip_text_surface);

        if (tooltip_gold_cost != 0) {
            render_sprite(SPRITE_UI_GOLD, xy(0, 0), tooltip_top_left + xy(5, 21), RENDER_SPRITE_NO_CULL);
            char gold_text[4];
            sprintf(gold_text, "%u", tooltip_gold_cost);
            render_text(FONT_WESTERN8, gold_text, COLOR_OFFBLACK, tooltip_top_left + xy(5 + 18, 23));
        }
        if (tooltip_population_cost != 0) {
            render_sprite(SPRITE_UI_HOUSE, xy(0, 0), tooltip_top_left + xy(5 + 18 + 32, 19), RENDER_SPRITE_NO_CULL);
            char population_text[4];
            sprintf(population_text, "%u", tooltip_population_cost);
            render_text(FONT_WESTERN8, population_text, COLOR_OFFBLACK, tooltip_top_left + xy(5 + 18 + 32 + 22, 23));
        }
    }

    // UI Status message
    if (state.ui_status_timer != 0) {
        render_text(FONT_HACK, state.ui_status_message.c_str(), COLOR_WHITE, xy(RENDER_TEXT_CENTERED, SCREEN_HEIGHT - 128));
    }

    // Resource counters
    char gold_text[8];
    sprintf(gold_text, "%u", state.player_gold[network_get_player_id()]);
    render_text(FONT_WESTERN8, gold_text, COLOR_WHITE, xy(SCREEN_WIDTH - 172 + 18, 4));
    render_sprite(SPRITE_UI_GOLD, xy(0, 0), xy(SCREEN_WIDTH - 172, 2), RENDER_SPRITE_NO_CULL);

    char population_text[8];
    sprintf(population_text, "%u/%u", match_get_player_population(state, network_get_player_id()), match_get_player_max_population(state, network_get_player_id()));
    render_text(FONT_WESTERN8, population_text, COLOR_WHITE, xy(SCREEN_WIDTH - 88 + 22, 4));
    render_sprite(SPRITE_UI_HOUSE, xy(0, 0), xy(SCREEN_WIDTH - 88, 0), RENDER_SPRITE_NO_CULL);

    // Scores
    char player_score_text[256];
    xy player_score_text_pos = xy(SCREEN_WIDTH - 4, 6);
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == PLAYER_STATUS_NONE) {
            continue;
        }
        // gets a percent but snapped to 10% intervals
        uint32_t win_percent = ((state.player_gold[player_id] * 10) / MATCH_WINNING_GOLD_AMOUNT) * 10;
        player_score_text_pos.y += 12;
        sprintf(player_score_text, "%s: %u%%", network_get_player(player_id).name, win_percent);
        render_text(FONT_HACK, player_score_text, COLOR_PLAYER.at(player_id), player_score_text_pos, TEXT_ANCHOR_TOP_RIGHT);
    }

    // Render minimap
    SDL_SetRenderTarget(engine.renderer, engine.minimap_texture);

    SDL_SetRenderDrawColor(engine.renderer, COLOR_SAND_DARK.r, COLOR_SAND_DARK.g, COLOR_SAND_DARK.b, COLOR_SAND_DARK.a);
    SDL_RenderClear(engine.renderer);

    SDL_SetRenderDrawColor(engine.renderer, COLOR_GOLD.r, COLOR_GOLD.g, COLOR_GOLD.b, COLOR_GOLD.a);
    for (const mine_t& mine : state.mines) {
        SDL_Rect mine_rect = (SDL_Rect) {
            .x = mine.cell.x,
            .y = mine.cell.y,
            .w = MINE_SIZE + 1,
            .h = MINE_SIZE + 1
        };
        SDL_RenderFillRect(engine.renderer, &mine_rect);
    }

    for (auto it : state.remembered_buildings) {
        const remembered_building_t& building = it.second;
        if (building.health == 0) {
            continue;
        }

        SDL_Rect building_rect = (SDL_Rect) { 
            .x = building.cell.x, 
            .y = building.cell.y, 
            .w = BUILDING_DATA.at(building.type).cell_size + 1,
            .h = BUILDING_DATA.at(building.type).cell_size + 1
        };
        SDL_Color color;
        if (building.player_id == network_get_player_id()) {
            color = state.buildings.get_by_id(it.first).taking_damage_flicker ? COLOR_RED : COLOR_GREEN;
        } else {
            color = COLOR_PLAYER.at((RecolorName)building.player_id);
        }
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(engine.renderer, &building_rect);
    }

    for (const unit_t& unit : state.units) {
        if (unit.player_id != network_get_player_id() && !map_is_cell_rect_revealed(state.map, network_get_player_id(), rect_t(unit.cell, unit_cell_size(unit.type)))) {
            continue;
        }
        if (unit.mode == UNIT_MODE_FERRY || unit.mode == UNIT_MODE_IN_MINE || unit.health == 0) {
            continue;
        }

        SDL_Rect unit_rect = (SDL_Rect) { 
            .x = unit.cell.x, 
            .y = unit.cell.y, 
            .w = UNIT_DATA.at(unit.type).cell_size + 1, 
            .h = UNIT_DATA.at(unit.type).cell_size + 1
        };
        SDL_Color color;
        if (unit.player_id == network_get_player_id()) {
            color = unit.taking_damage_flicker ? COLOR_RED : COLOR_GREEN;
        } else {
            color = COLOR_PLAYER.at((RecolorName)unit.player_id);
        }
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(engine.renderer, &unit_rect);
    }

    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect fog_rect = (SDL_Rect) { .x = 0, .y = 0, .w = 1, .h = 1 };
    for (int x = 0; x < state.map.width; x++) {
        for (int y = 0; y < state.map.height; y++) {
            FogType fog = state.map.player_fog[network_get_player_id()][x + (y * state.map.width)];
            if (fog == FOG_REVEALED) {
                continue;
            }

            fog_rect.x = x;
            fog_rect.y = y; 
            SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, fog == FOG_HIDDEN ? 255 : 128);
#ifndef GOLD_DEBUG_FOG_DISABLED
            SDL_RenderFillRect(engine.renderer, &fog_rect);
#endif
        }
    }
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);

    for (const alert_t& alert : state.alerts) {
        SDL_Color color;
        switch (alert.type) {
            case ALERT_UNIT_ATTACKED:
            case ALERT_BUILDING_ATTACKED:
                color = COLOR_RED;
                break;
            case ALERT_UNIT_FINISHED:
            case ALERT_BUILDING_FINISHED:
                color = COLOR_GREEN;
                break;
            case ALERT_MINE_COLLAPSED:
                color = COLOR_GOLD;
                break;
        }

        SDL_Rect alert_rect;
        switch (alert.type) {
            case ALERT_UNIT_ATTACKED:
            case ALERT_UNIT_FINISHED: {
                const unit_t& unit = state.units.get_by_id(alert.id);
                alert_rect = (SDL_Rect) {
                    .x = unit.cell.x,
                    .y = unit.cell.y,
                    .w = unit_cell_size(unit.type).x + 1,
                    .h = unit_cell_size(unit.type).y + 1
                };
                break;
            }
            case ALERT_BUILDING_ATTACKED:
            case ALERT_BUILDING_FINISHED: {
                const building_t& building = state.buildings.get_by_id(alert.id);
                alert_rect = (SDL_Rect) {
                    .x = building.cell.x,
                    .y = building.cell.y,
                    .w = building_cell_size(building.type).x + 1,
                    .h = building_cell_size(building.type).y + 1
                };
                break;
            }
            case ALERT_MINE_COLLAPSED: {
                const mine_t& mine = state.mines.get_by_id(alert.id);
                alert_rect = (SDL_Rect) {
                    .x = mine.cell.x,
                    .y = mine.cell.y,
                    .w = MINE_SIZE + 1,
                    .h = MINE_SIZE + 1
                };
            }
        }

        int alert_rect_margin = 3 + (alert.timer <= 60 ? 0 : ((alert.timer - 60) / 3));
        alert_rect.x -= alert_rect_margin;
        alert_rect.y -= alert_rect_margin;
        alert_rect.w += alert_rect_margin * 2;
        alert_rect.h += alert_rect_margin * 2;

        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawRect(engine.renderer, &alert_rect);
    }

    SDL_Rect camera_rect = (SDL_Rect) { 
        .x = state.camera_offset.x / TILE_SIZE,
        .y = state.camera_offset.y / TILE_SIZE,
        .w = SCREEN_WIDTH / TILE_SIZE,
        .h = 1 + ((SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE) 
    };
    SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(engine.renderer, &camera_rect);

    SDL_SetRenderTarget(engine.renderer, NULL);

    // Render the minimap texture
    SDL_Rect src_rect = (SDL_Rect) { .x = 0, .y = 0, .w = (int)state.map.width, .h = (int)state.map.height };
    SDL_Rect dst_rect = (SDL_Rect) { .x = MINIMAP_RECT.position.x, .y = MINIMAP_RECT.position.y, .w = MINIMAP_RECT.size.x, .h = MINIMAP_RECT.size.y };
    SDL_RenderCopy(engine.renderer, engine.minimap_texture, &src_rect, &dst_rect);

#ifdef GOLD_DEBUG_UNIT_STATE
    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        const unit_t& unit = state.units[unit_index];
        char unit_debug_text[128];
        static const char* MODE_STR[UNIT_MODE_DEATH_FADE + 1] = {
            "idle",
            "move",
            "blocked",
            "move_finshed",
            "build",
            "in_mine",
            "in_camp",
            "attack_windup",
            "attack_cooldown",
            "death",
            "death_fade"
        };
        static const char* TARGET_STR[UNIT_TARGET_ATTACK + 1] = {
            "none",
            "cell",
            "build",
            "unit",
            "building",
            "camp",
            "mine",
            "a_move"
        };
        sprintf(unit_debug_text, "%u: mode = %s target = %s", unit_index, MODE_STR[unit.mode], TARGET_STR[unit.target.type]);
        render_text(FONT_HACK, unit_debug_text, COLOR_BLACK, xy(0, 24 + (12 * unit_index)));
    }
#endif
}
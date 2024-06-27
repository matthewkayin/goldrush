#include "engine.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
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
        .size = 32 
    }}
};

SDL_Rect rect_to_sdl(const rect& r) {
    return (SDL_Rect) { .x = r.position.x, .y = r.position.y, .w = r.size.x, .h = r.size.y };
}

SDL_Color color_to_sdl(const color_t& color) {
    return (SDL_Color) { .r = color.r, .g = color.g, .b = color.b, .a = color.a };
}

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;

    // Input state
    bool mouse_button_state[INPUT_MOUSE_BUTTON_COUNT];
    bool mouse_button_previous_state[INPUT_MOUSE_BUTTON_COUNT];
    ivec2 mouse_position;
    std::string input_text;

    std::vector<TTF_Font*> fonts;
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

    log_info("%s initialized.", APP_NAME);
    return true;
}

void engine_quit() {
    for (TTF_Font* font : engine.fonts) {
        TTF_CloseFont(font);
    }

    SDL_DestroyRenderer(engine.renderer);
    SDL_DestroyWindow(engine.window);

    TTF_Quit();
    SDL_Quit();

    log_info("Application quit gracefully.");
    logger_quit();
}

// INPUT

bool input_pump_events() {
    memcpy(engine.mouse_button_previous_state, engine.mouse_button_state, INPUT_MOUSE_BUTTON_COUNT * sizeof(bool));

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Handle quit
        if (event.type == SDL_QUIT) {
            return false;
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
            if (event.button.button < INPUT_MOUSE_BUTTON_COUNT) {
                engine.mouse_button_state[event.button.button - 1] = event.type == SDL_MOUSEBUTTONDOWN;
            }
            break;
        }
        // Text input
        if (event.type == SDL_TEXTINPUT) {
            engine.input_text += std::string(event.text.text);
            break;
        }
        if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
            if (engine.input_text.length() > 0) {
                engine.input_text.pop_back();
            }
            break;
        }
    }

    return true;
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

void input_start_text_input(const rect& text_input_rect) {
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

void render_rect(rect r, color_t color, bool fill) {
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
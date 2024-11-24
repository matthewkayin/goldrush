#include "engine.h"

#include "logger.h"
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

    engine.window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
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

    log_info("Initialized renderer.");
    return true;
}

void engine_destroy_renderer() {
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
#include "defines.h"
#include "logger.h"
#include "platform.h"
#include <ctime>
#include <cstdio>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 && !defined GOLD_DEBUG
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return gold_main(0, NULL);
}
#else
int main(int argc, char** argv) {
    return gold_main(argc, argv);
}
#endif

struct engine_t {
    SDL_Window* window;
    SDL_Renderer* renderer;
};
static engine_t engine;

bool engine_init();
bool engine_init_renderer();
void engine_destroy_renderer();
void engine_quit();

int gold_main(int argc, char** argv) {
    char logfile_path[128];
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    sprintf(logfile_path, "./logs/%d-%02d-%02dT%02d%02d%02d.log", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);

    logger_init(logfile_path);
    platform_clock_init();
    engine_init();

    const double UPDATE_TIME = 1.0 / 60;
    double last_time = platform_get_absolute_time();
    double last_second = last_time;
    double update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    bool game_is_running = true;
    while (game_is_running) {
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
            log_info("FPS: %u / UPS: %u", fps, ups);
        }
        frames++;

        // INPUT
        if (update_accumulator >= UPDATE_TIME) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                // Handle quit
                if (event.type == SDL_QUIT) {
                    game_is_running = false;
                    break;
                }
            } // End while PollEvent
        } // End if should handle input

        // UPDATE
        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;
        }

        // RENDER
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderClear(engine.renderer);

        SDL_RenderPresent(engine.renderer);
    }

    engine_quit();
    logger_quit();
    
    return 0;
}

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

    log_info("Initialized renderer.");
    return true;
}

void engine_destroy_renderer() {
    SDL_DestroyRenderer(engine.renderer);
    log_info("Destroyed renderer");
}

void engine_quit() {
    engine_destroy_renderer();
    SDL_DestroyWindow(engine.window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}
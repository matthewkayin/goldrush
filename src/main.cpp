#include "defines.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/input.h"
#include "core/cursor.h"
#include "core/sound.h"
#include "core/options.h"
#include "network/network.h"
#include "render/render.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include <string>

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 and not defined GOLD_DEBUG
#include <windows.h>
#define MAX_ARGS 16

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance, LPSTR lp_cmd_line, int n_cmd_show) {
    char arg_buffer[MAX_ARGS][256];
    char* argv[MAX_ARGS];
    int argv_index = 0;
    int argc = 0;

    char* command_line = GetCommandLineA();
    int command_line_index = 0;
    bool is_in_quotes = false;
    while (command_line[command_line_index] != '\0') {
        if (command_line[command_line_index] == '"') {
            is_in_quotes = !is_in_quotes;
        } else {
            arg_buffer[argc][argv_index] = command_line[command_line_index];
            argv_index++;
        }
        command_line_index++;

        if ((command_line[command_line_index] == ' ' && !is_in_quotes) || command_line[command_line_index] == '\0') {
            command_line_index++;
            arg_buffer[argc][argv_index] = '\0';
            argv[argc] = arg_buffer[argc];
            argc++;
            argv_index = 0;
        }
    }

    return gold_main(argc, argv);
}
#else
int main(int argc, char** argv) {
    return gold_main(argc, argv);
}
#endif

int gold_main(int argc, char** argv) {
    std::string logfile_path = filesystem_get_timestamp_str() + ".log";

    // Parse system arguments
    for (int argn = 1; argn < argc; argn++) {
        #ifdef GOLD_DEBUG
            if (strcmp(argv[argn], "--logfile") == 0 && argn + 1 < argc) {
                argn++;
                logfile_path = argv[argn];
            }
        #endif
    }

    // Init SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return -1;
    }
    if (!TTF_Init()) {
        return -1;
    }

    logger_init(logfile_path.c_str());

    // Log initialization messages
    log_info("Initializing %s %s.", APP_NAME, APP_VERSION);
    #if defined(PLATFORM_WIN32)
        log_info("Detected platform WIN32.");
    #elif defined(PLATFORM_MACOS);
        log_info("Detected platform MACOS.");
    #elif defined(PLATFORM_LINUX)
        log_info("Detected platform LINUX.");
    #endif
    #ifdef GOLD_DEBUG
        log_info("Debug build.");
    #elif
        log_info("Release build.");
    #endif

    // Create window
    SDL_Window* window = SDL_CreateWindow(APP_NAME, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);

    // Init subsystems
    if (!render_init(window)) {
        logger_quit();
        return -1;
    }
    if (!cursor_init()) {
        logger_quit();
        return -1;
    }
    if (!sound_init()) {
        logger_quit();
        return -1;
    }
    options_load();

    bool is_running = true;
    const uint64_t UPDATE_DURATION = SDL_NS_PER_SECOND / UPDATES_PER_SECOND;
    uint64_t last_time = SDL_GetTicksNS();
    uint64_t last_second = last_time;
    uint64_t update_accumulator = 0;
    uint32_t frames = 0;
    uint32_t fps = 0;

    while (is_running) {
        // Timekeep
        uint64_t current_time = SDL_GetTicksNS();
        update_accumulator += current_time - last_time;
        last_time = current_time;

        if (current_time - last_second >= SDL_NS_PER_SECOND) {
            fps = frames;
            frames = 0;
            last_second += SDL_NS_PER_SECOND;
        }
        frames++;

        // Update
        while (update_accumulator >= UPDATE_DURATION) {
            update_accumulator -= UPDATE_DURATION;

            // Input
            input_poll_events();

            if (input_user_requests_exit()) {
                is_running = false;
                break;
            }

            // Network
            network_service();
            NetworkEvent event;
            while (network_poll_events(&event)) {
            }
        }

        sound_update();

        // Render
        render_prepare_frame();

        render_sprite_frame(SPRITE_UNIT_WAGON, ivec2(0, 0), ivec2(100, 100), 0, 0);
        render_sprite_frame(SPRITE_UNIT_WAGON, ivec2(0, 0), ivec2(200, 200), 0, 2);

        char fps_text[16];
        sprintf(fps_text, "FPS: %u", fps);
        render_text(FONT_HACK_WHITE, fps_text, ivec2(10, 10));

        render_vertical_line(50, 100, 200, RENDER_COLOR_WHITE);
        render_fill_rect({ .x = 400, .y = 200, .w = 20, .h = 40 }, RENDER_COLOR_RED);
        render_draw_rect({ .x = 200, .y = 200, .w = 20, .h = 40 }, RENDER_COLOR_GREEN);

        render_present_frame();
    }

    // Quit subsystems
    network_quit();
    sound_quit();
    cursor_quit();
    render_quit();
    logger_quit();

    return 0;
}
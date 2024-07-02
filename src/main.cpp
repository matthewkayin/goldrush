#include "defines.h"
#include "logger.h"
#include "engine.h"
#include "util.h"
#include "menu.h"
#include "match.h"
#include "network.h"
#include "platform.h"
#include <cstdint>
#include <cstdio>
#include <string>

const double UPDATE_TIME = 1.0 / 60.0;

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
    menu_init();

    double last_time = platform_get_absolute_time();
    double last_second = last_time;
    double update_accumulator = 0.0;
    /*
    uint32_t last_time = engine_get_ticks();
    uint32_t last_second = last_time;
    uint32_t accumulator = 0;
    */
    uint32_t frames = 0;
    uint32_t fps = 0;
    uint32_t updates = 0;
    uint32_t ups = 0;

    while (engine_is_running()) {
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

        while (update_accumulator >= UPDATE_TIME) {
            update_accumulator -= UPDATE_TIME;
            updates++;

            // INPUT
            input_pump_events();

            // UPDATE
            switch (mode) {
                case MODE_MENU: {
                    menu_update();
                    if (menu_get_mode() == MENU_MODE_MATCH_START) {
                        match_init();
                        mode = MODE_MATCH;
                    }
                    break;
                }
                case MODE_MATCH: {
                    match_update();
                    break;
                }
            }
        }

        // RENDER
        render_clear();

        switch (mode) {
            case MODE_MENU:
                menu_render();
                break;
            case MODE_MATCH:
                match_render();
                break;
        }

        char fps_text[16];
        sprintf(fps_text, "FPS: %u", fps);
        char ups_text[16];
        sprintf(ups_text, "UPS: %u", ups);
        render_text(FONT_HACK, fps_text, COLOR_BLACK, ivec2(0, 0));
        render_text(FONT_HACK, ups_text, COLOR_BLACK, ivec2(0, 12));

        render_present();
    } // End while running

    network_quit();
    engine_quit();
    logger_quit();

    return 0;
}
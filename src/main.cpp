#include "defines.h"
#include "logger.h"
#include "engine.h"
#include "util.h"
#include "menu.h"
#include "network.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <chrono>

const auto FRAME_TIME = std::chrono::seconds(1) / 60.0;

int main(int argc, char** argv) {
    std::string logfile_path = "goldrush.log";
    xy window_size = xy(1280, 720);
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

    log_info("%u %u %u %u", COLOR_SAND.r, COLOR_SAND.g, COLOR_SAND.b, COLOR_SAND.a);

    bool is_running = true;

    bool is_in_menu = true;
    menu_t menu;

    auto last_time = std::chrono::system_clock::now();
    auto last_second = last_time;
    unsigned int frames = 0;
    unsigned int fps = 0;

    while (is_running) {
        // TIMEKEEP
        auto current_time = std::chrono::system_clock::now();
        while (current_time - last_time < FRAME_TIME) {
            current_time = std::chrono::system_clock::now();
        }

        if (current_time - last_second >= std::chrono::seconds(1)) {
            fps = frames;
            frames = 0;
            last_second += std::chrono::seconds(1);
        }
        frames++;

        // INPUT
        is_running = input_pump_events();
        network_poll_events();

        // UPDATE
        if (is_in_menu) {
            menu.update();
        }

        // RENDER
        render_clear();

        if (is_in_menu) {
            menu.render();
        }

        char fps_text[16];
        sprintf(fps_text, "FPS: %u", fps);
        render_text(FONT_HACK, fps_text, COLOR_BLACK, xy(0, 0));

        render_present();
    } // End while running

    network_quit();
    engine_quit();
    logger_quit();

    return 0;
}
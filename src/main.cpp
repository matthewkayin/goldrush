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

int main() {
    logger_init();
    if (!engine_init()) {
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

        // UPDATE
        if (is_in_menu) {
            menu.update();
        }

        // RENDER
        render_clear(COLOR_BLACK);

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
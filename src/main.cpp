#include "defines.h"
#include "logger.h"
#include "engine.h"
#include "util.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <chrono>

const auto FRAME_TIME = std::chrono::seconds(1) / 60.0;

// MENU
static const int MENU_TEXT_INPUT_WIDTH = 264;
static const int MENU_TEXT_INPUT_HEIGHT = 35;
static const rect MENU_TEXT_INPUT_RECT = rect(xy((SCREEN_WIDTH / 2) - (MENU_TEXT_INPUT_WIDTH / 2), (SCREEN_HEIGHT / 2) - (MENU_TEXT_INPUT_HEIGHT / 2)), 
                                              xy(MENU_TEXT_INPUT_WIDTH, MENU_TEXT_INPUT_HEIGHT));
static const rect MENU_HOST_BUTTON_RECT = rect(xy(MENU_TEXT_INPUT_RECT.position.x + 112, MENU_TEXT_INPUT_RECT.position.y + MENU_TEXT_INPUT_RECT.size.y + 32), 
                                               xy(76, 30));
static const rect MENU_JOIN_BUTTON_RECT = rect(xy(MENU_HOST_BUTTON_RECT.position.x + MENU_HOST_BUTTON_RECT.size.x + 8, MENU_HOST_BUTTON_RECT.position.y), 
                                               xy(72, MENU_HOST_BUTTON_RECT.size.y));

enum MenuButton {
    MENU_BUTTON_NONE,
    MENU_BUTTON_HOST,
    MENU_BUTTON_JOIN
};

struct menu_state_t {
    std::string status_text;
    MenuButton button_hovered = MENU_BUTTON_NONE;
};

void menu_update(menu_state_t& menu_state);
void menu_render(const menu_state_t& menu_state);

int main() {
    logger_init();
    if (!engine_init()) {
        log_info("Closing logger...");
        logger_quit();
        return 1;
    }

    log_info("%u %u %u %u", COLOR_SAND.r, COLOR_SAND.g, COLOR_SAND.b, COLOR_SAND.a);

    bool is_running = true;

    bool is_in_menu = true;
    menu_state_t menu_state;

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

        last_time = current_time;
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
            menu_update(menu_state);
        }

        // RENDER
        render_clear(COLOR_BLACK);

        if (is_in_menu) {
            menu_render(menu_state);
        }

        char fps_text[16];
        sprintf(fps_text, "FPS: %u", fps);
        render_text(FONT_HACK, fps_text, COLOR_BLACK, xy(0, 0));

        render_present();
    } // End while running

    engine_quit();
    return 0;
}

void menu_update(menu_state_t& menu_state) {
    xy mouse_position = input_get_mouse_position();

    if (MENU_HOST_BUTTON_RECT.has_point(mouse_position)) {
        menu_state.button_hovered = MENU_BUTTON_HOST;
    } else if (MENU_JOIN_BUTTON_RECT.has_point(mouse_position)) {
        menu_state.button_hovered = MENU_BUTTON_JOIN;
    } else {
        menu_state.button_hovered = MENU_BUTTON_NONE;
    }

    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        bool input_text_focused = MENU_TEXT_INPUT_RECT.has_point(mouse_position);
        bool is_text_input_active = input_is_text_input_active();
        if (input_text_focused && !is_text_input_active) {
            input_start_text_input(MENU_TEXT_INPUT_RECT);
        } else if (!input_text_focused && is_text_input_active) {
            input_stop_text_input();
        }
    }
}

void menu_render(const menu_state_t& menu_state) {
    render_clear(COLOR_SAND);

    render_text(FONT_WESTERN32, "GOLD RUSH", COLOR_BLACK, xy(RENDER_TEXT_CENTERED, 36));

    char version_string[16];
    sprintf(version_string, "Version %s", APP_VERSION);
    render_text(FONT_WESTERN8, version_string, COLOR_BLACK, xy(4, SCREEN_HEIGHT - 14));

    if (menu_state.status_text.length() != 0) {
        render_text(FONT_WESTERN8, menu_state.status_text.c_str(), COLOR_RED, xy(RENDER_TEXT_CENTERED, MENU_TEXT_INPUT_RECT.position.y - 32));
    }

    render_text(FONT_WESTERN8, "USERNAME", COLOR_BLACK, MENU_TEXT_INPUT_RECT.position + xy(1, -13));
    render_rect(MENU_TEXT_INPUT_RECT, COLOR_BLACK);

    render_text(FONT_WESTERN16, input_get_text_input_value(), COLOR_BLACK, MENU_TEXT_INPUT_RECT.position + xy(2, MENU_TEXT_INPUT_RECT.size.y - 4), TEXT_ANCHOR_BOTTOM_LEFT);

    color_t button_color = menu_state.button_hovered == MENU_BUTTON_HOST ? COLOR_WHITE : COLOR_BLACK;
    render_text(FONT_WESTERN16, "HOST", button_color, MENU_HOST_BUTTON_RECT.position + xy(4, 4));
    render_rect(MENU_HOST_BUTTON_RECT, button_color);

    button_color = menu_state.button_hovered == MENU_BUTTON_JOIN ? COLOR_WHITE : COLOR_BLACK;
    render_text(FONT_WESTERN16, "JOIN", button_color, MENU_JOIN_BUTTON_RECT.position + xy(4, 4));
    render_rect(MENU_JOIN_BUTTON_RECT, button_color);
}
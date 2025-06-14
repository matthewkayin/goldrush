#include "feedback.h"

#include "report.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/input.h"
#include "core/options.h"
#include "util.h"
#include "menu/ui.h"
#include "render/render.h"
#include "math/gmath.h"
#include <curl/curl.h>
#include <unordered_map>
#include <string>

#ifdef PLATFORM_WIN32
    #include <windows.h>
    #include <DbgHelp.h>

    LONG WINAPI feedback_on_crash(EXCEPTION_POINTERS* exception_pointers) {
        // Build dumpfile path
        char dumpfile_path[128];
        char* dumpfile_ptr = dumpfile_path;
        dumpfile_ptr += sprintf(dumpfile_ptr, "logs/");
        dumpfile_ptr += sprintf_timestamp(dumpfile_ptr);
        dumpfile_ptr += sprintf(dumpfile_ptr, ".dmp");
        char dumpfile_fullpath[256];
        filesystem_get_data_path(dumpfile_fullpath, dumpfile_path);

        // Save dumpfile
        HANDLE dump_file = CreateFileA((LPCSTR)dumpfile_fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (dump_file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION dump_info;
            dump_info.ExceptionPointers = exception_pointers;
            dump_info.ThreadId = GetCurrentThreadId();
            dump_info.ClientPointers = TRUE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, MiniDumpNormal, &dump_info, NULL, NULL);
            CloseHandle(dump_file);
        }

        // Save logs
        logger_quit();

        // Send crash report
        feedback_report_send((FeedbackTicket) {
            .name = "Crash Report",
            .description = "Gold Rush caught an unhandled exception.",
            .attachments = { std::string(dumpfile_fullpath), logger_get_path() }
        });

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif

enum FeedbackMode {
    FEEDBACK_CLOSED,
    FEEDBACK_WELCOME,
    FEEDBACK_OPEN
};

enum FeedbackType {
    FEEDBACK_TYPE_FEEDBACK,
    FEEDBACK_TYPE_BUG
};

static const Rect WELCOME_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (336 / 2),
    .y = (SCREEN_HEIGHT / 2) - 72,
    .w = 336, .h = 112
};

static const Rect FEEDBACK_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (364 / 2),
    .y = 32,
    .w = 364, .h = 264
};

static const std::vector<std::string> FEEDBACK_TYPE_STRS = { "Feedback", "Bug" };
static const std::vector<std::string> FEEDBACK_YES_NO_STRS = { "No", "Yes" };
static const size_t FEEDBACK_NAME_MAX = 42;
static const size_t FEEDBACK_DESC_MAX = 268;
static const ivec2 SCREENSHOT_MINI_SIZE = ivec2(SCREEN_WIDTH / 5, SCREEN_HEIGHT / 5);

struct FeedbackState {
    FeedbackMode mode;
    UI ui;
    uint32_t feedback_type;
    uint32_t include_logs;
    uint32_t include_replay;
    uint32_t include_screenshot;
    std::string feedback_name;
    std::string feedback_desc;
    char screenshot_path[256];
    bool screenshot_successful;
};
static FeedbackState state;

void feedback_ui_yes_no(UI& ui, const char* prompt, uint32_t* value, bool disabled);

void feedback_init() {
    curl_global_init(CURL_GLOBAL_ALL);
    #ifdef PLATFORM_WIN32
        SetUnhandledExceptionFilter(feedback_on_crash);
    #endif

    state.ui = ui_init();
    state.feedback_type = FEEDBACK_TYPE_BUG;
    state.mode = FEEDBACK_WELCOME;
    state.include_logs = 1;
    state.include_replay = 1;
    state.include_screenshot = 1;
}

void feedback_quit() {
    curl_global_cleanup();
}

bool feedback_is_open() {
    return state.mode != FEEDBACK_CLOSED;
}

void feedback_update() {
    if (input_is_action_just_pressed(INPUT_ACTION_FEEDBACK_MENU)) {
        if (state.mode != FEEDBACK_OPEN) {
            char screenshot_subpath[128];
            char* screenshot_path_ptr = screenshot_subpath;
            screenshot_path_ptr += sprintf(screenshot_path_ptr, "screenshots/");
            screenshot_path_ptr += sprintf_timestamp(screenshot_path_ptr);
            screenshot_path_ptr += sprintf(screenshot_path_ptr, ".png");
            filesystem_get_data_path(state.screenshot_path, screenshot_subpath);
            state.screenshot_successful = render_take_screenshot(state.screenshot_path);
            state.include_screenshot = (uint32_t)state.screenshot_successful;

            state.mode = FEEDBACK_OPEN;
        } else {
            state.mode = FEEDBACK_CLOSED;
        }
    }
    if (state.mode == FEEDBACK_CLOSED) {
        return;
    }

    ui_begin(state.ui);
    ui_screen_shade(state.ui);

    if (state.mode == FEEDBACK_WELCOME) {
        ui_frame_rect(state.ui, WELCOME_RECT);

        ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Gold Rush Beta");
        ui_element_position(state.ui, ivec2(WELCOME_RECT.x + (WELCOME_RECT.w / 2) - (header_text_size.x / 2), WELCOME_RECT.y + 6));
        ui_text(state.ui, FONT_HACK_GOLD, "Gold Rush Beta");

        ui_begin_column(state.ui, ivec2(WELCOME_RECT.x + 8, WELCOME_RECT.y + 26), 4);
            ui_text(state.ui, FONT_HACK_GOLD, "Welcome to the Gold Rush Beta!");
            ui_text(state.ui, FONT_HACK_GOLD, "If you find any bugs or just want to provide feedback,");
            ui_text(state.ui, FONT_HACK_GOLD, "you can press F9 to open the feedback menu.");
            ui_element_position(state.ui, ivec2((WELCOME_RECT.w - 16) / 2, 4));
            if (ui_button(state.ui, "Ok", ivec2(-1, -1), true)) {
                state.mode = FEEDBACK_CLOSED;
            }
        ui_end_container(state.ui);
    } else if (state.mode == FEEDBACK_OPEN) {
        ui_frame_rect(state.ui, FEEDBACK_RECT);

        const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);

        ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Submit Feedback");
        ui_element_position(state.ui, ivec2(FEEDBACK_RECT.x + (FEEDBACK_RECT.w / 2) - (header_text_size.x / 2), FEEDBACK_RECT.y + 6));
        ui_text(state.ui, FONT_HACK_GOLD, "Submit Feedback");

        ui_begin_column(state.ui, ivec2(FEEDBACK_RECT.x + 8, FEEDBACK_RECT.y + 26), 4);
            ui_element_size(state.ui, ivec2(0, dropdown_sprite_info.frame_height));
            ui_begin_row(state.ui, ivec2(0, 0), 0);
                ui_element_position(state.ui, ivec2(0, 2));
                ui_text(state.ui, FONT_HACK_GOLD, "What type of feedback is this?");

                ui_element_position(state.ui, ivec2(FEEDBACK_RECT.w - 16 - dropdown_sprite_info.frame_width, 0));
                ui_dropdown(state.ui, UI_DROPDOWN_MINI, &state.feedback_type, FEEDBACK_TYPE_STRS, false);
            ui_end_container(state.ui);

            ui_text_input(state.ui, "Feedback Name: ", ivec2(FEEDBACK_RECT.w - 16, 24), &state.feedback_name, FEEDBACK_NAME_MAX);
            ui_text_input(state.ui, "Description: ", ivec2(FEEDBACK_RECT.w - 16, 92), &state.feedback_desc, FEEDBACK_DESC_MAX, true);

            feedback_ui_yes_no(state.ui, "Include logs?", &state.include_logs, false);
            feedback_ui_yes_no(state.ui, "Include last replay?", &state.include_replay, false);
            feedback_ui_yes_no(state.ui, "Include screenshot?", &state.include_screenshot, !state.screenshot_successful);
            if (state.screenshot_successful) {
                ui_element_size(state.ui, ivec2(0, SCREENSHOT_MINI_SIZE.y));
                ui_text(state.ui, FONT_HACK_WHITE, "Click the screenshot to preview.");
            } else {
                ui_text(state.ui, FONT_HACK_WHITE, "Error: could not save screenshot.");
            }
        ui_end_container(state.ui);
    }
}

void feedback_ui_yes_no(UI& ui, const char* prompt, uint32_t* value, bool disabled) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    ui_element_size(state.ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(state.ui, ivec2(0, 0), 0);
        ui_element_position(state.ui, ivec2(0, 2));
        ui_text(state.ui, FONT_HACK_GOLD, prompt);

        ui_element_position(state.ui, ivec2(FEEDBACK_RECT.w - 16 - dropdown_sprite_info.frame_width, 0));
        ui_dropdown(state.ui, UI_DROPDOWN_MINI, value, FEEDBACK_YES_NO_STRS, disabled);
    ui_end_container(state.ui);
}

void feedback_render() {
    if (state.mode == FEEDBACK_CLOSED) {
        return;
    }

    ui_render(state.ui);
    render_sprite_batch();

    if (state.screenshot_successful) {
        render_screenshot(ivec2(0, 0), ivec2(SCREEN_WIDTH / 4, SCREEN_HEIGHT / 4));
    }
}
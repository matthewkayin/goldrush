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

static const Rect WELCOME_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (336 / 2),
    .y = (SCREEN_HEIGHT / 2) - 72,
    .w = 336, .h = 112
};

struct FeedbackState {
    FeedbackMode mode;
    UI ui;
};
static FeedbackState state;

void feedback_init() {
    curl_global_init(CURL_GLOBAL_ALL);
    #ifdef PLATFORM_WIN32
        SetUnhandledExceptionFilter(feedback_on_crash);
    #endif

    state.mode = FEEDBACK_CLOSED;
    state.ui = ui_init();

    state.mode = FEEDBACK_WELCOME;
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
    }
}

void feedback_render() {
    if (state.mode == FEEDBACK_CLOSED) {
        return;
    }

    ui_render(state.ui);
    render_sprite_batch();
}
#include "feedback.h"

#include "report.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/input.h"
#include "util.h"
#include "menu/ui.h"
#include "render/render.h"
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

struct FeedbackState {
    bool is_open;
    UI ui;
};
static FeedbackState state;

void feedback_init() {
    curl_global_init(CURL_GLOBAL_ALL);
    #ifdef PLATFORM_WIN32
        SetUnhandledExceptionFilter(feedback_on_crash);
    #endif

    state.is_open = false;
    state.ui = ui_init();
}

void feedback_quit() {
    curl_global_cleanup();
}

bool feedback_is_open() {
    return state.is_open;
}

void feedback_update() {
    if (input_is_action_just_pressed(INPUT_ACTION_FEEDBACK_MENU)) {
        state.is_open = !state.is_open;
    }
    if (!state.is_open) {
        return;
    }

    ui_begin(state.ui);
    ui_screen_shade(state.ui);
}

void feedback_render() {
    if (!state.is_open) {
        return;
    }

    ui_render(state.ui);
    render_sprite_batch();
}
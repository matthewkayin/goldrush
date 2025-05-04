#include "platform.h"

#ifdef PLATFORM_WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "defines.h"
#include <ctime>
#include <cstdio>
#include <direct.h>
#include <vector>
#include <string>
#include "../util.h"

struct PlatformState {
    double clock_frequency;
    LARGE_INTEGER clock_start_time;
    std::vector<std::string> replay_files;
};
static PlatformState state;

void platform_clock_init() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    state.clock_frequency = 1.0 / (double)frequency.QuadPart;
    QueryPerformanceCounter(&state.clock_start_time);
}

double platform_get_absolute_time() {
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    return (double)(current_time.QuadPart - state.clock_start_time.QuadPart) * state.clock_frequency;
}

void platform_console_write(const char* message, int log_level) {
    #ifdef GOLD_DEBUG
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    switch (log_level) {
        case 0:
            // ERROR
            SetConsoleTextAttribute(console_handle, FOREGROUND_RED);
            break;
        case 1:
            // WARN
            SetConsoleTextAttribute(console_handle, FOREGROUND_RED | FOREGROUND_GREEN);
            break;
        case 2:
            // INFO
            SetConsoleTextAttribute(console_handle, FOREGROUND_GREEN);
            break;
        default:
            // TRACE
            SetConsoleTextAttribute(console_handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            break;
    }
    OutputDebugStringA(message);
    uint64_t length = strlen(message);
    LPDWORD number_written = 0;
    WriteConsoleA(GetStdHandle(log_level == 0 ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE), message, (DWORD)length, number_written, 0);
    SetConsoleTextAttribute(console_handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    #endif
}

void platform_get_resource_path(char* buffer, const char* subpath) {
    #ifdef GOLD_DEBUG
        sprintf(buffer, "../res/%s", subpath);
    #else
        sprintf(buffer, "./res/%s", subpath);
    #endif
}

void platform_get_datafile_path(char* buffer, const char* filename) {
    sprintf(buffer, "./%s", filename);
}

void platform_get_replay_path(char* buffer, const char* filename) {
    _mkdir("./replays");
    sprintf(buffer, "./replays/%s", filename);
}

void platform_search_replays_folder(const char* search_query) {
    state.replay_files.clear();

    char replay_search_filename[128];
    sprintf(replay_search_filename, "*%s*.rep", search_query);
    char replay_search_str[256];
    platform_get_replay_path(replay_search_str, replay_search_filename);

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(replay_search_str, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        std::string filename = std::string(find_data.cFileName);
        if (!filename_ends_in_rep(filename)) {
            continue;
        }

        // always put the latest replay first in the list
        if (filename == "latest.rep") {
            state.replay_files.insert(state.replay_files.begin(), filename);
        } else {
            state.replay_files.push_back(std::string(find_data.cFileName));
        }
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);
}

const char* platform_get_replay_file_name(size_t index) {
    return state.replay_files[index].c_str();
}

size_t platform_get_replay_file_count() {
    return state.replay_files.size();
}

#endif
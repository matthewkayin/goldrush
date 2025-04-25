#include "platform.h"

#ifdef PLATFORM_WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "defines.h"
#include <ctime>
#include <cstdio>

static double clock_frequency;
static LARGE_INTEGER clock_start_time;

void platform_clock_init() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    clock_frequency = 1.0 / (double)frequency.QuadPart;
    QueryPerformanceCounter(&clock_start_time);
}

double platform_get_absolute_time() {
    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    return (double)(current_time.QuadPart - clock_start_time.QuadPart) * clock_frequency;
}

void platform_console_write(const char* message, int log_level) {
    #ifdef GOLD_STD_OUT
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

#endif
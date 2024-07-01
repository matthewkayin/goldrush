#include "platform.h"

#ifdef PLATFORM_WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>

double platform_get_absolute_time() {
    static double clock_frequency = 0.0;
    if (!clock_frequency) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        clock_frequency = 1.0 / (double)frequency.QuadPart;
    }

    LARGE_INTEGER current_time;
    QueryPerformanceCounter(&current_time);
    return (double)current_time.QuadPart * clock_frequency;
}

void platform_console_write(const char* message) {
    HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(console_handle, 7);
    OutputDebugStringA(message);
    uint64_t length = strlen(message);
    LPDWORD number_written = 0;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), message, (DWORD)length, number_written, 0);
}

void platform_console_write_error(const char* message) {
    HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(console_handle, 4);
    OutputDebugStringA(message);
    uint64_t length = strlen(message);
    LPDWORD number_written = 0;
    WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)length, number_written, 0);
}


#endif
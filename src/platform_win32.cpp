#include "platform.h"

#ifdef PLATFORM_WIN32

#define WIN32_LEAN_AND_MEAN
#include "report.h"
#include <windows.h>
#include <DbgHelp.h>
#include <cstdint>
#include <ctime>
#include <cstdio>

static double clock_frequency;
static LARGE_INTEGER clock_start_time;
static FILE* logfile;
static char logfile_path[128];

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
}

LONG WINAPI platform_on_unhandled_exception(EXCEPTION_POINTERS* exception_pointers) {
    // Create dmp file
    char dumpfile_path[128];
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    sprintf(dumpfile_path, "./logs/gold-%d-%02d-%02dT%02d%02d%02d.dmp", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);
    HANDLE dump_file = CreateFile((LPCSTR)dumpfile_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (dump_file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dump_info;
        dump_info.ExceptionPointers = exception_pointers;
        dump_info.ThreadId = GetCurrentThreadId();
        dump_info.ClientPointers = TRUE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, MiniDumpNormal, &dump_info, NULL, NULL);
        CloseHandle(dump_file);
    }

    // Close logger
    fclose(logfile);
    report_send(logfile_path, dumpfile_path);

    return EXCEPTION_EXECUTE_HANDLER;
}


void platform_setup_unhandled_exception_filter(FILE* p_logfile, const char* p_logfile_path) {
    strcpy(logfile_path, p_logfile_path);
    logfile = p_logfile;
    SetUnhandledExceptionFilter(platform_on_unhandled_exception);
}

#endif
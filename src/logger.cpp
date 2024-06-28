#include "logger.h"

#include "asserts.h"
#include "defines.h"
#include "util.h"
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cstdio>

void platform_console_write(const char* message);
void platform_console_write_error(const char* message);

static FILE* logfile;

bool logger_init(const char* logfile_path) {
    logfile = fopen(logfile_path, "w");
    if (logfile == NULL) {
        log_error("Unable to open log file for writing.");
        return false;
    }

    return true;
}

void logger_quit() {
    fclose(logfile);
}

void logger_output(bool is_error, const char* message, ...) {
    const int MESSAGE_LENGTH = 32000;
    char out_message[MESSAGE_LENGTH];
    memset(out_message, 0, sizeof(out_message));

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    char* out_ptr = out_message;
    while (*message != '\0') {
        if (*message != '%') {
            *out_ptr = *message;
            out_ptr++;
            message++;
            continue;
        }

        message++;
        if (*message == '\0') {
            break;
        }

        switch (*message) {
            case 'c': {
                out_ptr += sprintf(out_ptr, "%c", (char)va_arg(arg_ptr, int));
                break;
            }
            case 's': {
                out_ptr += sprintf(out_ptr, "%s", va_arg(arg_ptr, char*));
                break;
            }
            case 'i': {
                out_ptr += sprintf(out_ptr, "%i", va_arg(arg_ptr, int));
                break;
            }
            case 'u': {
                out_ptr += sprintf(out_ptr, "%u", va_arg(arg_ptr, unsigned int));
                break;
            }
            case 'f': {
                out_ptr += sprintf(out_ptr, "%f", va_arg(arg_ptr, double));
                break;
            }
            case 'd': {
                fp8 fp = va_arg(arg_ptr, fp8);
                out_ptr += sprintf(out_ptr, "%i.%i", fp.integer_part(), fp.fractional_value());
                break;
            }
            case 'x': {
                out_ptr += sprintf(out_ptr, "%x", va_arg(arg_ptr, unsigned int));
                break;
            }
            case 'r': {
                rect* r = va_arg(arg_ptr, rect*);
                out_ptr += sprintf(out_ptr, "<%i %i %i %i>", r->position.x, r->position.y, r->size.x, r->size.y);
                break;
            }
        }

        message++;
    }
    va_end(arg_ptr);

    char log_message[MESSAGE_LENGTH];
    sprintf(log_message, "%s\n", out_message);

    if (is_error) {
        platform_console_write_error(log_message);
    } else {
        platform_console_write(log_message);
    }

    fprintf(logfile, "%s", log_message);
    fflush(logfile);
}

// Definition of function declared in asserts.h
void report_assertion_failure(const char* expression, const char* message, const char* file, int line) {
    logger_output(true, "Assertion failure: %s, message: '%s', in file: %s, line %d\n", expression, message, file, line);
}

// Platform specific console output
// Console output is done this way so that we can get custom colored text based on log level

#ifdef PLATFORM_WIN32

#include <windows.h>

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

#else

void platform_console_write(const char* message) {
    printf("\x1B[0m%s", message);
}

void platform_console_write_error(const char* message) {
    fprintf(stderr, "\x1B[31m%s", message);
}

#endif
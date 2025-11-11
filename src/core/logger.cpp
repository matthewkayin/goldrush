#include "logger.h"

#include "core/filesystem.h"
#include <SDL3/SDL.h>
#include <cstdio>

static FILE* logfile;

bool logger_init(const char* logfile_path) {
    // Create log folder
    std::string log_folder_path = filesystem_get_data_path() + "/logs/";
    SDL_CreateDirectory(log_folder_path.c_str());

    std::string logfile_full_path = log_folder_path + logfile_path;

    // Open logfile
    logfile = fopen(logfile_full_path.c_str(), "w");
    if (logfile == NULL) {
        log_error("Unable to open log file for writing.");
        return false;
    }

    return true;
}

void logger_quit() {
    fclose(logfile);
}

void logger_output(LogLevel log_level, const char* message, ...) {
    const size_t MESSAGE_BUFFER_LENGTH = 32000;
    char out_message[MESSAGE_BUFFER_LENGTH];
    memset(out_message, 0, sizeof(out_message));

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(out_message, MESSAGE_BUFFER_LENGTH, message, arg_ptr);
    va_end(arg_ptr);

    const char* LOG_PREFIX[4] = { "ERROR", "WARN", "INFO", "DEBUG" };
    fprintf(logfile, "[%s]: %s\n", LOG_PREFIX[log_level], out_message);
    fflush(logfile);

    #ifdef GOLD_DEBUG
        printf("[%s]: %s\n", LOG_PREFIX[log_level], out_message);
    #endif
}

// Definition of function delcared in asserts.h
void logger_report_assertion_failure(const char* expression, const char* message, const char* file, int line) {
    logger_output(LOG_LEVEL_ERROR, "Assertion failure: %s, message: '%s', in file %s, line %d", expression, message, file, line);
}
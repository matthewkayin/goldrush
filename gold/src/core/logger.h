#pragma once

#include "defines.h"

enum LogLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_TRACE = 3
};

bool logger_init(const char* logfile_path);
void logger_quit();
void logger_output(LogLevel log_level, const char* message, ...);

#define log_error(message, ...) logger_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);

#if GOLD_LOG_LEVEL >= 1
    #define log_warn(message, ...) logger_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
    #define log_warn(message, ...)
#endif

#if GOLD_LOG_LEVEL >= 2
    #define log_info(message, ...) logger_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
    #define log_info(message, ...)
#endif

#if GOLD_LOG_LEVEL >= 3
    #define log_trace(message, ...) logger_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#else
    #define log_trace(message, ...)
#endif
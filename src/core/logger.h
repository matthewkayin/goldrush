#pragma once

#include "defines.h"

enum LogLevel {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

bool logger_init(const char* logfile_path);
void logger_quit();
void logger_output(LogLevel log_level, const char* message, ...);

#define log_error(message, ...) logger_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);

#if GOLD_LOG_LEVEL >= LOG_LEVEL_WARN
    #define log_warn(message, ...) logger_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
    #define log_warn(message, ...)
#endif

#if GOLD_LOG_LEVEL >= LOG_LEVEL_INFO
    #define log_info(message, ...) logger_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
    #define log_info(message, ...)
#endif

#if GOLD_LOG_LEVEL >= LOG_LEVEL_DEBUG
    #define log_debug(message, ...) logger_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__);
#else
    #define log_debug(message, ...)
#endif
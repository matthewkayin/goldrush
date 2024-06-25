#pragma once

#ifndef LOG_LEVEL
#define LOG_LEVEL 1
#endif

bool logger_init(const char* logfile_path);
void logger_quit();
void logger_output(bool is_error, const char* message, ...);

#define log_error(message, ...) logger_output(true, message, ##__VA_ARGS__);

#if LOG_LEVEL >= 1
    #define log_info(message, ...) logger_output(false, message, ##__VA_ARGS__);
#else
    #define log_info(message, ...)
#endif
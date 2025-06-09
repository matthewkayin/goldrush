#pragma once

#include <string>
#include <ctime>

inline bool filename_ends_in_rep(const std::string& filename) {
    return filename.size() >= 5 &&
            filename.compare(filename.size() - 4, 4, ".rep") == 0;
}

inline int sprintf_timestamp(char* buffer) {
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    return sprintf(buffer, "%d-%02d-%02dT%02d%02d%02d", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);
}
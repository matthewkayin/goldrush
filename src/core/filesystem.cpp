#include "filesystem.h"

#include "defines.h"
#include <SDL3/SDL.h>
#include <ctime>
#include <vector>

std::string filesystem_get_timestamp_str() {
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    char buffer[128];
    sprintf(buffer, "%d-%02d-%02dT%02d%02d%02d", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);
    return std::string(buffer);
}

std::string filesystem_get_data_path() {
    #ifdef GOLD_DEBUG
        return std::string("./");
    #else
        char* pref_path = SDL_GetPrefPath(NULL, APP_NAME);
        std::string path = std::string(pref_path);
        SDL_free(pref_path);
        return path;
    #endif
}

std::string filesystem_get_resource_path() {
    #ifdef GOLD_DEBUG
        return std::string("../res/");
    #else
        return std::string(SDL_GetBasePath());
    #endif
}

void filesystem_create_required_folders() {
    std::vector<std::string> folder_paths;
    folder_paths.push_back(filesystem_get_data_path() + "logs/");
    folder_paths.push_back(filesystem_get_data_path() + "replays/");

    for (const std::string& path : folder_paths) {
        SDL_CreateDirectory(path.c_str());
    }
}
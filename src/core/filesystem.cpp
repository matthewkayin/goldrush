#include "filesystem.h"

#include "defines.h"
#include <SDL3/SDL.h>
#include <cstdio>

void filesystem_get_resource_path(char* buffer, const char* subpath) {
    #ifdef GOLD_DEBUG
        sprintf(buffer, "../res/%s", subpath);
    #else
        sprintf(buffer, "%s%s", SDL_GetBasePath(), subpath);
    #endif
}

void filesystem_get_data_path(char* buffer, const char* subpath) {
    #ifdef GOLD_DEBUG
        sprintf(buffer, "./%s", subpath);
    #else
        char* pref_path = SDL_GetPrefPath(NULL, APP_NAME);
        sprintf(buffer, "%s%s", pref_path, subpath);
        SDL_free(pref_path);
    #endif
}

void filesystem_get_replay_path(char* buffer, const char* subpath) {
    #ifdef GOLD_DEBUG
        sprintf(buffer, "./replays/%s", subpath);
    #else
        char* pref_path = SDL_GetPrefPath(NULL, APP_NAME);
        sprintf(buffer, "%sreplays/%s", pref_path, subpath);
        SDL_free(pref_path);
    #endif
}

void filesystem_create_data_folder(const char* name) {
    char data_folder_path[256];
    filesystem_get_data_path(data_folder_path, name);
    SDL_CreateDirectory(data_folder_path);
}
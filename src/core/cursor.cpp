#include "cursor.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include "filesystem.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <unordered_map>

struct CursorParams {
    const char* path;
    int hot_x;
    int hot_y;
};

static const std::unordered_map<CursorName, CursorParams> CURSOR_PARAMS = {
    { CURSOR_DEFAULT, (CursorParams) {
        .path = "cursor/default.png",
        .hot_x = 0,
        .hot_y = 0
    }},
    { CURSOR_TARGET, (CursorParams) {
        .path = "cursor/target.png",
        .hot_x = 9,
        .hot_y = 9
    }}
};

static SDL_Cursor* cursors[CURSOR_COUNT];
static CursorName current_cursor;

bool cursor_init() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        const CursorParams params = CURSOR_PARAMS.at((CursorName)cursor);
        std::string path = filesystem_get_resource_path() + "sprite/" + params.path;

        SDL_Surface* cursor_surface = SDL_LoadPNG(path.c_str());
        if (cursor_surface == NULL) {
            log_error("Unable to load surface %s for cursor: %s", path.c_str(), SDL_GetError());
            return false;
        }

        cursors[cursor] = SDL_CreateColorCursor(cursor_surface, params.hot_x, params.hot_y);
        if (cursors[cursor] == NULL) {
            log_error("Unable to create cursor with path %s: %s", path.c_str(), SDL_GetError());
            return false;
        }

        SDL_DestroySurface(cursor_surface);
    }

    current_cursor = CURSOR_DEFAULT;
    SDL_SetCursor(cursors[current_cursor]);

    return true;
}

void cursor_quit() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        SDL_DestroyCursor(cursors[cursor]);
    }
}

void cursor_set(CursorName cursor) {
    if (current_cursor == cursor) {
        return;
    }

    current_cursor = cursor;
    SDL_SetCursor(cursors[current_cursor]);
}
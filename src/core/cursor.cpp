#include "cursor.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <cstdio>
#include <unordered_map>

struct CursorParams {
    const char* path;
    int hot_x;
    int hot_y;
};

static const std::unordered_map<CursorName, CursorParams> CURSOR_PARAMS = {
    { CURSOR_DEFAULT, (CursorParams) {
        .path = "sprite/ui_cursor.png",
        .hot_x = 0,
        .hot_y = 0
    }},
    { CURSOR_TARGET, (CursorParams) {
        .path = "sprite/ui_cursor_target.png",
        .hot_x = 9,
        .hot_y = 9
    }}
};

static SDL_Cursor* cursors[CURSOR_COUNT];
static CursorName current_cursor;

bool cursor_init() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        const CursorParams params = CURSOR_PARAMS.at((CursorName)cursor);

        char path[128];
        sprintf(path, "%s%s", RESOURCE_PATH, params.path);

        SDL_Surface* cursor_surface = IMG_Load(path);
        if (cursor_surface == NULL) {
            log_error("Unable to load surface %s for cursor: %s", path, SDL_GetError());
            return false;
        }

        cursors[cursor] = SDL_CreateColorCursor(cursor_surface, params.hot_x, params.hot_y);
        if (cursors[cursor] == NULL) {
            log_error("Unable to create cursor with path %s: %s", path, SDL_GetError());
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
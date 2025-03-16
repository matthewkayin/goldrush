#include "cursor.h"

#include "defines.h"
#include "asserts.h"
#include "logger.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <unordered_map>

struct cursor_params_t {
    const char* path;
    int hot_x;
    int hot_y;
};

static const std::unordered_map<cursor_name, cursor_params_t> CURSOR_PARAMS = {
    { CURSOR_DEFAULT, (cursor_params_t) {
        .path = "sprite/ui_cursor.png",
        .hot_x = 0,
        .hot_y = 0
    }},
    { CURSOR_TARGET, (cursor_params_t) {
        .path = "sprite/ui_cursor_target.png",
        .hot_x = 9,
        .hot_y = 9
    }}
};

static SDL_Cursor* cursors[CURSOR_COUNT];
static cursor_name current_cursor;

bool cursor_init() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        const cursor_params_t params = CURSOR_PARAMS.at((cursor_name)cursor);

        char path[128];
        sprintf(path, "%s%s", RESOURCE_PATH, params.path);

        SDL_Surface* cursor_surface = IMG_Load(path);
        if (cursor_surface == NULL) {
            log_error("Unable to load surface %s for cursor: %s", path, IMG_GetError());
            return false;
        }

        cursors[cursor] = SDL_CreateColorCursor(cursor_surface, params.hot_x, params.hot_y);
        if (cursors[cursor] == NULL) {
            log_error("Unable to create cursor with path %s: %s", path, SDL_GetError());
            return false;
        }

        SDL_FreeSurface(cursor_surface);
    }

    current_cursor = CURSOR_DEFAULT;
    SDL_SetCursor(cursors[current_cursor]);

    return true;
}

void cursor_quit() {
    for (int cursor = 0; cursor < CURSOR_COUNT; cursor++) {
        SDL_FreeCursor(cursors[cursor]);
    }
}

void cursor_set(cursor_name cursor) {
    if (current_cursor == cursor) {
        return;
    }

    current_cursor = cursor;
    SDL_SetCursor(cursors[current_cursor]);
}
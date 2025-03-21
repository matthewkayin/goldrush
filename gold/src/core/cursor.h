#pragma once

enum CursorName {
    CURSOR_DEFAULT,
    CURSOR_TARGET,
    CURSOR_COUNT
};

bool cursor_init();
void cursor_quit();

void cursor_set(CursorName cursor);
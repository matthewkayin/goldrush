#pragma once

enum cursor_name {
    CURSOR_DEFAULT,
    CURSOR_TARGET,
    CURSOR_COUNT
};

bool cursor_init();
void cursor_quit();

void cursor_set(cursor_name cursor);
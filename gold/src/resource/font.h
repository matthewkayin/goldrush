#pragma once

enum font_name {
    FONT_HACK,
    FONT_COUNT
};

struct font_params_t {
    const char* path;
    int size;
};

font_params_t resource_get_font_params(font_name name);

#pragma once

#include <cstdint>

struct sprite_t {
    uint32_t texture;
    int width;
    int height;
};

sprite_t* sprite_load(const char* name);
void sprite_free(sprite_t* sprite);
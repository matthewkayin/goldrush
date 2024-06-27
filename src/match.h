#pragma once

#include <cstdint>

struct match_t {
    uint32_t map_width;
    uint32_t map_height;

    match_t();
    void update();
    void render() const;
};
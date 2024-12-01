#include "match.h"

#include "engine.h"

void map_init(match_state_t& state, uint32_t width, uint32_t height) {
    state.map_width = width;
    state.map_height = height;

    state.map_tiles = std::vector<tile_t>(state.map_width * state.map_height, (tile_t) {
        .index = engine.tile_index[TILE_SAND],
        .elevation = 0
    });
    for (uint32_t y = 0; y < state.map_height; y++) {
        for (uint32_t x = 0; x < state.map_width; x++) {
            if (x == 0 || y == 0 || x == state.map_width - 1 || y == state.map_height - 1) {
                state.map_tiles[x + (y * state.map_width)].index = TILE_SAND3;
            } else if ((x + (y * state.map_width)) % 3 == 0) {
                state.map_tiles[x + (y * state.map_width)].index = TILE_SAND2;
            }
        }
    }
}
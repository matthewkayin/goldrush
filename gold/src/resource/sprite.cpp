#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<sprite_name, sprite_params_t> SPRITE_PARAMS = {
    { SPRITE_DECORATION, (sprite_params_t) {
        .path = "tile_decorations.png",
        .strategy = SPRITE_IMPORT_DEFAULT,
        .hframes = 5,
        .vframes = 1
    }},
    { SPRITE_UNIT_MINER, (sprite_params_t) {
        .path = "unit_miner.png",
        .strategy = SPRITE_IMPORT_RECOLOR,
        .hframes = 15,
        .vframes = 6
    }}
};

sprite_params_t resource_get_sprite_params(sprite_name name) {
    return SPRITE_PARAMS.at(name);
}
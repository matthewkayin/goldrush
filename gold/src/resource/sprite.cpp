#include "sprite.h"

#include <unordered_map>

static const std::unordered_map<sprite_name, sprite_params_t> SPRITE_PARAMS = {
    { SPRITE_DECORATION, (sprite_params_t) {
        .path = "sprite/tile_decorations.png",
        .hframes = 5,
        .vframes = 1
    }}
};

sprite_params_t resource_get_sprite_params(sprite_name name) {
    return SPRITE_PARAMS.at(name);
}
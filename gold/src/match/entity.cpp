#include "match_state.h"

#include <unordered_map>

static const std::unordered_map<EntityType, EntityData> ENTITY_DATA = {
    { ENTITY_GOLDMINE, (EntityData) {
        .name = "Gold Mine",
        .sprite = SPRITE_GOLDMINE,
        .cell_size = 3
    }}
};

const EntityData& entity_get_data(EntityType type) {
    return ENTITY_DATA.at(type);
}

bool entity_is_unit(EntityType type) {
    return false;
}

bool entity_is_selectable(const Entity& entity) {
    return true;
}

uint16_t entity_get_elevation(const Entity& entity, const Map& map) {
    uint16_t elevation = map_get_tile(map, entity.cell).elevation;
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size; y++) {
        for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size; x++) {
            elevation = std::max(elevation, map_get_tile(map, ivec2(x, y)).elevation);
        }
    }
    
    return elevation;
}

Rect entity_get_rect(const Entity& entity) {
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    Rect rect = (Rect) {
        .x = entity.position.x.integer_part(),
        .y = entity.position.y.integer_part(),
        .w = entity_cell_size * TILE_SIZE,
        .h = entity_cell_size * TILE_SIZE
    };
    if (entity_is_unit(entity.type)) {
        rect.x -= rect.w / 2;
        rect.y -= rect.h / 2;
    }

    return rect;
}
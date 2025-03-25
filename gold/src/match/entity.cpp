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

EntityId entity_create_goldmine(MatchState& state, ivec2 cell, uint32_t gold_left) {
    Entity entity;
    entity.type = ENTITY_GOLDMINE;
    entity.player_id = PLAYER_NONE;
    entity.flags = 0;
    entity.mode = ENTITY_MODE_GOLDMINE;

    entity.cell = cell;
    entity.position = fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.garrison_id = ID_NULL;
    entity.gold_held = gold_left;

    EntityId id = state.entities.push_back(entity);
    map_set_cell_rect(state.map, entity.cell, entity_get_data(entity.type).cell_size, id);

    return id;
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
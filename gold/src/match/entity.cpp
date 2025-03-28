#include "match_state.h"

#include <unordered_map>

static const std::unordered_map<EntityType, EntityData> ENTITY_DATA = {
    { ENTITY_GOLDMINE, (EntityData) {
        .name = "Gold Mine",
        .sprite = SPRITE_GOLDMINE,
        .cell_size = 3,
    }},
    { ENTITY_MINER, (EntityData) {
        .name = "Miner",
        .sprite = SPRITE_UNIT_MINER,
        .cell_size = 1,
        
        .gold_cost = 50,
        .train_duration = 20,
        .max_health = 25,
        .sight = 7,
        .armor = 0,
        .attack_priority = 1,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),

            .damage = 3,
            .attack_cooldown = 22,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }}
};

const EntityData& entity_get_data(EntityType type) {
    return ENTITY_DATA.at(type);
}

bool entity_is_unit(EntityType type) {
    return type > ENTITY_GOLDMINE;
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

fvec2 entity_get_target_position(const Entity& entity) {
    int unit_size = entity_get_data(entity.type).cell_size * TILE_SIZE;
    return fvec2((entity.cell * TILE_SIZE) + ivec2(unit_size / 2, unit_size / 2));
}
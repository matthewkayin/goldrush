#pragma once

#include "map.h"
#include "math/gmath.h"
#include "render/sprite.h"

enum EntityType {
    ENTITY_GOLDMINE
};

enum EntityMode {
    ENTITY_MODE_IDLE,
    ENTITY_MODE_GOLDMINE
};

struct Entity {
    EntityType type;
    EntityMode mode;
    uint8_t player_id;
    uint32_t flags;

    ivec2 cell;
    fvec2 position;
    Direction direction;

    EntityId garrison_id;
    uint32_t gold_held;
};

struct EntityData {
    const char* name;
    SpriteName sprite;
    int cell_size;
};

const EntityData& entity_get_data(EntityType type);
bool entity_is_unit(EntityType type);
bool entity_is_selectable(const Entity& entity);
uint16_t entity_get_elevation(const Entity& entity, const Map& map);
Rect entity_get_rect(const Entity& entity);
#pragma once

#include "map.h"
#include "math/gmath.h"
#include "render/sprite.h"
#include "core/animation.h"

const uint32_t ENTITY_FLAG_HOLD_POSITION = 1;
const uint32_t ENTITY_FLAG_DAMAGE_FLICKER = 2;
const uint32_t ENTITY_FLAG_INVISIBLE = 4;
const uint32_t ENTITY_FLAG_CHARGED = 8;

enum EntityType {
    ENTITY_GOLDMINE,
    ENTITY_MINER
};

enum EntityMode {
    ENTITY_MODE_IDLE,
    ENTITY_MODE_GOLDMINE
};

enum EntityTargetType {
    ENTITY_TARGET_NONE,
    ENTITY_TARGET_CELL,
    ENTITY_TARGET_ENTITY,
    ENTITY_TARGET_ATTACK_CELL,
    ENTITY_TARGET_ATTACK_ENTITY,
    ENTITY_TARGET_REPAIR,
    ENTITY_TARGET_UNLOAD,
    ENTITY_TARGET_SMOKE,
    ENTITY_TARGET_BUILD,
    ENTITY_TARGET_BUILD_ASSIST
};

struct EntityTargetBuild {
    ivec2 unit_cell;
    ivec2 building_cell;
    EntityType building_type;
};

struct EntityTarget {
    EntityTargetType type;
    EntityId id;
    ivec2 cell;
    union {
        EntityTargetBuild build;
    };
};

struct Entity {
    EntityType type;
    EntityMode mode;
    uint8_t player_id;
    uint32_t flags;

    ivec2 cell;
    fvec2 position;
    Direction direction;

    int health;
    EntityTarget target;
    std::vector<EntityTarget> target_queue;
    std::vector<ivec2> path;
    ivec2 rally_point;
    uint32_t pathfind_attempts;
    uint32_t timer;

    Animation animation;

    std::vector<EntityId> garrisoned_units;
    EntityId garrison_id;
    uint32_t cooldown_timer;

    uint32_t gold_held;
    EntityId gold_mine_id;

    uint32_t taking_damage_counter;
    uint32_t taking_damage_timer;
    uint32_t health_regen_timer;
};

struct EntityDataUnit {
    uint32_t population_cost;
    fixed speed;

    int damage;
    int attack_cooldown;
    int range_squared;
    int min_range_squared;
};

struct EntityData {
    const char* name;
    SpriteName sprite;
    int cell_size;

    uint32_t gold_cost;
    uint32_t train_duration;
    int max_health;
    int sight;
    int armor;
    uint32_t attack_priority;

    uint32_t garrison_capacity;
    uint32_t garrison_size;
    bool has_detection;

    union {
        EntityDataUnit unit_data;
    };
};

const EntityData& entity_get_data(EntityType type);
bool entity_is_unit(EntityType type);
bool entity_is_selectable(const Entity& entity);
uint16_t entity_get_elevation(const Entity& entity, const Map& map);
Rect entity_get_rect(const Entity& entity);
fvec2 entity_get_target_position(const Entity& entity);
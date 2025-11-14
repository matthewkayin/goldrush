#pragma once

#include "math/gmath.h"
#include "core/sound.h"
#include "render/sprite.h"
#include "match/map.h"

const uint32_t ENTITY_CANNOT_GARRISON = 0;
const uint32_t BUILDING_CAN_RALLY = 1;
const uint32_t BUILDING_COSTS_ENERGY = 2;

enum EntityType {
    ENTITY_GOLDMINE,
    ENTITY_MINER,
    ENTITY_COWBOY,
    ENTITY_BANDIT,
    ENTITY_WAGON,
    ENTITY_JOCKEY,
    ENTITY_SAPPER,
    ENTITY_PYRO,
    ENTITY_SOLDIER,
    ENTITY_CANNON,
    ENTITY_DETECTIVE,
    ENTITY_BALLOON,
    ENTITY_HALL,
    ENTITY_HOUSE,
    ENTITY_SALOON,
    ENTITY_BUNKER,
    ENTITY_WORKSHOP,
    ENTITY_SMITH,
    ENTITY_COOP,
    ENTITY_BARRACKS,
    ENTITY_SHERIFFS,
    ENTITY_LANDMINE,
    ENTITY_TYPE_COUNT
};

struct EntityDataUnit {
    uint32_t population_cost;
    fixed speed;
    uint32_t max_energy;

    SoundName attack_sound;
    int damage;
    int accuracy;
    int evasion;
    uint32_t attack_cooldown;
    int range_squared;
    int min_range_squared;
};

struct EntityDataBuilding {
    int builder_positions_x[3];
    int builder_positions_y[3];
    int builder_flip_h[3];
    uint32_t options;
};

struct EntityDataGoldmine {

};

struct EntityData {
    const char* name;
    SpriteName sprite;
    SpriteName icon;
    SoundName death_sound;
    CellLayer cell_layer;
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
        EntityDataBuilding building_data;
        EntityDataGoldmine goldmine_data;
    };
};

const EntityData& entity_get_data(EntityType type);
bool entity_is_unit(EntityType type);
bool entity_is_building(EntityType type);
#pragma once

#include "map.h"
#include "noise.h"
#include "container/id_array.h"

#define MATCH_MAX_ENTITIES (400 * MAX_PLAYERS)
#define PLAYER_NONE UINT8_MAX

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

struct MatchPlayer {
    bool active;
    char name[MAX_USERNAME_LENGTH + 1];
    uint32_t team;
    int recolor_id;
    uint32_t gold;
    uint32_t upgrades;
    uint32_t upgrades_in_progress;
};

struct MatchState {
    Map map;
    IdArray<Entity, MATCH_MAX_ENTITIES> entities;
    MatchPlayer players[MAX_PLAYERS];
};

MatchState match_init(int32_t lcg_seed, Noise& noise, MatchPlayer players[MAX_PLAYERS]);
void match_update(MatchState& state);

const EntityData& entity_get_data(EntityType type);
EntityId entity_create_goldmine(MatchState& state, ivec2 cell, uint32_t gold_left);
uint16_t entity_get_elevation(const Entity& entity, const Map& map);
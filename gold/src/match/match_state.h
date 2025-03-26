#pragma once

#include "map.h"
#include "noise.h"
#include "entity.h"
#include "container/id_array.h"

#define MATCH_MAX_ENTITIES (400 * MAX_PLAYERS)
#define PLAYER_NONE UINT8_MAX

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
EntityId match_create_goldmine(MatchState& state, ivec2 cell, uint32_t gold_left);
bool match_is_entity_visible_to_player(const MatchState& state, const Entity& entity, uint8_t player_id);
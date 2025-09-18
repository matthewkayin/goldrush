#pragma once

#include "defines.h"
#include "../state.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

const int BOT_SQUAD_GATHER_DISTANCE = 16;

// Strategy

enum BotStrategy {
    BOT_STRATEGY_SALOON_COOP,
    BOT_STRATEGY_SALOON_WORKSHOP,
    BOT_STRATEGY_BARRACKS,
    BOT_STRATEGY_COUNT
};

// Danger

enum BotScoutDangerType {
    BOT_SCOUT_DANGER_TYPE_UNITS,
    BOT_SCOUT_DANGER_TYPE_BUNKER
};

struct BotScoutDangerUnits {
    ivec2 cell;
    uint32_t expiration_time_minutes;
};

struct BotScoutDanger {
    BotScoutDangerType type;
    union {
        BotScoutDangerUnits units;
        EntityId bunker_id;
    };
};

// Squads

enum BotSquadType {
    BOT_SQUAD_TYPE_NONE,
    BOT_SQUAD_TYPE_HARASS,
    BOT_SQUAD_TYPE_PUSH,
    BOT_SQUAD_TYPE_BUNKER,
    BOT_SQUAD_TYPE_LANDMINES,
    BOT_SQUAD_TYPE_RESERVES,
    BOT_SQUAD_TYPE_DEFENSE
};

struct BotSquad {
    BotSquadType type;
    std::vector<EntityId> entities;
    ivec2 target_cell;
};

// Goal

enum BotGoalType {
    BOT_GOAL_BANDIT_RUSH,
    BOT_GOAL_BUNKER,
    BOT_GOAL_EXPAND,
    BOT_GOAL_LANDMINES,
    BOT_GOAL_DEFENSE,
    BOT_GOAL_DETECTIVE_DEFENSE,
    BOT_GOAL_HARASS,
    BOT_GOAL_PUSH,
    BOT_GOAL_COUNT
};

struct BotGoal {
    BotGoalType type;
    BotSquadType desired_squad_type;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    uint32_t desired_upgrade;
};

// Bot

struct Bot {
    uint8_t player_id;
    int32_t lcg_seed;
    bool should_surrender;
    bool has_surrendered;

    BotStrategy strategy;
    BotGoal goal;

    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<BotSquad> squads;

    EntityId scout_id;
    uint32_t last_scout_time;
    std::vector<EntityId> entities_to_scout;
    std::vector<BotScoutDanger> scout_danger;
    std::unordered_map<uint32_t, bool> should_scout_goldmine;
    std::unordered_map<EntityId, bool> scout_assumed_entities;
    bool scout_enemy_has_detectives;
    bool scout_enemy_has_landmines;
};
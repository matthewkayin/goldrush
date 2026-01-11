#pragma once

#include "defines.h"
#include "match/noise.h"
#include "match/state.h"
#include "match/map.h"
#include "match/upgrade.h"
#include "scenario/objective.h"

#define SCENARIO_SQUAD_MAX_ENTITIES SELECTION_LIMIT
#define SCENARIO_MAX_SQUADS 32

struct ScenarioPlayer {
    char name[MAX_USERNAME_LENGTH + 1];
    uint32_t starting_gold;
};

struct ScenarioEntity {
    EntityType type;
    uint8_t player_id;
    uint32_t gold_held;
    ivec2 cell;
};

enum ScenarioSquadType {
    SCENARIO_SQUAD_TYPE_DEFEND,
    SCENARIO_SQUAD_TYPE_LANDMINES,
    SCENARIO_SQUAD_TYPE_COUNT
};

struct ScenarioSquad {
    char name[MAX_USERNAME_LENGTH + 1];
    uint8_t player_id;
    ScenarioSquadType type;
    uint32_t entity_count;
    uint32_t entities[SCENARIO_SQUAD_MAX_ENTITIES];
};

struct Scenario {
    Map map;
    Noise* noise;
    ivec2 player_spawn;
    ScenarioPlayer players[MAX_PLAYERS];
    uint32_t entity_count;
    ScenarioEntity entities[MATCH_MAX_ENTITIES];
    uint32_t squad_count;
    ScenarioSquad squads[SCENARIO_MAX_SQUADS];
    ScenarioObjective objective;
    bool allowed_entities[ENTITY_TYPE_COUNT];
    uint32_t allowed_upgrades;
};

Scenario* scenario_init_blank(MapType map_type, MapSize map_size);
Scenario* scenario_init_generated(MapType map_type, const NoiseGenParams& params);
void scenario_bake_map(Scenario* scenario, bool remove_artifacts = false);
void scenario_free(Scenario* scenario);

ScenarioSquad scenario_squad_init();
bool scenario_squads_are_equal(const ScenarioSquad& a, const ScenarioSquad& b);
const char* scenario_squad_type_str(ScenarioSquadType type);

uint8_t scenario_get_noise_map_value(Scenario* scenario, ivec2 cell);
void scenario_set_noise_map_value(Scenario* scenario, ivec2 cell, uint8_t value);

bool scenario_save_file(const Scenario* scenario, const char* path);
Scenario* scenario_open_file(const char* path);
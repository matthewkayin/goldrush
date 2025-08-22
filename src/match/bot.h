#pragma once

#include "defines.h"
#include "state.h"
#include <functional>
#include <unordered_map>

enum BotStrategy {
    BOT_STRATEGY_OPENER_BANDIT_RUSH,
    BOT_STRATEGY_OPENER_FAST_EXPAND,
    BOT_STRATEGY_OPENER_SAFE_EXPAND,
    BOT_STRATEGY_SALOON_HARASS
};

struct BotDesiredEntities {
    EntityType unit;
    EntityType building;
};

struct BotScoutInfo {
    EntityId goldmine_id;
    uint8_t occupying_player_id;
    bool should_scout;
};

enum BotSquadType {
    BOT_SQUAD_TYPE_NONE,
    BOT_SQUAD_TYPE_HARASS,
    BOT_SQUAD_TYPE_PUSH,
    BOT_SQUAD_TYPE_BUNKER,
    BOT_SQUAD_TYPE_LANDMINES
};

struct BotSquad {
    BotSquadType type;
    std::vector<EntityId> entities;
    ivec2 target_cell;
    uint32_t starting_entity_count;
};


struct Bot {
    uint8_t player_id;
    int32_t lcg_seed;

    BotStrategy strategy;
    BotSquadType desired_squad_type;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];

    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<BotSquad> squads;

    EntityId scout_id;
    std::vector<BotScoutInfo> scout_info;
    uint32_t last_scout_time;
    bool scout_enemy_has_invisible_units;
};

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes);

// Strategy

void bot_strategy_update(const MatchState& state, Bot& bot);
void bot_clear_desired_entities(Bot& bot);
void bot_choose_next_goal(const MatchState& state, Bot& bot);
void bot_set_goal_expand(const MatchState& state, Bot& bot);
bool bot_is_goal_empty(const Bot& bot);
bool bot_is_goal_met(const MatchState& state, const Bot& bot);
void bot_on_goal_finished(const MatchState& state, Bot& bot);

// Behaviors

MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
BotDesiredEntities bot_get_desired_entities(const MatchState& state, const Bot& bot, uint32_t match_time_minutes);
uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot);
MatchInput bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
MatchInput bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type);
MatchInput bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);
MatchInput bot_set_rally_points(const MatchState& state, const Bot& bot);
MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id);
MatchInput bot_unit_flee(const MatchState& state, Bot& bot, EntityId entity_id);

// Entity management

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

// Squads

void bot_squad_create_from_desired_entities(const MatchState& state, Bot& bot);
void bot_squad_dissolve(const MatchState& state, Bot& bot, BotSquad& squad);
MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad);

// Scouting

void bot_scout_update(const MatchState& state, Bot& bot);
MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
MatchInput bot_scout_next_target(const MatchState& state, Bot& bot, const Entity& scout, uint32_t match_time_minutes);
uint32_t bot_get_next_scout_time(const Bot& bot);
int bot_score_scout_type(EntityType type);

// Helpers

struct BotFindEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
};

struct BotFindBestEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
    std::function<bool(const Entity&, const Entity&)> compare;
};

EntityId bot_find_entity(BotFindEntityParams params);
EntityId bot_find_best_entity(BotFindBestEntityParams params);
std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell);

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_pull_worker_off_gold(const MatchState& state, const Bot& bot, EntityId goldmine_id);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);

ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot, uint32_t existing_hall_index);
uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);
bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell);

EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType building_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);

ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building);
bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
ivec2 bot_get_squad_center_point(const MatchState& state, const BotSquad& squad);
ivec2 bot_get_squad_attack_point(const MatchState& state, const Bot& bot, const BotSquad& squad);
bool bot_is_base_under_attack(const MatchState& state, const Bot& bot);
bool bot_is_unit_already_attacking_nearby_target(const MatchState& state, const Entity& infantry, const Entity& nearby_enemy);
int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell);
ivec2 bot_find_best_molotov_cell(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 attack_point);
void bot_pathfind_and_avoid_landmines(const MatchState& state, const Bot& bot, ivec2 from, ivec2 to, std::vector<ivec2>* path);
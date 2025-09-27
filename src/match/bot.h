#pragma once

#include "defines.h"
#include "state.h"
#include "input.h"
#include <functional>
#include <unordered_map>
#include <vector>

struct Bot;

enum BotStrategy {
    BOT_STRATEGY_SALOON_COOP,
    BOT_STRATEGY_SALOON_WORKSHOP,
    BOT_STRATEGY_BARRACKS,
    BOT_STRATEGY_COUNT
};

enum BotSquadType {
    BOT_SQUAD_TYPE_NONE,
    BOT_SQUAD_TYPE_ATTACK,
    BOT_SQUAD_TYPE_DEFEND,
    BOT_SQUAD_TYPE_RESERVES,
    BOT_SQUAD_TYPE_LANDMINES
};

struct BotSquad {
    BotSquadType type;
    ivec2 target_cell;
    std::vector<EntityId> entities;
};

struct BotGoal {
    BotSquadType desired_squad_type;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    std::function<bool(const MatchState& state, const Bot& bot)> should_be_abandoned;
};

enum BotGoalStatus {
    BOT_GOAL_STATUS_IN_PROGRESS,
    BOT_GOAL_STATUS_PURCHASED,
    BOT_GOAL_STATUS_FINISHED
};

struct BotFindEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
};

struct BotFindBestEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
    std::function<bool(const Entity&, const Entity&)> compare;
};

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

struct Bot {
    uint8_t player_id;
    int32_t lcg_seed;
    bool should_surrender;
    bool has_surrendered;

    BotStrategy strategy;
    std::vector<BotGoal> goals;

    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<BotSquad> squads;

    EntityId scout_id;
    uint32_t last_scout_time;
    std::vector<EntityId> entities_to_scout;
    std::vector<BotScoutDanger> scout_danger;
    std::unordered_map<uint32_t, bool> should_scout_goldmine;
    std::unordered_map<EntityId, bool> is_entity_assumed_to_be_scouted;
    bool scout_enemy_has_landmines;
    bool scout_enemy_has_detectives;
};

Bot bot_init(uint8_t player_id, int32_t lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
BotGoal bot_choose_next_goal(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot);

// Production

BotGoal bot_goal_create_empty();
BotGoalStatus bot_goal_get_status(const MatchState& state, const Bot& bot, const BotGoal& goal);
MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
void bot_get_desired_entities(const MatchState& state, const Bot& bot, const BotGoal& goal, EntityType* desired_unit, EntityType* desired_building);
MatchInput bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
MatchInput bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type);
MatchInput bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);
EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType building_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_pull_worker_off_gold(const MatchState& state, const Bot& bot, EntityId goldmine_id);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id, bool count_bunkers_only);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot, uint32_t existing_hall_index);
ivec2 bot_find_bunker_location(const MatchState& state, const Bot& bot, uint32_t nearby_hall_index);
uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);

// Squads

void bot_squad_create_from_goal(const MatchState& state, Bot& bot, const BotGoal& goal);
void bot_squad_create(const MatchState& state, Bot& bot, BotSquadType type, ivec2 target_cell, std::vector<EntityId>& entities);
void bot_squad_dissolve(Bot& bot, BotSquad& squad);
MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad);
bool bot_squad_should_retreat(const MatchState& state, const Bot& bot, const BotSquad& squad);
MatchInput bot_squad_return_to_nearest_base(const MatchState& state, Bot& bot, BotSquad& squad);
bool bot_is_unit_already_attacking_nearby_target(const MatchState& state, const Entity& infantry, const Entity& nearby_enemy);
int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell);
ivec2 bot_find_best_molotov_cell(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 attack_point);
bool bot_squad_is_bunker_defense(const MatchState& state, const BotSquad& squad);
bool bot_squad_is_detective_harass(const MatchState& state, const BotSquad& squad);
bool bot_squad_can_defend_against_detectives(const MatchState& state, const BotSquad& squad);
ivec2 bot_squad_get_center(const MatchState& state, const BotSquad& squad);
ivec2 bot_squad_choose_attack_point(const MatchState& state, const Bot& bot, const BotSquad& squad);
ivec2 bot_squad_choose_defense_point(const MatchState& state, const Bot& bot, const BotSquad& squad);
bool bot_handle_base_under_attack(const MatchState& state, Bot& bot);
bool bot_squad_is_engaged(const MatchState& state, const Bot& bot, const BotSquad& squad);

// Scout

void bot_scout_update(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
void bot_release_scout(Bot& bot);
bool bot_should_scout(const MatchState& state, const Bot& bot, uint32_t match_time_minutes);
bool bot_scout_danger_is_expired(const MatchState& state, const BotScoutDanger& danger, uint32_t match_time_mintutes);
ivec2 bot_scout_danger_get_cell(const MatchState& state, const BotScoutDanger& danger);

// Util

EntityId bot_find_entity(BotFindEntityParams params);
EntityId bot_find_best_entity(BotFindBestEntityParams params);
std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell);

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);
bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell);
bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);

int bot_score_entity(const Entity& entity);
MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id);
MatchInput bot_unit_flee(const MatchState& state, const Bot& bot, EntityId entity_id);
void bot_pathfind_and_avoid_landmines(const MatchState& state, const Bot& bot, ivec2 from, ivec2 to, std::vector<ivec2>* path);
std::unordered_map<uint32_t, uint32_t> bot_get_enemy_hall_defense_scores(const MatchState& state, const Bot& bot);
uint32_t bot_get_mining_base_count(const MatchState& state, const Bot& bot);

MatchInput bot_set_rally_points(const MatchState& state, const Bot& bot);
bool bot_is_rally_cell_valid(const MatchState& state, ivec2 rally_cell, int rally_margin);
ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building);
MatchInput bot_unload_unreserved_carriers(const MatchState& state, const Bot& bot);
MatchInput bot_rein_in_stray_units(const MatchState& state, const Bot& bot);
MatchInput bot_cancel_in_progress_buildings(const MatchState& state, const Bot& bot);
MatchInput bot_repair_burning_buildings(const MatchState& state, const Bot& bot);
bool bot_has_landmine_squad(const Bot& bot);
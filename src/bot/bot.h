#pragma once

#include "defines.h"
#include "bot/entity_count.h"
#include "match/state.h"
#include "core/match_setting.h"
#include <unordered_map>

enum BotMode {
    BOT_MODE_OPENER_BANDIT_RUSH,
    BOT_MODE_OPENER_BUNKER,
    BOT_MODE_EXPAND,
    BOT_MODE_ATTACK,
    BOT_MODE_SURRENDER
};

enum BotUnitComp {
    BOT_UNIT_COMP_COWBOY_BANDIT,
    BOT_UNIT_COMP_COWBOY_BANDIT_PYRO,
    BOT_UNIT_COMP_COWBOY_SAPPER_PYRO,
    BOT_UNIT_COMP_COWBOY_WAGON,
    BOT_UNIT_COMP_SOLDIER_BANDIT,
    BOT_UNIT_COMP_SOLDIER_CANNON
};

enum BotMetric {
    BOT_METRIC_ENTITY_COUNT,
    BOT_METRIC_UNRESERVED_ENTITY_COUNT,
    BOT_METRIC_IN_PROGRESS_ENTITY_COUNT,
    BOT_METRIC_AVAILABLE_PRODUCTION_BUILDING_COUNT,
    BOT_METRIC_COUNT
};

struct Bot {
    uint8_t player_id;
    int lcg_seed;
    BotMode mode;
    MatchSettingDifficultyValue difficulty;

    // Metrics
    EntityCount metrics[BOT_METRIC_COUNT];

    // Unit management
    std::unordered_map<EntityId, bool> is_entity_reserved;

    // Production
    BotUnitComp unit_comp;
    EntityCount desired_buildings;
    EntityCount desired_army_ratio;
    std::queue<EntityId> buildings_to_set_rally_points;
    uint32_t macro_cycle_timer;
    uint32_t macro_cycle_count;

    // Scouting
    EntityId scout_id;
    uint32_t scout_info;
};

Bot bot_init(const MatchState& state, uint8_t player_id, MatchSettingDifficultyValue difficulty, int lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_timer);

// Production

MatchInput bot_get_production_input(const MatchState& state, Bot& bot, uint32_t match_timer);
void bot_set_desired_production(const MatchState& state, const Bot& bot);
bool bot_should_use_army_ratio(const Bot& bot);

// Saturate bases

MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
ivec2 bot_get_position_near_hall_away_from_miners(const MatchState& state, ivec2 hall_cell, ivec2 goldmine_cell);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);

// Build buildings

bool bot_should_build_house(const MatchState& state, const Bot& bot);
MatchInput bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
ivec2 bot_find_building_location(const MatchState& state, ivec2 start_cell, int size);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id, bool count_bunkers_only);
ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot);
uint32_t bot_find_index_of_hall_next_to_goldmine(const MatchState& state, const Bot& bot);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
ivec2 bot_find_bunker_location(const MatchState& state, const Bot& bot, uint32_t nearby_hall_index);

// Research upgrades

uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot);
MatchInput bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);

// Train Units

bool bot_has_building_available_to_train_units(const Bot& bot, EntityCount desired_entities);
EntityType bot_get_unit_type_to_train(Bot& bot, EntityCount desired_entities);
MatchInput bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type, uint32_t match_time_minutes);

// Scouting

bool bot_check_scout_info(const Bot& bot, uint32_t flag);
void bot_set_scout_info(Bot& bot, uint32_t flag, bool value);

// Entity Reservation

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

// Entity Type Util

EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType building_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);

// Metrics

void bot_gather_metrics(const MatchState& state, Bot& bot);
bool bot_is_entity_type_production_building(EntityType type);
std::vector<EntityType> bot_entity_types_production_buildings();

// Util

bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);
bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell);
uint32_t bot_get_mining_base_count(const MatchState& state, const Bot& bot);
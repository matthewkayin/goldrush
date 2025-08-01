#pragma once

#include "input.h"
#include "state.h"
#include "entity.h"
#include <cstdint>
#include <vector>
#include <functional>

enum BotStrategy {
    BOT_STRATEGY_EXPAND,
    BOT_STRATEGY_RUSH,
    BOT_STRATEGY_LANDMINES,
    BOT_STRATEGY_BUNKER,
    BOT_STRATEGY_HARASS,
    BOT_STRATEGY_PUSH,
    BOT_STRATEGY_COUNT
};

#define BOT_STRATEGY_NONE BOT_STRATEGY_COUNT

enum BotSquadType {
    BOT_SQUAD_TYPE_NONE,
    BOT_SQUAD_TYPE_ATTACK,
    BOT_SQUAD_TYPE_LANDMINES,
    BOT_SQUAD_TYPE_DEFEND
};

enum BotSquadMode {
    BOT_SQUAD_MODE_GATHER,
    BOT_SQUAD_MODE_GARRISON,
    BOT_SQUAD_MODE_ATTACK,
    BOT_SQUAD_MODE_LANDMINES,
    BOT_SQUAD_MODE_DEFEND,
    BOT_SQUAD_MODE_DISSOLVED
};

struct BotSquad {
    BotSquadType type;
    BotSquadMode mode;
    ivec2 target_cell;
    std::vector<ivec2> attack_path;
    std::vector<EntityId> entities;
};

struct BotScoutInfo {
    EntityId goldmine_id;
    uint8_t occupying_player_id;
    uint32_t bunker_count;
    bool scouted;
};

struct Bot {
    uint8_t player_id;
    int32_t lcg_seed;

    int personality_aggressiveness;
    int personality_boldness;
    int personality_quirkiness;

    BotStrategy strategy;
    BotSquadType desired_squad_type;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    uint32_t desired_upgrade;

    EntityId scout_id;
    std::vector<BotScoutInfo> scout_info;
    uint32_t scouted_player_tech[ENTITY_TYPE_COUNT][MAX_PLAYERS];
    uint32_t last_scout_time;

    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<BotSquad> squads;
    std::vector<MatchInput> inputs;
};

Bot bot_init(uint8_t player_id, int32_t lcg_seed);
void bot_update(const MatchState& state, Bot& bot, uint32_t match_time_minutes);

// Behaviors

BotStrategy bot_choose_next_strategy(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
void bot_clear_strategy(Bot& bot);
void bot_set_strategy(const MatchState& state, Bot& bot, BotStrategy strategy);
bool bot_strategy_should_be_abandoned(const MatchState& state, const Bot& bot);
void bot_saturate_bases(const MatchState& state, Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
void bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
void bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type);
void bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);
void bot_get_desired_entities(const MatchState& state, const Bot& bot, uint32_t desired_entities[ENTITY_TYPE_COUNT]);
bool bot_has_desired_entities(const MatchState& state, const Bot& bot);
int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell);
void bot_throw_molotov(const MatchState& state, Bot& bot, EntityId pyro_id, ivec2 attack_point, int attack_radius);
void bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
int bot_get_defense_score(const MatchState& state, const Bot& bot);

// Squads

void bot_squad_create(const MatchState& state, Bot& bot);
void bot_squad_set_mode(const MatchState& state, Bot& bot, BotSquad& squad, BotSquadMode mode);
void bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad);

// Entity management

EntityId bot_find_entity(const MatchState& state, std::function<bool(const Entity&, EntityId)> filter_fn);
EntityId bot_find_best_entity(const MatchState& state, std::function<bool(const Entity&,EntityId)> filter_fn, std::function<bool(const Entity&, const Entity&)> compare_fn);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

// Helpers

uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, uint32_t existing_hall_index);
bool bot_is_goldmine_occupied(const MatchState& state, EntityId goldmine_id);
EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType unit_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);
void bot_get_base_info(const MatchState& state, EntityId base_id, ivec2* base_center, int* base_radius, uint32_t* landmine_count);
void bot_get_circle_draw(int x_center, int y_center, int x, int y, std::vector<ivec2>& points);
std::vector<ivec2> bot_get_cell_circle(ivec2 center, int radius);
ivec2 bot_get_best_rally_point(const MatchState& state, EntityId building_id);
bool bot_is_landmine_point_valid(const MatchState& state, ivec2 point);
bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);
bool bot_is_population_maxed_out(const MatchState& state, const Bot& bot);
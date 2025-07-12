#pragma once

#include "input.h"
#include "state.h"
#include "entity.h"
#include <cstdint>
#include <vector>
#include <functional>

enum BotStrategyType {
    BOT_STRATEGY_BANDIT_RUSH
};

struct BotStrategy {
    BotStrategyType type;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    uint32_t desired_upgrade;
};

enum BotSquadMode {
    BOT_SQUAD_MODE_GATHER,
    BOT_SQUAD_MODE_GARRISON,
    BOT_SQUAD_MODE_ATTACK,
    BOT_SQUAD_MODE_DISSOLVED
};

struct BotSquad {
    BotSquadMode mode;
    ivec2 target_cell;
    std::vector<EntityId> entities;
};

struct Bot {
    uint8_t player_id;
    BotStrategy strategy;
    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<BotSquad> squads;
    std::vector<MatchInput> inputs;
};

Bot bot_init(uint8_t player_id);
void bot_update(const MatchState& state, Bot& bot);

// Behaviors

void bot_set_strategy(Bot& bot, BotStrategyType type);
void bot_saturate_bases(const MatchState& state, Bot& bot);
bool bot_should_expand(const MatchState& state, const Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
void bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
void bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type);
void bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);
void bot_get_desired_entities(const MatchState& state, const Bot& bot, uint32_t desired_entities[ENTITY_TYPE_COUNT]);
bool bot_has_desired_entities(const MatchState& state, const Bot& bot);

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
EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType unit_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);
void bot_get_base_info(const MatchState& state, EntityId base_id, ivec2* base_center, int* base_radius, uint32_t* landmine_count);
void bot_get_circle_draw(int x_center, int y_center, int x, int y, std::vector<ivec2>& points);
std::vector<ivec2> bot_get_cell_circle(ivec2 center, int radius);
ivec2 bot_get_best_rally_point(const MatchState& state, EntityId building_id);
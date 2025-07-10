#pragma once

#include "input.h"
#include "state.h"
#include <cstdint>
#include <vector>
#include <functional>

struct Bot {
    uint8_t player_id;
    std::unordered_map<EntityId, bool> is_entity_reserved;
};

Bot bot_init(uint8_t player_id);
void bot_get_turn_inputs(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs);

// Behaviors

void bot_saturate_bases(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs);
bool bot_should_expand(const MatchState& state, const Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
void bot_build_building(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs, EntityType building_type);

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

uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot, const std::vector<MatchInput>& inputs);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, uint32_t existing_hall_index);
EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType unit_type);
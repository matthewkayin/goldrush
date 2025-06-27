#pragma once

#include "input.h"
#include "state.h"
#include <cstdint>
#include <vector>

struct Bot {
    uint8_t player_id;
    uint32_t effective_gold;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<EntityId> reserved_builders;
};

Bot bot_init(uint8_t player_id);
void bot_get_turn_inputs(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs);

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
EntityId bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
MatchInput bot_create_build_input(const MatchState& state, const Bot& bot, EntityType building_type);
EntityType bot_get_building_type_which_trains_unit_type(EntityType unit_type);
MatchInput bot_create_train_unit_input(const MatchState& state, const Bot& bot, EntityType unit_type);
#pragma once

#include "types.h"

MatchInput bot_get_production_input(const MatchState& state, Bot& bot);

MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
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
EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot, uint32_t existing_hall_index);
ivec2 bot_find_bunker_location(const MatchState& state, const Bot& bot, uint32_t nearby_hall_index);
uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);
bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell);
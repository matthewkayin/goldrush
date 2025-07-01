#pragma once

#include "input.h"
#include "state.h"
#include <cstdint>
#include <vector>

enum BotArmyType {
    BOT_ARMY_TYPE_ATTACK,
    BOT_ARMY_TYPE_DEFEND
};

enum BotArmyMode {
    BOT_ARMY_MODE_RAISE,
    BOT_ARMY_MODE_GATHER,
    BOT_ARMY_MODE_GARRISON,
    BOT_ARMY_MODE_MOVE_OUT,
    BOT_ARMY_MODE_ATTACK,
    BOT_ARMY_MODE_DEFEND,
    BOT_ARMY_MODE_DISSOLVED
};

struct BotArmy {
    BotArmyType type;
    BotArmyMode mode;
    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    std::vector<EntityId> unit_ids;
    ivec2 gather_point;
};

struct Bot {
    uint8_t player_id;
    uint32_t effective_gold;
    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<EntityId> reserved_builders;
    std::vector<BotArmy> armies;
};

Bot bot_init(uint8_t player_id);
void bot_get_turn_inputs(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs);
void bot_army_update(const MatchState& state, Bot& bot, BotArmy& army, std::vector<MatchInput>& inputs);

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

EntityType bot_get_desired_entity(const MatchState& state, const Bot& bot);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, uint32_t existing_hall_index);
EntityId bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
MatchInput bot_create_build_input(const MatchState& state, const Bot& bot, EntityType building_type);
EntityType bot_get_building_type_which_trains_unit_type(EntityType unit_type);
EntityType bot_get_building_pre_req(EntityType building_type);
MatchInput bot_create_train_unit_input(const MatchState& state, const Bot& bot, EntityType unit_type);
ivec2 bot_army_determine_attack_point(const MatchState& state, const Bot& bot);
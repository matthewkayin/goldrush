#pragma once

#include "types.h"
#include "../state.h"
#include <functional>

struct BotFindEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
};

struct BotFindBestEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
    std::function<bool(const Entity&, const Entity&)> compare;
};

const uint32_t BOT_INCLUDE_IN_PROGRESS = 1;
const uint32_t BOT_INCLUDE_RESERVED = 2;

EntityId bot_find_entity(BotFindEntityParams params);
EntityId bot_find_best_entity(BotFindBestEntityParams params);
std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell);

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

void bot_count_entities(const MatchState& state, const Bot& bot, uint32_t* entity_count, uint32_t options = 0);
bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id);
MatchInput bot_unit_flee(const MatchState& state, const Bot& bot, EntityId entity_id);
std::unordered_map<uint32_t, uint32_t> bot_get_enemy_hall_defense_scores(const MatchState& state, const Bot& bot);

uint32_t bot_score_entity(const Entity& entity);
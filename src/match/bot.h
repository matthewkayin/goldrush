#pragma once

#include "defines.h"
#include "state.h"
#include <functional>
#include <unordered_map>

struct Bot {
    uint8_t player_id;
    int32_t lcg_seed;

    std::unordered_map<EntityId, bool> is_entity_reserved;
};

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
MatchInput bot_set_rally_points(const MatchState& state, const Bot& bot);

// Entity management

struct BotFindEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
};

struct BotFindBestEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
    std::function<bool(const Entity&, const Entity&)> compare;
};

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

EntityId bot_find_entity(BotFindEntityParams params);
EntityId bot_find_best_entity(BotFindBestEntityParams params);
std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell);

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_pull_worker_off_gold(const MatchState& state, const Bot& bot, EntityId goldmine_id);

// Helpers

uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);
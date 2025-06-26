#pragma once

#include "input.h"
#include "state.h"
#include <cstdint>
#include <vector>

// TODO: use this guy to enforce max builders
#define BOT_TASK_BUILD_MAX_BUILDERS 8

enum BotTaskMode {
    BOT_TASK_FINISHED,
    BOT_TASK_SATURATE_BASES,
    BOT_TASK_BUILD_HOUSES,
    BOT_TASK_SCOUT,
    BOT_TASK_BUILD,
};

struct BotTaskBuildHouses {
    EntityId builder_id;
};

struct BotTaskScout {
    EntityId scout_id;
};

struct BotTaskBuild {
    uint32_t desired_buildings[ENTITY_TYPE_COUNT];
    EntityId reserved_builders[BOT_TASK_BUILD_MAX_BUILDERS];
    uint32_t reserved_builders_count;
};

struct BotTask {
    BotTaskMode mode;
    union {
        BotTaskBuildHouses build_houses;
        BotTaskScout scout;
        BotTaskBuild build;
    };
};

struct Bot {
    uint8_t player_id;
    uint32_t effective_gold;
    std::vector<BotTask> tasks;
    std::unordered_map<EntityId, bool> _is_entity_reserved;
};

Bot bot_init(uint8_t player_id);
void bot_get_turn_inputs(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs);

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);

void bot_add_task(Bot& bot, BotTaskMode mode);
void bot_run_task(const MatchState& state, Bot& bot, BotTask& task, std::vector<MatchInput>& inputs);

uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
EntityId bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
MatchInput bot_create_build_input(const MatchState& state, const Bot& bot, EntityType building_type);
void bot_count_entities(const MatchState& state, const Bot& bot, bool include_in_progress_entities, uint32_t* entity_counts);
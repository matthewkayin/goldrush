#pragma once

#include "defines.h"
#include "state.h"
#include "entity.h"
#include "menu/match_setting.h"
#include <unordered_map>
#include <array>
#include <functional>
#include <queue>
#include <vector>

using BotEntityCount = std::array<uint32_t, ENTITY_TYPE_COUNT>;

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
    BOT_UNIT_COMP_COWBOY_BANDIT_WAGON,
    BOT_UNIT_COMP_COWBOY_BANDIT_WAGON_JOCKEY,
    BOT_UNIT_COMP_SOLDIER_BANDIT,
    BOT_UNIT_COMP_SOLDIER_CANNON
};

enum BotSquadType {
    BOT_SQUAD_TYPE_ATTACK,
    BOT_SQUAD_TYPE_DEFEND,
    BOT_SQUAD_TYPE_RESERVES,
    BOT_SQUAD_TYPE_LANDMINES,
    BOT_SQUAD_TYPE_RETURN
};

struct BotSquad {
    BotSquadType type;
    ivec2 target_cell;
    std::vector<EntityId> entities;
};

struct BotDesiredSquad {
    BotSquadType type;
    BotEntityCount entities;
};

struct BotRetreatMemory {
    std::vector<EntityId> entity_list;
    int desired_lead;
};

enum BotScoutDangerType {
    BOT_SCOUT_DANGER_TYPE_UNITS,
    BOT_SCOUT_DANGER_TYPE_BUNKER
};

struct BotScoutDangerUnits {
    ivec2 cell;
    uint32_t expiration_time_minutes;
};

struct BotScoutDanger {
    BotScoutDangerType type;
    union {
        BotScoutDangerUnits units;
        EntityId bunker_id;
    };
};

struct BotFindEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
};

struct BotFindBestEntityParams {
    const MatchState& state;
    std::function<bool(const Entity&, EntityId)> filter;
    std::function<bool(const Entity&, const Entity&)> compare;
};

struct Bot {
    uint8_t player_id;
    int32_t lcg_seed;
    BotMode mode;
    MatchSettingDifficultyValue difficulty;

    BotUnitComp unit_comp;
    std::queue<EntityId> buildings_to_set_rally_points;

    std::unordered_map<EntityId, bool> is_entity_reserved;
    std::vector<BotDesiredSquad> desired_squads;
    std::vector<BotSquad> squads;
    std::unordered_map<EntityId, BotRetreatMemory> retreat_memory;
    std::unordered_map<EntityId, int> bunker_garrison_count_memory;
    uint32_t landmine_check_time;

    EntityId scout_id;
    uint32_t last_scout_time;
    std::vector<EntityId> entities_to_scout;
    std::vector<BotScoutDanger> scout_danger;
    std::unordered_map<EntityId, bool> is_entity_assumed_to_be_scouted;
    uint32_t scout_info;
};

Bot bot_init(const MatchState& state, uint8_t player_id, MatchSettingDifficultyValue difficulty, int32_t lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
bool bot_has_surrendered(const Bot& bot);

// Strategy

void bot_strategy_update(const MatchState& state, Bot& bot, bool is_base_under_attack);
bool bot_handle_base_under_attack(const MatchState& state, Bot& bot);
bool bot_is_area_safe(const MatchState& state, const Bot& bot, ivec2 cell);
void bot_defend_location(const MatchState& state, Bot& bot, ivec2 location, uint32_t options);

// Production

MatchInput bot_get_production_input(const MatchState& state, Bot& bot, bool is_base_under_attack);
void bot_get_desired_buildings_and_army_ratio(const MatchState& state, const Bot& bot, BotEntityCount& desired_buildings, BotEntityCount& desired_army_ratio);
bool bot_should_use_continuous_production(const Bot& bot);
MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
bool bot_should_build_house(const MatchState& state, const Bot& bot);
MatchInput bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
MatchInput bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type);
uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot);
MatchInput bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);
EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType building_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
EntityId bot_pull_worker_off_gold(const MatchState& state, const Bot& bot, EntityId goldmine_id);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id, bool count_bunkers_only);
ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot);
uint32_t bot_find_next_hall_goldmine_index(const MatchState& state, const Bot& bot);
ivec2 bot_find_bunker_location(const MatchState& state, const Bot& bot, uint32_t nearby_hall_index);
uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);

// Squads

std::vector<EntityId> bot_create_entity_list_from_entity_count(const MatchState& state, const Bot& bot, BotEntityCount desired_entities);
void bot_squad_create_from_entity_count(const MatchState& state, Bot& bot, BotSquadType squad_type, BotEntityCount desired_entities);
void bot_squad_create(const MatchState& state, Bot& bot, BotSquadType type, ivec2 target_cell, std::vector<EntityId>& entities);
void bot_squad_dissolve(Bot& bot, BotSquad& squad);
MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad, uint32_t match_time_minutes);
std::vector<EntityId> bot_squad_get_nearby_enemy_army(const MatchState& state, const Bot& bot, const BotSquad& squad);
bool bot_squad_should_retreat(const MatchState& state, const Bot& bot, const BotSquad& squad);
MatchInput bot_squad_return_to_nearest_base(const MatchState& state, Bot& bot, BotSquad& squad);
int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell);
ivec2 bot_find_best_molotov_cell(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 attack_point);
bool bot_squad_is_bunker_defense(const MatchState& state, const BotSquad& squad);
bool bot_squad_is_detective_harass(const MatchState& state, const BotSquad& squad);
bool bot_squad_can_defend_against_detectives(const MatchState& state, BotSquadType type, const std::vector<EntityId>& entities);
ivec2 bot_squad_get_center_point(const MatchState& state, const std::vector<EntityId>& entities);
ivec2 bot_squad_choose_attack_point(const MatchState& state, const Bot& bot, ivec2 squad_center);
ivec2 bot_squad_choose_defense_point(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entities);
bool bot_squad_is_engaged(const MatchState& state, const Bot& bot, const BotSquad& squad);

// Scout

void bot_scout_update(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
void bot_release_scout(Bot& bot);
bool bot_should_scout(const MatchState& state, const Bot& bot, uint32_t match_time_minutes);
bool bot_scout_danger_is_expired(const MatchState& state, const BotScoutDanger& danger, uint32_t match_time_mintutes);
ivec2 bot_scout_danger_get_cell(const MatchState& state, const BotScoutDanger& danger);
bool bot_scout_check_info(const Bot& bot, uint32_t flag);
void bot_scout_set_info(Bot& bot, uint32_t flag, bool value);

// Util

MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id);
MatchInput bot_unit_flee(const MatchState& state, const Bot& bot, EntityId entity_id);
MatchInput bot_cancel_in_progress_buildings(const MatchState& state, const Bot& bot);
MatchInput bot_repair_burning_buildings(const MatchState& state, const Bot& bot);
MatchInput bot_rein_in_stray_units(const MatchState& state, const Bot& bot);
MatchInput bot_set_rally_points(const MatchState& state, Bot& bot);
bool bot_is_rally_cell_valid(const MatchState& state, ivec2 rally_cell, int rally_margin, Rect origin_rect);
ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building);
MatchInput bot_unload_unreserved_carriers(const MatchState& state, const Bot& bot);

EntityId bot_find_entity(BotFindEntityParams params);
EntityId bot_find_best_entity(BotFindBestEntityParams params);
std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell);

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id);
void bot_release_entity(Bot& bot, EntityId entity_id);
int bot_score_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
int bot_score_entity_list(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entity_list);

BotEntityCount bot_entity_count_empty();
uint32_t bot_entity_count_size(const BotEntityCount& count);
bool bot_is_entity_count_empty(const BotEntityCount& count);
BotEntityCount bot_entity_count_add(const BotEntityCount& left, const BotEntityCount& right);
BotEntityCount bot_entity_count_subtract(const BotEntityCount& left, const BotEntityCount& right);
bool bot_entity_count_is_gte_to(const BotEntityCount& list, const BotEntityCount& other);

EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);
bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell);
bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
bool bot_has_landmine_squad(const Bot& bot);
bool bot_has_desired_squad_of_type(const Bot& bot, BotSquadType type);
std::unordered_map<uint32_t, int> bot_get_enemy_hall_defense_scores(const MatchState& state, const Bot& bot);
int bot_get_least_defended_enemy_hall_score(const MatchState& state, const Bot& bot);
uint32_t bot_get_mining_base_count(const MatchState& state, const Bot& bot);
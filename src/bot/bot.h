#pragma once

#include "defines.h"
#include "bot/entity_count.h"
#include "match/state.h"
#include "core/match_setting.h"
#include <unordered_map>

enum BotOpener {
    BOT_OPENER_BANDIT_RUSH,
    BOT_OPENER_BUNKER,
    BOT_OPENER_EXPAND_FIRST,
#ifdef GOLD_DEBUG
    BOT_OPENER_TECH_FIRST,
#endif
    BOT_OPENER_COUNT
};

enum BotUnitComp {
    BOT_UNIT_COMP_NONE,
    BOT_UNIT_COMP_COWBOY_BANDIT,
    BOT_UNIT_COMP_COWBOY_BANDIT_PYRO,
    BOT_UNIT_COMP_COWBOY_SAPPER_PYRO,
    BOT_UNIT_COMP_COWBOY_WAGON,
    BOT_UNIT_COMP_SOLDIER_BANDIT,
    BOT_UNIT_COMP_SOLDIER_CANNON
};

enum BotMetric {
    BOT_METRIC_ENTITY_COUNT,
    BOT_METRIC_UNRESERVED_ENTITY_COUNT,
    BOT_METRIC_IN_PROGRESS_ENTITY_COUNT,
    BOT_METRIC_AVAILABLE_PRODUCTION_BUILDING_COUNT
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
    EntityCount entity_count;
};

struct BotBaseInfo {
    uint8_t controlling_player;
    bool has_surrounding_hall;
    bool is_saturated;
    bool has_gold;
    bool is_low_on_gold;
    bool is_under_attack;
    int defense_score;
};

struct BotRetreatMemory {
    std::vector<EntityId> enemy_list;
    int retreat_count;
    uint32_t retreat_time;
};

struct Bot {
    uint8_t player_id;
    Difficulty difficulty;

    // Unit management
    std::unordered_map<EntityId, bool> is_entity_reserved;

    // Production
    BotUnitComp unit_comp;
    BotUnitComp preferred_unit_comp;
    EntityCount desired_buildings;
    EntityCount desired_army_ratio;
    std::vector<BotDesiredSquad> desired_squads;
    std::queue<EntityId> buildings_to_set_rally_points;
    uint32_t macro_cycle_timer;
    uint32_t macro_cycle_count;

    // Squads
    std::vector<BotSquad> squads;
    uint32_t next_landmine_time;

    // Scouting
    EntityId scout_id;
    uint32_t scout_info;
    uint32_t last_scout_time;
    std::vector<EntityId> entities_to_scout;
    std::unordered_map<EntityId, bool> is_entity_assumed_to_be_scouted;

    // Base info
    std::unordered_map<EntityId, BotBaseInfo> base_info;
    std::unordered_map<EntityId, BotRetreatMemory> retreat_memory;
};

Bot bot_init(uint8_t player_id, Difficulty difficulty, BotOpener opener, BotUnitComp preferred_unit_comp);
BotOpener bot_roll_opener(int* lcg_seed, Difficulty difficulty);
BotUnitComp bot_roll_preferred_unit_comp(int* lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_timer);

// Strategy

void bot_strategy_update(const MatchState& state, Bot& bot);
bool bot_should_surrender(const MatchState& state, const Bot& bot, uint32_t match_timer);
bool bot_has_non_miner_army(const MatchState& state, const Bot& bot);
bool bot_is_mining(const MatchState& state, const Bot& bot);
bool bot_should_expand(const MatchState& state, const Bot& bot);
bool bot_should_tech_into_preferred_unit_comp(const MatchState& state, const Bot& bot);
uint32_t bot_get_player_mining_base_count(const Bot& bot, uint8_t player_id);
bool bot_are_bases_fully_saturated(const Bot& bot);
uint32_t bot_get_low_on_gold_base_count(const Bot& bot);
uint32_t bot_get_max_enemy_mining_base_count(const MatchState& state, const Bot& bot);
bool bot_has_base_that_is_missing_a_hall(const Bot& bot, EntityId* goldmine_id = NULL);
bool bot_is_unoccupied_goldmine_available(const Bot& bot);
EntityId bot_get_least_defended_enemy_base_goldmine_id(const MatchState& state, const Bot& bot);
bool bot_is_under_attack(const Bot& bot);
void bot_defend_location(const MatchState& state, Bot& bot, ivec2 location, uint32_t options);
int bot_score_entities_at_location(const MatchState& state, const Bot& bot, ivec2 location, std::function<bool(const Entity& entity, EntityId entity_id)> filter);
bool bot_should_attack(const MatchState& state, const Bot& bot);
bool bot_should_all_in(const Bot& bot);

// Production

MatchInput bot_get_production_input(const MatchState& state, Bot& bot, uint32_t match_timer);
void bot_set_unit_comp(Bot& bot, BotUnitComp unit_comp);
void bot_update_desired_production(Bot& bot);

// Saturate bases

MatchInput bot_saturate_bases(const MatchState& state, Bot& bot);
ivec2 bot_get_position_near_hall_away_from_miners(const MatchState& state, ivec2 hall_cell, ivec2 goldmine_cell);
EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell);
uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot);

// Build buildings

bool bot_should_build_house(const MatchState& state, const Bot& bot);
MatchInput bot_build_building(const MatchState& state, Bot& bot, EntityType building_type);
ivec2 bot_find_building_location(const MatchState& state, ivec2 start_cell, int size);
uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id, bool count_bunkers_only);
ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot);
EntityId bot_find_goldmine_for_next_expansion(const MatchState& state, const Bot& bot);
EntityId bot_find_unoccupied_goldmine_nearest_to_entity(const MatchState& state, const Bot& bot, EntityId reference_entity_id);
EntityId bot_find_builder(const MatchState& state, const Bot& bot, ivec2 near_cell);
ivec2 bot_find_bunker_location(const MatchState& state, const Bot& bot, uint32_t nearby_hall_index);

// Research upgrades

uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot, EntityCount unreserved_and_in_progress_entity_count);
MatchInput bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade);

// Train Units

bool bot_has_building_available_to_train_units(const Bot& bot, EntityCount desired_entities, EntityCount available_building_count, EntityCount unreserved_and_in_progress_entity_count);
EntityType bot_get_unit_type_to_train(Bot& bot, EntityCount desired_entities, EntityCount available_building_count, EntityCount unreserved_and_in_progress_entity_count);
MatchInput bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type, uint32_t match_time_minutes);

// Squads

BotSquad bot_squad_create(Bot& bot, BotSquadType type, ivec2 target_cell, const std::vector<EntityId>& entity_list);
void bot_squad_dissolve(Bot& bot, BotSquad& squad);
void bot_squad_remove_entity_by_id(Bot& bot, BotSquad& squad, EntityId entity_id);
MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad, uint32_t match_timer);
void bot_squad_remove_dead_units(const MatchState& state, Bot& bot, BotSquad& squad);
bool bot_squad_is_entity_near_squad(const MatchState& state, const BotSquad& squad, const Entity& entity);
std::vector<EntityId> bot_squad_get_nearby_enemy_list(const MatchState& state, const Bot& bot, const BotSquad& squad);
bool bot_squad_should_retreat(const MatchState& state, const Bot& bot, const BotSquad& squad, int nearby_enemy_score);
MatchInput bot_squad_bunker_micro(const MatchState& state, const Bot& bot, const BotSquad& squad);
EntityId bot_squad_get_bunker_id(const MatchState& state, const BotSquad& squad);
bool bot_squad_has_bunker(const MatchState& state, const BotSquad& squad);
bool bot_squad_bunker_units_are_engaged(const MatchState& state, const BotSquad& squad, const Entity& bunker, EntityId bunker_id);
uint32_t bot_squad_get_carrier_capacity(const MatchState& state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id);
bool bot_squad_carrier_has_capacity(const MatchState& state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id);
MatchInput bot_squad_garrison_into_carrier(const MatchState& state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id, const std::vector<EntityId>& entity_list);
bool bot_squad_carrier_has_en_route_infantry(const MatchState& state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id, ivec2* en_route_infantry_center);
MatchInput bot_squad_move_carrier_toward_en_route_infantry(const MatchState& state, const Entity& carrier, EntityId carrier_id, ivec2 en_route_infantry_center);
bool bot_squad_should_carrier_unload_garrisoned_units(const MatchState& state, const BotSquad& squad, const Entity& carrier);
MatchInput bot_squad_pyro_micro(const MatchState& state, Bot& bot, BotSquad& squad, const Entity& pyro, EntityId pyro_id, ivec2 nearby_enemy_cell);
int bot_squad_get_molotov_cell_score(const MatchState& state, const Entity& pyro, ivec2 cell);
ivec2 bot_squad_find_best_molotov_cell(const MatchState& state, const Entity& pyro, ivec2 attack_point);
MatchInput bot_squad_detective_micro(const MatchState& state, Bot& bot, BotSquad& squad, const Entity& detective, EntityId detective_id);
MatchInput bot_squad_a_move_miners(const MatchState& state, const BotSquad& squad, const Entity& first_miner, EntityId first_miner_id, ivec2 nearby_enemy_cell);
MatchInput bot_squad_move_distant_units_to_target(const MatchState& state, const BotSquad& squad, const std::vector<EntityId>& entity_list);
MatchInput bot_squad_return_to_nearest_base(const MatchState& state, Bot& bot, BotSquad& squad);
EntityId bot_squad_get_nearest_base_goldmine_id(const MatchState& state, const Bot& bot, const BotSquad& squad);

std::vector<EntityId> bot_create_entity_list_from_entity_count(const MatchState& state, const Bot& bot, EntityCount entity_count);
void bot_entity_list_filter(const MatchState& state, std::vector<EntityId>& entity_list, std::function<bool(const Entity& entity, EntityId entity_id)> filter);
ivec2 bot_entity_list_get_center(const MatchState& state, const std::vector<EntityId>& entity_list);
ivec2 bot_squad_choose_target_cell(const MatchState& state, const Bot& bot, BotSquadType type, const std::vector<EntityId>& entity_list);
ivec2 bot_squad_get_landmine_target_cell(const MatchState& state, const Bot& bot, ivec2 pyro_cell);
ivec2 bot_squad_get_attack_target_cell(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entity_list);
ivec2 bot_squad_get_defend_target_cell(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entity_list);

MatchInput bot_squad_landmines_micro(const MatchState& state, Bot& bot, const BotSquad& squad, uint32_t match_timer);

// Scouting

void bot_scout_gather_info(const MatchState& state, Bot& bot);
void bot_update_base_info(const MatchState& state, Bot& bot);
MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_timer);
std::vector<EntityId> bot_determine_entities_to_scout(const MatchState& state, const Bot& bot);
bool bot_is_entity_in_entities_to_scout_list(const Bot& bot, EntityId entity_id);
void bot_release_scout(Bot& bot);
bool bot_should_scout(const Bot& bot, uint32_t match_timer);
bool bot_check_scout_info(const Bot& bot, uint32_t flag);
void bot_set_scout_info(Bot& bot, uint32_t flag, bool value);

// Entity Reservation

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id);
void bot_reserve_entity(Bot& bot, EntityId entity_id, bool log = true);
void bot_release_entity(Bot& bot, EntityId entity_id, bool log = true);

// Entity Type Util

EntityType bot_get_building_which_trains(EntityType unit_type);
EntityType bot_get_building_prereq(EntityType building_type);
EntityType bot_get_building_which_researches(uint32_t upgrade);

// Metrics

EntityCount bot_count_in_progress_entities(const MatchState& state, const Bot& bot);
EntityCount bot_count_unreserved_entities(const MatchState& state, const Bot& bot);
EntityCount bot_count_unreserved_army(const MatchState& state, const Bot& bot);
bool bot_is_entity_unreserved_army(const Bot& bot, const Entity& entity, EntityId entity_id);
EntityCount bot_count_available_production_buildings(const MatchState& state, const Bot& bot);
int bot_score_unreserved_army(const MatchState& state, const Bot& bot);
int bot_score_allied_army(const MatchState& state, const Bot& bot);
int bot_score_enemy_army(const MatchState& state, const Bot& bot);
bool bot_is_entity_type_production_building(EntityType type);
std::vector<EntityType> bot_entity_types_production_buildings();

// Misc

EntityId bot_find_threatened_in_progress_building(const MatchState& state, const Bot& bot);
EntityId bot_find_building_in_need_of_repair(const MatchState& state, const Bot& bot);
MatchInput bot_repair_building(const MatchState& state, const Bot& bot, EntityId building_id);
MatchInput bot_rein_in_stray_units(const MatchState& state, const Bot& bot);
MatchInput bot_update_building_rally_point(const MatchState& state, const Bot& bot, EntityId building_id);
ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building);
bool bot_is_rally_cell_valid(const MatchState& state, ivec2 rally_cell, int rally_margin, Rect origin_rect);
MatchInput bot_unload_unreserved_carriers(const MatchState& state, const Bot& bot);

// Score Util

int bot_score_entity(const MatchState& state, const Bot& bot, const Entity& entity);
int bot_score_entity_list(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entity_list);

// Util

bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id);
EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine);
bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell);
MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id);
MatchInput bot_unit_flee(const MatchState& state, const Bot& bot, EntityId entity_id);
ivec2 bot_get_unoccupied_cell_near_goldmine(const MatchState& state, const Bot& bot, EntityId goldmine_id);
uint32_t bot_get_index_of_squad_of_type(const Bot& bot, BotSquadType type);
bool bot_has_squad_of_type(const Bot& bot, BotSquadType type);
bool bot_has_desired_squad_of_type(const Bot& bot, BotSquadType type);
bool bot_is_bandit_rushing(const Bot& bot);
bool bot_is_area_safe(const MatchState& state, const Bot& bot, ivec2 cell);
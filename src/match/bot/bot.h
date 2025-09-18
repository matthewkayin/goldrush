#pragma once

#include "types.h"

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed);
MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes);

BotGoalType bot_choose_opening_goal_type(Bot& bot);
BotGoalType bot_choose_next_goal_type(const MatchState& state, const Bot& bot);

BotGoal bot_goal_create(const MatchState& state, Bot& bot, BotGoalType goal_type);
bool bot_goal_should_be_abandoned(const MatchState& state, const Bot& bot, const BotGoal& goal);
bool bot_goal_is_empty(const BotGoal& goal);
bool bot_goal_is_met(const MatchState& state, const Bot& bot, const BotGoal& goal);

bool bot_is_base_under_attack(const MatchState& state, const Bot& bot);
void bot_handle_base_under_attack(const MatchState& state, Bot& bot);
uint32_t bot_get_mining_base_count(const MatchState& state, const Bot& bot);

MatchInput bot_set_rally_points(const MatchState& state, const Bot& bot);
bool bot_is_rally_cell_valid(const MatchState& state, ivec2 rally_cell, int rally_margin);
ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building);
MatchInput bot_unload_unreserved_carriers(const MatchState& state, const Bot& bot);
MatchInput bot_rein_in_stray_units(const MatchState& state, const Bot& bot);
MatchInput bot_repair_burning_buildings(const MatchState& state, const Bot& bot);
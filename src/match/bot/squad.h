#pragma once

#include "../state.h"
#include "types.h"

void bot_squad_create_from_goal(const MatchState& state, Bot& bot, const BotGoal& goal);
void bot_squad_dissolve(const MatchState& state, Bot& bot, BotSquad& squad);
MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad);
bool bot_squad_should_retreat(const MatchState& state, const Bot& bot, const BotSquad& squad);
MatchInput bot_squad_return_to_nearest_base(const MatchState& state, Bot& bot, BotSquad& squad);
bool bot_squad_is_detective_harass(const MatchState& state, const BotSquad& squad);
bool bot_squad_can_defend_against_detectives(const MatchState& state, const BotSquad& squad);
bool bot_squad_is_engaged(const MatchState& state, const Bot& bot, const BotSquad& squad);
ivec2 bot_squad_get_center(const MatchState& state, const BotSquad& squad);
ivec2 bot_squad_choose_attack_point(const MatchState& state, const Bot& bot, const BotSquad& squad);
ivec2 bot_squad_choose_defense_point(const MatchState& state, const Bot& bot, const BotSquad& squad);
void bot_pathfind_and_avoid_landmines(const MatchState& state, const Bot& bot, ivec2 from, ivec2 to, std::vector<ivec2>* path);
bool bot_is_unit_already_attacking_nearby_target(const MatchState& state, const Entity& infantry, const Entity& nearby_enemy);
int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell);
ivec2 bot_find_best_molotov_cell(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 attack_point);
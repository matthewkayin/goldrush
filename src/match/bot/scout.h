#pragma once

#include "../state.h"
#include "types.h"

void bot_scout_update(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes);
void bot_release_scout(Bot& bot);
bool bot_should_scout(const MatchState& state, const Bot& bot, uint32_t match_time_minutes);
bool bot_scout_danger_is_expired(const MatchState& state, const BotScoutDanger& danger, uint32_t match_time_mintutes);
ivec2 bot_scout_danger_get_cell(const MatchState& state, const BotScoutDanger& danger);
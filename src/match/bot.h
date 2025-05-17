#pragma once

#include "input.h"
#include "state.h"
#include <queue>

MatchInput match_bot_compute_turn_input(uint8_t bot_player_id, const MatchState& state);
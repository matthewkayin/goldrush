#pragma once

#include "input.h"
#include "state.h"
#include <cstdint>
#include <vector>

void match_bot_get_turn_inputs(const MatchState& state, uint8_t bot_player_id, std::vector<MatchInput>& inputs);
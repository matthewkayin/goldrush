#pragma once

#include "input.h"
#include "state.h"
#include <cstdint>

MatchInput match_bot_get_turn_input(const MatchState& state, uint8_t bot_player_id);
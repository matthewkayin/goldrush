#pragma once

#include "map.h"
#include "noise.h"

struct MatchState {
    Map map;
};

MatchState match_init(int32_t lcg_seed, Noise& noise);
void match_update(MatchState& state);
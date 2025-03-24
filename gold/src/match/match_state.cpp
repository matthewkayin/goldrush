#include "match_state.h"

#include "core/logger.h"
#include "lcg.h"

MatchState match_init(int32_t lcg_seed, Noise& noise) {
    MatchState state;
    lcg_srand(lcg_seed);
    log_info("Set random seed to %i", lcg_seed);
    std::vector<ivec2> player_spawns;
    map_init(state.map, noise, player_spawns);

    return state;
}

void match_update(MatchState& world) {

}
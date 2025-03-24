#include "match_state.h"

#include "core/logger.h"
#include "lcg.h"

MatchState match_init(int32_t lcg_seed, Noise& noise) {
    MatchState state;
    lcg_srand(lcg_seed);
    log_info("Set random seed to %i", lcg_seed);
    map_init(state.map, noise);

    return state;
}

void match_update(MatchState& world) {

}
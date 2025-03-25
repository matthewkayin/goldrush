#include "match_state.h"

#include "core/logger.h"
#include "lcg.h"

static const uint32_t MATCH_PLAYER_STARTING_GOLD = 50;
static const uint32_t MATCH_GOLDMINE_STARTING_GOLD = 5000;

MatchState match_init(int32_t lcg_seed, Noise& noise, MatchPlayer players[MAX_PLAYERS]) {
    MatchState state;
    lcg_srand(lcg_seed);
    log_info("Set random seed to %i", lcg_seed);
    std::vector<ivec2> player_spawns;
    std::vector<ivec2> goldmine_cells;
    map_init(state.map, noise, player_spawns, goldmine_cells);
    memcpy(state.players, players, sizeof(state.players));

    // Generate goldmines
    for (ivec2 cell : goldmine_cells) {
        entity_create_goldmine(state, cell, MATCH_GOLDMINE_STARTING_GOLD);
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state.players[player_id].active) {
            continue;
        }
        state.players[player_id].gold = MATCH_PLAYER_STARTING_GOLD;
        state.players[player_id].upgrades = 0;
        state.players[player_id].upgrades_in_progress = 0;
    }

    return state;
}

void match_update(MatchState& world) {

}
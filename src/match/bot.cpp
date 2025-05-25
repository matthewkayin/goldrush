#include "bot.h"

#include "core/logger.h"

EntityId match_bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId match_bot_get_nearest_idle_worker(const MatchState& state, uint8_t bot_player_id, ivec2 cell);

MatchInput match_bot_get_turn_input(const MatchState& state, uint8_t bot_player_id) {
    // Saturate bases
    for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
        const Entity& hall = state.entities[hall_index];
        if (hall.player_id != bot_player_id || hall.type != ENTITY_HALL || hall.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        // Find nearest goldmine
        Target goldmine_target = match_entity_target_nearest_gold_mine(state, hall);
        if (goldmine_target.type == TARGET_NONE) {
            continue;
        }
        EntityId goldmine_id = goldmine_target.id;

        // Check the miner count
        uint32_t miner_count = match_get_miners_on_gold(state, goldmine_id, bot_player_id);
        // If oversaturated, pull workers off gold
        if (miner_count > MATCH_MAX_MINERS_ON_GOLD) {
            EntityId miner_id = match_bot_pull_worker_off_gold(state, bot_player_id, goldmine_id);
            if (miner_id != ID_NULL) {
                ivec2 goldmine_cell = state.entities.get_by_id(goldmine_id).cell;

                MatchInput input;
                input.type = MATCH_INPUT_MOVE_CELL;
                input.move.shift_command = 0;
                input.move.target_cell = hall.cell + ((hall.cell - goldmine_cell) * -1);
                input.move.target_id = ID_NULL;
                input.move.entity_count = 1;
                input.move.entity_ids[0] = miner_id;
                return input;
            }
        }
        // If undersaturated, put workers on gold
        if (miner_count < MATCH_MAX_MINERS_ON_GOLD) {
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_ENTITY;
            input.move.shift_command = 0;
            input.move.target_cell = ivec2(0, 0);
            input.move.target_id = goldmine_id;
            input.move.entity_count = 0;

            while (miner_count + input.move.entity_count < MATCH_MAX_MINERS_ON_GOLD) {
                EntityId idle_worker_id = match_bot_get_nearest_idle_worker(state, bot_player_id, hall.cell);
                if (idle_worker_id == ID_NULL) {
                    break;
                }

                input.move.entity_ids[input.move.entity_count] = idle_worker_id;
                input.move.entity_count++;
            }
            
            if (input.move.entity_count != 0) {
                return input;
            }

            // If we're still here, then there were no idle workers
            // So we'll create one out of the town hall
            if (state.players[bot_player_id].gold >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
                return (MatchInput) {
                    .type = MATCH_INPUT_BUILDING_ENQUEUE,
                    .building_enqueue = (MatchInputBuildingEnqueue) {
                        .building_id = state.entities.get_id_of(hall_index),
                        .item_type = BUILDING_QUEUE_ITEM_UNIT,
                        .item_subtype = ENTITY_MINER
                    }
                };
            }

            // If we're training a miner, rally to the mine
            if (!hall.queue.empty() && hall.rally_point.x == -1) {
                MatchInput rally_input;
                rally_input.type = MATCH_INPUT_RALLY;
                rally_input.rally.building_count = 1;
                rally_input.rally.building_ids[0] = state.entities.get_id_of(hall_index);
                rally_input.rally.rally_point = (state.entities.get_by_id(goldmine_id).cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
                return rally_input;
            }
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

EntityId match_bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id) {
    for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
        const Entity& miner = state.entities[miner_index];
        if (miner.player_id != bot_player_id || 
                !entity_is_selectable(miner) || 
                // is_entity_mining covers the check that this is a miner
                !match_is_entity_mining(state, miner) ||
                miner.gold_mine_id != goldmine_id) {
            continue;
        }

        return state.entities.get_id_of(miner_index);
    }

    return ID_NULL;
}

EntityId match_bot_get_nearest_idle_worker(const MatchState& state, uint8_t bot_player_id, ivec2 cell) {
    uint32_t idle_worker_index = INDEX_INVALID;

    for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
        const Entity& miner = state.entities[miner_index];
        // We don't have to check target queue because the bots don't use them
        if (miner.type != ENTITY_MINER || 
                miner.player_id != bot_player_id || 
                !entity_is_selectable(miner) || 
                miner.mode != MODE_UNIT_IDLE ||
                miner.target.type != TARGET_NONE)  {
            continue;
        }

        if (idle_worker_index == INDEX_INVALID || 
                ivec2::manhattan_distance(miner.cell, cell) < 
                ivec2::manhattan_distance(state.entities[idle_worker_index].cell, cell)) {
            idle_worker_index = miner_index;
        }
    }

    if (idle_worker_index == INDEX_INVALID) {
        return ID_NULL;
    }

    return state.entities.get_id_of(idle_worker_index);
}
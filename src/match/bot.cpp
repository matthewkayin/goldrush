#include "bot.h"

#include "core/logger.h"

EntityId match_bot_get_nearest_idle_worker(uint8_t bot_player_id, const MatchState& state, ivec2 cell);
EntityId match_bot_get_nearest_builder(uint8_t bot_player_id, const MatchState& state, ivec2 cell);
ivec2 match_bot_get_building_cell(uint8_t bot_player_id, const MatchState& state, ivec2 near_cell, int size);

MatchInput match_bot_compute_turn_input(uint8_t bot_player_id, const MatchState& state) {
    // Build house
    uint32_t max_population = match_get_player_max_population(state, bot_player_id);
    if (max_population != MATCH_MAX_POPULATION && 
            max_population - match_get_player_population(state, bot_player_id) <= 2 && 
            state.players[bot_player_id].gold >= entity_get_data(ENTITY_HOUSE).gold_cost) {

        ivec2 town_hall_cell = ivec2(0, 0);
        for (uint32_t town_hall_index = 0; town_hall_index < state.entities.size(); town_hall_index++) {
            const Entity& hall = state.entities[town_hall_index];
            if (hall.player_id != bot_player_id || hall.mode != MODE_BUILDING_FINISHED || hall.type != ENTITY_HALL) {
                continue;
            }

            town_hall_cell = hall.cell;
        }

        bool is_already_building_house = false;
        for (const Entity& entity : state.entities) {
            if (entity.player_id == bot_player_id && entity.target.type == TARGET_BUILD && entity.target.build.building_type == ENTITY_HOUSE) {
                is_already_building_house = true;
            }
        }

        if (!is_already_building_house) {
            EntityId builder_id = match_bot_get_nearest_builder(bot_player_id, state, town_hall_cell);
            if (builder_id != ID_NULL) {
                const Entity& builder = state.entities.get_by_id(builder_id);

                ivec2 building_cell = match_bot_get_building_cell(bot_player_id, state, builder.cell, entity_get_data(ENTITY_HOUSE).cell_size);
                if (building_cell.x != -1) {
                    MatchInput input;
                    input.type = MATCH_INPUT_BUILD;
                    input.build.shift_command = false;
                    input.build.building_type = ENTITY_HOUSE;
                    input.build.target_cell = building_cell;
                    input.build.entity_count = 1;
                    input.build.entity_ids[0] = builder_id;
                    return input;
                }
            }
        }
    }
    
    // Make sure all gold mines have miners on them
    for (uint32_t town_hall_index = 0; town_hall_index < state.entities.size(); town_hall_index++) {
        const Entity& hall = state.entities[town_hall_index];
        if (hall.player_id != bot_player_id || hall.mode != MODE_BUILDING_FINISHED || hall.type != ENTITY_HALL) {
            continue;
        }

        Target goldmine_target = match_entity_target_nearest_gold_mine(state, hall);
        if (goldmine_target.type == TARGET_NONE) {
            continue;
        }

        // Send idle workers to mine gold
        uint32_t miner_count = match_get_miners_on_goldmine(state, goldmine_target.id, bot_player_id);
        MatchInput mine_input;
        mine_input.type = MATCH_INPUT_MOVE_ENTITY;
        mine_input.move.shift_command = false;
        mine_input.move.target_cell = ivec2(0, 0);
        mine_input.move.target_id = goldmine_target.id;
        mine_input.move.entity_count = 0;
        while (miner_count < 8) {
            EntityId nearest_miner_id = match_bot_get_nearest_idle_worker(bot_player_id, state, hall.cell);
            if (nearest_miner_id == ID_NULL) {
                break;
            }

            mine_input.move.entity_ids[mine_input.move.entity_count] = nearest_miner_id;
            mine_input.move.entity_count++;
            miner_count++;
        }

        if (mine_input.move.entity_count != 0) {
            return mine_input;
        }

        // Hire miners
        if (miner_count < 8 && hall.queue.empty() && state.players[bot_player_id].gold >= entity_get_data(ENTITY_MINER).gold_cost) {
            return (MatchInput) {
                .type = MATCH_INPUT_BUILDING_ENQUEUE,
                .building_enqueue = (MatchInputBuildingEnqueue) {
                    .building_id = state.entities.get_id_of(town_hall_index),
                    .item_type = BUILDING_QUEUE_ITEM_UNIT,
                    .item_subtype = ENTITY_MINER
                }
            };
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

EntityId match_bot_get_nearest_idle_worker(uint8_t bot_player_id, const MatchState& state, ivec2 cell) {
    EntityId nearest_id = ID_NULL;
    int nearest_distance = -1;

    for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
        const Entity& miner = state.entities[miner_index];
        if (!entity_is_selectable(miner) || miner.player_id != bot_player_id || miner.type != ENTITY_MINER || miner.mode != MODE_UNIT_IDLE || miner.target.type != TARGET_NONE) {
            continue;
        }

        int miner_distance = ivec2::manhattan_distance(miner.cell, cell);
        if (nearest_id == ID_NULL || miner_distance < nearest_distance) {
            nearest_id = state.entities.get_id_of(miner_index);
            nearest_distance = miner_distance;
        }
    }

    return nearest_id;
}

EntityId match_bot_get_nearest_builder(uint8_t bot_player_id, const MatchState& state, ivec2 cell) {
    EntityId idle_worker_id = match_bot_get_nearest_idle_worker(bot_player_id, state, cell);
    if (idle_worker_id != ID_NULL) {
        return idle_worker_id;
    }

    EntityId nearest_id = ID_NULL;
    int nearest_distance = -1;

    for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
        const Entity& miner = state.entities[miner_index];
        if (!entity_is_selectable(miner) || miner.player_id != bot_player_id || miner.type != ENTITY_MINER) {
            continue;
        }

        int miner_distance = ivec2::manhattan_distance(miner.cell, cell);
        if (nearest_id == ID_NULL || miner_distance < nearest_distance) {
            nearest_id = state.entities.get_id_of(miner_index);
            nearest_distance = miner_distance;
        }
    }

    return nearest_id;
}

ivec2 match_bot_get_building_cell(uint8_t bot_player_id, const MatchState& state, ivec2 near_cell, int size) {
    std::vector<ivec2> frontier;
    std::vector<int> explored = std::vector<int>(state.map.width * state.map.height, 0);

    frontier.push_back(near_cell);
    while (!frontier.empty()) {
        uint32_t smallest_index = 0;
        for (uint32_t frontier_index = 1; frontier_index < frontier.size(); frontier_index++) {
            if (ivec2::manhattan_distance(near_cell, frontier[frontier_index]) < ivec2::manhattan_distance(near_cell, frontier[smallest_index])) {
                smallest_index = frontier_index;
            }
        }
        ivec2 smallest = frontier[smallest_index];
        frontier[smallest_index] = frontier[frontier.size() - 1];
        frontier.pop_back();

        // Assumed to be in bounds at this point 
        if (!map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, smallest - ivec2(2, 2), size + 4) && 
                map_is_cell_rect_same_elevation(state.map, smallest - ivec2(2, 2), size + 4)) {
            return smallest;
        }

        explored[smallest.x + (smallest.y * state.map.width)] = 1;
        
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = smallest + DIRECTION_IVEC2[direction];

            if (!map_is_cell_rect_in_bounds(state.map, child, size)) {
                continue;
            }

            if (explored[child.x + (child.y * state.map.width)] == 1) {
                continue;
            }

            bool is_in_frontier = false;
            for (const ivec2& cell : frontier) {
                if (cell == child) {
                    is_in_frontier = true;
                    break;
                }
            }

            if (is_in_frontier) {
                continue;
            }

            frontier.push_back(child);
        } // End for each direction
    } // End while frontier not empty

    log_warn("Bot %u could not find place to put building.", bot_player_id);
    return ivec2(-1, -1);
}
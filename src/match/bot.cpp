#include "bot.h"

#include "core/logger.h"

struct BotGoal {
    EntityType entity_type;
    uint32_t entity_count;
};

bool match_bot_should_build_house(const MatchState& state, uint8_t bot_player_id);
MatchInput match_bot_get_build_house_input(const MatchState& state, uint8_t bot_player_id);

EntityId match_bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId match_bot_get_nearest_idle_worker(const MatchState& state, uint8_t bot_player_id, ivec2 cell);
EntityId match_bot_find_builder(const MatchState& state, uint8_t bot_player_id, uint32_t near_hall_index);
uint32_t match_bot_find_hall_index_with_least_nearby_houses(const MatchState& state, uint8_t bot_player_id);
ivec2 match_bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
uint32_t match_bot_get_effective_gold(const MatchState& state, uint8_t bot_player_id);
EntityId match_bot_get_idle_scout(const MatchState& state, uint8_t bot_player_id);

MatchInput match_bot_get_turn_input(const MatchState& state, uint8_t bot_player_id) {
    uint32_t bot_effective_gold = match_bot_get_effective_gold(state, bot_player_id);

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
            if (bot_effective_gold >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
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

    // Build houses
    if (match_bot_should_build_house(state, bot_player_id)) {
        if (bot_effective_gold >= entity_get_data(ENTITY_HOUSE).gold_cost) {
            MatchInput build_house_input = match_bot_get_build_house_input(state, bot_player_id);
            if (build_house_input.type != MATCH_INPUT_NONE) {
                return build_house_input;
            } 
        } else {
            bot_effective_gold = 0;
        }
    }

    // Scout
    EntityId scout_id = match_bot_get_idle_scout(state, bot_player_id);
    if (scout_id != ID_NULL) {
        for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
            const Entity& goldmine = state.entities[goldmine_index];
            if (goldmine.type != ENTITY_GOLDMINE) {
                continue;
            }
            if (!match_is_cell_rect_explored(state, state.players[bot_player_id].team, goldmine.cell, entity_get_data(goldmine.type).cell_size)) {
                MatchInput scout_input;
                scout_input.type = MATCH_INPUT_MOVE_ENTITY;
                scout_input.move.shift_command = 0;
                scout_input.move.target_cell = ivec2(0, 0);
                scout_input.move.target_id = state.entities.get_id_of(goldmine_index);
                scout_input.move.entity_count = 1;
                scout_input.move.entity_ids[0] = scout_id;
                return scout_input;
            }
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

bool match_bot_should_build_house(const MatchState& state, uint8_t bot_player_id) {
    // Count in-progress houses and units
    uint32_t houses_in_progress = 0;
    uint32_t units_in_progress = 0;
    for (const Entity& entity : state.entities) {
        if (entity.player_id != bot_player_id) {
            continue;
        }
        // This will count up both miners who are en-route to build and miners who are actively building
        if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD && 
                (entity.target.build.building_type == ENTITY_HOUSE || entity.target.build.building_type == ENTITY_HALL)) {
            houses_in_progress++;
        } else if (entity_is_building(entity.type) && !entity.queue.empty() && entity.queue.front().type == BUILDING_QUEUE_ITEM_UNIT) {
            units_in_progress += entity_get_data(entity.queue.front().unit_type).unit_data.population_cost;
        }
    }
    uint32_t future_max_population = match_get_player_max_population(state, bot_player_id) + (10 * houses_in_progress);
    uint32_t future_population = match_get_player_population(state, bot_player_id) + units_in_progress;

    return future_max_population < MATCH_MAX_POPULATION && (int)future_max_population - (int)future_population <= 1;
}

MatchInput match_bot_get_build_house_input(const MatchState& state, uint8_t bot_player_id) {
    uint32_t hall_index = match_bot_find_hall_index_with_least_nearby_houses(state, bot_player_id);
    if (hall_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId builder_id = match_bot_find_builder(state, bot_player_id, hall_index);
    if (builder_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 building_location = match_bot_find_building_location(state, bot_player_id, state.entities.get_by_id(builder_id).cell, entity_get_data(ENTITY_HOUSE).cell_size);
    if (building_location.x == -1) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILD;
    input.build.shift_command = 0;
    input.build.target_cell = building_location;
    input.build.building_type = ENTITY_HOUSE;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = builder_id;

    return input;
}

EntityId match_bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id) {
    for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
        const Entity& miner = state.entities[miner_index];
        // match_is_entity_mining() will also check that this is a miner
        if (miner.player_id != bot_player_id || 
                !entity_is_selectable(miner) || 
                !match_is_entity_mining(state, miner) || 
                (goldmine_id != ID_NULL && miner.gold_mine_id != goldmine_id)) {
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

EntityId match_bot_find_builder(const MatchState& state, uint8_t bot_player_id, uint32_t near_hall_index) {
    GOLD_ASSERT(near_hall_index != INDEX_INVALID);
    const Entity& hall = state.entities[near_hall_index];

    // First try an idle worker
    EntityId builder = match_bot_get_nearest_idle_worker(state, bot_player_id, hall.cell);
    if (builder != ID_NULL) {
        return builder;
    }

    Target mine_target = match_entity_target_nearest_gold_mine(state, hall);
    if (mine_target.type == TARGET_NONE) {
        return ID_NULL;
    }

    // Then try a worker from the gold mine
    return match_bot_pull_worker_off_gold(state, bot_player_id, mine_target.id);
}

uint32_t match_bot_find_hall_index_with_least_nearby_houses(const MatchState& state, uint8_t bot_player_id) {
    // First find all the town halls for this bot
    std::vector<uint32_t> hall_indices;
    std::vector<uint32_t> houses_near_hall;
    for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
        const Entity& hall = state.entities[hall_index];
        if (hall.player_id != bot_player_id || hall.type != ENTITY_HALL || hall.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        hall_indices.push_back(hall_index);
        houses_near_hall.push_back(0);
    }

    // Return INDEX_INVALID if the bot has no halls
    if (hall_indices.empty()) {
        return INDEX_INVALID;
    }

    // Then find all the houses for this bot and determine which hall they are closest to
    for (uint32_t house_index = 0; house_index < state.entities.size(); house_index++) {
        const Entity& house = state.entities[house_index];
        if (house.player_id != bot_player_id || house.type != ENTITY_HOUSE || house.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        uint32_t nearest_hall_indicies_index = 0;
        for (uint32_t hall_indices_index = 1; hall_indices_index < hall_indices.size(); hall_indices_index++) {
            if (ivec2::manhattan_distance(house.cell, state.entities[hall_indices[hall_indices_index]].cell) <
                    ivec2::manhattan_distance(house.cell, state.entities[hall_indices[nearest_hall_indicies_index]].cell)) {
                nearest_hall_indicies_index = hall_indices_index;
            }
        }

        houses_near_hall[nearest_hall_indicies_index]++;
    }

    // Finally choose the hall with the least houses
    uint32_t least_houses_hall_indices_index = 0;
    for (uint32_t hall_indices_index = 1; hall_indices_index < hall_indices.size(); hall_indices_index++) {
        if (houses_near_hall[hall_indices_index] < houses_near_hall[least_houses_hall_indices_index]) {
            least_houses_hall_indices_index = hall_indices_index;
        }
    }

    return hall_indices[least_houses_hall_indices_index];
}

ivec2 match_bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size) {
    std::vector<ivec2> frontier = { start_cell };
    std::vector<uint32_t> explored = std::vector<uint32_t>(state.map.width * state.map.height, 0);

    while (!frontier.empty()) {
        int nearest_index = 0;
        for (int frontier_index = 1; frontier_index < frontier.size(); frontier_index++) {
            if (ivec2::manhattan_distance(frontier[frontier_index], start_cell) < 
                    ivec2::manhattan_distance(frontier[nearest_index], start_cell)) {
                nearest_index = frontier_index;
            }
        }
        ivec2 nearest = frontier[nearest_index];
        frontier[nearest_index] = frontier[frontier.size() - 1];
        frontier.pop_back();

        if (!map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, nearest - ivec2(2, 2), size + 4) && 
                map_is_cell_rect_same_elevation(state.map, nearest - ivec2(2, 2), size + 4)) {
            return nearest;
        }

        explored[nearest.x + (nearest.y * state.map.width)] = 1;

        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = nearest + DIRECTION_IVEC2[direction];
            if (!map_is_cell_rect_in_bounds(state.map, child - ivec2(2, 2), size + 4)) {
                continue;
            }

            if (explored[child.x + (child.y * state.map.width)] != 0) {
                continue;
            }

            bool is_in_frontier = false;
            for (ivec2 cell : frontier) {
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
    } // End while not frontier empty

    // If we got here it means we didn't find anywhere to build
    // Would be crazy if that happened
    return ivec2(-1, -1);
}

uint32_t match_bot_get_effective_gold(const MatchState& state, uint8_t bot_player_id) {
    uint32_t gold = state.players[bot_player_id].gold;
    for (const Entity& entity : state.entities) {
        if (entity.player_id == bot_player_id && 
                entity.type == ENTITY_MINER && 
                entity.target.type == TARGET_BUILD) {
            gold = std::max(gold - entity_get_data(entity.target.build.building_type).gold_cost, 0U);
        }
    }

    return gold;
}

EntityId match_bot_get_idle_scout(const MatchState& state, uint8_t bot_player_id) {
    for (uint32_t scout_index = 0; scout_index < state.entities.size(); scout_index++) {
        const Entity& scout = state.entities[scout_index];
        if (scout.player_id == bot_player_id && scout.type == ENTITY_WAGON && scout.mode == MODE_UNIT_IDLE) {
            return state.entities.get_id_of(scout_index);
        }
    }

    return ID_NULL;
}
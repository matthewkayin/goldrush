#include "bot.h"

#include "core/logger.h"

Bot bot_init(uint8_t player_id) {
    Bot bot;

    bot.player_id = player_id;
    bot_add_task(bot, BOT_TASK_SATURATE_BASES);
    bot_add_task(bot, BOT_TASK_BUILD_HOUSES);
    bot_add_task(bot, BOT_TASK_SCOUT);

    return bot;
};

void bot_get_turn_inputs(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs) {
    // Determine effective gold
    bot.effective_gold = state.players[bot.player_id].gold;
    for (const Entity& entity : state.entities) {
        if (entity.player_id == bot.player_id && 
                entity.type == ENTITY_MINER && 
                entity.target.type == TARGET_BUILD) {
            uint32_t gold_cost = entity_get_data(entity.target.build.building_type).gold_cost;
            bot.effective_gold = bot.effective_gold > gold_cost ? bot.effective_gold - gold_cost : 0;
        }
    }

    // Run tasks
    for (BotTask& task : bot.tasks) {
        bot_run_task(state, bot, task, inputs);
    }

    // Remove finished tasks
    uint32_t task_index = 0;
    while (task_index < bot.tasks.size()) {
        if (bot.tasks[task_index].mode == BOT_TASK_FINISHED) {
            // No pop and swap because we want the tasks to stay in order
            // The ordering of the tasks in their array determines the priority
            // in which they should be run
            bot.tasks.erase(bot.tasks.begin() + task_index);
        } else {
            task_index++;
        }
    }
}

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id) {
    return bot._is_entity_reserved.find(entity_id) != bot._is_entity_reserved.end();
}

void bot_reserve_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: reserve entity %u", entity_id);
    bot._is_entity_reserved[entity_id] = true;
}

void bot_release_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: release entity %u", entity_id);
    GOLD_ASSERT(bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end());
    bot._is_entity_reserved.erase(entity_id);
}

void bot_add_task(Bot& bot, BotTaskMode mode) {
    BotTask task;
    task.mode = mode;

    switch (task.mode) {
        case BOT_TASK_BUILD_HOUSES: {
            task.build_houses.builder_id = ID_NULL;
            break;
        }
        case BOT_TASK_SCOUT: {
            task.scout.scout_id = ID_NULL;
            break;
        }
        case BOT_TASK_BUILD: {
            memset(task.build.desired_buildings, 0, sizeof(task.build.desired_buildings));
            task.build.desired_buildings[ENTITY_HALL] = 1;
            task.build.desired_buildings[ENTITY_SALOON] = 1;
            task.build.reserved_builders_count = 0;
            break;
        }
        case BOT_TASK_FINISHED:
        case BOT_TASK_SATURATE_BASES:
            break;
    }

    bot.tasks.push_back(task);
}

void bot_run_task(const MatchState& state, Bot& bot, BotTask& task, std::vector<MatchInput>& inputs) {
    switch (task.mode) {
        case BOT_TASK_FINISHED: {
            GOLD_ASSERT(false);
            break;
        }
        case BOT_TASK_SATURATE_BASES: {
            for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
                const Entity& hall = state.entities[hall_index];
                if (hall.player_id != bot.player_id || hall.type != ENTITY_HALL || hall.mode != MODE_BUILDING_FINISHED) {
                    continue;
                }

                // Find nearest goldmine
                Target goldmine_target = match_entity_target_nearest_gold_mine(state, hall);
                if (goldmine_target.type == TARGET_NONE) {
                    continue;
                }
                EntityId goldmine_id = goldmine_target.id;

                // Check the miner count
                uint32_t miner_count = match_get_miners_on_gold(state, goldmine_id, bot.player_id);

                // If oversaturated, pull workers off gold
                if (miner_count > MATCH_MAX_MINERS_ON_GOLD) {
                    EntityId miner_id = bot_pull_worker_off_gold(state, bot.player_id, goldmine_id);
                    if (miner_id != ID_NULL) {
                        ivec2 goldmine_cell = state.entities.get_by_id(goldmine_id).cell;

                        MatchInput input;
                        input.type = MATCH_INPUT_MOVE_CELL;
                        input.move.shift_command = 0;
                        input.move.target_cell = hall.cell + ((hall.cell - goldmine_cell) * -1);
                        input.move.target_id = ID_NULL;
                        input.move.entity_count = 1;
                        input.move.entity_ids[0] = miner_id;
                        inputs.push_back(input);
                        break;
                    }
                }

                // If undersaturated, put workers on gold
                if (miner_count < MATCH_MAX_MINERS_ON_GOLD) {
                    EntityId idle_worker_id = bot_find_nearest_idle_worker(state, bot, hall.cell);
                    if (idle_worker_id != ID_NULL) {
                        MatchInput input;
                        input.type = MATCH_INPUT_MOVE_ENTITY;
                        input.move.shift_command = 0;
                        input.move.target_cell = ivec2(0, 0);
                        input.move.target_id = goldmine_id;
                        input.move.entity_count = 1;
                        input.move.entity_ids[0] = idle_worker_id;
                        inputs.push_back(input);
                        break;
                    }

                    // If we're still here, then there were no idle workers
                    // So we'll create one out of the town hall
                    if (bot.effective_gold >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
                        bot.effective_gold -= entity_get_data(ENTITY_MINER).gold_cost;
                        inputs.push_back((MatchInput) {
                            .type = MATCH_INPUT_BUILDING_ENQUEUE,
                            .building_enqueue = (MatchInputBuildingEnqueue) {
                                .building_id = state.entities.get_id_of(hall_index),
                                .item_type = BUILDING_QUEUE_ITEM_UNIT,
                                .item_subtype = ENTITY_MINER
                            }
                        });

                        // Since we're training a miner, rally to the mine
                        if (hall.rally_point.x == -1) {
                            MatchInput rally_input;
                            rally_input.type = MATCH_INPUT_RALLY;
                            rally_input.rally.building_count = 1;
                            rally_input.rally.building_ids[0] = state.entities.get_id_of(hall_index);
                            rally_input.rally.rally_point = (state.entities.get_by_id(goldmine_id).cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
                            inputs.push_back(rally_input);
                        }
                        return;
                    }
                }
            } // End for each hall index
            break;
        } // End task saturate bases
        case BOT_TASK_BUILD_HOUSES: {
            // If we are already building a house, check on our builder's progress
            if (task.build_houses.builder_id != ID_NULL) {
                uint32_t builder_index = state.entities.get_index_of(task.build_houses.builder_id);
                if (builder_index == INDEX_INVALID || 
                        state.entities[builder_index].health == 0 ||
                        state.entities[builder_index].target.type != TARGET_BUILD) {
                    bot_release_entity(bot, task.build_houses.builder_id);
                    task.build_houses.builder_id = ID_NULL;
                } else {
                    // This break will make it so that we only build one house at a time
                    // Maybe later on we will want to allow it to build two at a time?
                    break;
                }
            }

            // Determine if we should build a house
            uint32_t future_max_population = match_get_player_max_population(state, bot.player_id);
            uint32_t future_population = match_get_player_population(state, bot.player_id);
            bool has_hall = false;
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                const Entity& entity = state.entities[entity_index];
                if (entity.player_id != bot.player_id || !entity_is_selectable(entity)) {
                    continue;
                }

                if (entity.type == ENTITY_HALL) {
                    has_hall = true;
                }
                if ((entity.type == ENTITY_HALL || entity.type == ENTITY_HOUSE) && entity.mode == MODE_BUILDING_IN_PROGRESS) {
                    future_max_population += 10;
                } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty() && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) {
                    future_population += entity_get_data(entity.queue[0].unit_type).unit_data.population_cost;
                } else if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD && 
                                (entity.target.build.building_type == ENTITY_HOUSE || entity.target.build.building_type == ENTITY_HALL)) {
                    future_max_population += 10;
                }
            }
            bool should_build_houses = has_hall && future_max_population < MATCH_MAX_POPULATION && (int)future_max_population - (int)future_population <= 1;
            if (!should_build_houses) {
                break;
            }

            // Check if we have enough money
            if (bot.effective_gold < entity_get_data(ENTITY_HOUSE).gold_cost) {
                break;
            }

            // Create build house input
            MatchInput build_input = bot_create_build_input(state, bot, ENTITY_HOUSE);
            if (build_input.type != MATCH_INPUT_NONE) {
                bot_reserve_entity(bot, build_input.build.entity_ids[0]);
                task.build_houses.builder_id = build_input.build.entity_ids[0];
                bot.effective_gold -= entity_get_data(ENTITY_HOUSE).gold_cost;
                inputs.push_back(build_input);
            }

            break;
        }
        case BOT_TASK_SCOUT: {
            // Check on scout health
            if (task.scout.scout_id != ID_NULL) {
                uint32_t scout_index = state.entities.get_index_of(task.scout.scout_id);
                if (scout_index == INDEX_INVALID || state.entities[scout_index].health == 0) {
                    bot_release_entity(bot, task.scout.scout_id);
                    task.scout.scout_id = ID_NULL;
                }
            }

            if (task.scout.scout_id == ID_NULL) {
                // Find scout
                EntityId bot_scout_id = ID_NULL;
                for (uint32_t scout_index = 0; scout_index < state.entities.size(); scout_index++) {
                    const Entity& scout = state.entities[scout_index];
                    EntityId scout_id = state.entities.get_id_of(scout_index);
                    if (scout.player_id != bot.player_id || scout.type != ENTITY_WAGON ||
                            !scout.garrisoned_units.empty() || bot_is_entity_reserved(bot, scout_id)) {
                        continue;
                    }
                    bot_scout_id = scout_id;
                    break;
                }

                if (bot_scout_id == ID_NULL) {
                    break;
                }
                bot_reserve_entity(bot, bot_scout_id);
                task.scout.scout_id = bot_scout_id;
            }

            // Now that we have a scout, use it to actually scout
            const Entity& scout = state.entities.get_by_id(task.scout.scout_id);
            // Check to make sure the scout is idle before sending it somewhere
            // If it's not idle, that should mean that we told it to scout something,
            // so we should let it finish its job before giving it a new place to scout
            if (scout.mode != MODE_UNIT_IDLE || scout.target.type != TARGET_NONE) {
                break;
            }
            EntityId goldmine_to_scout_id = ID_NULL;
            for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
                const Entity& goldmine = state.entities[goldmine_index];
                if (goldmine.type != ENTITY_GOLDMINE) {
                    continue;
                }
                if (!match_is_cell_rect_explored(state, state.players[bot.player_id].team, goldmine.cell, entity_get_data(goldmine.type).cell_size)) {
                    goldmine_to_scout_id = state.entities.get_id_of(goldmine_index);
                    break;
                }
            }
            if (goldmine_to_scout_id != ID_NULL) {
                MatchInput scout_input;
                scout_input.type = MATCH_INPUT_MOVE_ENTITY;
                scout_input.move.shift_command = 0;
                scout_input.move.target_cell = ivec2(0, 0);
                scout_input.move.target_id = goldmine_to_scout_id;
                scout_input.move.entity_count = 1;
                scout_input.move.entity_ids[0] = task.scout.scout_id;
                inputs.push_back(scout_input);
                break;
            }

            // If we got here, it means the scout was free for us to scout, but
            // there was nothing to scout, so the task is finished
            bot_release_entity(bot, task.scout.scout_id);
            task.scout.scout_id = ID_NULL;
            task.mode = BOT_TASK_FINISHED;

            bot_add_task(bot, BOT_TASK_BUILD);

            break;
        }
        case BOT_TASK_BUILD: {
            // Check all of our reserved builders to see if we should release one
            uint32_t reserved_builders_index = 0;
            while (reserved_builders_index < task.build.reserved_builders_count) {
                EntityId builder_id = task.build.reserved_builders[reserved_builders_index];
                uint32_t entity_index = state.entities.get_index_of(builder_id);
                if (entity_index == INDEX_INVALID || state.entities[entity_index].health == 0 ||
                        state.entities[entity_index].target.type != TARGET_BUILD) {
                    // Release
                    bot_release_entity(bot, builder_id);
                    // Swap and pop
                    task.build.reserved_builders[reserved_builders_index] = task.build.reserved_builders[task.build.reserved_builders_count - 1];
                    task.build.reserved_builders_count--;
                } else {
                    reserved_builders_index++;
                }
            }

            // Count all entities, not in progress to see if we have anything left to build
            uint32_t entity_counts[ENTITY_TYPE_COUNT];
            bot_count_entities(state, bot, false, entity_counts);

            // Subtract entity counts against buildings desired to see what we still need to make
            uint32_t desired_buildings[ENTITY_TYPE_COUNT];
            memcpy(desired_buildings, task.build.desired_buildings, sizeof(desired_buildings));
            for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
                if (entity_counts[entity_type] > desired_buildings[entity_type]) {
                    desired_buildings[entity_type] = 0;
                } else {
                    desired_buildings[entity_type] -= entity_counts[entity_type];
                }
            }

            // Choose the first building desired
            EntityType desired_building_type = ENTITY_TYPE_COUNT;
            for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
                if (desired_buildings[entity_type] > 0) {
                    desired_building_type = (EntityType)entity_type;
                    break;
                }
            }

            // If no buildings are desired, then this task is finished
            if (desired_building_type == ENTITY_TYPE_COUNT) {
                for (uint32_t builders_index = 0; builders_index < task.build.reserved_builders_count; builders_index++) {
                    EntityId builder_id = task.build.reserved_builders[builders_index];
                    log_warn("BOT: Task BUILD_BUILDINGS is finished, but we still have a reserved entity with ID %u", builder_id);
                    bot_release_entity(bot, builder_id);
                }
                task.mode = BOT_TASK_FINISHED;
                break;
            }

            // Otherwise, we still have buildings to build, so re-count all entities
            // including in-progress this time, to see if we need to make anything
            bot_count_entities(state, bot, true, entity_counts);

            // Subtract entity counts against buildings desired to see what we still need to make
            memcpy(desired_buildings, task.build.desired_buildings, sizeof(desired_buildings));
            for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
                if (entity_counts[entity_type] > desired_buildings[entity_type]) {
                    desired_buildings[entity_type] = 0;
                } else {
                    desired_buildings[entity_type] -= entity_counts[entity_type];
                }
            }

            // Choose the first building desired
            desired_building_type = ENTITY_TYPE_COUNT;
            for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
                if (desired_buildings[entity_type] > 0) {
                    desired_building_type = (EntityType)entity_type;
                    break;
                }
            }

            // If no building is desired at this point, then it means that the task is not
            // complete because we still have an in-progress building, but there's nothing 
            // left for us to do because the building is already being made, so just break
            if (desired_building_type == ENTITY_TYPE_COUNT) {
                break;
            }

            // Otherwise, we have a desired_building_type so let's try making a building

            // First let's check if we can afford this building
            if (bot.effective_gold < entity_get_data(desired_building_type).gold_cost) {
                break;
            }

            MatchInput build_input = bot_create_build_input(state, bot, desired_building_type);
            if (build_input.type != MATCH_INPUT_NONE) {
                bot.effective_gold -= entity_get_data(desired_building_type).gold_cost;
                bot_reserve_entity(bot, build_input.build.entity_ids[0]);
                task.build.reserved_builders[task.build.reserved_builders_count] = build_input.build.entity_ids[0];
                task.build.reserved_builders_count++;
                inputs.push_back(build_input);
            }

            break;
        }
    }
}

uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id) {
    // First find all the town halls for this bot
    std::vector<uint32_t> hall_indices;
    std::vector<uint32_t> buildings_near_hall;
    for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
        const Entity& hall = state.entities[hall_index];
        if (hall.player_id != bot_player_id || hall.type != ENTITY_HALL || hall.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        hall_indices.push_back(hall_index);
        buildings_near_hall.push_back(0);
    }

    // Return INDEX_INVALID if the bot has no halls
    if (hall_indices.empty()) {
        return INDEX_INVALID;
    }

    // Then find all the houses for this bot and determine which hall they are closest to
    for (uint32_t building_index = 0; building_index < state.entities.size(); building_index++) {
        const Entity& building = state.entities[building_index];
        if (building.player_id != bot_player_id || !entity_is_building(building.type) || !entity_is_selectable(building)) {
            continue;
        }

        uint32_t nearest_hall_indicies_index = 0;
        for (uint32_t hall_indices_index = 1; hall_indices_index < hall_indices.size(); hall_indices_index++) {
            if (ivec2::manhattan_distance(building.cell, state.entities[hall_indices[hall_indices_index]].cell) <
                    ivec2::manhattan_distance(building.cell, state.entities[hall_indices[nearest_hall_indicies_index]].cell)) {
                nearest_hall_indicies_index = hall_indices_index;
            }
        }

        buildings_near_hall[nearest_hall_indicies_index]++;
    }

    // Finally choose the hall with the least houses
    uint32_t least_buildings_hall_indices_index = 0;
    for (uint32_t hall_indices_index = 1; hall_indices_index < hall_indices.size(); hall_indices_index++) {
        if (buildings_near_hall[hall_indices_index] < buildings_near_hall[least_buildings_hall_indices_index]) {
            least_buildings_hall_indices_index = hall_indices_index;
        }
    }

    return hall_indices[least_buildings_hall_indices_index];
}

ivec2 bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size) {
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

EntityId bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id) {
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

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell) {
    uint32_t idle_worker_index = INDEX_INVALID;

    for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
        const Entity& miner = state.entities[miner_index];
        EntityId miner_id = state.entities.get_id_of(miner_index);
        // We don't have to check target queue because the bots don't use them
        if (miner.type != ENTITY_MINER || 
                miner.player_id != bot.player_id || 
                !entity_is_selectable(miner) || 
                miner.mode != MODE_UNIT_IDLE ||
                miner.target.type != TARGET_NONE ||
                bot_is_entity_reserved(bot, miner_id)) {
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

EntityId bot_find_builder(const MatchState& state, const Bot& bot, uint32_t near_hall_index) {
    GOLD_ASSERT(near_hall_index != INDEX_INVALID);
    const Entity& hall = state.entities[near_hall_index];

    // First try an idle worker
    EntityId builder = bot_find_nearest_idle_worker(state, bot, hall.cell);
    if (builder != ID_NULL) {
        return builder;
    }

    Target mine_target = match_entity_target_nearest_gold_mine(state, hall);
    if (mine_target.type == TARGET_NONE) {
        return ID_NULL;
    }

    // Then try a worker from the gold mine
    return bot_pull_worker_off_gold(state, bot.player_id, mine_target.id);
}

MatchInput bot_create_build_input(const MatchState& state, const Bot& bot, EntityType building_type) {
    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id);
    if (hall_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId builder_id = bot_find_builder(state, bot, hall_index);
    if (builder_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 building_location = bot_find_building_location(state, bot.player_id, state.entities.get_by_id(builder_id).cell, entity_get_data(building_type).cell_size);
    if (building_location.x == -1) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILD;
    input.build.shift_command = 0;
    input.build.target_cell = building_location;
    input.build.building_type = building_type;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = builder_id;

    return input;
}

void bot_count_entities(const MatchState& state, const Bot& bot, bool include_in_progress_entities, uint32_t* entity_counts) {
    memset(entity_counts, 0, sizeof(uint32_t) * ENTITY_TYPE_COUNT);
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot.player_id || !entity_is_selectable(entity)) {
            continue;
        }

        entity_counts[entity.type]++;
        if (include_in_progress_entities && entity.type == ENTITY_MINER && 
                entity.mode != MODE_UNIT_BUILD && entity.target.type == TARGET_BUILD) {
            entity_counts[entity.target.build.building_type]++;
        }
        if (include_in_progress_entities && entity_is_building(entity.type) &&
                !entity.queue.empty() && entity.queue.front().type == BUILDING_QUEUE_ITEM_UNIT) {
            entity_counts[entity.queue.front().unit_type]++;
        }
    }
}
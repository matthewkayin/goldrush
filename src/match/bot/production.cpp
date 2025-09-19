#include "production.h"

#include "util.h"
#include "../upgrade.h"
#include "core/logger.h"

MatchInput bot_get_production_input(const MatchState& state, Bot& bot) {
    MatchInput saturate_bases_input = bot_saturate_bases(state, bot);
    if (saturate_bases_input.type != MATCH_INPUT_NONE) {
        return saturate_bases_input;
    }

    if (bot_should_build_house(state, bot)) {
        return bot_build_building(state, bot, ENTITY_HOUSE);
    }

    uint32_t desired_upgrade = bot_get_desired_upgrade(state, bot);
    if (desired_upgrade != 0) {
        EntityType prereq_type = bot_get_building_which_researches(desired_upgrade);
        EntityId prereq_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&bot, &prereq_type](const Entity& building, EntityId building_id) {
                return building.type == prereq_type && building.player_id == bot.player_id && entity_is_selectable(building);
            }
        });
        if (prereq_id == ID_NULL) {
            return bot_build_building(state, bot, prereq_type);
        }
        return bot_research_upgrade(state, bot, desired_upgrade);
    }

    uint32_t entity_count[ENTITY_TYPE_COUNT];
    bot_count_entities(state, bot, entity_count, BOT_INCLUDE_IN_PROGRESS);

    EntityType desired_building_type = ENTITY_TYPE_COUNT;
    for (uint32_t building_type = ENTITY_HALL; building_type < ENTITY_TYPE_COUNT; building_type++) {
        if (bot.goal.desired_entities[building_type] > entity_count[building_type]) {
            desired_building_type = (EntityType)building_type;
        }
    }
    if (desired_building_type != ENTITY_TYPE_COUNT) {
        return bot_build_building(state, bot, desired_building_type);
    }

    EntityType desired_unit_type = ENTITY_TYPE_COUNT;
    for (uint32_t unit_type = ENTITY_MINER; unit_type < ENTITY_HALL; unit_type++) {
        if (bot.goal.desired_entities[unit_type] > entity_count[unit_type]) {
            desired_unit_type = (EntityType)unit_type;
        }
    }
    if (desired_unit_type != ENTITY_TYPE_COUNT) {
        return bot_train_unit(state, bot, desired_unit_type);
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

MatchInput bot_saturate_bases(const MatchState& state, Bot& bot) {
    for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
        const Entity& goldmine = state.entities[goldmine_index];
        EntityId goldmine_id = state.entities.get_id_of(goldmine_index);
        if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
            continue;
        }

        EntityId hall_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&goldmine, &bot](const Entity& hall, EntityId hall_id) {
                return hall.player_id == bot.player_id &&
                        hall.type == ENTITY_HALL &&
                        hall.mode == MODE_BUILDING_FINISHED &&
                        bot_does_entity_surround_goldmine(hall, goldmine.cell);
            }
        });

        // If there is no hall, tell any miners to stop
        // This handles the case that the town hall is destroyed
        if (hall_id == ID_NULL) {
            MatchInput input;
            input.type = MATCH_INPUT_STOP;
            input.stop.entity_count = 0;

            for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
                const Entity& miner = state.entities[miner_index];
                EntityId miner_id = state.entities.get_id_of(miner_index);
                if (miner_id == bot.scout_id) {
                    continue;
                }
                if (miner.player_id == bot.player_id && miner.goldmine_id == goldmine_id) {
                    input.stop.entity_ids[input.stop.entity_count] = state.entities.get_id_of(miner_index);
                    input.stop.entity_count++;
                }
            }

            if (input.stop.entity_count != 0) {
                GOLD_ASSERT(input.stop.entity_count <= SELECTION_LIMIT);
                return input;
            }

            continue;
        }

        const Entity& hall = state.entities.get_by_id(hall_id);

        // Check the miner count
        uint32_t miner_count = match_get_miners_on_gold(state, goldmine_id, bot.player_id);

        // If oversaturated, pull workers off gold
        if (miner_count > MATCH_MAX_MINERS_ON_GOLD) {
            EntityId miner_id = bot_pull_worker_off_gold(state, bot, goldmine_id);
            if (miner_id != ID_NULL) {
                ivec2 goldmine_cell = state.entities.get_by_id(goldmine_id).cell;

                MatchInput input;
                input.type = MATCH_INPUT_MOVE_CELL;
                input.move.shift_command = 0;
                input.move.target_cell = map_clamp_cell(state.map, hall.cell + ((hall.cell - goldmine_cell) * -1));
                input.move.target_id = ID_NULL;
                input.move.entity_count = 1;
                input.move.entity_ids[0] = miner_id;
                return input;
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
                return input;
            }

            // If we're still here, then there were no idle workers
            // So we'll create one out of the town hall
            if (bot_get_effective_gold(state, bot) >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
                MatchInput input;
                input.type = MATCH_INPUT_BUILDING_ENQUEUE;
                input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
                input.building_enqueue.item_subtype = (uint32_t)ENTITY_MINER;
                input.building_enqueue.building_count = 1;
                input.building_enqueue.building_ids[0] = hall_id;
                return input;
            }
        }

        // If saturated, cancel any in-progress miners
        if (miner_count == MATCH_MAX_MINERS_ON_GOLD && !hall.queue.empty()) {
            MatchInput input;
            input.type = MATCH_INPUT_BUILDING_DEQUEUE;
            input.building_dequeue.building_id = hall_id;
            input.building_dequeue.index = BUILDING_DEQUEUE_POP_FRONT;
            return input;
        }
    } // End for each goldmine

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

bool bot_should_build_house(const MatchState& state, const Bot& bot) {
    uint32_t future_max_population = match_get_player_max_population(state, bot.player_id);
    uint32_t future_population = match_get_player_population(state, bot.player_id);
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot.player_id || !entity_is_selectable(entity)) {
            continue;
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

    return future_max_population < MATCH_MAX_POPULATION && (int)future_max_population - (int)future_population <= 1;
}

MatchInput bot_build_building(const MatchState& state, Bot& bot, EntityType building_type) {
    GOLD_ASSERT(building_type != ENTITY_TYPE_COUNT && entity_is_building(building_type));

    // Check pre-req
    EntityType prereq_type = bot_get_building_prereq(building_type);
    if (prereq_type != ENTITY_TYPE_COUNT) {
        EntityId prereq_id = bot_find_entity((BotFindEntityParams) {
            .state = state, 
            .filter = [&bot, &prereq_type](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id && entity.mode == MODE_BUILDING_FINISHED && entity.type == prereq_type;
            }
        });
        if (prereq_id == ID_NULL) {
            EntityId prereq_in_progress = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&bot, &prereq_type](const Entity& entity, EntityId entity_id) {
                    return entity.player_id == bot.player_id && 
                            ((entity.mode == MODE_BUILDING_IN_PROGRESS && entity.type == prereq_type) ||
                            (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD && entity.target.build.building_type == prereq_type));
                }
            });
            if (prereq_in_progress == ID_NULL) {
                return bot_build_building(state, bot, prereq_type);
            }
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

    // Check gold cost
    if (bot_get_effective_gold(state, bot) < entity_get_data(building_type).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id, building_type == ENTITY_BUNKER);
    if (hall_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId builder_id = bot_find_builder(state, bot, hall_index);
    if (builder_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 building_location; 
    if (building_type == ENTITY_HALL) {
        building_location = bot_find_hall_location(state, bot, hall_index);
    } else if (building_type == ENTITY_BUNKER) {
        building_location = bot_find_bunker_location(state, bot, hall_index);
    } else {
        building_location = bot_find_building_location(state, bot.player_id, state.entities[hall_index].cell + ivec2(1, 1), entity_get_data(building_type).cell_size);
    }
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

MatchInput bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type) {
    GOLD_ASSERT(unit_type != ENTITY_TYPE_COUNT && entity_is_unit(unit_type));

    // Find building to train unit
    EntityType building_type = bot_get_building_which_trains(unit_type);
    EntityId building_id = bot_find_entity((BotFindEntityParams) {
        .state = state, 
        .filter = [&bot, &building_type](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot.player_id && 
                    entity.type == building_type &&
                    entity.mode == MODE_BUILDING_FINISHED &&
                    entity.queue.empty();
        }
    });
    if (building_id == ID_NULL) {
        return bot_build_building(state, bot, building_type);
    }

    // Check gold
    if (bot_get_effective_gold(state, bot) < entity_get_data(unit_type).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = (uint32_t)unit_type;
    input.building_enqueue.building_count = 1;
    input.building_enqueue.building_ids[0] = building_id;
    return input;
}

uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot) {
    bool has_landmines_squad = false;
    for (const BotSquad& squad : bot.squads) {
        if (squad.type == BOT_SQUAD_TYPE_LANDMINES) {
            has_landmines_squad = true;
        }
    }
    if (has_landmines_squad && match_player_upgrade_is_available(state, bot.player_id, UPGRADE_LANDMINES)) {
        return UPGRADE_LANDMINES;
    }

    bool has_detective_squad = false;
    for (const BotSquad& squad : bot.squads) {
        for (EntityId entity_id : squad.entities) {
            uint32_t entity_index = state.entities.get_index_of(entity_id);
            if (entity_index != INDEX_INVALID && state.entities[entity_index].type == ENTITY_DETECTIVE) {
                has_detective_squad = true;
                break;
            }
        }
        if (has_detective_squad) {
            break;
        }
    }
    if (has_detective_squad && match_player_upgrade_is_available(state, bot.player_id, UPGRADE_PRIVATE_EYE)) {
        return UPGRADE_PRIVATE_EYE;
    }

    if (BOT_STRATEGY_SALOON_COOP && bot.goal.type == BOT_GOAL_PUSH && match_player_upgrade_is_available(state, bot.player_id, UPGRADE_WAR_WAGON)) {
        return UPGRADE_WAR_WAGON;
    }

    if (BOT_STRATEGY_SALOON_WORKSHOP && bot.goal.type == BOT_GOAL_PUSH && match_player_upgrade_is_available(state, bot.player_id, UPGRADE_FAN_HAMMER)) {
        return UPGRADE_BAYONETS;
    }

    if (bot.strategy == BOT_STRATEGY_BARRACKS && bot.goal.type == BOT_GOAL_PUSH && match_player_upgrade_is_available(state, bot.player_id, UPGRADE_BAYONETS)) {
        return UPGRADE_BAYONETS;
    }

    EntityId balloon_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& entity, EntityId entity_id) {
            return entity.type == ENTITY_BALLOON && entity.player_id == bot.player_id && entity_is_selectable(entity);
        }
    });
    if (balloon_id != ID_NULL && match_player_upgrade_is_available(state, bot.player_id, UPGRADE_TAILWIND)) {
        return UPGRADE_TAILWIND;
    }

    return 0;
}

MatchInput bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade) {
    GOLD_ASSERT(match_player_upgrade_is_available(state, bot.player_id, upgrade));

    EntityType building_type = bot_get_building_which_researches(upgrade);
    EntityId building_id = bot_find_entity((BotFindEntityParams) {
        .state = state, 
        .filter = [&bot, &building_type](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot.player_id && 
                    entity.type == building_type &&
                    entity.mode == MODE_BUILDING_FINISHED &&
                    entity.queue.empty();
        }
    });
    if (building_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Check gold
    if (bot_get_effective_gold(state, bot) < upgrade_get_data(upgrade).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UPGRADE;
    input.building_enqueue.item_subtype = upgrade;
    input.building_enqueue.building_count = 1;
    input.building_enqueue.building_ids[0] = building_id;
    return input;
}

EntityType bot_get_building_which_trains(EntityType unit_type) {
    switch (unit_type) {
        case ENTITY_MINER:
            return ENTITY_HALL;
        case ENTITY_COWBOY:
        case ENTITY_BANDIT:
            return ENTITY_SALOON;
        case ENTITY_SAPPER:
        case ENTITY_PYRO:
        case ENTITY_BALLOON:
            return ENTITY_WORKSHOP;
        case ENTITY_JOCKEY:
        case ENTITY_WAGON:
            return ENTITY_COOP;
        case ENTITY_SOLDIER:
        case ENTITY_CANNON:
            return ENTITY_BARRACKS;
        case ENTITY_DETECTIVE:
            return ENTITY_SHERIFFS;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

EntityType bot_get_building_prereq(EntityType building_type) {
    switch (building_type) {
        case ENTITY_HALL:
            return ENTITY_TYPE_COUNT;
        case ENTITY_SALOON:
        case ENTITY_HOUSE:
            return ENTITY_HALL;
        case ENTITY_BUNKER:
        case ENTITY_WORKSHOP:
        case ENTITY_SMITH:
            return ENTITY_SALOON;
        case ENTITY_COOP:
        case ENTITY_BARRACKS:
        case ENTITY_SHERIFFS:
            return ENTITY_SMITH;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

EntityType bot_get_building_which_researches(uint32_t upgrade) {
    switch (upgrade) {
        case UPGRADE_WAR_WAGON:
        case UPGRADE_BAYONETS:
        case UPGRADE_FAN_HAMMER:
        case UPGRADE_SERRATED_KNIVES:
            return ENTITY_SMITH;
        case UPGRADE_STAKEOUT:
        case UPGRADE_PRIVATE_EYE:
            return ENTITY_SHERIFFS;
        case UPGRADE_LANDMINES:
        case UPGRADE_TAILWIND:
            return ENTITY_WORKSHOP;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell) {
    return bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& entity, EntityId entity_id) {
            return entity.type == ENTITY_MINER &&
                    entity.player_id == bot.player_id &&
                    entity.mode == MODE_UNIT_IDLE &&
                    entity.target.type == TARGET_NONE &&
                    entity_id != bot.scout_id &&
                    !bot_is_entity_reserved(bot, entity_id);
        },
        .compare = bot_closest_manhattan_distance_to(cell)
    });
}

EntityId bot_pull_worker_off_gold(const MatchState& state, const Bot& bot, EntityId goldmine_id) {
    return bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &goldmine_id](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot.player_id &&
                    entity_is_selectable(entity) &&
                    match_is_entity_mining(state, entity) &&
                    entity.goldmine_id == goldmine_id;
        }
    });
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
    return bot_pull_worker_off_gold(state, bot, mine_target.id);
}

uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id, bool count_bunkers_only) {
    // First find all the town halls for this bot
    std::vector<uint32_t> hall_indices;
    std::vector<uint32_t> buildings_near_hall;
    for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
        const Entity& hall = state.entities[hall_index];
        if (hall.player_id != bot_player_id || hall.type != ENTITY_HALL || !entity_is_selectable(hall)) {
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
        if (count_bunkers_only && building.type != ENTITY_BUNKER) {
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

EntityId bot_find_hall_surrounding_goldmine(const MatchState& state, const Bot& bot, const Entity& goldmine) {
    return bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &goldmine](const Entity& hall, EntityId hall_id) {
            return hall.type == ENTITY_HALL &&
                    entity_is_selectable(hall) &&
                    bot_has_scouted_entity(state, bot, hall, hall_id) &&
                    bot_does_entity_surround_goldmine(hall, goldmine.cell);
        }
    });
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

ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot, uint32_t existing_hall_index) {
    // Find the unoccupied goldmine nearest to the existing hall
    uint32_t nearest_goldmine_index = INDEX_INVALID;
    for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
        const Entity& goldmine = state.entities[goldmine_index];
        if (goldmine.type != ENTITY_GOLDMINE) {
            continue;
        }

        EntityId surrounding_hall_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&goldmine](const Entity& hall, EntityId hall_id) {
                return hall.type == ENTITY_HALL &&
                        entity_is_selectable(hall) &&
                        bot_does_entity_surround_goldmine(hall, goldmine.cell);
            }
        });
        if (surrounding_hall_id != ID_NULL) {
            continue;
        }

        if (nearest_goldmine_index == INDEX_INVALID || 
                ivec2::manhattan_distance(state.entities[goldmine_index].cell, state.entities[existing_hall_index].cell) < 
                ivec2::manhattan_distance(state.entities[nearest_goldmine_index].cell, state.entities[existing_hall_index].cell)) {
            nearest_goldmine_index = goldmine_index;
        }
    }

    // If no goldmines are found, we will not build a hall
    if (nearest_goldmine_index == INDEX_INVALID) {
        log_trace("No nearest goldmine");
        return ivec2(-1, -1);
    }

    // To find the hall location, we will search around the perimeter
    // of the building_block_rect for this goldmine and evaluate each 
    // placement, choosing the one with the least obstacles nearby
    Rect building_block_rect = entity_goldmine_get_block_building_rect(state.entities[nearest_goldmine_index].cell);
    Rect goldmine_rect = (Rect) {
        .x = state.entities[nearest_goldmine_index].cell.x,
        .y = state.entities[nearest_goldmine_index].cell.y,
        .w = entity_get_data(ENTITY_GOLDMINE).cell_size,
        .h = entity_get_data(ENTITY_GOLDMINE).cell_size
    };
    const int HALL_SIZE = entity_get_data(ENTITY_HALL).cell_size;
    ivec2 corners[4] = { 
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(-HALL_SIZE, -HALL_SIZE),
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(-HALL_SIZE, building_block_rect.h), 
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(building_block_rect.w, building_block_rect.h), 
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(building_block_rect.w, -HALL_SIZE) 
    };
    ivec2 directions[4] = {
        DIRECTION_IVEC2[DIRECTION_SOUTH],
        DIRECTION_IVEC2[DIRECTION_EAST],
        DIRECTION_IVEC2[DIRECTION_NORTH],
        DIRECTION_IVEC2[DIRECTION_WEST]
    };
    ivec2 best_hall_cell = ivec2(-1, -1);
    int best_hall_score = -1;

    ivec2 hall_cell = corners[0];
    ivec2 hall_step = directions[0];
    while (true) {
        // Evaluate hall cell score
        int hall_score = 0;
        // Add the hall distance to the score so that we prefer less diagonal base placement
        Rect hall_rect = (Rect) {
            .x = hall_cell.x, .y = hall_cell.y,
            .w = HALL_SIZE, .h = HALL_SIZE
        };
        // If the area is blocked (by a cactus, for example) then don't build there
        if (!map_is_cell_rect_in_bounds(state.map, hall_cell, HALL_SIZE) || map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, hall_cell, HALL_SIZE)) {
            hall_score = -1;
        } else {
            // Check for obstacles (including stairs) in a rect around where the hall would be
            // The more obstacles there are, the higher the score
            hall_score = 0;
            Rect hall_surround_rect = (Rect) {
                .x = hall_cell.x - 2,
                .y = hall_cell.y - 2,
                .w = HALL_SIZE + 4,
                .h = HALL_SIZE + 4
            };
            for (int y = hall_surround_rect.y; y < hall_surround_rect.y + hall_surround_rect.h; y++) {
                for (int x = hall_surround_rect.x; x < hall_surround_rect.x + hall_surround_rect.w; x++) {
                    if (!map_is_cell_in_bounds(state.map, ivec2(x, y))) {
                        continue;
                    }
                    if (map_is_tile_ramp(state.map, ivec2(x, y))) {
                        hall_score++;
                    } 
                }
            }

            ivec2 rally_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, state.entities[nearest_goldmine_index].cell + ivec2(1, 1), 1, hall_cell, entity_get_data(ENTITY_HALL).cell_size, true);
            ivec2 mine_exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, state.entities[nearest_goldmine_index].cell, entity_get_data(ENTITY_GOLDMINE).cell_size, 1, rally_cell, MAP_IGNORE_MINERS);

            GOLD_ASSERT(mine_exit_cell.x != -1);
            std::vector<ivec2> path;
            map_pathfind(state.map, CELL_LAYER_GROUND, mine_exit_cell, rally_cell, 1, &path, MAP_IGNORE_MINERS);
            hall_score += path.size();
            hall_score += Rect::euclidean_distance_squared_between(hall_rect, goldmine_rect);
        }

        // Compare hall score to best hall score
        if (hall_score != -1 && (best_hall_score == -1 || hall_score < best_hall_score))  {
            best_hall_cell = hall_cell;
            best_hall_score = hall_score;
        }

        // Increment perimeter search
        hall_cell += hall_step;
        int corner = 0;
        while (corner < 4) {
            if (hall_cell == corners[corner]) {
                break;
            } 
            corner++;
        }

        // Determine whether to end the search or change the search direction
        if (corner == 0) {
            break;
        } else if (corner < 4) {
            hall_step = directions[corner];
        }
    } // End for each cell in perimeter

    // Implicit, we will return ivec2(-1, -1) if there was no good hall cell
    // found in the search above, but the only reason that would happen is 
    // if we had a really bad map gen bug or the player surrounded the
    // goldmine with units 
    return best_hall_cell;
}

ivec2 bot_find_bunker_location(const MatchState& state, const Bot& bot, uint32_t nearby_hall_index) {
    const Entity& nearby_hall = state.entities[nearby_hall_index];
    ivec2 nearby_hall_cell = nearby_hall.cell;
    EntityId nearest_enemy_building_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&state, &bot](const Entity& building, EntityId building_id) {
            return entity_is_building(building.type) &&
                    entity_is_selectable(building) &&
                    state.players[building.player_id].team != state.players[bot.player_id].team;
        },
        .compare = [&nearby_hall_cell](const Entity& a, const Entity& b) {
            if (a.type == ENTITY_HALL && b.type != ENTITY_HALL) {
                return true;
            }
            if (a.type != ENTITY_HALL && b.type == ENTITY_HALL) {
                return false;
            }
            return ivec2::manhattan_distance(a.cell, nearby_hall_cell) < ivec2::manhattan_distance(b.cell, nearby_hall_cell);
        }
    });

    // If there are no enemy buildings, then it doesn't matter where we put the bunker
    if (nearest_enemy_building_id == ID_NULL) {
        return bot_find_building_location(state, bot.player_id, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }

    const Entity& nearest_enemy_building = state.entities.get_by_id(nearest_enemy_building_id);
    std::vector<ivec2> path;
    ivec2 path_start_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, nearby_hall_cell, entity_get_data(ENTITY_HALL).cell_size, 1, nearest_enemy_building.cell, MAP_IGNORE_UNITS | MAP_IGNORE_MINERS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_building.cell, entity_get_data(nearest_enemy_building.type).cell_size, MAP_IGNORE_UNITS | MAP_IGNORE_MINERS);
    if (path_start_cell.x == -1 || path_end_cell.x == -1) {
        return bot_find_building_location(state, bot.player_id, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }
    map_pathfind(state.map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, &path, MAP_IGNORE_UNITS | MAP_IGNORE_MINERS);

    ivec2 search_start_cell = path_start_cell;
    int path_index = 0;
    while (path_index < path.size() &&
            (ivec2::manhattan_distance(path[path_index], path_start_cell) < 17 &&
            map_get_tile(state.map, path[path_index]).elevation == 
            map_get_tile(state.map, path_start_cell).elevation)) {
        path_index++;
    }
    if (path_index > 1 && path_index < path.size() && path_index < path.size()) {
        search_start_cell = path[path_index - 1];
    }

    return bot_find_building_location(state, bot.player_id, search_start_cell, entity_get_data(ENTITY_BUNKER).cell_size);
}

uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot) {
    uint32_t effective_gold = state.players[bot.player_id].gold;

    for (const Entity& entity : state.entities) {
        if (entity.player_id == bot.player_id &&
                entity.type == ENTITY_MINER &&
                entity.mode != MODE_UNIT_BUILD &&
                entity.target.type == TARGET_BUILD) {
            uint32_t gold_cost = entity_get_data(entity.target.build.building_type).gold_cost;
            effective_gold = effective_gold > gold_cost ? effective_gold - gold_cost : 0;
        }
    }

    return effective_gold;
}

bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell) {
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    Rect entity_rect = (Rect) {
        .x = entity.cell.x, .y = entity.cell.y,
        .w = entity_cell_size, .h = entity_cell_size
    };

    Rect goldmine_rect = entity_goldmine_get_block_building_rect(goldmine_cell);
    goldmine_rect.x -= 1;
    goldmine_rect.y -= 1;
    goldmine_rect.w += 2;
    goldmine_rect.h += 2;
    
    return entity_rect.intersects(goldmine_rect);
}
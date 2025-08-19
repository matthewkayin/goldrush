#include "bot.h"

#include "core/logger.h"
#include "upgrade.h"

static const int BOT_SQUAD_GATHER_DISTANCE = 16;

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed) {
    Bot bot;
    bot.player_id = player_id;

    // The bot takes a copy of the random seed, that way
    // it can make deterministic random decisions that will be
    // synced across each player's computer, but since the 
    // bot code will not be rerun during a replay, the bot will
    // not mess up replay playback
    bot.lcg_seed = lcg_seed;

    bot.strategy = BOT_STRATEGY_SALOON_CORE;
    bot_clear_goal(bot);
    bot.desired_upgrades = 0;

    bot.scout_id = ID_NULL;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.type != ENTITY_GOLDMINE) {
            continue;
        }

        // Leaving the occupying player id uninitialized because it will be
        // picked up the next time we call bot_scout_update
        BotScoutInfo scout_info;
        scout_info.goldmine_id = state.entities.get_id_of(entity_index);
        scout_info.occupying_player_id = PLAYER_NONE;
        scout_info.should_scout = true;

        if (match_is_entity_visible_to_player(state, entity, bot.player_id)) {
            EntityId surrounding_hall_id = bot_find_hall_surrounding_goldmine(state, bot, entity);
            if (surrounding_hall_id != ID_NULL) {
                scout_info.occupying_player_id = state.entities.get_by_id(surrounding_hall_id).player_id;
            } 
        }

        bot.scout_info.push_back(scout_info);
    }
    bot.last_scout_time = 0;

    return bot;
}

MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    bot_scout_update(state, bot);

    // Strategy update
    if (bot_goal_should_be_abandoned(state, bot)) {
        bot_clear_goal(bot);
    }
    if (bot_is_goal_met(state, bot)) {
        bot_on_goal_finished(state, bot);
        bot_clear_goal(bot);
    }
    if (bot.goal.type == BOT_GOAL_NONE) {
        bot_choose_next_goal(state, bot, match_time_minutes);
    }

    MatchInput saturate_bases_input = bot_saturate_bases(state, bot);
    if (saturate_bases_input.type != MATCH_INPUT_NONE) {
        return saturate_bases_input;
    }

    // Get desired entities
    BotDesiredEntities desired_entities = bot_get_desired_entities(state, bot, match_time_minutes);

    // Train unit
    if (desired_entities.unit != ENTITY_TYPE_COUNT) {
        MatchInput input = bot_train_unit(state, bot, desired_entities.unit);
        if (input.type != MATCH_INPUT_NONE) {
            return input;
        }
    }
    
    // Build building
    if (desired_entities.building != ENTITY_TYPE_COUNT) {
        MatchInput input = bot_build_building(state, bot, desired_entities.building);
        if (input.type != MATCH_INPUT_NONE) {
            return input;
        }
    }

    // Research upgrade
    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        uint32_t upgrade = 1 << upgrade_index;
        bool bot_desires_upgrade = (bot.desired_upgrades & upgrade) == upgrade;
        if (bot_desires_upgrade && match_player_upgrade_is_available(state, bot.player_id, upgrade)) {
            MatchInput input = bot_research_upgrade(state, bot, upgrade);
            if (input.type != MATCH_INPUT_NONE) {
                return input;
            }
        }
    }

    // Squad update
    uint32_t squad_index = 0;
    while (squad_index < bot.squads.size()) {
        MatchInput input = bot_squad_update(state, bot, bot.squads[squad_index]);

        if (bot.squads[squad_index].entities.empty()) {
            bot.squads[squad_index] = bot.squads[bot.squads.size() - 1];
            bot.squads.pop_back();
        } else {
            squad_index++;
        }

        if (input.type != MATCH_INPUT_NONE) {
            return input;
        }
    }

    MatchInput scout_input = bot_scout(state, bot, match_time_minutes);
    if (scout_input.type != MATCH_INPUT_NONE) {
        return scout_input;
    }

    MatchInput rally_input = bot_set_rally_points(state, bot);
    if (rally_input.type != MATCH_INPUT_NONE) {
        return rally_input;
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

// Strategy

void bot_clear_goal(Bot& bot) {
    bot.goal.type = BOT_GOAL_NONE;
    memset(bot.goal.desired_entities, 0, sizeof(bot.goal.desired_entities));
}

void bot_choose_next_goal(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    bot_clear_goal(bot);

    if (match_time_minutes == 0) {
        bot.goal.type = BOT_GOAL_HARASS;
        bot.goal.squad_type = BOT_SQUAD_TYPE_HARASS;
        bot.goal.desired_entities[ENTITY_PYRO] = 2;
        bot.goal.desired_entities[ENTITY_COWBOY] = 4;
        return;
    }

    bot.goal.type = BOT_GOAL_HARASS;
    bot.goal.squad_type = BOT_SQUAD_TYPE_HARASS;
    bot.goal.desired_entities[ENTITY_COWBOY] = 4;
    bot.goal.desired_entities[ENTITY_BANDIT] = 4;
}

bool bot_goal_should_be_abandoned(const MatchState& state, const Bot& bot) {
    if (bot.goal.type == BOT_GOAL_NONE) {
        return false;
    }

    if (bot.goal.type == BOT_GOAL_RUSH &&
            bot_is_base_under_attack(state, bot)) {
        return true;
    }

    if (bot.goal.type == BOT_GOAL_RUSH) {
        EntityId wagon_id = bot_find_entity((BotFindEntityParams) {
            .state = state, 
            .filter = [&bot](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id && 
                            entity.type == ENTITY_WAGON && 
                            entity_is_selectable(entity);
            }
        });
        if (wagon_id == ID_NULL) {
            return true;
        }
    }

    return false;
}

bool bot_is_goal_met(const MatchState& state, const Bot& bot) {
    if (bot.goal.type == BOT_GOAL_NONE) {
        return false;
    }

    // TODO: make sure that full pop cap doesn't block goal from being
    // met, but I feel like the best way to do this is to just make it
    // so that we don't request armies that are bigger than mox pop

    uint32_t entity_count[ENTITY_TYPE_COUNT];
    memset(entity_count, 0, sizeof(entity_count));

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.player_id != bot.player_id || !entity_is_selectable(entity) || bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        entity_count[entity.type]++;
    }

    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (entity_count[entity_type] < bot.goal.desired_entities[entity_type]) {
            return false;
        }
    }

    return true;
}

void bot_on_goal_finished(const MatchState& state, Bot& bot) {
    if (bot.goal.squad_type == BOT_SQUAD_TYPE_NONE) {
        return;
    }

    BotSquad squad;
    uint32_t squad_entity_count[ENTITY_TYPE_COUNT];
    memset(squad_entity_count, 0, sizeof(squad_entity_count));
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        // Filter to only our units
        if (entity.player_id != bot.player_id ||
                !entity_is_selectable(entity) ||
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        // Filter out entities that we no longer want
        if (squad_entity_count[entity.type] >= bot.goal.desired_entities[entity.type]) {
            continue;
        }

        if (entity_id == bot.scout_id) {
            bot.scout_id = ID_NULL;
        }

        squad.entities.push_back(entity_id);
        squad_entity_count[entity.type]++;
    }

    if (squad.entities.empty()) {
        log_trace("BOT: no entities in squad, abandoning...");
        return;
    }

    squad.type = bot.goal.squad_type;
    squad.target_cell = ivec2(-1, -1);
    if (squad.type == BOT_SQUAD_TYPE_HARASS || squad.type == BOT_SQUAD_TYPE_PUSH) {
        squad.target_cell = bot_get_squad_attack_point(state, bot, squad);

        if (squad.target_cell.x == -1) {
            log_trace("BOT: no attack point found. Abandoning squad.");
            // If we got here it means that there were no enemy buildings
            // to attack, so we set the last scout time to 0. This makes
            // it so that we will scout again immediately and discover
            // any hidden enemy buildings that the bot cannot yet see
            bot.last_scout_time = 0;
            return;
        }
    } else if (squad.type == BOT_SQUAD_TYPE_BUNKER) {
        for (EntityId entity_id : squad.entities) {
            const Entity& entity = state.entities.get_by_id(entity_id);
            if (entity.type == ENTITY_BUNKER) {
                squad.target_cell = entity.cell;
                break;
            }
        }
    }
    GOLD_ASSERT(squad.target_cell.x != -1);

    // This is done here in case we abandon squad creation
    log_trace("BOT: -- creating squad with type %u --", squad.type);
    for (EntityId entity_id : squad.entities) {
        bot_reserve_entity(bot, entity_id);
    }
    log_trace("BOT: -- end squad --");
    bot.squads.push_back(squad);
}

// Behaviors

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
                        entity_is_selectable(hall) &&
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
                return (MatchInput) {
                    .type = MATCH_INPUT_BUILDING_ENQUEUE,
                    .building_enqueue = (MatchInputBuildingEnqueue) {
                        .building_id = hall_id,
                        .item_type = BUILDING_QUEUE_ITEM_UNIT,
                        .item_subtype = ENTITY_MINER
                    }
                };
            }
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

BotDesiredEntities bot_get_desired_entities(const MatchState& state, const Bot& bot, uint32_t match_time_minutes) {
    if (bot_should_build_house(state, bot)) {
        return (BotDesiredEntities) {
            .unit = ENTITY_TYPE_COUNT,
            .building = ENTITY_HOUSE,
        };
    }

    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    memcpy(desired_entities, bot.goal.desired_entities, sizeof(desired_entities));

    // Subtract from desired units any unit which we already have or which is in-progress
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.player_id != bot.player_id ||
                !entity_is_selectable(entity) ||
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        if (entity.mode == MODE_BUILDING_FINISHED && 
                !entity.queue.empty() && 
                entity.queue.front().type == BUILDING_QUEUE_ITEM_UNIT &&
                desired_entities[entity.queue.front().unit_type] > 0) {
            desired_entities[entity.queue.front().unit_type]--;
        } else if (entity_is_unit(entity.type) && desired_entities[entity.type] > 0) {
            desired_entities[entity.type]--;
        }
    }
    // Determine desired buildings based on desired army
    uint32_t desired_units_from_building[ENTITY_TYPE_COUNT];
    memset(desired_units_from_building, 0, sizeof(desired_units_from_building));
    for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
        if (desired_entities[entity_type] == 0) {
            continue;
        }
        EntityType building_type = bot_get_building_which_trains((EntityType)entity_type);
        desired_units_from_building[building_type] += desired_entities[entity_type];
    }
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (desired_units_from_building[entity_type] == 0) {
            continue;
        }

        uint32_t desired_building_count;
        if (desired_units_from_building[entity_type] < 4) {
            desired_building_count = 1;
        } else if (desired_units_from_building[entity_type] < 16) {
            desired_building_count = 2;
        } else if (desired_units_from_building[entity_type] < 32) {
            desired_building_count = 3;
        } else if (desired_units_from_building[entity_type] < 48) {
            desired_building_count = 4;
        } else if (desired_units_from_building[entity_type] < 64) {
            desired_building_count = 5;
        } else {
            desired_building_count = 6;
        }
        desired_entities[entity_type] = std::max(desired_entities[entity_type], desired_building_count);
    }

    // Determine any other desired buildings based on building pre-reqs
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (desired_entities[entity_type] == 0) {
            continue;
        }
        EntityType prereq = bot_get_building_prereq((EntityType)entity_type);
        while (prereq != ENTITY_TYPE_COUNT && desired_entities[prereq] == 0) {
            desired_entities[prereq] = 1;
            prereq = bot_get_building_prereq((EntityType)prereq);
        }
    }

    // Subtract from desired buildings based on buildings we already have
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.player_id != bot.player_id || !entity_is_selectable(entity)) {
            continue;
        }

        if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD) {
            if (desired_entities[entity.target.build.building_type] > 0) {
                desired_entities[entity.target.build.building_type]--;
            }
        } else if (entity_is_building(entity.type) && entity_is_selectable(entity) && !bot_is_entity_reserved(bot, entity_id)) {
            if (desired_entities[entity.type] > 0) {
                desired_entities[entity.type]--;
            }
        }
    }

    // If we want to scout but haven't grabbed a scout yet, 
    // it means we there is probably nothing to scout with,
    // so add a scout to our desired entities
    if (match_time_minutes > bot_get_next_scout_time(bot) &&
            match_time_minutes - bot_get_next_scout_time(bot) > 1 &&
            bot.scout_id == ID_NULL) {
        EntityId scout_train_building_id = bot_find_best_entity((BotFindBestEntityParams) {
            .state = state,
            .filter = [&bot](const Entity& building, EntityId building_id) {
                return building.player_id == bot.player_id &&
                        (building.type == ENTITY_COOP || building.type == ENTITY_SALOON) &&
                        entity_is_selectable(building);
            },
            .compare = [](const Entity& a, const Entity& b) {
                return a.type == ENTITY_COOP && b.type != ENTITY_COOP;
            }
        });
        if (scout_train_building_id != ID_NULL) {
            EntityType building_type = state.entities.get_by_id(scout_train_building_id).type;
            if (building_type == ENTITY_COOP) {
                desired_entities[ENTITY_WAGON]++;
            } else if (building_type == ENTITY_SALOON) {
                desired_entities[ENTITY_BANDIT]++;
            }
        }
    }

    // Determine desired unit
    uint32_t desired_unit;
    for (desired_unit = ENTITY_MINER; desired_unit < ENTITY_HALL; desired_unit++) {
        if (desired_entities[desired_unit] > 0) {
            break;
        }
    }
    if (desired_unit == ENTITY_HALL) {
        desired_unit = ENTITY_TYPE_COUNT;
    }

    // Determine desired building
    uint32_t desired_building;
    for (desired_building = ENTITY_HALL; desired_building < ENTITY_TYPE_COUNT; desired_building++) {
        if (desired_entities[desired_building] > 0) {
            break;
        }
    }

    return (BotDesiredEntities) {
        .unit = (EntityType)desired_unit,
        .building = (EntityType)desired_building
    };
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
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

    // Check gold cost
    if (bot_get_effective_gold(state, bot) < entity_get_data(building_type).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id);
    if (hall_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId builder_id = bot_find_builder(state, bot, hall_index);
    if (builder_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 building_location = building_type == ENTITY_HALL ? bot_find_hall_location(state, bot, hall_index) : bot_find_building_location(state, bot.player_id, state.entities[hall_index].cell + ivec2(1, 1), entity_get_data(building_type).cell_size);
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
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Check gold
    if (bot_get_effective_gold(state, bot) < entity_get_data(unit_type).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = unit_type;
    input.building_enqueue.building_id = building_id;
    return input;
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
    input.building_enqueue.building_id = building_id;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UPGRADE;
    input.building_enqueue.item_subtype = upgrade;
    return input;
}

MatchInput bot_set_rally_points(const MatchState& state, const Bot& bot) {
    for (uint32_t building_index = 0; building_index < state.entities.size(); building_index++) {
        const Entity& building = state.entities[building_index];
        EntityId building_id = state.entities.get_id_of(building_index);

        // Filter down to allied buildings which are training a unit
        if (building.player_id != bot.player_id ||
                !entity_is_selectable(building) ||
                building.queue.empty() || building.queue.front().type != BUILDING_QUEUE_ITEM_UNIT) {
            continue;
        }

        // Halls only need their rally point set once
        if (building.type == ENTITY_HALL && building.rally_point.x != -1) {
            continue;
        }

        // Set hall rally point
        if (building.type == ENTITY_HALL) {
            EntityId goldmine_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state,
                .filter = [](const Entity& goldmine, EntityId goldmine_id) {
                    return goldmine.type == ENTITY_GOLDMINE;
                },
                .compare = bot_closest_manhattan_distance_to(building.cell)
            });

            MatchInput rally_input;
            rally_input.type = MATCH_INPUT_RALLY;
            rally_input.rally.building_count = 1;
            rally_input.rally.building_ids[0] = building_id;
            rally_input.rally.rally_point = (state.entities.get_by_id(goldmine_id).cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
            return rally_input;
        }

        ivec2 rally_cell = bot_choose_building_rally_point(state, bot, building);
        if (rally_cell.x == -1) {
            continue;
        }
        ivec2 rally_point = cell_center(rally_cell).to_ivec2();
        if (building.rally_point == rally_point) {
            continue;
        }

        MatchInput input;
        input.type = MATCH_INPUT_RALLY;
        input.rally.building_ids[0] = building_id;
        input.rally.building_count = 1;
        input.rally.rally_point = rally_point;
        return input;
    } // End for each building

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id) {
    const Entity& entity = state.entities.get_by_id(entity_id);

    EntityId nearest_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& hall, EntityId hall_id) {
            return hall.type == ENTITY_HALL &&
                entity_is_selectable(hall) &&
                hall.player_id == bot.player_id;
        },
        .compare = bot_closest_manhattan_distance_to(entity.cell)
    });

    if (nearest_hall_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    int entity_size = entity_get_data(entity.type).cell_size;
    ivec2 target_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_size, state.entities.get_by_id(nearest_hall_id).cell, entity_get_data(ENTITY_HALL).cell_size, MAP_IGNORE_UNITS);

    std::vector<ivec2> path_to_hall;
    map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, target_cell, entity_size, &path_to_hall, MAP_IGNORE_UNITS);

    static const int DISTANCE_FROM_HALL = 4;
    if (path_to_hall.size() > DISTANCE_FROM_HALL) {
        target_cell = path_to_hall[path_to_hall.size() - DISTANCE_FROM_HALL];
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = target_cell;
    input.move.target_id = ID_NULL;
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

MatchInput bot_unit_flee(const MatchState& state, Bot& bot, EntityId entity_id) {
    static const int FLEE_CELL_DISTANCE = 16;

    const Entity& entity = state.entities.get_by_id(entity_id);
    ivec2 flee_cells[DIRECTION_COUNT];
    int flee_cell_scores[DIRECTION_COUNT];
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        flee_cells[direction] = map_clamp_cell(state.map, entity.cell + (DIRECTION_IVEC2[direction] * FLEE_CELL_DISTANCE));
        flee_cell_scores[direction] = 0;
    }

    for (const Entity& enemy : state.entities) {
        if (!entity_is_selectable(enemy) ||
                enemy.type == ENTITY_GOLDMINE ||
                state.players[enemy.player_id].team == state.players[entity.player_id].team) {
            continue;
        }

        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            if (ivec2::manhattan_distance(enemy.cell, flee_cells[direction]) < FLEE_CELL_DISTANCE) {
                flee_cell_scores[direction]++;
            }
        }
    }

    // Negatively weight flee cells that are close to the edges of the map
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        ivec2 distance_from_center = ivec2(
            flee_cells[direction].x < state.map.width / 2
                ? (state.map.width / 2) - flee_cells[direction].x
                : flee_cells[direction].x - (state.map.width / 2),
            flee_cells[direction].y < state.map.height / 2
                ? (state.map.height / 2) - flee_cells[direction].y
                : flee_cells[direction].y - (state.map.height / 2)
        );
        // The dividor can be adjusted here to make the weight bigger or smaller
        ivec2 negative_weights = distance_from_center / FLEE_CELL_DISTANCE;
        flee_cell_scores[direction] -= negative_weights.x + negative_weights.y;
    }

    int safest_direction = -1;
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        if (safest_direction == -1 || flee_cell_scores[direction] < flee_cell_scores[safest_direction]) {
            safest_direction = direction;
        }
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = flee_cells[safest_direction];
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

// Entity management

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id) {
    return bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end();
}

void bot_reserve_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: reserve entity %u", entity_id);
    bot.is_entity_reserved[entity_id] = true;
}

void bot_release_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: release entity %u", entity_id);
    GOLD_ASSERT(bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end());
    bot.is_entity_reserved.erase(entity_id);
}

// Squads

void bot_squad_dissolve(const MatchState& state, Bot& bot, BotSquad& squad) {
    for (EntityId entity_id : squad.entities) {
        bot_release_entity(bot, entity_id);
    }
    squad.entities.clear();
}

MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad) {
    // Remove dead units
    {
        uint32_t squad_entity_index = 0;
        while (squad_entity_index < squad.entities.size()) {
            uint32_t entity_index = state.entities.get_index_of(squad.entities[squad_entity_index]);
            if (entity_index == INDEX_INVALID || state.entities[entity_index].health == 0) {
                // Release and remove the entity
                bot_release_entity(bot, squad.entities[squad_entity_index]);
                squad.entities[squad_entity_index] = squad.entities[squad.entities.size() - 1];
                squad.entities.pop_back();
            } else {
                squad_entity_index++;
            }
        }
    }

    if (squad.entities.empty()) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Attack micro
    std::vector<EntityId> unengaged_units;
    for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entities.size(); squad_entity_index++) {
        EntityId unit_id = squad.entities[squad_entity_index];
        const Entity& unit = state.entities.get_by_id(unit_id);

        // Filter down to only units
        if (!entity_is_unit(unit.type)) {
            continue;
        }

        // Filter down to only ungarrisoned units (non-carriers)
        const EntityData& unit_data = entity_get_data(unit.type);
        if (unit.garrison_id != ID_NULL || unit_data.garrison_capacity != 0) {
            continue;
        }

        // Check if there is a target nearby
        EntityId nearby_enemy_id = bot_find_best_entity((BotFindBestEntityParams) {
            .state = state,
            .filter = [&state, &unit](const Entity& enemy, EntityId enemy_id) {
                return enemy.type != ENTITY_GOLDMINE &&
                        state.players[enemy.player_id].team != state.players[unit.player_id].team &&
                        entity_is_selectable(enemy) &&
                        ivec2::manhattan_distance(enemy.cell, unit.cell) < BOT_SQUAD_GATHER_DISTANCE &&
                        match_is_entity_visible_to_player(state, enemy, unit.player_id);
            },
            .compare = [&unit](const Entity& a, const Entity& b) {
                if (entity_get_data(a.type).attack_priority > entity_get_data(b.type).attack_priority) {
                    return true;
                }
                return ivec2::manhattan_distance(a.cell, unit.cell) < 
                        ivec2::manhattan_distance(b.cell, unit.cell);
            }
        });

        // If not, skip for now, we'll get back to this unit later
        if (nearby_enemy_id == ID_NULL) {
            unengaged_units.push_back(unit_id);
            continue;
        }

        const Entity& nearby_enemy = state.entities.get_by_id(nearby_enemy_id);

        // If this unit is a pyro, then throw molotov
        if (unit.type == ENTITY_PYRO) {
            // If the pyro is engaged but out of energy, just run away
            if (unit.energy < MOLOTOV_ENERGY_COST) {
                squad.entities[squad_entity_index] = squad.entities[squad.entities.size() - 1];
                squad.entities.pop_back();
                bot_release_entity(bot, unit_id);
                return bot_return_entity_to_nearest_hall(state, bot, unit_id);
            }
            ivec2 molotov_cell = bot_find_best_molotov_cell(state, bot, unit, nearby_enemy.cell);
            if (molotov_cell.x == -1) {
                continue;
            }
            if (unit.target.type == TARGET_MOLOTOV && ivec2::manhattan_distance(unit.target.cell, molotov_cell) < BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }

            MatchInput input;
            input.type = MATCH_INPUT_MOVE_MOLOTOV;
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;
            input.move.target_cell = molotov_cell;
            input.move.entity_count = 1;
            input.move.entity_ids[0] = unit_id;
            return input;
        }

        // Skip this infantry if it's already attacking something
        if (bot_is_unit_already_attacking_nearby_target(state, unit, nearby_enemy)) {
            continue;
        }

        // Infantry attack the nearby enemy
        MatchInput input;
        input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
        input.move.shift_command = 0;
        input.move.target_id = ID_NULL;
        input.move.target_cell = nearby_enemy.cell;
        input.move.entity_count = 1;
        input.move.entity_ids[0] = unit_id;

        // See if any other units in the squad want to join in on this input
        for (EntityId other_id : squad.entities) {
            // Don't consider the current entity
            if (unit_id == other_id) {
                continue;
            }

            const Entity& other = state.entities.get_by_id(other_id);
            // Filter out non-units
            if (!entity_is_unit(other.type)) {
                continue;
            }
            // Filter out non-attackers
            if (entity_get_data(other.type).unit_data.damage == 0) {
                continue;
            }
            // Filter out far away units
            if (ivec2::manhattan_distance(other.cell, nearby_enemy.cell) > BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }
            // Filter out units which are already busy with an important target
            if (bot_is_unit_already_attacking_nearby_target(state, other, nearby_enemy)) {
                continue;
            }

            input.move.entity_ids[input.move.entity_count] = other_id;
            input.move.entity_count++;
            if (input.move.entity_count == SELECTION_LIMIT) {
                break;
            }
        } // End for each other

        return input;
    } // End for each infantry
    // End attack micro

    // Unengaged infantry micro
    // This section mostly just sorts distant infantry into a list
    // But it also handles the pyro landmine placement
    std::vector<EntityId> distant_infantry;
    std::vector<EntityId> distant_cavalry;
    for (EntityId unit_id : unengaged_units) {
        const Entity& unit = state.entities.get_by_id(unit_id);

        // Skip units that are already garrisoning
        if (unit.target.type == TARGET_ENTITY) {
            continue;
        } 

        // If the unit is far away, add it to the distant infantry/cavalry list
        if (ivec2::manhattan_distance(unit.cell, squad.target_cell) > BOT_SQUAD_GATHER_DISTANCE) {
            if (entity_get_data(unit.type).garrison_size == ENTITY_CANNOT_GARRISON) {
                distant_cavalry.push_back(unit_id);
            } else {
                distant_infantry.push_back(unit_id);
            }
        }

        // Otherwise, do any unit-specific micro
        // TODO: pyro landmine placement here
    }

    // Garrison micro
    if (!distant_infantry.empty()) {
        for (EntityId carrier_id : squad.entities) {
            const Entity& carrier = state.entities.get_by_id(carrier_id);
            const uint32_t GARRISON_CAPACITY = entity_get_data(carrier.type).garrison_capacity;

            // Filter out non-carriers and carriers which are full
            if (carrier.garrisoned_units.size() == GARRISON_CAPACITY) {
                continue;
            }

            // Count units that are already going into this carrier
            uint32_t garrison_size = carrier.garrisoned_units.size();
            for (EntityId infantry_id : squad.entities) {
                const Entity& infantry = state.entities.get_by_id(infantry_id);
                if (infantry.target.type == TARGET_ENTITY && infantry.target.id == carrier_id) {
                    garrison_size++;
                }
            }

            // If we're already at capacity, then don't add any more to this carrier
            if (garrison_size >= GARRISON_CAPACITY) {
                continue;
            }

            MatchInput input;
            input.type = MATCH_INPUT_MOVE_ENTITY;
            input.move.shift_command = 0;
            input.move.target_id = carrier_id;
            input.move.target_cell = ivec2(0, 0);
            input.move.entity_count = 0;

            for (EntityId infantry_id : distant_infantry) {
                if (garrison_size + input.move.entity_count < GARRISON_CAPACITY) {
                    input.move.entity_ids[input.move.entity_count] = infantry_id;
                    input.move.entity_count++;
                } else {
                    // Find the furthest unit from the carrier that is already in the input
                    uint32_t furthest_unit_index = 0;
                    for (uint32_t unit_index = 1; unit_index < input.move.entity_count; unit_index++) {
                        if (ivec2::manhattan_distance(state.entities.get_by_id(input.move.entity_ids[unit_index]).cell, carrier.cell) > 
                                ivec2::manhattan_distance(state.entities.get_by_id(input.move.entity_ids[furthest_unit_index]).cell, carrier.cell)) {
                            furthest_unit_index = unit_index;
                        }
                    }

                    // If this infantry is closer than the furthest unit, then replace the furthest with this infantry
                    if (ivec2::manhattan_distance(state.entities.get_by_id(input.move.entity_ids[furthest_unit_index]).cell, carrier.cell) > 
                            ivec2::manhattan_distance(state.entities.get_by_id(infantry_id).cell, carrier.cell)) {
                        input.move.entity_ids[furthest_unit_index] = infantry_id;
                    }
                }
            } // End for each infantry in infantry_id

            GOLD_ASSERT(input.move.entity_count != 0);
            return input;
        }
    }

    // Carrier micro
    for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entities.size(); squad_entity_index++) { 
        EntityId carrier_id = squad.entities[squad_entity_index];
        const Entity& carrier = state.entities.get_by_id(carrier_id);

        // Filter out buildings (bunkers) since we cannot micro them
        if (!entity_is_unit(carrier.type)) {
            continue;
        }

        // Filter out non-carriers
        const EntityData& carrier_data = entity_get_data(carrier.type);
        if (carrier_data.garrison_capacity == 0) {
            continue;
        }

        // Determine position of infantry which are en-route to carrier
        ivec2 infantry_position_sum = ivec2(0, 0);
        int infantry_count = 0;
        for (EntityId infantry_id : squad.entities) {
            const Entity& infantry = state.entities.get_by_id(infantry_id);
            if (infantry.target.type == TARGET_ENTITY && infantry.target.id == carrier_id) {
                infantry_position_sum += infantry.cell;
                infantry_count++;
            }
        }

        // Move toward en-route infantry
        if (infantry_count != 0) {
            ivec2 infantry_center = infantry_position_sum / infantry_count;
            std::vector<ivec2> path_to_infantry_center;
            map_pathfind(state.map, CELL_LAYER_GROUND, carrier.cell, infantry_center, 2, &path_to_infantry_center, MAP_IGNORE_UNITS);

            // If the path is empty for some reason (or just a small path), then don't worry about moving this carrier
            if (path_to_infantry_center.size() < BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }

            // If the carrier is already en-route to the midpoint, then don't worry about moving the carrier
            ivec2 path_midpoint = path_to_infantry_center[path_to_infantry_center.size() / 2];
            ivec2 carrier_target_cell = carrier.target.type == TARGET_CELL ? carrier.target.cell : carrier.cell;
            if (ivec2::manhattan_distance(carrier_target_cell, path_midpoint) < BOT_SQUAD_GATHER_DISTANCE * 2) {
                continue;
            }

            // Finally, if there is a good midpoint and the carrier is not close to it or not moving close to it,
            // then order the carrier to move to it
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_CELL;
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;
            input.move.target_cell = path_midpoint;
            input.move.entity_ids[0] = carrier_id;
            input.move.entity_count++;
            return input;
        } // End if infantry count != 0

        // The rest of this block applies only to carriers
        // which have no entities en-route to them

        // Since there is no one en-route and no one to pick up,
        // if the carrier is empty, release it from the squad
        if (carrier.garrisoned_units.empty()) {
            bot_release_entity(bot, carrier_id);
            squad.entities[squad_entity_index] = squad.entities.back();
            squad.entities.pop_back();

            return bot_return_entity_to_nearest_hall(state, bot, carrier_id);
        }

        // Unload garrisoned units if the carrier is 
        // 1. close to the target cell or 2. nearby an enemy
        bool should_unload_garrisoned_units = false;
        if (ivec2::manhattan_distance(carrier.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
            should_unload_garrisoned_units = true;
        } else {
            EntityId nearby_enemy_id = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&state, &carrier](const Entity& enemy, EntityId enemy_id) {
                    return enemy.type != ENTITY_GOLDMINE &&
                            state.players[enemy.player_id].team != state.players[carrier.player_id].team &&
                            entity_is_selectable(enemy) &&
                            ivec2::manhattan_distance(enemy.cell, carrier.cell) < BOT_SQUAD_GATHER_DISTANCE &&
                            match_is_entity_visible_to_player(state, enemy, carrier.player_id);
                }
            });
            if (nearby_enemy_id != ID_NULL) {
                should_unload_garrisoned_units = true;
            }
        }
        if (should_unload_garrisoned_units && 
                carrier.target.type == TARGET_UNLOAD &&
                ivec2::manhattan_distance(carrier.cell, carrier.target.cell) < 4) {
            continue;
        }
        if (should_unload_garrisoned_units) {
            MatchInput unload_input;
            unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
            unload_input.move.shift_command = 0;
            unload_input.move.target_cell = carrier.cell;
            unload_input.move.target_id = ID_NULL;
            unload_input.move.entity_ids[0] = carrier_id;
            unload_input.move.entity_count = 1;
            return unload_input;
        }

        // Otherwise if we're not near the target cell, go there
        // But first check to see if we are already on the way
        if (carrier.target.type == TARGET_UNLOAD && 
                ivec2::manhattan_distance(carrier.target.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
            continue;
        }

        // If we're not on the way, then unload garrisoned units at target cell
        MatchInput unload_input;
        unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
        unload_input.move.shift_command = 0;
        unload_input.move.target_cell = squad.target_cell;
        unload_input.move.target_id = ID_NULL;
        unload_input.move.entity_ids[0] = carrier_id;
        unload_input.move.entity_count = 1;
        return unload_input;
    }

    // Combine the distant infantry / cavalry lists into one
    for (EntityId entity_id : distant_cavalry) {
        distant_infantry.push_back(entity_id);
    }

    // Move the distant units to the target
    {
        MatchInput input;
        input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
        input.move.shift_command = 0;
        input.move.target_cell = squad.target_cell;
        input.move.target_id = ID_NULL;
        input.move.entity_count = 0;

        for (EntityId unit_id : distant_infantry) {
            const Entity& unit = state.entities.get_by_id(unit_id);
            if (unit.target.type == TARGET_ATTACK_CELL && 
                    ivec2::manhattan_distance(unit.target.cell, input.move.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }

            input.move.entity_ids[input.move.entity_count] = unit_id;
            input.move.entity_count++;
            if (input.move.entity_count == SELECTION_LIMIT) {
                break;
            }
        }

        if (input.move.entity_count != 0) {
            return input;
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

// Scouting

void bot_scout_update(const MatchState& state, Bot& bot) {
    // Check for scout death
    if (bot.scout_id != ID_NULL) {
        uint32_t scout_index = state.entities.get_index_of(bot.scout_id);
        if (scout_index == INDEX_INVALID) {
            bot_release_entity(bot, bot.scout_id);
            bot.scout_id = ID_NULL;
        }
    }
}

MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    // If we have no scout, it means we're not scouting
    if (bot.scout_id == ID_NULL) {
        // Determine if we should be
        if (bot_get_next_scout_time(bot) > match_time_minutes) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        // If we got here, it means we should do another scout, so let's find a scout
        bot.scout_id = bot_find_best_entity((BotFindBestEntityParams) {
            .state = state, 
            .filter = [&bot](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id && 
                    bot_score_scout_type(entity.type) != 0 &&
                    entity.garrisoned_units.empty() && 
                    !bot_is_entity_reserved(bot, entity_id);
            },
            .compare = [](const Entity&a, const Entity& b) {
                return bot_score_scout_type(a.type) > bot_score_scout_type(b.type);
            }
        });
        if (bot.scout_id == ID_NULL) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
        if (bot.goal.type != BOT_GOAL_RUSH) {
            bot_reserve_entity(bot, bot.scout_id);
        }

        // Reset the scouted status of each scout info
        // TODO: decide whether to do a full scout or a small scout
        for (BotScoutInfo& scout_info : bot.scout_info) {
            const Entity& goldmine = state.entities.get_by_id(scout_info.goldmine_id);
            scout_info.should_scout = !match_is_entity_visible_to_player(state, goldmine, bot.player_id);
        }
    }

    const Entity& scout = state.entities.get_by_id(bot.scout_id);

    if (scout.target.type == TARGET_NONE) {
        return bot_scout_next_target(state, bot, scout, match_time_minutes);
    }

    if (scout.target.type == TARGET_ENTITY) {
        uint32_t target_index = state.entities.get_index_of(scout.target.id);
        if (target_index == INDEX_INVALID || entity_is_unit(state.entities[target_index].type)) {
            return bot_scout_next_target(state, bot, scout, match_time_minutes);
        }

        if (match_is_entity_visible_to_player(state, state.entities[target_index], bot.player_id)) {
            // We are targeting either a goldmine or a building
            // Find the closest goldmine to our target (which may be our target itself)
            EntityId goldmine_id;
            const Entity& target = state.entities[target_index];
            goldmine_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state, 
                .filter = [](const Entity& goldmine, EntityId goldmine_id) {
                    return goldmine.type == ENTITY_GOLDMINE;
                },
                .compare = [&target](const Entity& a, const Entity& b) {
                    return ivec2::manhattan_distance(a.cell, target.cell) <
                            ivec2::manhattan_distance(b.cell, target.cell);
                }
            });
            GOLD_ASSERT((target.type == ENTITY_GOLDMINE && scout.target.id == goldmine_id) || (target.type != ENTITY_GOLDMINE && scout.target.id != goldmine_id));

            const Entity& goldmine = state.entities.get_by_id(goldmine_id);
            EntityId surrounding_hall_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);

            int scout_info_index = -1;
            for (int index = 0; index < bot.scout_info.size(); index++) {
                if (bot.scout_info[index].goldmine_id == goldmine_id) {
                    scout_info_index = index;
                    break;
                }
            }
            GOLD_ASSERT(scout_info_index != -1);

            bot.scout_info[scout_info_index].occupying_player_id = surrounding_hall_id != ID_NULL 
                                                                        ? state.entities.get_by_id(surrounding_hall_id).player_id
                                                                        : PLAYER_NONE;

            if (surrounding_hall_id == ID_NULL) {
                bot.scout_info[scout_info_index].should_scout = false;
                return bot_scout_next_target(state, bot, scout, match_time_minutes);
            }

            EntityId unscouted_building_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state,
                .filter = [&state, &bot, &goldmine_id](const Entity& entity, EntityId entity_id) {
                    // Filter out non-buildings and already-scouted buildings
                    if (!entity_is_building(entity.type) ||
                            !entity_is_selectable(entity) ||
                            bot_has_scouted_entity(state, bot, entity, entity_id)) {
                        return false;
                    }

                    // Find the goldmine nearest to this building
                    uint32_t nearest_goldmine_index = INDEX_INVALID;
                    for (const BotScoutInfo& scout_info : bot.scout_info) {
                        uint32_t goldmine_index = state.entities.get_index_of(scout_info.goldmine_id);
                        if (nearest_goldmine_index == INDEX_INVALID ||
                                ivec2::manhattan_distance(entity.cell, state.entities[goldmine_index].cell) <
                                ivec2::manhattan_distance(entity.cell, state.entities[nearest_goldmine_index].cell)) {
                            nearest_goldmine_index = goldmine_index;
                        }
                    }

                    // And return true only if the nearest goldmine is 
                    // equal to the goldmine that we are scouting right now
                    GOLD_ASSERT(nearest_goldmine_index != INDEX_INVALID);
                    return state.entities.get_id_of(nearest_goldmine_index) == goldmine_id;
                },
                .compare = bot_closest_manhattan_distance_to(scout.cell)
            });

            if (unscouted_building_id == ID_NULL) {
                bot.scout_info[scout_info_index].should_scout = false;
                return bot_scout_next_target(state, bot, scout, match_time_minutes);
            } else {
                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ENTITY;
                input.move.shift_command = 0;
                input.move.target_id = unscouted_building_id;
                input.move.target_cell = ivec2(0, 0);
                input.move.entity_ids[0] = bot.scout_id;
                input.move.entity_count = 1;
                return input;
            }
        }
    }

    // If we are taking damage, flee and mark the nearest goldmine
    // as scouted. This way we don't sacrifice the scout
    if (scout.taking_damage_timer != 0) {
        for (BotScoutInfo& scout_info : bot.scout_info) {
            const Entity& goldmine = state.entities.get_by_id(scout_info.goldmine_id);
            if (ivec2::manhattan_distance(scout.cell, goldmine.cell) < 32) {
                scout_info.should_scout = false;
            }
        }
        return bot_unit_flee(state, bot, bot.scout_id);
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

MatchInput bot_scout_next_target(const MatchState& state, Bot& bot, const Entity& scout, uint32_t match_time_minutes) {
    // Choose the next goldmine to scout
    EntityId next_goldmine_id = ID_NULL;
    int next_goldmine_distance;
    for (const BotScoutInfo& scout_info : bot.scout_info) {
        if (!scout_info.should_scout) {
            continue;
        }
        const Entity& goldmine = state.entities.get_by_id(scout_info.goldmine_id);
        int goldmine_distance = ivec2::manhattan_distance(scout.cell, goldmine.cell);
        if (next_goldmine_id == ID_NULL || goldmine_distance < next_goldmine_distance) {
            next_goldmine_id = scout_info.goldmine_id;
            next_goldmine_distance = goldmine_distance;
        }
    }

    if (next_goldmine_id != ID_NULL) {
        MatchInput input;
        input.type = MATCH_INPUT_MOVE_ENTITY;
        input.move.shift_command = 0;
        input.move.target_id = next_goldmine_id;
        input.move.target_cell = ivec2(0, 0);
        input.move.entity_ids[0] = bot.scout_id;
        input.move.entity_count = 1;
        return input;
    } else {
        // If there are no goldmines to scout, then the scout is complete
        MatchInput input = bot_return_entity_to_nearest_hall(state, bot, bot.scout_id);

        bot.last_scout_time = match_time_minutes;
        if (bot_is_entity_reserved(bot, bot.scout_id)) {
            bot_release_entity(bot, bot.scout_id);
        }
        bot.scout_id = ID_NULL;

        return input;
    } // end if next goldmine is null
}

uint32_t bot_get_next_scout_time(const Bot& bot) {
    uint32_t next_scout_time;
    if (bot.last_scout_time == 0) {
        next_scout_time = 0;
    } else if (bot.last_scout_time < 10) {
        next_scout_time = bot.last_scout_time + 5;
    } else {
        next_scout_time = bot.last_scout_time + 10;
    }

    return next_scout_time;
}

int bot_score_scout_type(EntityType type) {
    switch (type) {
        case ENTITY_WAGON:
            return 4;
        case ENTITY_JOCKEY:
            return 3;
        case ENTITY_DETECTIVE:
            return 2;
        case ENTITY_BANDIT:
            return 1;
        default:
            return 0;
    }
}

// Helpers

EntityId bot_find_entity(BotFindEntityParams params) {
    for (uint32_t entity_index = 0; entity_index < params.state.entities.size(); entity_index++) {
        const Entity& entity = params.state.entities[entity_index];
        EntityId entity_id = params.state.entities.get_id_of(entity_index);
        if (params.filter(entity, entity_id)) {
            return entity_id;
        }
    }

    return ID_NULL;
}

EntityId bot_find_best_entity(BotFindBestEntityParams params) {
    uint32_t best_entity_index = INDEX_INVALID;
    for (uint32_t entity_index = 0; entity_index < params.state.entities.size(); entity_index++) {
        const Entity& entity = params.state.entities[entity_index];
        EntityId entity_id = params.state.entities.get_id_of(entity_index);

        if (!params.filter(entity, entity_id)) {
            continue;
        }
        if (best_entity_index == INDEX_INVALID || params.compare(entity, params.state.entities[best_entity_index])) {
            best_entity_index = entity_index;
        }
    }

    if (best_entity_index == INDEX_INVALID) {
        return ID_NULL;
    }
    return params.state.entities.get_id_of(best_entity_index);
}

std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 p_cell) {
    // We have to copy the variable because the lambda is passing a reference, but p_cell is itself just a reference to the actual variable
    ivec2 cell = p_cell;
    return [&cell](const Entity& a, const Entity& b) {
        return ivec2::manhattan_distance(a.cell, cell) < ivec2::manhattan_distance(b.cell, cell);
    };
}

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell) {
    return bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& entity, EntityId entity_id) {
            return entity.type == ENTITY_MINER &&
                    entity.player_id == bot.player_id &&
                    entity.mode == MODE_UNIT_IDLE &&
                    entity.target.type == TARGET_NONE &&
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
            return ENTITY_WORKSHOP;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building) {
    std::vector<ivec2> frontier;
    std::vector<bool> explored(state.map.width * state.map.height, false);
    frontier.push_back(building.cell);
    ivec2 rally_cell = ivec2(-1, -1);

    while (!frontier.empty()) {
        ivec2 next = frontier[0];
        frontier.erase(frontier.begin());

        if (explored[next.x + (next.y * state.map.width)]) {
            continue;
        }

        static const int RALLY_MARGIN = 4;
        bool is_rally_point_valid = map_get_tile(state.map, next).elevation == map_get_tile(state.map, building.cell).elevation;
        for (int y = next.y - RALLY_MARGIN; y < next.y + RALLY_MARGIN + 1; y++) {
            for (int x = next.x - RALLY_MARGIN; x < next.x + RALLY_MARGIN + 1; x++) {
                ivec2 cell = ivec2(x, y);
                if (!map_is_cell_in_bounds(state.map, cell)) {
                    is_rally_point_valid = false;
                }
                if (map_is_tile_ramp(state.map, cell)) {
                    is_rally_point_valid = false;
                }
                Cell map_cell = map_get_cell(state.map, CELL_LAYER_GROUND, cell);
                if (map_cell.type == CELL_BUILDING || map_cell.type == CELL_GOLDMINE) {
                    is_rally_point_valid = false;
                }
            }
        }
        if (is_rally_point_valid) {
            return next;
        }

        explored[next.x + (next.y * state.map.width)] = true;
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(state.map, child)) {
                continue;
            }
            frontier.push_back(child);
        }
    }

    return ivec2(-1, -1);
}

bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id) {
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    uint32_t bot_team = state.players[bot.player_id].team;
    // If fog is revealed, then they can see it
    if (match_is_cell_rect_revealed(state, bot_team, entity.cell, entity_cell_size)) {
        return true;
    }
    // If for is only explored but they have seen the entity before, then they can see it
    if (match_is_cell_rect_explored(state, bot_team, entity.cell, entity_cell_size) &&
            state.remembered_entities[bot_team].find(entity_id) != state.remembered_entities[bot_team].end()) {
        return true;
    }
    // Otherwise, they have not scouted it
    return false;
}

ivec2 bot_get_squad_center_point(const MatchState& state, const BotSquad& squad) {
    struct CenterPoint {
        ivec2 sum;
        int size;
        ivec2 point() {
            return sum / size;
        }
    };
    std::vector<CenterPoint> center_points;

    for (EntityId entity_id : squad.entities) {
        ivec2 entity_cell = state.entities.get_by_id(entity_id).cell;

        int nearest_center_point_index = -1;
        for (int center_point_index = 0; center_point_index < center_points.size(); center_point_index++) {
            if (ivec2::manhattan_distance(
                    entity_cell, 
                    center_points[center_point_index].point()) > BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }

            if (nearest_center_point_index == -1 || 
                    ivec2::manhattan_distance(
                        entity_cell, 
                        center_points[center_point_index].point()) <
                    ivec2::manhattan_distance(
                        entity_cell, 
                        center_points[nearest_center_point_index].point())) {
                nearest_center_point_index = center_point_index;
            }
        }

        if (nearest_center_point_index == -1) {
            center_points.push_back((CenterPoint) {
                .sum = entity_cell,
                .size = 1
            });
        } else {
            center_points[nearest_center_point_index].sum += entity_cell;
            center_points[nearest_center_point_index].size++;
        }
    }

    GOLD_ASSERT(!center_points.empty());
    int biggest_center_point = 0;
    for (int index = 1; index < center_points.size(); index++) {
        if (center_points[index].size > center_points[biggest_center_point].size) {
            biggest_center_point = index;
        }
    }

    return center_points[biggest_center_point].point();
}

ivec2 bot_get_squad_attack_point(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    ivec2 center_point = bot_get_squad_center_point(state, squad);
    EntityId nearest_building_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&state, &bot](const Entity& building, EntityId building_id) {
            return state.players[building.player_id].team !=
                    state.players[bot.player_id].team &&
                    entity_is_building(building.type) &&
                    entity_is_selectable(building) &&
                    bot_has_scouted_entity(state, bot, building, building_id);
        },
        .compare = [&center_point](const Entity& a, const Entity& b) {
            if (a.type == ENTITY_HALL && b.type != ENTITY_HALL) {
                return true;
            }
            return ivec2::manhattan_distance(a.cell, center_point) <
                    ivec2::manhattan_distance(b.cell, center_point);
        }
    });

    // There are no buildings that we know of
    if (nearest_building_id == ID_NULL) {
        return ivec2(-1, -1);
    }

    // If nearest building is not a hall, that means there are no halls that
    // we know of, so just attack the nearest building
    const Entity& nearest_building = state.entities.get_by_id(nearest_building_id);
    if (nearest_building.type != ENTITY_HALL) {
        return nearest_building.cell;
    }

    std::unordered_map<uint32_t, uint32_t> enemy_hall_defense_score;
    // First pass, find all enemy halls
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (state.players[entity.player_id].team == 
                state.players[bot.player_id].team ||
                entity.type != ENTITY_HALL ||
                !entity_is_selectable(entity) ||
                !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }

        enemy_hall_defense_score[entity_index] = 0;
    }

    // Second pass, find all hall defenses
    // (this way allows us to avoid O(n^2) because we don't have to search
    // all entities for every defense just to find the nearest hall)
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (state.players[entity.player_id].team == 
                state.players[bot.player_id].team ||
                entity.type == ENTITY_HALL ||
                !entity_is_building(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        uint32_t nearest_hall_index = INDEX_INVALID;
        for (auto it : enemy_hall_defense_score) {
            if (nearest_hall_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(entity.cell, state.entities[it.first].cell) <
                    ivec2::manhattan_distance(entity.cell, state.entities[nearest_hall_index].cell)) {
                nearest_hall_index = it.first;
            }
        }

        GOLD_ASSERT(nearest_hall_index != INDEX_INVALID);
        uint32_t entity_defense_value = entity.type == ENTITY_BUNKER ? 2 : 1;
        enemy_hall_defense_score[nearest_hall_index] += entity_defense_value;
    }

    // Find the least defended hall
    uint32_t least_defended_hall_index = INDEX_INVALID;
    for (auto it : enemy_hall_defense_score) {
        if (least_defended_hall_index == INDEX_INVALID ||
                enemy_hall_defense_score[it.first] < enemy_hall_defense_score[least_defended_hall_index]) {
            least_defended_hall_index = it.first;
        }
    }

    // Compare the least defended hall with the nearest one. If the nearest
    // one is basically as well defended as the least defended hall, then
    // prefer the nearest
    uint32_t nearest_building_index = state.entities.get_index_of(nearest_building_id);
    GOLD_ASSERT(nearest_building_index != INDEX_INVALID && 
                    enemy_hall_defense_score.find(nearest_building_index) != enemy_hall_defense_score.end());
    if (enemy_hall_defense_score[nearest_building_index] - enemy_hall_defense_score[least_defended_hall_index] < 2) {
        least_defended_hall_index = nearest_building_index;
    }

    return state.entities[least_defended_hall_index].cell;
}

bool bot_is_base_under_attack(const MatchState& state, const Bot& bot) {
    EntityId building_under_attack_id = bot_find_entity((BotFindEntityParams) {
        .state = state, 
        .filter = [&bot](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot.player_id && 
                    entity_is_selectable(entity) && 
                    entity_is_building(entity.type) &&
                    entity.taking_damage_timer != 0;
        }
    });
    return building_under_attack_id != ID_NULL;
}

bool bot_is_unit_already_attacking_nearby_target(const MatchState& state, const Entity& infantry, const Entity& nearby_enemy) {
    if (infantry.target.type == TARGET_ATTACK_ENTITY) {
        uint32_t target_index = state.entities.get_index_of(infantry.target.id);
        if (target_index != INDEX_INVALID && 
                entity_get_data(state.entities[target_index].type).attack_priority >= 
                entity_get_data(nearby_enemy.type).attack_priority) {
            return true;
        }
    }
    if ((infantry.target.type == TARGET_ATTACK_CELL || infantry.target.type == TARGET_MOLOTOV) && 
            ivec2::manhattan_distance(infantry.target.cell, nearby_enemy.cell) < BOT_SQUAD_GATHER_DISTANCE) {
        return true;
    }

    return false;
}

int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell) {
    int score = 0;
    std::unordered_map<EntityId, bool> entity_considered;

    for (int y = cell.y - PROJECTILE_MOLOTOV_FIRE_SPREAD; y < cell.y + PROJECTILE_MOLOTOV_FIRE_SPREAD; y++) {
        for (int x = cell.x - PROJECTILE_MOLOTOV_FIRE_SPREAD; x < cell.x + PROJECTILE_MOLOTOV_FIRE_SPREAD; x++) {
            // Don't count the corners of the square
            if ((y == cell.y - PROJECTILE_MOLOTOV_FIRE_SPREAD || y == cell.y + PROJECTILE_MOLOTOV_FIRE_SPREAD - 1) &&
                    (x == cell.x - PROJECTILE_MOLOTOV_FIRE_SPREAD || x == cell.x + PROJECTILE_MOLOTOV_FIRE_SPREAD - 1)) {
                continue;
            }

            ivec2 fire_cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state.map, fire_cell)) {
                continue;
            }
            if (fire_cell == pyro.cell) {
                score -= 4;
                continue;
            }
            if (match_is_cell_on_fire(state, fire_cell)) {
                score--;
                continue;
            }

            Cell map_cell = map_get_cell(state.map, CELL_LAYER_GROUND, fire_cell);
            if (map_cell.type == CELL_BUILDING || map_cell.type == CELL_UNIT || map_cell.type == CELL_MINER) {
                if (entity_considered.find(map_cell.id) != entity_considered.end()) {
                    continue;
                }
                entity_considered[map_cell.id] = true;

                const Entity& target = state.entities.get_by_id(map_cell.id);
                if (state.players[target.player_id].team == state.players[pyro.player_id].team) {
                    score -= 2;
                } else if (entity_is_unit(target.type)) {
                    score += 2;
                } else {
                    score++;
                }
            }
        }
    } // End for each fire y

    // Consider other existing or about to exist molotov fires
    std::vector<ivec2> existing_molotov_cells;
    for (const Entity& entity : state.entities) {
        if (entity.target.type == TARGET_MOLOTOV) {
            existing_molotov_cells.push_back(entity.target.cell);
        }
    }
    for (const Projectile& projectile : state.projectiles) {
        if (projectile.type == PROJECTILE_MOLOTOV) {
            existing_molotov_cells.push_back(projectile.target.to_ivec2());
        }
    }
    for (ivec2 existing_molotov_cell : existing_molotov_cells) {
        int distance = ivec2::manhattan_distance(cell, existing_molotov_cell);
        if (distance < PROJECTILE_MOLOTOV_FIRE_SPREAD) {
            score -= PROJECTILE_MOLOTOV_FIRE_SPREAD - distance;
        }
    }

    return score;
}

ivec2 bot_find_best_molotov_cell(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 attack_point) {
    ivec2 best_molotov_cell = ivec2(-1, -1);
    int best_molotov_cell_score = 0;

    for (int y = attack_point.y - BOT_SQUAD_GATHER_DISTANCE; y < attack_point.y + BOT_SQUAD_GATHER_DISTANCE; y++) {
        for (int x = attack_point.x - BOT_SQUAD_GATHER_DISTANCE; x < attack_point.x + BOT_SQUAD_GATHER_DISTANCE; x++) {
            ivec2 cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }

            int molotov_cell_score = bot_get_molotov_cell_score(state, bot, pyro, cell);
            if (molotov_cell_score > best_molotov_cell_score) {
                best_molotov_cell = cell;
                best_molotov_cell_score = molotov_cell_score;
            }
        }
    }

    return best_molotov_cell;
}
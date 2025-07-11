#include "bot.h"

#include "core/logger.h"
#include "upgrade.h"

static const int SQUAD_GATHER_DISTANCE = 8;
static const uint32_t SQUAD_LANDMINE_MAX = 6;

Bot bot_init(uint8_t player_id) {
    Bot bot;

    bot.player_id = player_id;
    bot_set_strategy(bot, BOT_STRATEGY_BANDIT_RUSH);

    return bot;
};

void bot_update(const MatchState& state, Bot& bot) {
    bot_saturate_bases(state, bot);
    if (bot_should_expand(state, bot)) {
        bot_build_building(state, bot, ENTITY_HALL);
    } else if (bot_should_build_house(state, bot)) {
        bot_build_building(state, bot, ENTITY_HOUSE);
    } else {
        // Get desired entities
        uint32_t desired_entities[ENTITY_TYPE_COUNT];
        bot_get_desired_entities(state, bot, desired_entities);

        // Choose desired unit
        EntityType desired_unit = ENTITY_TYPE_COUNT;
        for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
            if (desired_entities[entity_type] > 0) {
                desired_unit = (EntityType)entity_type;
                break;
            }
        }

        // Chose desired building
        EntityType desired_building = ENTITY_TYPE_COUNT;
        for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
            if (desired_entities[entity_type] > 0) {
                desired_building = (EntityType)entity_type;
                break;
            }
        }

        // Train unit
        if (desired_unit != ENTITY_TYPE_COUNT) {
            bot_train_unit(state, bot, desired_unit);
        }

        // Build building
        if (desired_building != ENTITY_TYPE_COUNT) {
            bot_build_building(state, bot, desired_building);
        }

        // Research upgrade
        if (bot.strategy.desired_upgrade != 0 && match_player_upgrade_is_available(state, bot.player_id, bot.strategy.desired_upgrade)) {
            bot_research_upgrade(state, bot, bot.strategy.desired_upgrade);
        }

        // If we have no desired unit, building, or upgrade, then this strategy is done
        if (bot_has_desired_entities(state, bot) &&
                (bot.strategy.desired_upgrade == 0 || match_player_has_upgrade(state, bot.player_id, bot.strategy.desired_upgrade))) {
            bot_squad_create(state, bot);
            // TODO: set next strategy
        }
    }

    // Squad update
    uint32_t squad_index = 0;
    while (squad_index < bot.squads.size()) {
        bot_squad_update(state, bot, bot.squads[squad_index]);

        // Check if squad is dissolved
        if (bot.squads[squad_index].mode == BOT_SQUAD_MODE_DISSOLVED) {
            bot.squads[squad_index] = bot.squads[bot.squads.size() - 1];
            bot.squads.pop_back();
        } else {
            squad_index++;
        }
    }
}

// Behaviors

void bot_set_strategy(Bot& bot, BotStrategyType type) {
    BotStrategy strategy;
    memset(strategy.desired_entities, 0, sizeof(strategy.desired_entities));
    strategy.desired_upgrade = 0;
    strategy.type = type;

    switch (strategy.type) {
        case BOT_STRATEGY_BANDIT_RUSH: {
            strategy.desired_entities[ENTITY_BANDIT] = 4;
            strategy.desired_entities[ENTITY_WAGON] = 1;
            break;
        }
    }

    bot.strategy = strategy;
}

void bot_saturate_bases(const MatchState& state, Bot& bot) {
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
                bot.inputs.push_back(input);
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
                bot.inputs.push_back(input);
                break;
            }

            // If we're still here, then there were no idle workers
            // So we'll create one out of the town hall
            if (bot_get_effective_gold(state, bot) >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
                bot.inputs.push_back((MatchInput) {
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
                    bot.inputs.push_back(rally_input);
                }
                return;
            }
        }
    } // End for each hall index
}

bool bot_should_expand(const MatchState& state, const Bot& bot) {
    return false;
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

void bot_build_building(const MatchState& state, Bot& bot, EntityType building_type) {
    GOLD_ASSERT(building_type != ENTITY_TYPE_COUNT && entity_is_building(building_type));

    // Check pre-req
    EntityType prereq_type = bot_get_building_prereq(building_type);
    if (prereq_type != ENTITY_TYPE_COUNT) {
        EntityId prereq_id = bot_find_entity(state, [&bot, &prereq_type](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot.player_id && entity.mode == MODE_BUILDING_FINISHED && entity.type == prereq_type;
        });
        if (prereq_id == ID_NULL) {
            return;
        }
    }

    // Check gold cost
    if (bot_get_effective_gold(state, bot) < entity_get_data(building_type).gold_cost) {
        return;
    }

    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id);
    if (hall_index == INDEX_INVALID) {
        return;
    }

    EntityId builder_id = bot_find_builder(state, bot, hall_index);
    if (builder_id == ID_NULL) {
        return;
    }

    ivec2 building_location = building_type == ENTITY_HALL ? bot_find_hall_location(state, hall_index) : bot_find_building_location(state, bot.player_id, state.entities[hall_index].cell + ivec2(1, 1), entity_get_data(building_type).cell_size);
    if (building_location.x == -1) {
        return;
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILD;
    input.build.shift_command = 0;
    input.build.target_cell = building_location;
    input.build.building_type = building_type;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = builder_id;
    bot.inputs.push_back(input);
}

void bot_train_unit(const MatchState& state, Bot& bot, EntityType unit_type) {
    GOLD_ASSERT(unit_type != ENTITY_TYPE_COUNT && entity_is_unit(unit_type));

    // Find building to train unit
    EntityType building_type = bot_get_building_which_trains(unit_type);
    EntityId building_id = bot_find_entity(state, [&bot, &building_type](const Entity& entity, EntityId entity_id) {
        return entity.player_id == bot.player_id && 
                entity.type == building_type &&
                entity.mode == MODE_BUILDING_FINISHED &&
                entity.queue.empty();
    });
    if (building_id == ID_NULL) {
        return;
    }

    // Check gold
    if (bot_get_effective_gold(state, bot) < entity_get_data(unit_type).gold_cost) {
        return;
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = unit_type;
    input.building_enqueue.building_id = building_id;
    bot.inputs.push_back(input);

    // Set rally points
    MatchInput rally_input;
    rally_input.type = MATCH_INPUT_RALLY;
    rally_input.rally.rally_point = bot_get_best_rally_point(state, building_id);
    rally_input.rally.building_count = 1;
    rally_input.rally.building_ids[0] = building_id;
    bot.inputs.push_back(rally_input);
}

void bot_research_upgrade(const MatchState& state, Bot& bot, uint32_t upgrade) {
    GOLD_ASSERT(match_player_upgrade_is_available(state, bot.player_id, upgrade));

    EntityType building_type = bot_get_building_which_researches(upgrade);
    EntityId building_id = bot_find_entity(state, [&bot, &building_type](const Entity& entity, EntityId entity_id) {
        return entity.player_id == bot.player_id && 
                entity.type == building_type &&
                entity.mode == MODE_BUILDING_FINISHED &&
                entity.queue.empty();
    });
    if (building_id == ID_NULL) {
        return;
    }

    // Check gold
    if (bot_get_effective_gold(state, bot) < upgrade_get_data(upgrade).gold_cost) {
        return;
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UPGRADE;
    input.building_enqueue.item_subtype = upgrade;
    bot.inputs.push_back(input);
}

void bot_get_desired_entities(const MatchState& state, const Bot& bot, uint32_t desired_entities[ENTITY_TYPE_COUNT]) {
    memcpy(desired_entities, bot.strategy.desired_entities, sizeof(bot.strategy.desired_entities));

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
        } else if (desired_entities[entity.type] > 0) {
            desired_entities[entity.type]--;
        }
    }

    // Determine desired buildings based on desired army
    for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
        if (desired_entities[entity_type] == 0) {
            continue;
        }
        EntityType building_type = bot_get_building_which_trains((EntityType)entity_type);
        if (desired_entities[entity_type] < 4) {
            desired_entities[building_type] += 1;
        } else if (desired_entities[entity_type] < 16) {
            desired_entities[building_type] += 2;
        } else if (desired_entities[entity_type] < 32) {
            desired_entities[building_type] += 3;
        } else if (desired_entities[entity_type] < 48) {
            desired_entities[building_type] += 4;
        } else if (desired_entities[entity_type] < 64) {
            desired_entities[building_type] += 5;
        } else {
            desired_entities[building_type] += 6;
        }
    }

    // Determine desired buildings based on desired upgrade
    if (bot.strategy.desired_upgrade != 0 && !match_player_upgrade_is_available(state, bot.player_id, bot.strategy.desired_upgrade)) {
        EntityType building_type = bot_get_building_which_researches(bot.strategy.desired_upgrade);
        if (desired_entities[building_type] == 0) {
            desired_entities[building_type] = 1;
        }
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
}

bool bot_has_desired_entities(const MatchState& state, const Bot& bot) {
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
        if (entity_count[entity_type] < bot.strategy.desired_entities[entity_type]) {
            return false;
        }
    }

    return true;
}

// Squads

void bot_squad_create(const MatchState& state, Bot& bot) {
    BotSquad squad;

    // Reserve units
    uint32_t squad_entity_count[ENTITY_TYPE_COUNT];
    memset(squad_entity_count, 0, sizeof(squad_entity_count));
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        while (squad_entity_count[entity_type] < bot.strategy.desired_entities[entity_type]) {
            // Search for an entity of this type
            EntityId entity_id = bot_find_entity(state, [&bot, &entity_type](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id &&
                            entity.type == entity_type &&
                            entity_is_selectable(entity) &&
                            !bot_is_entity_reserved(bot, entity_id);
            });
            // We should never enter this function unless we know we have every desired entity
            GOLD_ASSERT(entity_id != ID_NULL);

            bot_reserve_entity(bot, entity_id);
            squad.entities.push_back(entity_id);
            squad_entity_count[entity_type]++;
        }
    }

    bot_squad_set_mode(state, bot, squad, BOT_SQUAD_MODE_GATHER);
    bot.squads.push_back(squad);
}

void bot_squad_set_mode(const MatchState& state, Bot& bot, BotSquad& squad, BotSquadMode mode) {
    squad.mode = mode;
    switch (squad.mode) {
        case BOT_SQUAD_MODE_GATHER: {
            // Determine gather point
            std::vector<ivec2> gather_points;
            std::vector<uint32_t> units_near_gather_point;

            // Search for and score gather points
            for (EntityId entity_id : squad.entities) {
                const Entity& entity = state.entities.get_by_id(entity_id);
                // Special case for bunkers
                if (entity_is_building(entity.type)) {
                    continue;
                }

                int nearest_gather_point_index = -1;
                for (int gather_point_index = 0; gather_point_index < gather_points.size(); gather_point_index++) {
                    int gather_point_distance = ivec2::manhattan_distance(entity.cell, gather_points[gather_point_index]);
                    if (gather_point_distance > SQUAD_GATHER_DISTANCE) {
                        continue;
                    }
                    if (nearest_gather_point_index == -1 || 
                            gather_point_distance < ivec2::manhattan_distance(entity.cell, gather_points[nearest_gather_point_index])) {
                        nearest_gather_point_index = gather_point_index;
                    }
                }
                if (nearest_gather_point_index == -1) {
                    gather_points.push_back(entity.cell);
                    units_near_gather_point.push_back(1);
                } else {
                    units_near_gather_point[nearest_gather_point_index]++;
                }
            }

            GOLD_ASSERT(!gather_points.empty());

            // Choose the highest gather point
            uint32_t highest_scored_gather_point_index = 0;
            for (uint32_t gather_point_index = 1; gather_point_index < gather_points.size(); gather_point_index++) {
                if (units_near_gather_point[gather_point_index] > units_near_gather_point[highest_scored_gather_point_index]) {
                    highest_scored_gather_point_index = gather_point_index;
                }
            }

            squad.target_cell = gather_points[highest_scored_gather_point_index];
            break;
        }
        case BOT_SQUAD_MODE_ATTACK: {
            // Determine target attack cell
            EntityId nearest_enemy_building_id = bot_find_best_entity(state, 
                // Filter
                [&state, &bot](const Entity& entity, EntityId entity_id) {
                    return state.players[entity.player_id].team != state.players[bot.player_id].team &&
                            entity_is_building(entity.type) &&
                            entity_is_selectable(entity);
                },
                // Compare
                [&squad](const Entity& a, const Entity& b) {
                    return ivec2::manhattan_distance(a.cell, squad.target_cell) < 
                            ivec2::manhattan_distance(b.cell, squad.target_cell);
                });

            if (nearest_enemy_building_id == ID_NULL) {
                bot_squad_set_mode(state, bot, squad, BOT_SQUAD_MODE_DISSOLVED);
                return;
            }

            ivec2 base_center;
            bot_get_base_info(state, nearest_enemy_building_id, &base_center, NULL, NULL);

            squad.target_cell = base_center;
            log_trace("BOT: entered attack mode with squad, cell %vi", &squad.target_cell);
            break;
        }
        case BOT_SQUAD_MODE_DISSOLVED: {
            for (EntityId entity_id : squad.entities) {
                uint32_t entity_index = state.entities.get_index_of(entity_id);
                if (entity_index != INDEX_INVALID && !state.entities[entity_index].garrisoned_units.empty()) {
                    MatchInput input;
                    if (state.entities[entity_index].type == ENTITY_WAGON) {
                        input.type = MATCH_INPUT_MOVE_UNLOAD;
                        input.move.shift_command = 0;
                        input.move.target_id = ID_NULL;
                        input.move.target_cell = state.entities[entity_index].cell;
                        input.move.entity_count = 1;
                        input.move.entity_ids[0] = entity_id;
                    } else {
                        input.type = MATCH_INPUT_UNLOAD;
                        input.unload.carrier_count = 1;
                        input.unload.carrier_ids[0] = entity_id;
                    }
                    bot.inputs.push_back(input);
                }
                bot_release_entity(bot, entity_id);
            }
            break;
        }
        default:
            break;
    }
}

void bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad) {
    // Check the squad entity list for any entities which have died
    uint32_t squad_entity_index = 0;
    while (squad_entity_index < squad.entities.size()) {
        uint32_t entity_index = state.entities.get_index_of(squad.entities[squad_entity_index]);
        if (entity_index == INDEX_INVALID || state.entities[entity_index].health == 0 ||
                (squad.mode == BOT_SQUAD_MODE_ATTACK && state.entities[entity_index].type == ENTITY_WAGON && state.entities[entity_index].garrisoned_units.empty())) {
            bot_release_entity(bot, squad.entities[squad_entity_index]);
            squad.entities[squad_entity_index] = squad.entities[squad.entities.size() - 1];
            squad.entities.pop_back();
        } else {
            squad_entity_index++;
        }
    }

    // If we have no more entities after that, then dissolve the squad
    if (squad.entities.empty()) {
        bot_squad_set_mode(state, bot, squad, BOT_SQUAD_MODE_DISSOLVED);
        return;
    }

    // Update squad behavior
    switch (squad.mode) {
        case BOT_SQUAD_MODE_GATHER: {
            bool entities_have_reached_gather_point = true;

            std::queue<EntityId> entities_to_move;
            for (EntityId entity_id : squad.entities) {
                const Entity& entity = state.entities.get_by_id(entity_id);
                // Special case for bunkers
                if (entity_is_building(entity.type)) {
                    continue;
                }

                // Check if entity has reached gather point
                if (ivec2::manhattan_distance(entity.cell, squad.target_cell) > SQUAD_GATHER_DISTANCE) {
                    entities_have_reached_gather_point = false;
                    // If entity is not on the way to gather point, add it to the list of entities to move
                    if (!(entity.target.type == TARGET_CELL && ivec2::manhattan_distance(entity.target.cell, squad.target_cell) < SQUAD_GATHER_DISTANCE)) {
                        entities_to_move.push(entity_id);
                    }
                }
            }

            // If there are any entities to move, move them
            while (!entities_to_move.empty()) {
                MatchInput input;
                input.type = MATCH_INPUT_MOVE_CELL;
                input.move.shift_command = 0;
                input.move.target_id = ID_NULL;
                input.move.target_cell = squad.target_cell;
                input.move.entity_count = 0;
                while (!entities_to_move.empty() && input.move.entity_count < SELECTION_LIMIT) {
                    input.move.entity_ids[input.move.entity_count] = entities_to_move.front();
                    input.move.entity_count++;
                    entities_to_move.pop();
                }
                bot.inputs.push_back(input);
            }

            if (entities_have_reached_gather_point) {
                bot_squad_set_mode(state, bot, squad, BOT_SQUAD_MODE_GARRISON);
            }
            break;
        }
        case BOT_SQUAD_MODE_GARRISON: {
            // Determine if entities need to garrison
            std::queue<EntityId> infantry;
            std::queue<EntityId> carriers;
            bool all_infantry_are_garrisoned = true;
            for (EntityId entity_id : squad.entities) {
                const Entity& entity = state.entities.get_by_id(entity_id);
                const EntityData& entity_data = entity_get_data(entity.type);

                // Add carrier to the list but only if it has capacity for more units
                if (entity_data.garrison_capacity != 0 && entity.garrisoned_units.size() < entity_data.garrison_capacity) {
                    carriers.push(entity_id);
                // Add infantry to the list but only if it is not garrisoned
                } else if (entity_data.garrison_size != ENTITY_CANNOT_GARRISON && entity.garrison_id == ID_NULL) {
                    all_infantry_are_garrisoned = false;
                    // Only add this infantry to the list if it is not targeting an entity
                    // if it is targeting an entity, we assume it's already being garrisoned
                    if (entity.target.type != TARGET_ENTITY) {
                        infantry.push(entity_id);
                    }
                }
            }

            // If there's no more garrisoning to do, then attack
            if (carriers.empty() || all_infantry_are_garrisoned) {
                bot_squad_set_mode(state, bot, squad, BOT_SQUAD_MODE_ATTACK);
                break;
            }

            // Otherwise, send garrison inputs
            while (!carriers.empty() && !infantry.empty()) {
                EntityId carrier_id = carriers.front();
                carriers.pop();
                const Entity& carrier = state.entities.get_by_id(carrier_id);
                const uint32_t carrier_capacity = entity_get_data(carrier.type).garrison_capacity;

                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ENTITY;
                input.move.shift_command = 0;
                input.move.target_id = carriers.front();
                input.move.target_cell = ivec2(0, 0);
                input.move.entity_count = 0;

                while (!infantry.empty() && input.move.entity_count + carrier.garrisoned_units.size() < carrier_capacity) {
                    input.move.entity_ids[input.move.entity_count] = infantry.front();
                    input.move.entity_count++;
                    infantry.pop();
                }

                GOLD_ASSERT(input.move.entity_count != 0);
                bot.inputs.push_back(input);
            }

            break;
        }
        case BOT_SQUAD_MODE_ATTACK: {
            static const int ATTACK_SIGHT_RADIUS = 16;

            MatchInput attack_move_input;
            attack_move_input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
            attack_move_input.move.shift_command = 0;
            attack_move_input.move.target_id = ID_NULL;
            attack_move_input.move.target_cell = squad.target_cell;
            attack_move_input.move.entity_count = 0;

            // Manage attack targets for each entity
            for (EntityId entity_id : squad.entities) {
                const Entity& entity = state.entities.get_by_id(entity_id);
                // Garrisoned unit case
                if (!entity_is_selectable(entity)) {
                    continue;
                }
                // Special case for bunkers
                if (!entity_is_unit(entity.type)) {
                    continue;
                }

                // Determine what the target would be if we were to switch targets
                Target attack_target = match_entity_target_nearest_enemy(state, entity);
                if (attack_target.type == TARGET_ATTACK_ENTITY) {
                    const Entity& attack_target_entity = state.entities.get_by_id(attack_target.id);

                    bool should_change_targets = false;
                    if (entity.target.type == TARGET_ATTACK_ENTITY) {
                        uint32_t existing_target_index = state.entities.get_index_of(entity.target.id);
                        // If we're attacking an entity, but that entity no longer exists, change targets
                        if (existing_target_index == INDEX_INVALID) {
                            should_change_targets = true;
                        // If we're attacking an entity, but that entity has a lower attack priority than this new entity, change targets
                        } else {
                            const Entity& existing_target_entity = state.entities[existing_target_index];
                            if (entity_get_data(existing_target_entity.type).attack_priority < entity_get_data(attack_target_entity.type).attack_priority) {
                                should_change_targets = true;
                            }
                        }
                    // If we're throwing a molotov or unloading, then change targets only if our current target cell is too far away from the new target
                    // This is because we generally want to allow molotov / unload actions to finish, but if such an action is targeting a base far away,
                    // then we want to refocus our efforts on the more immediate danger of this nearby target
                    } else if ((entity.target.type == TARGET_MOLOTOV || entity.target.type == TARGET_UNLOAD)) {
                        if (ivec2::manhattan_distance(entity.target.cell, attack_target_entity.cell) > 2 * ATTACK_SIGHT_RADIUS) {
                            should_change_targets = true;
                        }
                    // Finally, all other target types will require a target change
                    } else {
                        should_change_targets = true;
                    }

                    if (should_change_targets && entity_get_data(entity.type).unit_data.damage != 0) {
                        // I'm kind of concerned about how many inputs this could result in
                        MatchInput attack_input;
                        attack_input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
                        attack_input.move.shift_command = 0;
                        attack_input.move.target_cell = ivec2(0, 0);
                        attack_input.move.target_id = attack_target.id;
                        attack_input.move.entity_count = 1;
                        attack_input.move.entity_ids[0] = entity_id;
                        bot.inputs.push_back(attack_input);
                    } else if (should_change_targets && !entity.garrisoned_units.empty()) {
                        log_trace("BOT: should change targets unload input");
                        MatchInput unload_input;
                        unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
                        unload_input.move.shift_command = 0;
                        unload_input.move.target_cell = attack_target_entity.cell;
                        unload_input.move.target_id = ID_NULL;
                        unload_input.move.entity_count = 1;
                        unload_input.move.entity_ids[0] = entity_id;
                        bot.inputs.push_back(unload_input);
                    }
                    // TODO: molotov input

                    continue;
                } // End if attack_target type is TARGET_ATTACK_ENTITY

                // If we reached here, it means there was no attack target
                if (ivec2::manhattan_distance(entity.cell, squad.target_cell) > ATTACK_SIGHT_RADIUS &&
                        !(entity.target.type == TARGET_ATTACK_CELL && ivec2::manhattan_distance(entity.target.cell, squad.target_cell) < ATTACK_SIGHT_RADIUS)) {
                    log_trace("BOT: No attack target, giving A move.");
                    attack_move_input.move.entity_ids[attack_move_input.move.entity_count] = entity_id;
                    attack_move_input.move.entity_count++;
                    if (attack_move_input.move.entity_count == SELECTION_LIMIT) {
                        bot.inputs.push_back(attack_move_input);
                        attack_move_input.move.entity_count = 0;
                    }
                }
            } // End for each entity in squad

            if (attack_move_input.move.entity_count != 0) {
                bot.inputs.push_back(attack_move_input);
            }

            break;
        }
        case BOT_SQUAD_MODE_DISSOLVED:
            GOLD_ASSERT(false);
    }
}

// Entity management

EntityId bot_find_entity(const MatchState& state, std::function<bool(const Entity&, EntityId)> filter_fn) {
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (filter_fn(entity, entity_id)) {
            return entity_id;
        }
    }

    return ID_NULL;
}

EntityId bot_find_best_entity(const MatchState& state, std::function<bool(const Entity&,EntityId)> filter_fn, std::function<bool(const Entity&, const Entity&)> compare_fn) {
    uint32_t best_entity_index = INDEX_INVALID;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        if (!filter_fn(entity, entity_id)) {
            continue;
        }
        if (best_entity_index == INDEX_INVALID || compare_fn(entity, state.entities[best_entity_index])) {
            best_entity_index = entity_index;
        }
    }

    if (best_entity_index == INDEX_INVALID) {
        return ID_NULL;
    }
    return state.entities.get_id_of(best_entity_index);
}

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell) {
    return bot_find_best_entity(state, 
        // Filter
        [&bot](const Entity& entity, EntityId entity_id) {
            return entity.type == ENTITY_MINER &&
                    entity.player_id == bot.player_id &&
                    entity.mode == MODE_UNIT_IDLE && 
                    entity.target.type == TARGET_NONE &&
                    !bot_is_entity_reserved(bot, entity_id);
        },
        // Compare
        [&cell](const Entity& a, const Entity& b) {
            return ivec2::manhattan_distance(a.cell, cell) < 
                    ivec2::manhattan_distance(a.cell, cell);
        });
}

EntityId bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id) {
    return bot_find_entity(state, 
        // Filter
        [&bot_player_id, &state, &goldmine_id](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot_player_id && 
                    entity_is_selectable(entity) &&
                    match_is_entity_mining(state, entity) &&
                    entity.gold_mine_id == goldmine_id;
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
    return bot_pull_worker_off_gold(state, bot.player_id, mine_target.id);
}

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

// Helpers

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

    for (const MatchInput& input : bot.inputs) {
        uint32_t gold_cost = 0;
        if (input.type == MATCH_INPUT_BUILD) {
            gold_cost = entity_get_data((EntityType)input.build.building_type).gold_cost;
        } else if (input.type == MATCH_INPUT_BUILDING_ENQUEUE && input.building_enqueue.item_type == BUILDING_QUEUE_ITEM_UNIT) {
            gold_cost = entity_get_data((EntityType)input.building_enqueue.item_subtype).gold_cost;
        } else if (input.type == MATCH_INPUT_BUILDING_ENQUEUE && input.building_enqueue.item_type == BUILDING_QUEUE_ITEM_UPGRADE) {
            gold_cost = upgrade_get_data(input.building_enqueue.item_subtype).gold_cost;
        }
        effective_gold = effective_gold > gold_cost ? effective_gold - gold_cost : 0;
    }

    return effective_gold;
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

ivec2 bot_find_hall_location(const MatchState& state, uint32_t existing_hall_index) {
    // Find the unoccupied goldmine nearest to the existing hall
    uint32_t nearest_goldmine_index = INDEX_INVALID;
    for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
        if (state.entities[goldmine_index].type != ENTITY_GOLDMINE || state.entities[goldmine_index].gold_held == 0) {
            continue;
        }
        // Take the block building rect and expand it by one
        // Then check all the cells, and if there's a building in one,
        // then we can consider this mine occupied and ignore it
        
        // This does mean that players can just "claim" all the mines by building a house next to them
        Rect building_block_rect = entity_goldmine_get_block_building_rect(state.entities[goldmine_index].cell);
        building_block_rect.y--;
        building_block_rect.x--;
        building_block_rect.w += 2;
        building_block_rect.h += 2;
        bool is_mine_unoccupied = true;
        for (int y = building_block_rect.y; y < building_block_rect.y + building_block_rect.h; y++) {
            ivec2 cell = ivec2(building_block_rect.x, y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }
            if (map_get_cell(state.map, CELL_LAYER_GROUND, cell).type == CELL_BUILDING) {
                is_mine_unoccupied = false;
            }
            cell = ivec2(building_block_rect.x + building_block_rect.w - 1, y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }
            if (map_get_cell(state.map, CELL_LAYER_GROUND, cell).type == CELL_BUILDING) {
                is_mine_unoccupied = false;
            }
        }
        for (int x = building_block_rect.x; x < building_block_rect.x + building_block_rect.w; x++) {
            ivec2 cell = ivec2(x, building_block_rect.y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }
            if (map_get_cell(state.map, CELL_LAYER_GROUND, cell).type == CELL_BUILDING) {
                is_mine_unoccupied = false;
            }
            cell = ivec2(x, building_block_rect.y + building_block_rect.h - 1);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }
            if (map_get_cell(state.map, CELL_LAYER_GROUND, cell).type == CELL_BUILDING) {
                is_mine_unoccupied = false;
            }
        }

        if (!is_mine_unoccupied) {
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

    // Do find the hall location, we will search around the perimeter
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
            ivec2 mine_exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, state.entities[nearest_goldmine_index].cell, entity_get_data(ENTITY_GOLDMINE).cell_size, 1, rally_cell, true);

            GOLD_ASSERT(mine_exit_cell.x != -1);
            std::vector<ivec2> path;
            map_pathfind(state.map, CELL_LAYER_GROUND, mine_exit_cell, rally_cell, 1, &path, true);
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

void bot_get_base_info(const MatchState& state, EntityId base_id, ivec2* base_center, int* base_radius, uint32_t* landmine_count) {
    static const int SEARCH_MARGIN = 4;

    int base_min_x = state.map.width;
    int base_max_x = -1;
    int base_min_y = state.map.height;
    int base_max_y = -1;

    const Entity& base = state.entities.get_by_id(base_id);
    std::unordered_map<EntityId, bool> explored;
    explored[base_id] = true;
    std::queue<EntityId> frontier;
    frontier.push(base_id);

    while (!frontier.empty()) {
        EntityId next_id = frontier.front();
        frontier.pop();

        const Entity& entity = state.entities.get_by_id(next_id);
        int entity_cell_size = entity_get_data(entity.type).cell_size;

        if (entity.player_id != base.player_id || !entity_is_selectable(entity)) {
            continue;
        }

        base_min_x = std::min(base_min_x, entity.cell.x);
        base_max_x = std::max(base_max_x, entity.cell.x + entity_cell_size);
        base_min_y = std::min(base_min_y, entity.cell.y);
        base_max_y = std::max(base_max_y, entity.cell.y + entity_cell_size);

        for (int y = entity.cell.y - SEARCH_MARGIN; y < entity.cell.y + entity_cell_size + SEARCH_MARGIN; y++) {
            for (int x = entity.cell.x - SEARCH_MARGIN; x < entity.cell.x + entity_cell_size + SEARCH_MARGIN; x++) {
                ivec2 cell = ivec2(x, y);
                if (!map_is_cell_in_bounds(state.map, cell)) {
                    continue;
                }
                Cell map_cell = map_get_cell(state.map, CELL_LAYER_GROUND, cell);
                if (map_cell.type == CELL_BUILDING && explored.find(map_cell.id) == explored.end()) {
                    explored[map_cell.id] = true;
                    frontier.push(map_cell.id);
                }
            }
        }
    }

    int base_x_radius = (base_max_x - base_min_x) / 2;
    int base_y_radius = (base_max_y - base_min_y) / 2;
    if (base_center != NULL) {
        *base_center = ivec2(base_min_x + base_x_radius, base_min_y + base_y_radius);
    }
    if (base_radius != NULL) {
        *base_radius = std::max(base_x_radius, base_y_radius) + SEARCH_MARGIN;
    }

    if (landmine_count != NULL) {
        *landmine_count = 0;
        for (int y = base_min_y - SEARCH_MARGIN; y < base_max_y + SEARCH_MARGIN; y++) {
            for (int x = base_min_x - SEARCH_MARGIN; x < base_max_x + SEARCH_MARGIN; x++) {
                ivec2 cell = ivec2(x, y);
                if (!map_is_cell_in_bounds(state.map, cell)) {
                    continue;
                }
                if (map_get_cell(state.map, CELL_LAYER_UNDERGROUND, cell).type == CELL_BUILDING) {
                    (*landmine_count)++;
                }
            }
        }
    }
}

void bot_get_circle_draw(int x_center, int y_center, int x, int y, std::vector<ivec2>& points) {
    points.push_back(ivec2(x_center + x, y_center + y));
    points.push_back(ivec2(x_center - x, y_center + y));
    points.push_back(ivec2(x_center + x, y_center - y));
    points.push_back(ivec2(x_center - x, y_center - y));

    points.push_back(ivec2(x_center + y, y_center + x));
    points.push_back(ivec2(x_center - y, y_center + x));
    points.push_back(ivec2(x_center + y, y_center - x));
    points.push_back(ivec2(x_center - y, y_center - x));
}

std::vector<ivec2> bot_get_cell_circle(ivec2 center, int radius) {
    // Using Besenham's Circle 
    // https://www.geeksforgeeks.org/c/bresenhams-circle-drawing-algorithm/
    std::vector<ivec2> circle_points;
    // Guessing we will have circumference, 2PI*R points on the circle, 7 is 2PI rounded up
    circle_points.reserve(7 * radius);
    int x = 0;
    int y = radius;
    int d = 3 - (2 * radius);
    bot_get_circle_draw(center.x, center.y, x, y, circle_points);
    while (y >= x) {
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + (4 * x) + 6;
        }

        x++;

        bot_get_circle_draw(center.x, center.y, x, y, circle_points);
    }

    return circle_points;
}

ivec2 bot_get_best_rally_point(const MatchState& state, EntityId building_id) {
    ivec2 base_center;
    int base_radius;
    bot_get_base_info(state, building_id, &base_center, &base_radius, NULL);
    std::vector<ivec2> circle_points = bot_get_cell_circle(base_center, base_radius);

    const Entity& building = state.entities.get_by_id(building_id);
    EntityId nearest_enemy_id = bot_find_best_entity(state, 
        // Filter
        [&state, &building](const Entity& entity, EntityId entity_id) {
            return entity.player_id != PLAYER_NONE &&
                        state.players[entity.player_id].team != state.players[building.player_id].team && 
                        entity_is_selectable(entity);
        },
        // Compare
        [&base_center](const Entity& a, const Entity& b) {
            return ivec2::manhattan_distance(a.cell, base_center) < ivec2::manhattan_distance(b.cell, base_center);
        });

    // If there are no enemies, then really doesn't matter where we rally to
    if (nearest_enemy_id == ID_NULL) {
        return ivec2(-1, -1);
    }

    ivec2 nearest_enemy_cell = state.entities.get_by_id(nearest_enemy_id).cell;
    std::vector<ivec2> path;
    map_pathfind(state.map, CELL_LAYER_GROUND, building.cell + ivec2(-1, 0), nearest_enemy_cell + ivec2(-1, 0), 1, &path, false);
    if (path.empty()) {
        return ivec2(-1, -1);
    }

    int best_path_index = 0;
    while (best_path_index < path.size() && ivec2::manhattan_distance(path[best_path_index], base_center) < base_radius) {
        best_path_index++;
    }

    int best_circle_point_index = -1;
    for (int circle_point_index = 0; circle_point_index < circle_points.size(); circle_point_index++) {
        ivec2 point = circle_points[circle_point_index];
        if (!map_is_cell_rect_in_bounds(state.map, point - ivec2(1, 1), 3)) {
            continue;
        }

        bool is_circle_point_valid = true;
        for (int y = point.y - 1; y < point.y + 2; y++) {
            for (int x = point.x - 1; x < point.x + 2; x++) {
                if (map_is_tile_ramp(state.map, ivec2(x, y)) || 
                        map_get_tile(state.map, ivec2(x, y)).elevation != map_get_tile(state.map, building.cell).elevation ||
                        map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y)).type == CELL_BUILDING ||
                        map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y)).type == CELL_GOLDMINE) {
                    is_circle_point_valid = false;
                }
            }
        }
        if (!is_circle_point_valid) {
            continue;
        }

        if (best_circle_point_index == -1 ||
                ivec2::manhattan_distance(circle_points[circle_point_index], path[best_path_index]) <
                ivec2::manhattan_distance(circle_points[best_circle_point_index], path[best_path_index])) {
            best_circle_point_index = circle_point_index;
        }
    }

    if (best_circle_point_index == -1) {
        return ivec2(-1, -1);
    }

    return cell_center(circle_points[best_circle_point_index]).to_ivec2();
}
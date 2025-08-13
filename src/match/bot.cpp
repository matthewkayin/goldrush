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

    memset(bot.desired_entities, 0, sizeof(bot.desired_entities));
    bot.desired_upgrades = 0;

    bot.desired_entities[ENTITY_BANDIT] = 4;
    bot.desired_entities[ENTITY_WAGON] = 1;

    bot.scout_id = ID_NULL;

    return bot;
}

MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    MatchInput saturate_bases_input = bot_saturate_bases(state, bot);
    if (saturate_bases_input.type != MATCH_INPUT_NONE) {
        return saturate_bases_input;
    }

    // Get desired entities
    BotDesiredEntities desired_entities = bot_get_desired_entities(state, bot);

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

    // Check for production goal completion
    uint32_t desired_unit_count = 0;
    for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
        desired_unit_count += bot.desired_entities[entity_type];
    }
    if (bot_has_desired_entities(state, bot) ||
            (desired_unit_count != 0 && match_get_player_population(state, bot.player_id) >= 98)) {
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

    MatchInput rally_input = bot_set_rally_points(state, bot);
    if (rally_input.type != MATCH_INPUT_NONE) {
        return rally_input;
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

BotDesiredEntities bot_get_desired_entities(const MatchState& state, const Bot& bot) {
    if (bot_should_build_house(state, bot)) {
        return (BotDesiredEntities) {
            .unit = ENTITY_TYPE_COUNT,
            .building = ENTITY_HOUSE,
        };
    }

    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    memcpy(desired_entities, bot.desired_entities, sizeof(desired_entities));

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

    // TODO: add scout entity

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
        if (entity_count[entity_type] < bot.desired_entities[entity_type]) {
            return false;
        }
    }

    return true;
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

void bot_squad_create(const MatchState& state, Bot& bot) {
    BotSquad squad;

    // Reserve units
    uint32_t squad_entity_count[ENTITY_TYPE_COUNT];
    memset(squad_entity_count, 0, sizeof(squad_entity_count));
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        log_trace("BOT: desired entity %s desired count %u", entity_get_data((EntityType)entity_type).name, bot.desired_entities[entity_type]);
        while (squad_entity_count[entity_type] < bot.desired_entities[entity_type]) {
            // Search for an entity of this type
            EntityId entity_id = bot_find_entity((BotFindEntityParams) {
                .state = state, 
                .filter = [&bot, &entity_type](const Entity& entity, EntityId entity_id) {
                    return entity.player_id == bot.player_id &&
                                entity.type == entity_type &&
                                entity_is_selectable(entity) &&
                                !bot_is_entity_reserved(bot, entity_id);
                }
            });
            // If we don't find an entity, that should mean we just hit the population cap and could not make the squad as big as we wanted
            if (entity_id == ID_NULL) {
                bot.desired_entities[entity_type] = squad_entity_count[entity_type];
                continue;
            }

            if (entity_id == bot.scout_id) {
                bot.scout_id = ID_NULL;
            }
            bot_reserve_entity(bot, entity_id);
            squad.entities.push_back(entity_id);
            squad_entity_count[entity_type]++;
        }
        log_trace("BOT: squad entities of type is now %u", squad_entity_count[entity_type]);
    }

    log_trace("BOT: squad entity count %u", squad.entities.size());

    if (squad.entities.empty()) {
        log_trace("BOT: no entities in squad, abandoning...");
        return;
    }

    // TODO
    squad.type = BOT_SQUAD_ATTACK;
    squad.target_cell = state.entities.get_by_id(bot_find_entity((BotFindEntityParams) { .state = state, .filter = [&bot](const Entity& hall, EntityId hall_id) { return hall.type == ENTITY_HALL && hall.player_id != bot.player_id; } })).cell;

    bot.squads.push_back(squad);
}

void bot_squad_dissolve(const MatchState& state, Bot& bot, BotSquad& squad) {
    for (EntityId entity_id : squad.entities) {
        bot_release_entity(bot, entity_id);
    }
    squad.entities.clear();
}

MatchInput bot_squad_update(const MatchState& state, Bot& bot, BotSquad& squad) {
    // Check the squad entity list for any entities which have died
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

    // Garrison into carriers
    if (squad.type == BOT_SQUAD_DEFEND || squad.type == BOT_SQUAD_ATTACK) {
        // Count the list of infantry that want to garrison
        std::vector<EntityId> infantry_ids;
        for (EntityId infantry_id : squad.entities) {
            const Entity& infantry = state.entities.get_by_id(infantry_id);

            // Only take non-garrisoned infantry who are not garrisoning to a unit already
            if (infantry.garrison_id != ID_NULL || 
                    infantry.target.type == TARGET_ENTITY ||
                    entity_get_data(infantry.type).garrison_size != ENTITY_CANNOT_GARRISON) {
                continue;
            }

            // If attacking and this infantry is already close enough, then don't worry about garrisoning
            if (squad.type == BOT_SQUAD_ATTACK && 
                    ivec2::manhattan_distance(infantry.cell, squad.target_cell) > BOT_SQUAD_GATHER_DISTANCE * 2) {
                continue;
            }

            infantry_ids.push_back(infantry_id);
        }

        // If there are infantry who want to garrison, we're going to try to garrison them
        if (!infantry_ids.empty()) {
            // Then find a carrier who has room for more units
            for (EntityId carrier_id : squad.entities) {
                // Determine if entity is a carrier
                const Entity& carrier = state.entities.get_by_id(carrier_id);
                const uint32_t GARRISON_CAPACITY = entity_get_data(carrier.type).garrison_size;
                // This will also filter out non-carriers, because they will always have a 
                // garrison size of 0, matching their garrison capacity
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

                for (EntityId infantry_id : infantry_ids) {
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
            } // End for each carrier in squad
        // If there are no infantry who want to garrison, we're going to manage the carriers
        } else {
            // For each carrier
            for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entities.size(); squad_entity_index++) {
                EntityId carrier_id = squad.entities[squad_entity_index];
                const Entity& carrier = state.entities.get_by_id(carrier_id);
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

                // If this carrier has infantry en-route to it, check if we should move the carrier closer to them
                // Note that we continue along the for loop here if we do not decide to give an input to this carrier
                // This means the rest of the function will not apply until all the infantry have garrisoned inside the carrier
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

                // At this point, the carrier does not have any infantry en-route to it
                // If the carrier is empty, and no one is en-route to it, and there is no one to pick up,
                // then release the carrier from the army
                if (BOT_SQUAD_ATTACK && carrier.garrisoned_units.empty()) {
                    bot_release_entity(bot, carrier_id);
                    squad.entities[squad_entity_index] = squad.entities.back();
                    squad.entities.pop_back();

                    return bot_return_entity_to_nearest_hall(state, bot, carrier_id);
                }

                // If there are no infantry en-route and this is a defense squad, then just continue
                if (carrier.garrisoned_units.empty()) {
                    continue;
                }

                // If the carrier is near the target cell, then just unload where the carrier stands
                if (ivec2::manhattan_distance(carrier.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
                    MatchInput unload_input;
                    unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
                    unload_input.move.shift_command = 0;
                    unload_input.move.target_cell = carrier.cell;
                    unload_input.move.target_id = ID_NULL;
                    unload_input.move.entity_ids[0] = carrier_id;
                    unload_input.move.entity_count = 1;
                    return unload_input;
                }

                // If there are no infantry en-route and we have units, then 
                // drop them off at the target.
                // Except first make sure we haven't already told the carrier to do this
                if (carrier.target.type == TARGET_UNLOAD && 
                        ivec2::manhattan_distance(carrier.target.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
                    continue;
                }

                std::vector<ivec2> path_to_target;
                map_pathfind(state.map, CELL_LAYER_GROUND, carrier.cell, squad.target_cell, carrier_data.cell_size, &path_to_target, MAP_IGNORE_UNITS);

                // The path would probably only be empty is they were boxed in
                if (path_to_target.empty()) {
                    continue;
                }

                // Find the point of the path along the radius of the squad gather distance
                uint32_t path_index = path_to_target.size() - 1;
                while (path_index >= 0 && ivec2::manhattan_distance(path_to_target[path_index], squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
                    path_index--;
                }
                path_index = std::min(path_index + 1, (uint32_t)path_to_target.size() - 1);

                MatchInput unload_input;
                unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
                unload_input.move.shift_command = 0;
                unload_input.move.target_cell = path_to_target[path_index];
                unload_input.move.target_id = ID_NULL;
                unload_input.move.entity_ids[0] = carrier_id;
                unload_input.move.entity_count = 1;
                return unload_input;
            } // End for each carrier
        } // End if ungarrisoned infantry empty
    } // End if squad is defend or attack
    // End carrier management

    // Move distant infantry closer to target
    MatchInput move_input;
    move_input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    move_input.move.shift_command = 0;
    move_input.move.target_cell = squad.target_cell;
    move_input.move.target_id = ID_NULL;
    move_input.move.entity_count = 0;
    for (EntityId entity_id : squad.entities) {
        const Entity& entity = state.entities.get_by_id(entity_id);
        const EntityData& entity_data = entity_get_data(entity.type);

        // Filter out carriers
        if (entity_data.garrison_capacity != 0) {
            continue;
        }

        // If the infantry is not distant, then don't order movement
        if (ivec2::manhattan_distance(entity.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
            continue;
        } 
        // If the infantry is not close to the target, but is on its way, then don't order movement
        if (entity.target.type == TARGET_ENTITY ||
                (entity.target.type == TARGET_ATTACK_CELL &&
                ivec2::manhattan_distance(entity.target.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE)) {
            continue;
        }

        move_input.move.entity_ids[move_input.move.entity_count] = entity_id;
        move_input.move.entity_count++;
        if (move_input.move.entity_count == SELECTION_LIMIT) {
            break;
        }
    }

    if (move_input.move.entity_count != 0) {
        return move_input;
    }

    // See if we need to redirect targets of our infantry
    EntityId nearby_enemy_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &squad](const Entity& entity, EntityId entity_id) {
            return entity_is_unit(entity.type) &&
                    entity_is_selectable(entity) &&
                    state.players[entity.player_id].team != state.players[bot.player_id].team &&
                    ivec2::manhattan_distance(entity.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE;
        }
    });
    if (nearby_enemy_id != ID_NULL) {
        const Entity& nearby_enemy = state.entities.get_by_id(nearby_enemy_id);
        const EntityData& nearby_enemy_data = entity_get_data(nearby_enemy.type);

        // Create an input to a move to the enemy
        MatchInput attack_input;
        attack_input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
        attack_input.move.shift_command = 0;
        attack_input.move.target_cell = nearby_enemy.cell;
        attack_input.move.target_id = ID_NULL;
        attack_input.move.entity_count = 0;

        // Find infantry to a move
        for (EntityId entity_id : squad.entities) {
            const Entity& entity = state.entities.get_by_id(entity_id);
            const EntityData& entity_data = entity_get_data(entity.type);

            // Filter out non-units, non-infantry, and non-attackers
            if (!entity_is_unit(entity.type) ||
                     entity_data.garrison_capacity != 0 || 
                     entity_data.unit_data.damage == 0) {
                continue;
            }

            // Filter out units that aren't close to the target
            if (ivec2::manhattan_distance(entity.cell, squad.target_cell) > BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }

            // Filter out units that are already attacking someone important
            if (entity.target.type == TARGET_ATTACK_ENTITY) {
                uint32_t target_index = state.entities.get_index_of(entity.target.id);
                if (target_index != INDEX_INVALID && entity_get_data(state.entities[target_index].type).attack_priority >= nearby_enemy_data.attack_priority) {
                    continue;
                }
            }

            attack_input.move.entity_ids[attack_input.move.entity_count] = entity_id;
            attack_input.move.entity_count++;
            if (attack_input.move.entity_count == SELECTION_LIMIT) {
                break;
            }
        } // End for each infantry

        if (attack_input.move.entity_count != 0) {
            return attack_input;
        }
    } // End if nearby enemy id is not null

    return (MatchInput) { .type = MATCH_INPUT_NONE };
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

std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell) {
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
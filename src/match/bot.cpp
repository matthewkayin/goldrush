#include "bot.h"

#include "core/logger.h"

// TODO: this needs to be dynamic based on the size of the army
// Maybe we can do something like say that a unit has been gathered if it is
// close to the gather point or if it is nearby another unit who is also gathered?
static const int ARMY_GATHER_DISTANCE = 8;

Bot bot_init(uint8_t player_id) {
    Bot bot;

    bot.player_id = player_id;
    bot.desired_entities[ENTITY_SALOON] = 1;
    bot.desired_entities[ENTITY_BANDIT] = 4;

    // TODO: make it so that the army desired units somehow influences the bot desired entities
    // Maybe desired entities could simply be derivative of the desired units of the army?
    bot.army.mode = BOT_ARMY_MODE_RAISE;
    memset(bot.army.desired_units, 0, sizeof(bot.army.desired_units));
    bot.army.desired_units[ENTITY_BANDIT] = 4;

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

    // Saturate Bases
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

    // Decide if we need to make a house
    uint32_t future_max_population = match_get_player_max_population(state, bot.player_id);
    uint32_t future_population = match_get_player_population(state, bot.player_id);
    uint32_t house_count = 0;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot.player_id || !entity_is_selectable(entity)) {
            continue;
        }

        if ((entity.type == ENTITY_HALL || entity.type == ENTITY_HOUSE) && entity.mode == MODE_BUILDING_IN_PROGRESS) {
            if (entity.type == ENTITY_HOUSE) {
                house_count++;
            }
            future_max_population += 10;
        } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty() && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) {
            future_population += entity_get_data(entity.queue[0].unit_type).unit_data.population_cost;
        } else if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD && 
                        (entity.target.build.building_type == ENTITY_HOUSE || entity.target.build.building_type == ENTITY_HALL)) {
            if (entity.target.build.building_type == ENTITY_HOUSE) {
                house_count++;
            }
            future_max_population += 10;
        }
    }
    if (future_max_population < MATCH_MAX_POPULATION && (int)future_max_population - (int)future_population <= 1) {
        bot.desired_entities[ENTITY_HOUSE] = house_count + 1;
    } else {
        bot.desired_entities[ENTITY_HOUSE] = 0;
    }

    // Check all of our reserved builders to see if we should release one
    uint32_t reserved_builders_index = 0;
    while (reserved_builders_index < bot.reserved_builders.size()) {
        EntityId builder_id = bot.reserved_builders[reserved_builders_index];
        uint32_t entity_index = state.entities.get_index_of(builder_id);
        if (entity_index == INDEX_INVALID || 
                state.entities[entity_index].health == 0 ||
                state.entities[entity_index].target.type != TARGET_BUILD) {
            bot_release_entity(bot, builder_id);
            bot.reserved_builders[reserved_builders_index] = bot.reserved_builders[bot.reserved_builders.size() - 1];
            bot.reserved_builders.pop_back();
        } else {
            reserved_builders_index++;
        }
    }

    // Count existing and in-progress entities
    uint32_t entity_count[ENTITY_TYPE_COUNT];
    memset(entity_count, 0, sizeof(entity_count));
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot.player_id || 
                !entity_is_selectable(entity)) {
            continue;
        }

        entity_count[entity.type]++; 
        if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD) {
            entity_count[entity.target.build.building_type]++; 
        } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty() && entity.queue.front().type == BUILDING_QUEUE_ITEM_UNIT) {
            entity_count[entity.queue.front().unit_type]++;
        }
    }

    // Determine which building to make
    EntityType desired_building = ENTITY_TYPE_COUNT;
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (entity_count[entity_type] < bot.desired_entities[entity_type]) {
            desired_building = (EntityType)entity_type;
            break;
        }
    }
    // Create build input
    if (desired_building != ENTITY_TYPE_COUNT && 
            entity_is_building(desired_building) && 
            bot.effective_gold >= entity_get_data(desired_building).gold_cost) {
        MatchInput build_input = bot_create_build_input(state, bot, desired_building);
        if (build_input.type != MATCH_INPUT_NONE) {
            bot_reserve_entity(bot, build_input.build.entity_ids[0]);
            bot.reserved_builders.push_back(build_input.build.entity_ids[0]);
            bot.effective_gold -= entity_get_data(desired_building).gold_cost;
            inputs.push_back(build_input);
        }
    }

    // Determine which unit to make
    EntityType desired_unit = ENTITY_TYPE_COUNT;
    for (uint32_t entity_type = 0; entity_type < ENTITY_HALL; entity_type++) {
        if (entity_count[entity_type] < bot.desired_entities[entity_type]) {
            desired_unit = (EntityType)entity_type;
            break;
        }
    }
    // Create train unit input
    if (desired_unit != ENTITY_TYPE_COUNT &&
            entity_is_unit(desired_unit) &&
            bot.effective_gold >= entity_get_data(desired_unit).gold_cost) {
        MatchInput train_input = bot_create_train_unit_input(state, bot, desired_unit);
        if (train_input.type != MATCH_INPUT_NONE) {
            bot.effective_gold -= entity_get_data(desired_unit).gold_cost;
            inputs.push_back(train_input);
        }
    }

    // Find scout
    EntityId bot_scout_id = ID_NULL;
    for (uint32_t scout_index = 0; scout_index < state.entities.size(); scout_index++) {
        const Entity& scout = state.entities[scout_index];
        EntityId scout_id = state.entities.get_id_of(scout_index);
        if (scout.player_id != bot.player_id || scout.type != ENTITY_WAGON ||
                scout.mode != MODE_UNIT_IDLE || scout.target.type != TARGET_NONE ||
                !scout.garrisoned_units.empty() || bot_is_entity_reserved(bot, scout_id)) {
            continue;
        }
        bot_scout_id = scout_id;
        break;
    }
    if (bot_scout_id != ID_NULL) {
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
            scout_input.move.entity_ids[0] = bot_scout_id;
            inputs.push_back(scout_input);
        }
    }

    // Army management
    // Check army IDs and release any dead units
    uint32_t army_index = 0;
    while (army_index < bot.army.unit_ids.size()) {
        EntityId army_id = bot.army.unit_ids[army_index];
        uint32_t army_entity_index = state.entities.get_index_of(army_id);
        if (army_entity_index == INDEX_INVALID || 
                state.entities[army_entity_index].health == 0) {
            bot_release_entity(bot, bot.army.unit_ids[army_index]);
            bot.army.unit_ids[army_index] = bot.army.unit_ids[bot.army.unit_ids.size() - 1];
            bot.army.unit_ids.pop_back();
        } else {
            army_index++;
        }
    }
    switch (bot.army.mode) {
        case BOT_ARMY_MODE_RAISE: {
            // Count how much of the army we have currently raised
            uint32_t unit_count[ENTITY_HALL];
            memset(unit_count, 0, sizeof(unit_count));
            for (EntityId army_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(army_id);
                unit_count[entity.type]++;
            }

            // Check the list of entities for any units we could add to this army
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                const Entity& entity = state.entities[entity_index];
                if (entity.player_id != bot.player_id ||
                        !entity_is_selectable(entity) ||
                        !entity_is_unit(entity.type) ||
                        bot_is_entity_reserved(bot, state.entities.get_id_of(entity_index))) {
                    continue;
                }

                if (unit_count[entity.type] < bot.army.desired_units[entity.type]) {
                    GOLD_ASSERT(bot.army.desired_units[entity.type] != 0);
                    EntityId entity_id = state.entities.get_id_of(entity_index);
                    bot_reserve_entity(bot, entity_id);
                    bot.army.unit_ids.push_back(entity_id);
                    unit_count[entity.type]++;
                }
            }

            // Check if the army has finished being raised
            bool is_army_raised = true;
            for (uint32_t entity_type = 0; entity_type < ENTITY_HALL; entity_type++) {
                if (unit_count[entity_type] < bot.army.desired_units[entity_type]) {
                    is_army_raised = false;
                    break;
                }
            }
            if (!is_army_raised) {
                break;
            }

            // If we've reached here, it means that we have finished raising
            // our army, so switch army modes to GATHER

            // Count how many units we could need to carry if we were to use transports
            uint32_t carryable_count = 0;
            bool army_has_cannons = false;
            for (EntityId army_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(army_id);
                if (entity.type == ENTITY_CANNON) {
                    army_has_cannons = true;
                    break;
                }
                carryable_count += entity_get_data(entity.type).garrison_size;
            }

            // Decide if we have any transports to use
            std::vector<EntityId> available_transports;
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                const Entity& entity = state.entities[entity_index];
                EntityId entity_id = state.entities.get_id_of(entity_index);
                if (entity.player_id != bot.player_id || 
                        !(entity.type == ENTITY_WAGON || entity.type == ENTITY_WAR_WAGON) ||
                        !entity_is_selectable(entity) || bot_is_entity_reserved(bot, entity_id)) {
                    continue;
                }
                available_transports.push_back(entity_id);
            }

            // If we are using transports, add them to the army
            if (!army_has_cannons && carryable_count <= available_transports.size() * entity_get_data(ENTITY_WAGON).garrison_capacity) {
                uint32_t desired_transports = carryable_count / entity_get_data(ENTITY_WAGON).garrison_capacity;
                if (carryable_count % entity_get_data(ENTITY_WAGON).garrison_capacity != 0) {
                    desired_transports++;
                }
                for (uint32_t transport_index = 0; transport_index < desired_transports; transport_index++) {
                    bot_reserve_entity(bot, available_transports[transport_index]);
                    bot.army.unit_ids.push_back(available_transports[transport_index]);
                }
            }

            // Determine the army gather point
            std::vector<ivec2> gather_points;
            std::vector<uint32_t> gather_point_score;
            for (EntityId army_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(army_id);

                int nearest_gather_point = -1;
                for (int gather_index = 0; gather_index < gather_points.size(); gather_index++) {
                    if (ivec2::manhattan_distance(entity.cell, gather_points[gather_index]) > ARMY_GATHER_DISTANCE) {
                        continue;
                    }
                    if (nearest_gather_point == -1 ||
                            ivec2::manhattan_distance(entity.cell, gather_points[gather_index]) <
                            ivec2::manhattan_distance(entity.cell, gather_points[nearest_gather_point])) {
                        nearest_gather_point = gather_index;
                    }
                }

                if (nearest_gather_point != -1) {
                    gather_point_score[nearest_gather_point]++;
                } else {
                    gather_points.push_back(entity.cell);
                    gather_point_score.push_back(1);
                }
            }

            GOLD_ASSERT(!gather_points.empty());
            uint32_t highest_scored_gather_index = 0;
            for (uint32_t gather_index = 1; gather_index < gather_points.size(); gather_index++) {
                if (gather_point_score[gather_index] > gather_point_score[highest_scored_gather_index]) {
                    highest_scored_gather_index = gather_index;
                }
            }

            bot.army.mode = BOT_ARMY_MODE_GATHER;
            bot.army.gather_point = gather_points[highest_scored_gather_index];
            break;
        }
        case BOT_ARMY_MODE_GATHER: {
            // Move entities to the gather point
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_CELL;
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;
            input.move.target_cell = bot.army.gather_point;
            input.move.entity_count = 0;
            bool all_units_are_gathered = true;

            for (EntityId army_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(army_id);
                if (ivec2::manhattan_distance(entity.cell, bot.army.gather_point) < ARMY_GATHER_DISTANCE) {
                    continue;
                }
                if (entity.target.type == TARGET_CELL && ivec2::manhattan_distance(entity.target.cell, bot.army.gather_point) < ARMY_GATHER_DISTANCE) {
                    continue;
                }
                all_units_are_gathered = false;
                input.move.entity_ids[input.move.entity_count] = army_id;
                input.move.entity_count++;
                
                if (input.move.entity_count == SELECTION_LIMIT) {
                    inputs.push_back(input);
                    input.move.entity_count = 0;
                }
            }

            if (input.move.entity_count != 0) {
                inputs.push_back(input);
            }

            if (!all_units_are_gathered) {
                break;
            }

            // If all units are gathered, let's either move out or garrison the army
            bool army_has_cannons = false;
            uint32_t transport_count = 0;
            uint32_t carryable_count = 0;
            for (EntityId army_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(army_id);
                if (entity.type == ENTITY_CANNON) {
                    army_has_cannons = true;
                    break;
                }
                if (entity.type == ENTITY_WAGON || entity.type == ENTITY_WAR_WAGON) {
                    transport_count++;
                } else {
                    carryable_count += entity_get_data(entity.type).garrison_size;
                }
            }

            bot.army.gather_point = bot_army_determine_attack_point(state, bot);
            if (army_has_cannons || carryable_count > transport_count * entity_get_data(ENTITY_WAGON).garrison_capacity) {
                bot.army.mode = BOT_ARMY_MODE_ATTACK;
            } else {
                bot.army.mode = BOT_ARMY_MODE_GARRISON;
            }
            break;
        }
        case BOT_ARMY_MODE_GARRISON: {
            // Get a list of all available transports and units to put inside them
            std::vector<EntityId> available_transports;
            std::vector<EntityId> ungarrisoned_units;
            bool all_units_are_garrisoned = true;
            for (EntityId army_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(army_id);
                const EntityData& entity_data = entity_get_data(entity.type);

                if (entity_data.garrison_capacity > entity.garrisoned_units.size()) {
                    available_transports.push_back(army_id);
                } else if (entity_data.garrison_size != ENTITY_CANNOT_GARRISON && entity.garrison_id == ID_NULL) {
                    all_units_are_garrisoned = false;

                    // Check if this unit is already on the way to a transport
                    bool entity_is_already_garrisoning = false;
                    if (entity.target.type == TARGET_ENTITY) {
                        uint32_t target_index = state.entities.get_index_of(entity.target.id);
                        if (target_index != INDEX_INVALID && entity_get_data(state.entities[target_index].type).garrison_capacity > state.entities[target_index].garrisoned_units.size()) {
                            entity_is_already_garrisoning = true;
                        }
                    }

                    // If it's not, then add it to the list
                    if (!entity_is_already_garrisoning) {
                        ungarrisoned_units.push_back(army_id);
                    }
                }
            }

            if (all_units_are_garrisoned || available_transports.empty()) {
                bot.army.mode = BOT_ARMY_MODE_ATTACK;
                break;
            }

            // Load units into transports
            for (EntityId transport_id : available_transports) {
                // First determine how many units this transport is already carrying
                const Entity& transport = state.entities.get_by_id(transport_id);
                uint32_t transport_garrison_size = transport.garrisoned_units.size();
                for (EntityId unit_id : bot.army.unit_ids) {
                    const Entity& unit = state.entities.get_by_id(unit_id);
                    if (unit.target.type == TARGET_ENTITY && unit.target.id == transport_id) {
                        transport_garrison_size++;
                    }
                }

                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ENTITY;
                input.move.shift_command = 0;
                input.move.target_id = transport_id;
                input.move.target_cell = ivec2(0, 0);
                input.move.entity_count = 0;

                while (!ungarrisoned_units.empty() && input.move.entity_count + transport_garrison_size < entity_get_data(ENTITY_WAGON).garrison_capacity) {
                    int nearest_unit_index = -1;
                    for (uint32_t unit_index = 0; unit_index < ungarrisoned_units.size(); unit_index++) {
                        if (nearest_unit_index == -1 ||
                                (ivec2::manhattan_distance(transport.cell, state.entities.get_by_id(ungarrisoned_units[unit_index]).cell) <
                                 ivec2::manhattan_distance(transport.cell, state.entities.get_by_id(ungarrisoned_units[nearest_unit_index]).cell))) {
                            nearest_unit_index = unit_index;
                        }
                    }
                    GOLD_ASSERT(nearest_unit_index != -1);
                    input.move.entity_ids[input.move.entity_count] = ungarrisoned_units[nearest_unit_index];
                    input.move.entity_count++;

                    ungarrisoned_units[nearest_unit_index] = ungarrisoned_units[ungarrisoned_units.size() - 1];
                    ungarrisoned_units.pop_back();
                }

                if (input.move.entity_count != 0) {
                    inputs.push_back(input);
                }
            }

            break;
        }
        case BOT_ARMY_MODE_ATTACK: {
            // If there's any units away from the attack point, then move them
            MatchInput unload_input;
            unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
            unload_input.move.shift_command = 0;
            unload_input.move.target_id = ID_NULL;
            unload_input.move.target_cell = bot.army.gather_point;
            unload_input.move.entity_count = 0;
            MatchInput move_input = unload_input;
            move_input.type = MATCH_INPUT_MOVE_CELL;

            for (EntityId unit_id : bot.army.unit_ids) {
                const Entity& entity = state.entities.get_by_id(unit_id);

                // Don't do anything with a unit that isn't selectable
                // Unselectable units should only be garrisoned units in this case
                if (!entity_is_selectable(entity)) {
                    continue;
                }

                // Check if entity is at the attack point
                if (ivec2::manhattan_distance(entity.cell, bot.army.gather_point) >= ARMY_GATHER_DISTANCE) {
                    // If it is not, but it is on the way, then don't bother it
                    if ((entity.target.type == TARGET_CELL || entity.target.type == TARGET_UNLOAD) &&
                            ivec2::manhattan_distance(entity.target.cell, bot.army.gather_point) < ARMY_GATHER_DISTANCE) {
                        continue;
                    }

                    // Otherwise, send an input to move it closer
                    if (entity.garrisoned_units.empty()) {
                        move_input.move.entity_ids[move_input.move.entity_count] = unit_id;
                        move_input.move.entity_count++;
                        if (move_input.move.entity_count == SELECTION_LIMIT) {
                            inputs.push_back(move_input);
                            move_input.move.entity_count = 0;
                        }
                    } else {
                        unload_input.move.entity_ids[unload_input.move.entity_count] = unit_id;
                        unload_input.move.entity_count++;
                        if (unload_input.move.entity_count == SELECTION_LIMIT) {
                            inputs.push_back(unload_input);
                            unload_input.move.entity_count = 0;
                        }
                    }

                    break;
                }

                // At this point, entity is at the attack point and should be attacking
                // Let's check to make sure this unit is attacking the best target

                Target attack_target = match_entity_target_nearest_enemy(state, entity);
                if (attack_target.type == TARGET_ATTACK_ENTITY && 
                        entity.target.type == TARGET_ATTACK_ENTITY && 
                        entity_get_data(state.entities.get_by_id(attack_target.id).type).attack_priority > 
                        entity_get_data(state.entities.get_by_id(entity.target.id).type).attack_priority) {
                    MatchInput attack_input;
                    attack_input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
                    attack_input.move.shift_command = 0;
                    attack_input.move.target_id = attack_target.id;
                    attack_input.move.target_cell = ivec2(0, 0);
                    attack_input.move.entity_ids[0] = unit_id;
                    attack_input.move.entity_count = 1;
                    inputs.push_back(attack_input);
                }
            }

            if (move_input.move.entity_count != 0) {
                inputs.push_back(move_input);
            }
            if (unload_input.move.entity_count != 0) {
                inputs.push_back(unload_input);
            }

            break;
        }
    }
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

EntityType bot_get_building_type_which_trains_unit_type(EntityType unit_type) {
    switch (unit_type) {
        case ENTITY_MINER:
            return ENTITY_HALL;
        case ENTITY_COWBOY:
        case ENTITY_BANDIT:
            return ENTITY_SALOON;
        case ENTITY_SAPPER:
        case ENTITY_PYRO:
        case ENTITY_BALLOON:
            return ENTITY_BALLOON;
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

MatchInput bot_create_train_unit_input(const MatchState& state, const Bot& bot, EntityType unit_type) {
    EntityId building_id = ID_NULL;
    EntityType building_type = bot_get_building_type_which_trains_unit_type(unit_type);

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot.player_id ||
                entity.mode != MODE_BUILDING_FINISHED ||
                entity.type != building_type ||
                !entity.queue.empty()) {
            continue;
        }
        building_id = state.entities.get_id_of(entity_index);
        break;
    }

    if (building_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.building_id = building_id;
    input.building_enqueue.item_type = BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = unit_type;
    return input;
}

ivec2 bot_army_determine_attack_point(const MatchState& state, const Bot& bot) {
    int least_defended_hall_index = -1;
    uint32_t least_defended_hall_defense_score;

    for (int entity_index = 0; entity_index < (int)state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.type != ENTITY_HALL || !entity_is_selectable(entity) ||
                state.players[entity.player_id].team == state.players[bot.player_id].team) {
            continue;
        }

        uint32_t defense_score = 0;
        for (int y = entity.cell.y - 8; y < entity.cell.y + 8; y++) {
            for (int x = entity.cell.x - 8; x < entity.cell.x + 8; x++) {
                if (!map_is_cell_in_bounds(state.map, ivec2(x, y))) {
                    continue;
                }
                Cell cell = map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y));
                if (cell.type == CELL_UNIT || cell.type == CELL_BUILDING) {
                    const Entity& defending_entity = state.entities.get_by_id(cell.id);
                    if (!entity_is_selectable(defending_entity) ||
                            state.players[entity.player_id].team != state.players[defending_entity.player_id].team ||
                            (entity_is_building(defending_entity.type) && defending_entity.type != ENTITY_BUNKER)) {
                        continue;
                    }
                    defense_score++;
                }
            }
        }

        if (least_defended_hall_index == -1 || defense_score < least_defended_hall_defense_score) {
            least_defended_hall_index = entity_index;
            least_defended_hall_defense_score = defense_score;
        }
    }

    if (least_defended_hall_index != -1) {
        return state.entities[least_defended_hall_index].cell + ivec2(-1, -1);
    }

    for (const Entity& entity : state.entities) {
        if (state.players[entity.player_id].team == state.players[bot.player_id].team ||
                !entity_is_building(entity.type) || !entity_is_selectable(entity)) {
            return entity.cell + ivec2(-1, -1);
        }
    }

    return ivec2(-1, -1);
}
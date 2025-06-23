#include "bot.h"

#include "core/logger.h"

bool match_bot_should_build_house(const MatchState& state, uint8_t bot_player_id);
void match_bot_get_desired_entities(const MatchState& state, uint8_t bot_player_id, uint32_t* desired_entities);
void match_bot_get_entity_counts(const MatchState& state, uint8_t bot_player_id, uint32_t* entity_count);
MatchInput match_bot_get_build_input(const MatchState& state, uint8_t bot_player_id, EntityType building_type);
EntityType match_bot_get_building_type_which_trains_unit_type(EntityType unit_type);
MatchInput match_bot_get_train_unit_input(const MatchState& state, uint8_t bot_player_id, EntityType unit_type);
bool match_bot_should_attack(const MatchState& state, uint8_t bot_player_id, EntityId* army_ids, uint32_t* army_size);
bool match_bot_should_garrison_army_into_wagons(const MatchState& state, uint8_t bot_player_id, EntityId* army_ids, uint32_t army_size, EntityId* wagon_ids, uint32_t* wagon_size);

EntityId match_bot_pull_worker_off_gold(const MatchState& state, uint8_t bot_player_id, EntityId goldmine_id);
EntityId match_bot_get_nearest_idle_worker(const MatchState& state, uint8_t bot_player_id, ivec2 cell);
EntityId match_bot_find_builder(const MatchState& state, uint8_t bot_player_id, uint32_t near_hall_index);
uint32_t match_bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id);
ivec2 match_bot_find_building_location(const MatchState& state, uint8_t bot_player_id, ivec2 start_cell, int size);
uint32_t match_bot_get_effective_gold(const MatchState& state, uint8_t bot_player_id);
EntityId match_bot_get_idle_scout(const MatchState& state, uint8_t bot_player_id);
ivec2 match_bot_get_army_center(const MatchState& state, EntityId* army_ids, uint32_t army_size);
bool match_bot_should_unload_wagons(const MatchState& state, uint8_t bot_player_id, EntityId* wagon_ids, uint32_t* wagon_size);
ivec2 match_bot_get_attack_position(const MatchState& state, uint8_t bot_player_id);

void match_bot_get_turn_inputs(const MatchState& state, uint8_t bot_player_id, std::vector<MatchInput>& inputs) {
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
                inputs.push_back(input);
                return;
            }
        }
        // If undersaturated, put workers on gold
        if (miner_count < MATCH_MAX_MINERS_ON_GOLD) {
            EntityId idle_worker_id = match_bot_get_nearest_idle_worker(state, bot_player_id, hall.cell);
            if (idle_worker_id != ID_NULL) {
                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ENTITY;
                input.move.shift_command = 0;
                input.move.target_cell = ivec2(0, 0);
                input.move.target_id = goldmine_id;
                input.move.entity_count = 1;
                input.move.entity_ids[0] = idle_worker_id;
                inputs.push_back(input);
                return;
            }

            // If we're still here, then there were no idle workers
            // So we'll create one out of the town hall
            if (bot_effective_gold >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
                inputs.push_back((MatchInput) {
                    .type = MATCH_INPUT_BUILDING_ENQUEUE,
                    .building_enqueue = (MatchInputBuildingEnqueue) {
                        .building_id = state.entities.get_id_of(hall_index),
                        .item_type = BUILDING_QUEUE_ITEM_UNIT,
                        .item_subtype = ENTITY_MINER
                    }
                });
                return;
            }

            // If we're training a miner, rally to the mine
            if (!hall.queue.empty() && hall.rally_point.x == -1) {
                MatchInput rally_input;
                rally_input.type = MATCH_INPUT_RALLY;
                rally_input.rally.building_count = 1;
                rally_input.rally.building_ids[0] = state.entities.get_id_of(hall_index);
                rally_input.rally.rally_point = (state.entities.get_by_id(goldmine_id).cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
                inputs.push_back(rally_input);
                return;
            }
        }
    }

    // Build houses
    if (match_bot_should_build_house(state, bot_player_id)) {
        if (bot_effective_gold >= entity_get_data(ENTITY_HOUSE).gold_cost) {
            MatchInput build_input = match_bot_get_build_input(state, bot_player_id, ENTITY_HOUSE);
            if (build_input.type != MATCH_INPUT_NONE) {
                inputs.push_back(build_input);
                return;
            }
        } else {
            bot_effective_gold = 0;
        }
    }

    uint32_t desired_entities[ENTITY_TYPE_COUNT];
    match_bot_get_desired_entities(state, bot_player_id, desired_entities);
    uint32_t entity_count[ENTITY_TYPE_COUNT];
    match_bot_get_entity_counts(state, bot_player_id, entity_count);

    // Determine desired building
    EntityType desired_building_type = ENTITY_TYPE_COUNT;
    for (uint32_t building_type = ENTITY_HALL; building_type < ENTITY_TYPE_COUNT; building_type++) {
        if (entity_count[building_type] < desired_entities[building_type]) {
            desired_building_type = (EntityType)building_type;
            break;
        }
    }

    // Build buildings
    if (desired_building_type != ENTITY_TYPE_COUNT) {
        if (bot_effective_gold >= entity_get_data(desired_building_type).gold_cost) {
            MatchInput build_input = match_bot_get_build_input(state, bot_player_id, desired_building_type);
            if (build_input.type != MATCH_INPUT_NONE) {
                inputs.push_back(build_input);
                return;
            }
        } else {
            bot_effective_gold = 0;
        }
    }

    // Determine desired unit
    EntityType desired_unit_type = ENTITY_TYPE_COUNT;
    for (uint32_t unit_type = 0; unit_type < ENTITY_HALL; unit_type++) {
        if (entity_count[unit_type] < desired_entities[unit_type]) {
            desired_unit_type = (EntityType)unit_type;
            break;
        }
    }

    // Make units
    if (desired_unit_type != ENTITY_TYPE_COUNT) {
        if (bot_effective_gold >= entity_get_data(desired_unit_type).gold_cost) {
            MatchInput input = match_bot_get_train_unit_input(state, bot_player_id, desired_unit_type);
            if (input.type != MATCH_INPUT_NONE) {
                inputs.push_back(input);
                return;
            }
        } else {
            bot_effective_gold = 0;
        }
    }

    // Decide whether to attack
    EntityId army_ids[MATCH_MAX_POPULATION];
    uint32_t army_size;
    if (match_bot_should_attack(state, bot_player_id, army_ids, &army_size)) {
        EntityId wagon_ids[MATCH_MAX_POPULATION];
        uint32_t wagon_size;
        if (match_bot_should_garrison_army_into_wagons(state, bot_player_id, army_ids, army_size, wagon_ids, &wagon_size)) {
            // Move the wagons to the army center
            MatchInput wagon_move_input;
            wagon_move_input.type = MATCH_INPUT_MOVE_CELL;
            wagon_move_input.move.shift_command = 0;
            wagon_move_input.move.target_id = ID_NULL;
            wagon_move_input.move.target_cell = match_bot_get_army_center(state, army_ids, army_size);
            wagon_move_input.move.entity_count = (uint8_t)wagon_size;
            memcpy(wagon_move_input.move.entity_ids, wagon_ids, sizeof(EntityId) * wagon_size);
            inputs.push_back(wagon_move_input);

            // Move the army into the wagons
            uint32_t army_index = 0;
            uint32_t wagon_index = 0;
            while (wagon_index < wagon_size) {
                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ENTITY;
                input.move.shift_command = 0;
                input.move.target_cell = ivec2(0, 0);
                input.move.target_id = wagon_ids[wagon_index];
                input.move.entity_count = 0;

                uint32_t wagon_capacity = entity_get_data(ENTITY_WAGON).garrison_capacity;
                while (army_index < army_size && wagon_capacity > 0) {
                    uint32_t army_garrison_size = entity_get_data(state.entities.get_by_id(army_ids[army_index]).type).garrison_size;
                    if (army_garrison_size == ENTITY_CANNOT_GARRISON) {
                        army_index++;
                        continue;
                    }
                    input.move.entity_ids[input.move.entity_count] = army_ids[army_index];
                    army_index++;
                    input.move.entity_count++;
                    wagon_capacity -= army_garrison_size;
                }

                if (input.move.entity_count != 0) {
                    inputs.push_back(input);
                }

                wagon_index++;
            }
            return;
        } // End if should garrison wagons

        // If not garrison wagons, then just A-move
        // TODO: go back through and make sure we never send a move command with more than 20 units
        uint32_t army_index = 0;
        ivec2 target_cell = match_bot_get_attack_position(state, bot_player_id);
        if (target_cell.x != -1) {
            while (army_index < army_size) {
                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
                input.move.shift_command = 0;
                input.move.target_id = ID_NULL;
                input.move.target_cell = match_bot_get_attack_position(state, bot_player_id);

                uint32_t army_size_left = army_size - army_index;
                input.move.entity_count = (uint8_t)std::max(army_size_left, SELECTION_LIMIT);
                memcpy(input.move.entity_ids, army_ids + army_index, sizeof(EntityId) * input.move.entity_count);
                army_index += (uint32_t)input.move.entity_count;
                inputs.push_back(input);
            }

            return;
        }
    }

    // Wagon unload
    EntityId wagon_ids[MATCH_MAX_POPULATION];
    uint32_t wagon_size;
    if (match_bot_should_unload_wagons(state, bot_player_id, wagon_ids, &wagon_size)) {
        MatchInput input;
        input.type = MATCH_INPUT_MOVE_UNLOAD;
        input.move.shift_command = 0;
        input.move.target_id = ID_NULL;
        input.move.target_cell = match_bot_get_attack_position(state, bot_player_id);
        input.move.entity_count = (uint8_t)wagon_size;
        memcpy(input.move.entity_ids, wagon_ids, sizeof(EntityId) * wagon_size);

        if (input.move.target_cell.x != -1) {
            inputs.push_back(input);
            return;
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
                inputs.push_back(scout_input);
                return;
            }
        }
    }
}

bool match_bot_should_build_house(const MatchState& state, uint8_t bot_player_id) {
    uint32_t future_max_population = match_get_player_max_population(state, bot_player_id);
    uint32_t future_population = match_get_player_population(state, bot_player_id);
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot_player_id || !entity_is_selectable(entity)) {
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

void match_bot_get_desired_entities(const MatchState& state, uint8_t bot_player_id, uint32_t* desired_entities) {
    memset(desired_entities, 0, sizeof(uint32_t) * ENTITY_TYPE_COUNT);

    desired_entities[ENTITY_HALL] = 1;
    desired_entities[ENTITY_SALOON] = 1;
    desired_entities[ENTITY_BANDIT] = 4;
}

void match_bot_get_entity_counts(const MatchState& state, uint8_t bot_player_id, uint32_t* entity_count) {
    memset(entity_count, 0, sizeof(uint32_t) * ENTITY_TYPE_COUNT);
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot_player_id || !entity_is_selectable(entity)) {
            continue;
        }

        entity_count[entity.type]++;
        if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD) {
            entity_count[entity.target.build.building_type]++;
        } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty() && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) {
            entity_count[entity.queue[0].unit_type]++;
        }
    }
}

MatchInput match_bot_get_build_input(const MatchState& state, uint8_t bot_player_id, EntityType building_type) {
    uint32_t hall_index = match_bot_find_hall_index_with_least_nearby_buildings(state, bot_player_id);
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
    input.build.building_type = building_type;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = builder_id;

    return input;
}

EntityType match_bot_get_building_type_which_trains_unit_type(EntityType unit_type) {
    switch (unit_type) {
        case ENTITY_MINER:
            return ENTITY_HALL;
        case ENTITY_COWBOY:
        case ENTITY_BANDIT:
            return ENTITY_SALOON;
        case ENTITY_WAGON:
        case ENTITY_WAR_WAGON:
        case ENTITY_JOCKEY:
            return ENTITY_COOP;
        case ENTITY_SAPPER:
        case ENTITY_PYRO:
        case ENTITY_BALLOON:
            return ENTITY_WORKSHOP;
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

MatchInput match_bot_get_train_unit_input(const MatchState& state, uint8_t bot_player_id, EntityType unit_type) {
    // Determine the type of building required to train this unit
    EntityType building_type = match_bot_get_building_type_which_trains_unit_type(unit_type);
    if (building_type == ENTITY_TYPE_COUNT) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Find a building of that type
    EntityId building_id = ID_NULL;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot_player_id || entity.mode != MODE_BUILDING_FINISHED || 
                entity.type != building_type || !entity.queue.empty()) {
            continue;
        }

        building_id = state.entities.get_id_of(entity_index);
        break;
    }
    if (building_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Train the unit using the building
    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.building_id = building_id;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = unit_type;
    return input;
}

bool match_bot_should_attack(const MatchState& state, uint8_t bot_player_id, EntityId* army_ids, uint32_t* army_size) {
    // Determine the bots desired attacking army
    uint32_t desired_army[ENTITY_HALL];
    memset(desired_army, 0, sizeof(uint32_t) * ENTITY_HALL);
    desired_army[ENTITY_BANDIT] = 4;

    // If this bot doesn't want to attack, don't
    bool bot_desires_attack = false;
    for (uint32_t entity_type = 0; entity_type < ENTITY_HALL; entity_type++) {
        if (desired_army[entity_type] != 0) {
            bot_desires_attack = true;
            break;
        }
    }
    if (!bot_desires_attack) {
        return false;
    }

    // Build army list out of units available
    memset(army_ids, 0, sizeof(EntityId) * MATCH_MAX_POPULATION);
    *army_size = 0;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot_player_id || 
                !entity_is_selectable(entity) || 
                !entity_is_unit(entity.type) || 
                (entity.mode != MODE_UNIT_IDLE && entity.target.type != TARGET_NONE)) {
            continue;
        }

        // TODO: allow bot to sometimes attack with everything
        // TODO: allow bot to detect when a unit has been deployed already (maybe check proximity to town halls?)
        // TODO: And finally, don't attack if under attack
        if (desired_army[entity.type] > 0) {
            army_ids[*army_size] = state.entities.get_id_of(entity_index);
            (*army_size)++;
            desired_army[entity.type]--;
        }
    }

    // Determine if we have the army we want
    bool has_desired_army = true;
    for (uint32_t entity_type = 0; entity_type < ENTITY_HALL; entity_type++) {
        if (desired_army[entity_type] != 0) {
            has_desired_army = false;
            break;
        }
    }
    return has_desired_army;
}

bool match_bot_should_garrison_army_into_wagons(const MatchState& state, uint8_t bot_player_id, EntityId* army_ids, uint32_t army_size, EntityId* wagon_ids, uint32_t* wagon_size) {
    // Determine garrison size of army
    uint32_t army_garrison_size = 0;
    for (uint32_t army_index = 0; army_index < army_size; army_index++) {
        const Entity& entity = state.entities.get_by_id(army_ids[army_index]);
        if (entity.type == ENTITY_CANNON) {
            return false;
        }
        // If an entity cannot garrison, their garrison size is 0, so this code still works
        army_garrison_size += entity_get_data(entity.type).garrison_size;
    }

    uint32_t army_garrison_capacity = 0;
    memset(wagon_ids, 0, sizeof(EntityId) * MATCH_MAX_POPULATION);
    *wagon_size = 0;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        // We're not checking if it's idle here because it might be scouting
        if (entity.player_id != bot_player_id || !entity_is_selectable(entity) || 
                !(entity.type == ENTITY_WAGON || entity.type == ENTITY_WAR_WAGON) ||
                !entity.garrisoned_units.empty()) {
            continue;
        }

        wagon_ids[*wagon_size] = state.entities.get_id_of(entity_index);
        (*wagon_size)++;
        army_garrison_capacity += entity_get_data(entity.type).garrison_capacity;
    }

    return army_garrison_capacity >= army_garrison_size;
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

uint32_t match_bot_find_hall_index_with_least_nearby_buildings(const MatchState& state, uint8_t bot_player_id) {
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
            uint32_t gold_cost = entity_get_data(entity.target.build.building_type).gold_cost;
            gold = gold > gold_cost ? gold - gold_cost : 0;
        }
    }

    return gold;
}

EntityId match_bot_get_idle_scout(const MatchState& state, uint8_t bot_player_id) {
    for (uint32_t scout_index = 0; scout_index < state.entities.size(); scout_index++) {
        const Entity& scout = state.entities[scout_index];
        if (scout.player_id == bot_player_id && scout.type == ENTITY_WAGON && 
                scout.mode == MODE_UNIT_IDLE && scout.target.type == TARGET_NONE &&
                scout.garrisoned_units.empty()) {
            return state.entities.get_id_of(scout_index);
        }
    }

    return ID_NULL;
}

ivec2 match_bot_get_army_center(const MatchState& state, EntityId* army_ids, uint32_t army_size) {
    ivec2 first_unit_cell = state.entities.get_by_id(army_ids[0]).cell;
    int x_min = first_unit_cell.x;
    int x_max = first_unit_cell.x;
    int y_min = first_unit_cell.y;
    int y_max = first_unit_cell.y;

    for (uint32_t army_index = 1; army_index < army_size; army_index++) {
        ivec2 cell = state.entities.get_by_id(army_ids[army_index]).cell;
        x_min = std::min(x_min, cell.x);
        x_max = std::max(x_max, cell.x);
        y_min = std::min(y_min, cell.y);
        y_max = std::max(y_max, cell.y);
    }

    return ivec2(x_min + ((x_max - x_min) / 2), y_min + ((y_max - y_min) / 2));
}

bool match_bot_should_unload_wagons(const MatchState& state, uint8_t bot_player_id, EntityId* wagon_ids, uint32_t* wagon_size) {
    memset(wagon_ids, 0, sizeof(EntityId) * MATCH_MAX_POPULATION);
    *wagon_size = 0;

    // I'm going to make it so that it just sends any idle wagon with units, but we could
    // maybe modify this so that it waits until all the wagons for a given attack
    // are idle before sending them?

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot_player_id || !entity_is_selectable(entity) || 
                !(entity.type == ENTITY_WAGON || entity.type == ENTITY_WAR_WAGON) ||
                entity.garrisoned_units.empty() || entity.mode != MODE_UNIT_IDLE ||
                entity.target.type != TARGET_NONE) {
            continue;
        }
        wagon_ids[*wagon_size] = state.entities.get_id_of(entity_index);
        (*wagon_size)++;
    }

    return *wagon_size != 0;
}

ivec2 match_bot_get_attack_position(const MatchState& state, uint8_t bot_player_id) {
    // TODO: have bot use scouting information to determine this?
    uint32_t least_defended_hall_index = INDEX_INVALID;
    uint32_t least_defended_hall_defense_size = 0;

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.type != ENTITY_HALL || state.players[entity.player_id].team == state.players[bot_player_id].team) {
            continue;
        }

        // Determine defense size of hall
        uint32_t hall_defense_size = 0;
        for (int y = entity.cell.y - 16; y < entity.cell.y + entity_get_data(entity.type).cell_size + 16; y++) {
            for (int x = entity.cell.x - 16; x < entity.cell.x + entity_get_data(entity.type).cell_size + 16; x++) {
                if (!map_is_cell_in_bounds(state.map, ivec2(x, y))) {
                    continue;
                }
                // This code will end up counting cannons 4 times, which is fine
                Cell cell = map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y));
                if (cell.type == CELL_UNIT) {
                    const Entity& nearby_unit = state.entities.get_by_id(cell.id);
                    if (state.players[nearby_unit.player_id].team == state.players[entity.player_id].team) {
                        hall_defense_size++;
                    }
                } else if (cell.type == CELL_BUILDING) {
                    const Entity& nearby_building = state.entities.get_by_id(cell.id);
                    if (nearby_building.type == ENTITY_BUNKER && !nearby_building.garrisoned_units.empty()) {
                        hall_defense_size++;
                    }
                }
            } // End for each x
        } // End for each y

        if (least_defended_hall_index == INDEX_INVALID || hall_defense_size > least_defended_hall_defense_size) {
            least_defended_hall_index = entity_index;
            least_defended_hall_defense_size = hall_defense_size;
        }
    }

    if (least_defended_hall_index == INDEX_INVALID) {
        return ivec2(-1, -1);
    }

    return state.entities[least_defended_hall_index].cell + ivec2(0, -1);
}
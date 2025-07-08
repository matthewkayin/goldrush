#include "bot.h"

#include "core/logger.h"
#include "upgrade.h"

static const int ARMY_GATHER_DISTANCE = 8;
static const int ARMY_DEFEND_DISTANCE = 16;
static const uint32_t ARMY_DEFEND_LANDMINE_MAX = 6;

Bot bot_init(uint8_t player_id) {
    Bot bot;

    bot.player_id = player_id;
    bot.task = (BotTask) {
        .type = BOT_TASK_NONE
    };

    return bot;
};

void bot_get_turn_inputs(const MatchState& state, Bot& bot, std::vector<MatchInput>& inputs) {
    if (bot_task_is_finished(state, bot)) {
        log_trace("BOT: task is finished");
        if (bot.task.type == BOT_TASK_RAISE_ARMY) {
            log_trace("BOT: creating army");
            bot_army_create(state, bot);
        }
        bot.task = (BotTask) {
            .type = BOT_TASK_NONE
        };
    }
    if (bot.task.type == BOT_TASK_NONE) {
        if (bot.task_queue.empty()) {
            BotStrategy strategy = bot_get_best_strategy(state, bot);
            bot_task_enqueue_from_strategy(state, bot, strategy);
        } 

        bot.task = bot.task_queue.front();
        bot.task_queue.pop();
    }

    // Determine effective gold
    bot.effective_gold = state.players[bot.player_id].gold;
    for (const Entity& entity : state.entities) {
        if (entity.player_id == bot.player_id && 
                entity.type == ENTITY_MINER && 
                entity.mode != MODE_UNIT_BUILD &&
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

    // Determine desired entities
    EntityType desired_building = ENTITY_TYPE_COUNT;
    EntityType desired_unit = ENTITY_TYPE_COUNT;
    if (bot.task.type == BOT_TASK_EXPAND) {
        desired_building = ENTITY_HALL;
    } else if (bot_should_build_house(state, bot)) {
        desired_building = ENTITY_HOUSE;
    } else if (bot.task.type == BOT_TASK_RAISE_ARMY) {
        // Determine which entity to make
        uint32_t desired_entities[ENTITY_TYPE_COUNT];
        bot_get_desired_entities(state, bot, desired_entities);

        // Determine desired unit
        desired_unit = ENTITY_TYPE_COUNT;
        for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
            if (desired_entities[entity_type] != 0) {
                desired_unit = (EntityType)entity_type;
                break;
            }
        }

        // Determine desired building
        desired_building = ENTITY_TYPE_COUNT;
        for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
            if (desired_entities[entity_type] != 0) {
                desired_building = (EntityType)entity_type;
                break;
            }
        }
    }

    // Create train unit input
    if (desired_unit != ENTITY_TYPE_COUNT &&
            bot.effective_gold >= entity_get_data(desired_unit).gold_cost) {
        MatchInput train_input = bot_create_train_unit_input(state, bot, desired_unit);
        if (train_input.type != MATCH_INPUT_NONE) {
            bot.effective_gold -= entity_get_data(desired_unit).gold_cost;
            inputs.push_back(train_input);
            inputs.push_back(bot_create_building_rally_input(state, bot, train_input.building_enqueue.building_id));
        }
    }

    // Create build input
    if (desired_building != ENTITY_TYPE_COUNT && 
            bot.effective_gold >= entity_get_data(desired_building).gold_cost) {
        MatchInput build_input = bot_create_build_input(state, bot, desired_building);
        if (build_input.type != MATCH_INPUT_NONE) {
            bot_reserve_entity(bot, build_input.build.entity_ids[0]);
            bot.reserved_builders.push_back(build_input.build.entity_ids[0]);
            bot.effective_gold -= entity_get_data(desired_building).gold_cost;
            inputs.push_back(build_input);
        }
    }

    // Research upgrades
    if (bot.task.type == BOT_TASK_RESEARCH && bot.effective_gold >= upgrade_get_data(bot.task.research.upgrade).gold_cost) {
        MatchInput upgrade_input = bot_create_upgrade_input(state, bot, bot.task.research.upgrade);
        if (upgrade_input.type != MATCH_INPUT_NONE) {
            inputs.push_back(upgrade_input);
            bot.effective_gold -= upgrade_get_data(bot.task.research.upgrade).gold_cost;
        }
    }

    // Find scout
    EntityId bot_scout_id = bot_find_first_entity(state, [&bot](const Entity& entity, EntityId entity_id) {
        return entity.player_id == bot.player_id && entity.type == ENTITY_WAGON &&
                entity.mode == MODE_UNIT_IDLE && entity.target.type == TARGET_NONE &&
                entity.garrisoned_units.empty() && !bot_is_entity_reserved(bot, entity_id);
    });
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

    // Repair buildings
    EntityId on_fire_building_id = bot_find_first_entity(state, [&state, &bot](const Entity& building, EntityId building_id) {
        if (building.player_id != bot.player_id || !entity_check_flag(building, ENTITY_FLAG_ON_FIRE)) {
            return false;
        }
        EntityId existing_repairer_id = bot_find_first_entity(state, [&building_id](const Entity& unit, EntityId unit_id) {
            return unit.target.type == TARGET_REPAIR && unit.target.id == building_id;
        });
        if (existing_repairer_id != ID_NULL) {
            return false;
        }
        return true;
    });
    if (on_fire_building_id != ID_NULL) {
        MatchInput repair_input = bot_create_repair_building_input(state, bot, on_fire_building_id);
        if (repair_input.type != MATCH_INPUT_NONE) {
            bot_reserve_entity(bot, repair_input.move.entity_ids[0]);
            inputs.push_back(repair_input);
            log_trace("BOT: repairing building with id %u and unit id %u", repair_input.move.target_id, repair_input.move.entity_ids[0]);
        }
    }

    // Release idle repairers
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.player_id == bot.player_id && entity.type == ENTITY_MINER &&
                bot_is_entity_reserved(bot, entity_id) && entity.target.type == TARGET_NONE) {
            bot_release_entity(bot, entity_id);
        }
    }

    uint32_t army_index = 0;
    while (army_index < bot.armies.size()) {
        bot_army_update(state, bot, bot.armies[army_index], inputs);
        if (bot.armies[army_index].mode == BOT_ARMY_MODE_DISSOLVED) {
            for (EntityId unit_id : bot.armies[army_index].units) {
                const Entity& entity = state.entities.get_by_id(unit_id);
                if (entity.type == ENTITY_WAGON && !entity.garrisoned_units.empty()) {
                    MatchInput unload_input;
                    unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
                    unload_input.move.shift_command = 0;
                    unload_input.move.target_id = ID_NULL;
                    unload_input.move.target_cell = entity.cell;
                    unload_input.move.entity_count = 1;
                    unload_input.move.entity_ids[0] = unit_id;
                    inputs.push_back(unload_input);
                }
                bot_release_entity(bot, unit_id);
            }
            bot.armies.erase(bot.armies.begin() + army_index);
        } else {
            army_index++;
        }
    }
}

bool bot_task_is_finished(const MatchState& state, const Bot& bot) {
    // TODO: how to make bot abandon rush on wagon death / abandon task in general if it becomes unfavorable?
    switch (bot.task.type) {
        case BOT_TASK_NONE:
            return true;
        case BOT_TASK_RESEARCH: {
            return !match_player_upgrade_is_available(state, bot.player_id, bot.task.research.upgrade);
        }
        case BOT_TASK_EXPAND: {
            EntityId in_progress_hall_id = bot_find_first_entity(state, [&bot](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id && entity.type == ENTITY_HALL && entity.mode == MODE_BUILDING_IN_PROGRESS;
            });
            return in_progress_hall_id != ID_NULL;
        }
        case BOT_TASK_RAISE_ARMY: {
            return bot_has_desired_army(state, bot);
        }
    }
}

BotStrategy bot_get_best_strategy(const MatchState& state, const Bot& bot) {
    int strategy_scores[BOT_STRATEGY_COUNT];
    memset(strategy_scores, 0, sizeof(strategy_scores));

    EntityId saloon_id = bot_find_first_entity(state, [&bot](const Entity& entity, EntityId entity_id) {
        return entity.player_id == bot.player_id && entity.type == ENTITY_SALOON;
    });
    if (saloon_id == ID_NULL) {
        strategy_scores[BOT_STRATEGY_RUSH] = 2;
    }

    strategy_scores[BOT_STRATEGY_EXPAND] = 0;
    int player_base_count[MAX_PLAYERS];
    memset(player_base_count, 0, sizeof(player_base_count));
    for (const Entity& entity : state.entities) {
        if (entity.type == ENTITY_HALL && entity_is_selectable(entity)) {
            player_base_count[entity.player_id]++;
        }
    }
    int max_base_count_difference = -1;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == bot.player_id) {
            continue;
        }
        max_base_count_difference = std::max(max_base_count_difference, player_base_count[player_id] - player_base_count[bot.player_id]);
    }
    if (max_base_count_difference == 0) {
        strategy_scores[BOT_STRATEGY_EXPAND]++;
    } else if (max_base_count_difference > 0) {
        strategy_scores[BOT_STRATEGY_EXPAND] += max_base_count_difference;
    }

    bool has_landmine_army = false;
    for (const BotArmy& army : bot.armies) {
        if (army.type == BOT_ARMY_LANDMINES) {
            has_landmine_army = true;
            break;
        }
    }
    if (!has_landmine_army) {
        strategy_scores[BOT_STRATEGY_LANDMINES] = 1;
    }

    strategy_scores[BOT_STRATEGY_PUSH] = 1;

    BotStrategy best_strategy = (BotStrategy)0;
    for (uint32_t strategy = 1; strategy < BOT_STRATEGY_COUNT; strategy++) {
        if (strategy_scores[strategy] > strategy_scores[best_strategy]) {
            best_strategy = (BotStrategy)strategy;
        }
    }

    return best_strategy;
}

void bot_task_enqueue_from_strategy(const MatchState& state, Bot& bot, BotStrategy strategy) {
    log_trace("BOT: enqueue from strategy %u", strategy);
    switch (strategy) {
        case BOT_STRATEGY_RUSH: {
            BotTask task;
            task.type = BOT_TASK_RAISE_ARMY;
            task.raise_army.army_type = BOT_ARMY_OFFENSIVE;
            memset(task.raise_army.desired_army, 0, sizeof(task.raise_army.desired_army));
            task.raise_army.desired_army[ENTITY_BANDIT] = 4;
            task.raise_army.desired_army[ENTITY_WAGON] = 1;
            bot.task_queue.push(task);
            break;
        }
        case BOT_STRATEGY_EXPAND: {
            bot.task_queue.push((BotTask) { .type = BOT_TASK_EXPAND });
            break;
        }
        case BOT_STRATEGY_BUNKER: {
            BotTask task;
            task.type = BOT_TASK_RAISE_ARMY;
            task.raise_army.army_type = BOT_ARMY_DEFENSIVE;
            memset(task.raise_army.desired_army, 0, sizeof(task.raise_army.desired_army));
            task.raise_army.desired_army[ENTITY_COWBOY] = 4;
            task.raise_army.desired_army[ENTITY_BUNKER] = 1;
            bot.task_queue.push(task);
            break;
        }
        case BOT_STRATEGY_LANDMINES: {
            BotTask raise_army_task;
            raise_army_task.type = BOT_TASK_RAISE_ARMY;
            raise_army_task.raise_army.army_type = BOT_ARMY_LANDMINES;
            memset(raise_army_task.raise_army.desired_army, 0, sizeof(raise_army_task.raise_army.desired_army));
            raise_army_task.raise_army.desired_army[ENTITY_PYRO] = 1;
            bot.task_queue.push(raise_army_task);

            if (match_player_upgrade_is_available(state, bot.player_id, UPGRADE_LANDMINES)) {
                BotTask research_task;
                research_task.type = BOT_TASK_RESEARCH;
                research_task.research.upgrade = UPGRADE_LANDMINES;
                bot.task_queue.push(research_task);
            }

            break;
        }
        case BOT_STRATEGY_HARASS: {
            BotTask task;
            task.type = BOT_TASK_RAISE_ARMY;
            task.raise_army.army_type = BOT_ARMY_OFFENSIVE;
            memset(task.raise_army.desired_army, 0, sizeof(task.raise_army.desired_army));
            task.raise_army.desired_army[ENTITY_COWBOY] = 8;
            bot.task_queue.push(task);
            break;
        }
        case BOT_STRATEGY_PUSH: {
            BotTask task;
            task.type = BOT_TASK_RAISE_ARMY;
            task.raise_army.army_type = BOT_ARMY_OFFENSIVE;
            memset(task.raise_army.desired_army, 0, sizeof(task.raise_army.desired_army));
            task.raise_army.desired_army[ENTITY_SOLDIER] = 12;
            task.raise_army.desired_army[ENTITY_BANDIT] = 8;
            task.raise_army.desired_army[ENTITY_CANNON] = 2;
            bot.task_queue.push(task);
        }
        case BOT_STRATEGY_COUNT: {
            GOLD_ASSERT(false);
            break;
        }
    }
}

void bot_army_create(const MatchState& state, Bot& bot) {
    BotArmy army;
    uint32_t desired_army[ENTITY_TYPE_COUNT];
    memcpy(desired_army, bot.task.raise_army.desired_army, sizeof(desired_army));

    // Reserve units into the army
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.player_id == bot.player_id && 
                entity_is_selectable(entity) && 
                !bot_is_entity_reserved(bot, entity_id) &&
                desired_army[entity.type] > 0) {
            army.units.push_back(entity_id);
            desired_army[entity.type]--;
            bot_reserve_entity(bot, entity_id);
        }
    }

    army.mode = BOT_ARMY_MODE_GATHER;
    army.type = bot.task.raise_army.army_type;
    army.gather_point = bot_army_get_gather_point(state, army.units);
    if (army.type == BOT_ARMY_OFFENSIVE) {
        army.attack_cell = bot_army_get_attack_cell(state, bot, army.gather_point);
        if (army.attack_cell.x == -1) {
            log_warn("Army has no target cell.");
            return;
        }
    } else {
        army.defending_base_id = bot_army_get_defending_base(state, bot, army.gather_point);
        if (army.defending_base_id == ID_NULL) {
            log_warn("Army has no defending base.");
            return;
        }
    }
    bot.armies.push_back(army);
}

ivec2 bot_army_get_gather_point(const MatchState& state, std::vector<EntityId>& units) {
    std::vector<ivec2> gather_points;
    std::vector<ivec2> gather_points_min;
    std::vector<ivec2> gather_points_max;
    std::vector<uint32_t> gather_point_score;
    for (EntityId army_id : units) {
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
            gather_points_min[nearest_gather_point].x = std::min(gather_points_min[nearest_gather_point].x, entity.cell.x);
            gather_points_min[nearest_gather_point].y = std::min(gather_points_min[nearest_gather_point].y, entity.cell.y);
            gather_points_max[nearest_gather_point].x = std::max(gather_points_min[nearest_gather_point].x, entity.cell.x);
            gather_points_max[nearest_gather_point].y = std::max(gather_points_min[nearest_gather_point].y, entity.cell.y);
            gather_points[nearest_gather_point] = ivec2(
                gather_points_min[nearest_gather_point].x + ((gather_points_max[nearest_gather_point].x - gather_points_min[nearest_gather_point].x) / 2),
                gather_points_min[nearest_gather_point].y + ((gather_points_max[nearest_gather_point].y - gather_points_min[nearest_gather_point].y) / 2));
            gather_point_score[nearest_gather_point]++;
        } else {
            gather_points_min.push_back(entity.cell);
            gather_points_max.push_back(entity.cell);
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

    return gather_points[highest_scored_gather_index];
}

ivec2 bot_army_get_attack_cell(const MatchState& state, const Bot& bot, ivec2 army_gather_point) {
    uint32_t nearest_hall_index = INDEX_INVALID;
    uint32_t nearest_building_index = INDEX_INVALID;

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (!entity_is_building(entity.type) || !entity_is_selectable(entity) ||
                state.players[entity.player_id].team == state.players[bot.player_id].team) {
            continue;
        }

        if (nearest_hall_index == INDEX_INVALID || 
                ivec2::manhattan_distance(army_gather_point, entity.cell) < 
                ivec2::manhattan_distance(army_gather_point, state.entities[nearest_hall_index].cell)) {
            nearest_hall_index = entity_index;
        }
        if (nearest_building_index == INDEX_INVALID || 
                ivec2::manhattan_distance(army_gather_point, entity.cell) < 
                ivec2::manhattan_distance(army_gather_point, state.entities[nearest_building_index].cell)) {
            nearest_building_index = entity_index;
        }
    }

    if (nearest_hall_index != INDEX_INVALID) {
        return state.entities[nearest_hall_index].cell + ivec2(1, 1);
    }
    if (nearest_building_index != INDEX_INVALID) {
        return state.entities[nearest_building_index].cell + ivec2(1, 1);
    }
    return ivec2(-1, -1);
}

EntityId bot_army_get_defending_base(const MatchState& state, const Bot& bot, ivec2 gather_point) {
    // Identify all the halls that we currently have
    std::unordered_map<EntityId, uint32_t> hall_defense_score;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id != bot.player_id || 
                entity.type != ENTITY_HALL ||
                !entity_is_selectable(entity)) {
            continue;
        }

        hall_defense_score[state.entities.get_id_of(entity_index)] = 0;
    }

    // Check our armies to see how well defended each hall is
    for (const BotArmy& army : bot.armies) {
        if (army.type != BOT_ARMY_DEFENSIVE) {
            continue;
        }
        GOLD_ASSERT(hall_defense_score.find(army.defending_base_id) != hall_defense_score.end());

        for (EntityId unit_id : army.units) {
            uint32_t unit_index = state.entities.get_index_of(unit_id);
            if (unit_index == INDEX_INVALID) {
                continue;
            }
            const Entity& unit = state.entities[unit_index];
            hall_defense_score[army.defending_base_id]++;
            if (unit.type == ENTITY_BUNKER) {
                // Explicit equals here, since we don't want to count an empty bunker
                hall_defense_score[army.defending_base_id] = unit.garrisoned_units.size();
            }
        }
    }

    if (hall_defense_score.empty()) {
        return ID_NULL;
    }

    // Choose to defend the army that is least defended, or otherwise the closest
    EntityId least_defended_hall_id = ID_NULL;
    for (auto it : hall_defense_score) {
        if (least_defended_hall_id == ID_NULL ||
                it.second < hall_defense_score[least_defended_hall_id]) {
            least_defended_hall_id = it.first;
        }
    }
    GOLD_ASSERT(least_defended_hall_id != ID_NULL);

    // Check if there are multiple least defended hals
    bool are_there_multiple_least_defended_halls = false;
    for (auto it : hall_defense_score) {
        if (it.first == least_defended_hall_id) {
            continue;
        }
        if (std::abs((int)it.second - (int)hall_defense_score[least_defended_hall_id]) < 5) {
            are_there_multiple_least_defended_halls = true;
        }
    }
    if (!are_there_multiple_least_defended_halls) {
        return least_defended_hall_id;
    }

    // If there are mutliple least defended halls, choose the closest one
    uint32_t nearest_hall_id = ID_NULL;
    for (auto it : hall_defense_score) {
        if (std::abs((int)it.second - (int)hall_defense_score[least_defended_hall_id]) >= 5) {
            continue;
        }

        if (nearest_hall_id == INDEX_INVALID || 
                ivec2::manhattan_distance(gather_point, state.entities.get_by_id(it.first).cell) <
                ivec2::manhattan_distance(gather_point, state.entities.get_by_id(nearest_hall_id).cell)) {
            nearest_hall_id = it.first;
        }
    }
    GOLD_ASSERT(nearest_hall_id != ID_NULL);
    return nearest_hall_id;
}

void bot_army_update(const MatchState& state, Bot& bot, BotArmy& army, std::vector<MatchInput>& inputs) {
    GOLD_ASSERT(army.mode != BOT_ARMY_MODE_DISSOLVED);

    // Check army IDs and release any dead units
    bool is_army_under_attack = false;
    uint32_t army_index = 0;
    while (army_index < army.units.size()) {
        EntityId army_id = army.units[army_index];
        uint32_t army_entity_index = state.entities.get_index_of(army_id);
        if (army_entity_index == INDEX_INVALID || 
                state.entities[army_entity_index].health == 0) {
            bot_release_entity(bot, army.units[army_index]);
            army.units[army_index] = army.units[army.units.size() - 1];
            army.units.pop_back();
        } else {
            if (state.entities[army_entity_index].taking_damage_timer != 0) {
                is_army_under_attack = true;
            }
            army_index++;
        }
    }
    if (is_army_under_attack && army.mode != BOT_ARMY_MODE_ATTACK) {
        army.mode = BOT_ARMY_MODE_ATTACK;
    }

    switch (army.mode) {
        case BOT_ARMY_MODE_GATHER: {
            // Move entities to the gather point
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_CELL;
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;
            input.move.target_cell = army.gather_point;
            input.move.entity_count = 0;
            bool all_units_are_gathered = true;

            for (EntityId army_id : army.units) {
                const Entity& entity = state.entities.get_by_id(army_id);
                if (entity_is_building(entity.type)) {
                    continue;
                }
                if (ivec2::manhattan_distance(entity.cell, army.gather_point) < ARMY_GATHER_DISTANCE) {
                    continue;
                }
                all_units_are_gathered = false;
                if (entity.target.type == TARGET_CELL && ivec2::manhattan_distance(entity.target.cell, army.gather_point) < ARMY_GATHER_DISTANCE) {
                    continue;
                }
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
            bool army_has_transports = false;
            for (EntityId army_id : army.units) {
                const Entity& entity = state.entities.get_by_id(army_id);
                if (entity_get_data(entity.type).garrison_capacity != 0) {
                    army_has_transports = true;
                    break;
                } 
            }

            army.mode = army_has_transports ? BOT_ARMY_MODE_GARRISON : BOT_ARMY_MODE_GARRISON;

            break;
        }
        case BOT_ARMY_MODE_GARRISON: {
            // Get a list of all available transports and units to put inside them
            std::vector<EntityId> available_transports;
            std::vector<EntityId> ungarrisoned_units;
            bool all_units_are_garrisoned = true;
            for (EntityId army_id : army.units) {
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
                army.mode = army.type == BOT_ARMY_OFFENSIVE ? BOT_ARMY_MODE_ATTACK : BOT_ARMY_MODE_DEFEND;
                break;
            }

            // Load units into transports
            // And also tell the transports to stop so that they're not moving away from
            // the units that are trying to load into them
            MatchInput stop_input;
            stop_input.type = MATCH_INPUT_STOP;
            stop_input.stop.entity_count = 0;

            for (EntityId transport_id : available_transports) {
                // First determine how many units this transport is already carrying
                const Entity& transport = state.entities.get_by_id(transport_id);
                uint32_t transport_garrison_size = transport.garrisoned_units.size();
                for (EntityId unit_id : army.units) {
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

                while (!ungarrisoned_units.empty() && input.move.entity_count + transport_garrison_size < entity_get_data(transport.type).garrison_capacity) {
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
                    if (entity_is_unit(transport.type)) {
                        stop_input.stop.entity_ids[stop_input.stop.entity_count] = input.move.target_id;
                        stop_input.stop.entity_count++;
                    }
                }
            }

            if (stop_input.stop.entity_count != 0) {
                inputs.push_back(stop_input);
            }

            break;
        }
        case BOT_ARMY_MODE_ATTACK: {
            bool units_have_reached_move_out_point = true;
            bool units_are_attacking = false;

            // If there's any units away from the attack point, then move them
            MatchInput unload_input;
            unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
            unload_input.move.shift_command = 0;
            unload_input.move.target_id = ID_NULL;
            unload_input.move.target_cell = army.attack_cell;
            unload_input.move.entity_count = 0;

            MatchInput move_input = unload_input;
            move_input.type = MATCH_INPUT_MOVE_ATTACK_CELL;

            MatchInput camo_input;
            camo_input.type = MATCH_INPUT_CAMO;
            camo_input.camo.unit_count = 0;

            for (EntityId unit_id : army.units) {
                const Entity& entity = state.entities.get_by_id(unit_id);

                // Don't do anything with a unit that isn't selectable
                // Unselectable units should only be garrisoned units in this case
                if (!entity_is_selectable(entity)) {
                    continue;
                }

                Target attack_target = match_entity_target_nearest_enemy(state, entity);
                if (entity.target.type == TARGET_ATTACK_ENTITY ||
                         entity.target.type == TARGET_ATTACK_CELL ||
                         attack_target.type == TARGET_ATTACK_ENTITY) {
                    units_are_attacking = true;
                }

                // Check if entity is at the attack point
                if (ivec2::manhattan_distance(entity.cell, army.attack_cell) >= ARMY_GATHER_DISTANCE) {
                    units_have_reached_move_out_point = false;
                    // If it is not, and it is not attacking something, and it is not on the way, then move it closer
                    if (attack_target.type == TARGET_NONE && 
                            !((entity.target.type == TARGET_ATTACK_CELL || entity.target.type == TARGET_UNLOAD || entity.target.type == TARGET_MOLOTOV) &&
                            ivec2::manhattan_distance(entity.target.cell, army.attack_cell) < ARMY_GATHER_DISTANCE)) {
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
                    }
                }

                // If a transport is here and it has units in it, then unload them
                // Otherwise release the transport
                if (entity.type == ENTITY_WAGON) {
                    if (attack_target.type == TARGET_ATTACK_ENTITY && !entity.garrisoned_units.empty()) {
                        MatchInput unload_input;
                        unload_input.type = MATCH_INPUT_MOVE_UNLOAD;
                        unload_input.move.shift_command = 0;
                        unload_input.move.target_id = ID_NULL;
                        unload_input.move.target_cell = entity.cell;
                        unload_input.move.entity_count = 1;
                        unload_input.move.entity_ids[0] = unit_id;
                        inputs.push_back(unload_input);
                    } else if (entity.garrisoned_units.empty()) {
                        uint32_t nearest_hall_index = INDEX_INVALID;
                        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                            const Entity& hall = state.entities[entity_index];
                            if (hall.player_id != bot.player_id || hall.mode != MODE_BUILDING_FINISHED || hall.type != ENTITY_HALL) {
                                continue;
                            }

                            if (nearest_hall_index == INDEX_INVALID || ivec2::manhattan_distance(hall.cell, entity.cell) < ivec2::manhattan_distance(state.entities[nearest_hall_index].cell, entity.cell)) {
                                nearest_hall_index = entity_index;
                            }
                        }

                        if (nearest_hall_index != INDEX_INVALID) {
                            MatchInput move_input;
                            move_input.type = MATCH_INPUT_MOVE_ENTITY;
                            move_input.move.shift_command = 0;
                            move_input.move.target_id = state.entities.get_id_of(nearest_hall_index);
                            move_input.move.target_cell = ivec2(0, 0);
                            move_input.move.entity_count = 1;
                            move_input.move.entity_ids[0] = unit_id;
                            inputs.push_back(move_input);
                        }

                        bot_release_entity(bot, unit_id);
                        army.units[army_index] = army.units[army.units.size() - 1];
                        army.units.pop_back();
                        break;
                    }
                } 

                // Check to make sure this unit is attacking the best target
                if (attack_target.type == TARGET_ATTACK_ENTITY && 
                        entity_get_data(entity.type).unit_data.damage != 0 &&
                        (entity.target.type != TARGET_ATTACK_ENTITY ||
                        entity_get_data(state.entities.get_by_id(attack_target.id).type).attack_priority > 
                        entity_get_data(state.entities.get_by_id(entity.target.id).type).attack_priority)) {
                    MatchInput attack_input;
                    attack_input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
                    attack_input.move.shift_command = 0;
                    attack_input.move.target_id = attack_target.id;
                    attack_input.move.target_cell = ivec2(0, 0);
                    attack_input.move.entity_ids[0] = unit_id;
                    attack_input.move.entity_count = 1;
                    inputs.push_back(attack_input);
                }

                // Throw molotovs
                if (entity.type == ENTITY_PYRO && attack_target.type == TARGET_ATTACK_ENTITY) {
                    if (entity.target.type != TARGET_MOLOTOV && entity.energy > MOLOTOV_ENERGY_COST) {
                        MatchInput molotov_input = bot_create_molotov_input(state, unit_id, army.attack_cell);
                        if (molotov_input.type != MATCH_INPUT_NONE) {
                            inputs.push_back(molotov_input);
                        }
                    }
                }

                // Camo the detective
                if (entity.type == ENTITY_DETECTIVE && 
                        attack_target.type == TARGET_ATTACK_ENTITY &&
                        !entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) &&
                        entity.energy >= CAMO_ENERGY_COST) {
                    camo_input.camo.unit_ids[camo_input.camo.unit_count] = unit_id;
                    camo_input.camo.unit_count++;
                }
            } // End for each unit in army

            if (move_input.move.entity_count != 0) {
                inputs.push_back(move_input);
            }
            if (unload_input.move.entity_count != 0) {
                inputs.push_back(unload_input);
            }
            if (camo_input.camo.unit_count != 0) {
                inputs.push_back(camo_input);
            }

            if (units_have_reached_move_out_point && !units_are_attacking) {
                army.mode = BOT_ARMY_MODE_DISSOLVED;
            }

            break;
        }
        case BOT_ARMY_MODE_DEFEND: {
            ivec2 base_center;
            bot_get_base_info(state, army.defending_base_id, &base_center, NULL, NULL);

            // Prepare move to defend input
            MatchInput move_input;
            move_input.type = MATCH_INPUT_MOVE_CELL;
            move_input.move.shift_command = 0;
            move_input.move.target_id = ID_NULL;
            move_input.move.target_cell = base_center;
            move_input.move.entity_count = 0;

            for (EntityId unit_id : army.units) {
                const Entity& entity = state.entities.get_by_id(unit_id);

                // Don't do anything with a unit that isn't selectable (should be none of the units, but just in case)
                // Also don't do anything with non-units i.e. bunkers
                if (!entity_is_selectable(entity) || !entity_is_unit(entity.type)) {
                    continue;
                }

                // Check if entity is at defend point
                if (ivec2::manhattan_distance(entity.cell, base_center) >= ARMY_DEFEND_DISTANCE) {
                    // Move it closer if it's not already on its way
                    if (!(entity.target.type == TARGET_CELL && ivec2::manhattan_distance(entity.target.cell, base_center) < ARMY_DEFEND_DISTANCE)) {
                        move_input.move.entity_ids[move_input.move.entity_count] = unit_id;
                        move_input.move.entity_count++;
                        if (move_input.move.entity_count == SELECTION_LIMIT) {
                            inputs.push_back(move_input);
                            move_input.move.entity_count = 0;
                        }
                    }

                    continue;
                }

                // If we reached here, entity is at the defend point
            }

            if (move_input.move.entity_count != 0) {
                inputs.push_back(move_input);
            }

            break;
        }
        case BOT_ARMY_MODE_LANDMINES: {
            // If we don't have the landmines upgrade, just wait until we get it
            if (!match_player_has_upgrade(state, bot.player_id, UPGRADE_LANDMINES)) {
                break;
            }

            const Entity& pyro = state.entities.get_by_id(army.units[0]);
            GOLD_ASSERT(pyro.type == ENTITY_PYRO);

            // Find the nearest base which needs to be landmined
            EntityId nearest_base_id = ID_NULL;
            ivec2 nearest_base_center;
            int nearest_base_radius;
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                // Filter entities down to only bases
                const Entity& entity = state.entities[entity_index];
                if (entity.player_id != bot.player_id ||
                        entity.type != ENTITY_HALL || 
                        !entity_is_selectable(entity)) {
                    continue;
                }

                // Get info of this base
                EntityId base_id = state.entities.get_id_of(entity_index);
                ivec2 base_center;
                int base_radius;
                uint32_t landmine_count;
                bot_get_base_info(state, base_id, &base_center, &base_radius, &landmine_count);

                // Skip this base if we already have enough mines at it
                if (landmine_count >= ARMY_DEFEND_LANDMINE_MAX) {
                    continue;
                }

                // Otherwise, decide whether this base is closer than the other bases
                if (nearest_base_id == ID_NULL || 
                        ivec2::manhattan_distance(base_center, pyro.cell) <
                        ivec2::manhattan_distance(nearest_base_center, pyro.cell)) {
                    nearest_base_id = base_id;
                    nearest_base_center = base_center;
                    nearest_base_radius = base_radius;
                }
            }

            // If the ID is null, it means that all the bases have been fully landmined
            if (nearest_base_id == ID_NULL) {
                army.mode = BOT_ARMY_MODE_DISSOLVED;
                break;
            }

            // Lay mines
            if (pyro.target.type == TARGET_NONE && pyro.energy >= entity_get_data(ENTITY_LANDMINE).gold_cost) {
                MatchInput landmine_input = bot_create_landmine_input(state, bot, army.units[0], nearest_base_center, nearest_base_radius);
                if (landmine_input.type != MATCH_INPUT_NONE) {
                    inputs.push_back(landmine_input);
                } 
            }

            // Make sure that the pyro moves to the next base, even if it's out of energy
            if (pyro.target.type == TARGET_NONE && 
                    pyro.energy < entity_get_data(ENTITY_LANDMINE).gold_cost && 
                    ivec2::manhattan_distance(pyro.cell, nearest_base_center) > nearest_base_radius) {
                MatchInput move_input;
                move_input.type = MATCH_INPUT_MOVE_CELL;
                move_input.move.shift_command = 0;
                move_input.move.target_id = ID_NULL;
                move_input.move.target_cell = nearest_base_center;
                move_input.move.entity_count = 1;
                move_input.move.entity_ids[0] = army.units[0];
                inputs.push_back(move_input);
            }

            break;
        }
        case BOT_ARMY_MODE_DISSOLVED: 
            break;
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

bool bot_has_desired_army(const MatchState& state, const Bot& bot) {
    GOLD_ASSERT(bot.task.type == BOT_TASK_RAISE_ARMY);

    // Check if bot even desires an army at all
    bool bot_desires_army = false;
    for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (bot.task.raise_army.desired_army[entity_type] != 0) {
            bot_desires_army = true;
            break;
        }
    }
    if (!bot_desires_army) {
        return false;
    }

    // Get current entities
    uint32_t entity_count[ENTITY_TYPE_COUNT];
    memset(entity_count, 0, sizeof(entity_count));
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id == bot.player_id && 
                entity_is_selectable(entity) && 
                !bot_is_entity_reserved(bot, state.entities.get_id_of(entity_index))) {
            entity_count[entity.type]++;
        }
    }

    for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (entity_count[entity_type] < bot.task.raise_army.desired_army[entity_type]) {
            return false;
        }
    }

    return true;
}

bool bot_should_build_house(const MatchState& state, const Bot& bot) {
    // Decide if we need to make a house
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

EntityType bot_get_desired_entities(const MatchState& state, const Bot& bot, uint32_t* desired_entities) {
    // Determine the desired units based on the current task
    memcpy(desired_entities, bot.task.raise_army.desired_army, sizeof(bot.task.raise_army.desired_army));

    // Subtract desired units any unit which we already have or which is in-progress
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
        EntityType building_type = bot_get_building_type_which_trains_unit_type((EntityType)entity_type);
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

    // Determine any other desired buildings based on building pre-reqs
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (desired_entities[entity_type] == 0) {
            continue;
        }
        EntityType pre_req = bot_get_building_pre_req((EntityType)entity_type);
        while (pre_req != ENTITY_TYPE_COUNT && desired_entities[pre_req] == 0) {
            desired_entities[pre_req] = 1;
            pre_req = bot_get_building_pre_req((EntityType)pre_req);
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

    return ENTITY_TYPE_COUNT;
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
        hall_score += Rect::euclidean_distance_squared_between(hall_rect, goldmine_rect);
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
                    if (map_is_tile_ramp(state.map, ivec2(x, y)) ||
                            map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y)).type != CELL_EMPTY) {
                        hall_score++;
                    } 
                }
            }
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
    EntityType building_pre_req = bot_get_building_pre_req(building_type);
    if (building_pre_req != ENTITY_TYPE_COUNT) {
        bool has_pre_req = false;
        for (const Entity& entity : state.entities) {
            if (entity.player_id == bot.player_id && entity.mode == MODE_BUILDING_FINISHED && entity.type == building_pre_req) {
                has_pre_req = true;
                break;
            }
        }
        if (!has_pre_req) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id);
    if (hall_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId builder_id = bot_find_builder(state, bot, hall_index);
    if (builder_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 building_location = building_type == ENTITY_HALL ? bot_find_hall_location(state, hall_index) : bot_find_building_location(state, bot.player_id, state.entities[hall_index].cell + ivec2(1, 1), entity_get_data(building_type).cell_size);
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

EntityType bot_get_building_pre_req(EntityType building_type) {
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

MatchInput bot_create_building_rally_input(const MatchState& state, const Bot& bot, EntityId building_id) {
    static const int RALLY_CELL_RADIUS = 4;

    std::queue<ivec2> frontier;
    std::unordered_map<uint32_t, bool> explored;
    frontier.push(state.entities.get_by_id(building_id).cell);

    ivec2 rally_cell = ivec2(-1, -1);
    while (!frontier.empty()) {
        ivec2 next = frontier.front();
        frontier.pop();

        if (explored.find(next.x + (next.y * state.map.width)) != explored.end()) {
            continue;
        }

        bool is_next_valid = true;
        for (int y = next.y - RALLY_CELL_RADIUS; y < next.y + RALLY_CELL_RADIUS; y++) {
            for (int x = next.x - RALLY_CELL_RADIUS; x < next.x + RALLY_CELL_RADIUS; x++) {
                if (!map_is_cell_in_bounds(state.map, ivec2(x, y))) {
                    is_next_valid = false;
                    break;
                }
                CellType cell_type = map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y)).type;
                if (!(cell_type == CELL_EMPTY || cell_type == CELL_UNIT)) {
                    is_next_valid = false;
                    break;
                }
            }
            if (!is_next_valid) {
                break;
            }
        }
        if (is_next_valid) {
            rally_cell = next;
            break;
        }

        explored[next.x + (next.y * state.map.width)] = true;
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            frontier.push(child);
        }
    }

    GOLD_ASSERT(rally_cell.x != -1);

    MatchInput rally_input;
    rally_input.type = MATCH_INPUT_RALLY;
    rally_input.rally.building_ids[0] = building_id;
    rally_input.rally.building_count = 1;
    rally_input.rally.rally_point = cell_center(rally_cell).to_ivec2();

    return rally_input;
}

bool bot_get_desired_upgrade(const MatchState& state, const Bot& bot, uint32_t* upgrade) {
    if (match_player_upgrade_is_available(state, bot.player_id, UPGRADE_LANDMINES)) {
        for (const BotArmy& army : bot.armies) {
            if (army.type != BOT_ARMY_DEFENSIVE) {
                continue;
            }
            for (EntityId unit_id : army.units) {
                uint32_t unit_index = state.entities.get_index_of(unit_id);
                if (unit_index == INDEX_INVALID) {
                    continue;
                }
                if (state.entities[unit_index].type == ENTITY_PYRO) {
                    *upgrade = UPGRADE_LANDMINES;
                    return true;
                }
            }
        }
    }

    return false;
}

EntityType bot_get_building_type_which_researches_upgrade(uint32_t upgrade) {
    switch (upgrade) {
        case UPGRADE_WAR_WAGON:
        case UPGRADE_BAYONETS:
        case UPGRADE_SERRATED_KNIVES:
        case UPGRADE_FAN_HAMMER:
            return ENTITY_SMITH;
        case UPGRADE_LANDMINES:
            return ENTITY_WORKSHOP;
        case UPGRADE_PRIVATE_EYE:
        case UPGRADE_STAKEOUT:
            return ENTITY_SHERIFFS;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

MatchInput bot_create_upgrade_input(const MatchState& state, const Bot& bot, uint32_t upgrade) {
    EntityId building_id = ID_NULL;
    EntityType building_type = bot_get_building_type_which_researches_upgrade(upgrade);
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.player_id == bot.player_id && 
                entity.mode == MODE_BUILDING_FINISHED && 
                entity.type == building_type) {
            building_id = state.entities.get_id_of(entity_index);
            break;
        }
    }

    if (building_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.building_id = building_id;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UPGRADE;
    input.building_enqueue.item_subtype = upgrade;
    return input;
}

void bot_create_landmine_draw_circle(int x_center, int y_center, int x, int y, std::vector<ivec2>& points) {
    points.push_back(ivec2(x_center + x, y_center + y));
    points.push_back(ivec2(x_center - x, y_center + y));
    points.push_back(ivec2(x_center + x, y_center - y));
    points.push_back(ivec2(x_center - x, y_center - y));

    points.push_back(ivec2(x_center + y, y_center + x));
    points.push_back(ivec2(x_center - y, y_center + x));
    points.push_back(ivec2(x_center + y, y_center - x));
    points.push_back(ivec2(x_center - y, y_center - x));
}

bool bot_is_landmine_point_valid(const MatchState& state, ivec2 point) {
    static const int MINE_SPACING = 1;

    ivec2 point_rect_top_left = point - ivec2(MINE_SPACING, MINE_SPACING);
    int point_rect_size = 1 + (2 * MINE_SPACING);
    if (!map_is_cell_rect_in_bounds(state.map, point_rect_top_left, point_rect_size) ||
            map_is_cell_rect_occupied(state.map, CELL_LAYER_UNDERGROUND, point_rect_top_left, point_rect_size)) {
        return false;
    }
    for (int y = point_rect_top_left.y; y < point_rect_top_left.y + point_rect_size; y++) {
        for (int x = point_rect_top_left.x; x < point_rect_top_left.x + point_rect_size; x++) {
            CellType cell_type = map_get_cell(state.map, CELL_LAYER_GROUND, ivec2(x, y)).type;
            if ((x == point.x && y == point.y && cell_type != CELL_EMPTY) ||
                    (x != point.x && y != point.y && cell_type == CELL_BUILDING)) {
                return false;
            }
        }
    }

    return true;
}

MatchInput bot_create_landmine_input(const MatchState& state, const Bot& bot, EntityId pyro_id, ivec2 base_center, int base_radius) {
    // Using Besenham's Circle to create list of points along which to place landmines
    // https://www.geeksforgeeks.org/c/bresenhams-circle-drawing-algorithm/
    std::vector<ivec2> circle_points;
    // Guessing we will have circumference, 2PI*R points on the circle, 7 is 2PI rounded up
    circle_points.reserve(7 * base_radius);
    int x = 0;
    int y = base_radius;
    int d = 3 - (2 * base_radius);
    bot_create_landmine_draw_circle(base_center.x, base_center.y, x, y, circle_points);
    while (y >= x) {
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + (4 * x) + 6;
        }

        x++;

        bot_create_landmine_draw_circle(base_center.x, base_center.y, x, y, circle_points);
    }

    // Find the nearest enemy base, to determine where to place mines
    uint32_t nearest_enemy_base_index = INDEX_INVALID;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        if (entity.type != ENTITY_HALL || !entity_is_selectable(entity) ||
                state.players[entity.player_id].team == state.players[bot.player_id].team) {
            continue;
        }

        if (nearest_enemy_base_index == INDEX_INVALID || 
                ivec2::manhattan_distance(base_center, state.entities[entity_index].cell) <
                ivec2::manhattan_distance(base_center, state.entities[nearest_enemy_base_index].cell)) {
            nearest_enemy_base_index = entity_index;
        }
    }
    if (nearest_enemy_base_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 nearest_mine_cell = ivec2(-1, -1);
    ivec2 enemy_cell = state.entities[nearest_enemy_base_index].cell;
    for (ivec2 point : circle_points) {
        if (!bot_is_landmine_point_valid(state, point)) {
            continue;
        }

        if (nearest_mine_cell.x == -1 || 
                ivec2::manhattan_distance(enemy_cell, point) < 
                ivec2::manhattan_distance(enemy_cell, nearest_mine_cell)) {
            nearest_mine_cell = point;
        }
    }

    if (nearest_mine_cell.x == -1) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILD;
    input.build.shift_command = 0;
    input.build.building_type = ENTITY_LANDMINE;
    input.build.target_cell = nearest_mine_cell;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = pyro_id;
    return input;
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

MatchInput bot_create_molotov_input(const MatchState& state, EntityId pyro_id, ivec2 attack_point) {
    static const int MOLOTOV_THROW_SEARCH_MARGIN = 32;
    const Entity& pyro = state.entities.get_by_id(pyro_id);

    ivec2 best_molotov_cell = ivec2(-1, -1);
    int best_molotov_cell_score = 0;

    // Consider all cells in the search range
    for (int y = attack_point.y - MOLOTOV_THROW_SEARCH_MARGIN; y < attack_point.y + MOLOTOV_THROW_SEARCH_MARGIN; y++) {
        for (int x = attack_point.x - MOLOTOV_THROW_SEARCH_MARGIN; x < attack_point.x + MOLOTOV_THROW_SEARCH_MARGIN; x++) {
            ivec2 cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }

            // For each cell, score them based on what units we would hit if we threw fire there
            int molotov_cell_score = 0;
            std::unordered_map<EntityId, bool> entity_considered;
            for (int fire_y = cell.y - PROJECTILE_MOLOTOV_FIRE_SPREAD; fire_y < cell.y + PROJECTILE_MOLOTOV_FIRE_SPREAD; fire_y++) {
                for (int fire_x = cell.x - PROJECTILE_MOLOTOV_FIRE_SPREAD; fire_x < cell.x + PROJECTILE_MOLOTOV_FIRE_SPREAD; fire_x++) {
                    // Don't count the corners of this square
                    if ((fire_y == cell.y - PROJECTILE_MOLOTOV_FIRE_SPREAD || fire_y == cell.y + PROJECTILE_MOLOTOV_FIRE_SPREAD - 1) &&
                            (fire_x == cell.x - PROJECTILE_MOLOTOV_FIRE_SPREAD || fire_x == cell.x + PROJECTILE_MOLOTOV_FIRE_SPREAD - 1)) {
                        continue;
                    }
                    ivec2 fire_cell = ivec2(fire_x, fire_y);
                    if (!map_is_cell_in_bounds(state.map, fire_cell)) {
                        continue;
                    }

                    Cell fire_map_cell = map_get_cell(state.map, CELL_LAYER_GROUND, fire_cell);
                    if (fire_map_cell.type == CELL_BUILDING || fire_map_cell.type == CELL_UNIT || fire_map_cell.type == CELL_MINER) {
                        if (entity_considered.find(fire_map_cell.id) != entity_considered.end()) {
                            continue;
                        }
                        entity_considered[fire_map_cell.id] = true;
                        const Entity& fire_target = state.entities.get_by_id(fire_map_cell.id);
                        if (fire_map_cell.id == pyro_id) {
                            molotov_cell_score -= 4;
                        } else if (state.players[fire_target.player_id].team == state.players[pyro.player_id].team) {
                            molotov_cell_score -= 2;
                        } else if (entity_is_unit(fire_target.type)) {
                            molotov_cell_score += 2;
                        } else {
                            molotov_cell_score++;
                        }
                    }

                    if (match_is_cell_on_fire(state, fire_cell)) {
                        molotov_cell_score--;
                    }
                }
            } // End for each fire_y

            // Consider other existing or about to exist molotov fires
            for (const Entity& entity : state.entities) {
                if (entity.target.type == TARGET_MOLOTOV && ivec2::manhattan_distance(cell, entity.target.cell) < PROJECTILE_MOLOTOV_FIRE_SPREAD) {
                    molotov_cell_score--;
                }
            }
            for (const Projectile& projectile : state.projectiles) {
                if (projectile.type == PROJECTILE_MOLOTOV && ivec2::manhattan_distance(cell, projectile.target.to_ivec2() / TILE_SIZE) < PROJECTILE_MOLOTOV_FIRE_SPREAD) {
                    molotov_cell_score--;
                }
            }

            // Also score based on how far away the molotov cell is 
            // Divide by the molotov range so that we don't weight this too heavily
            // The real reason we care about the range is if we have multiple pyros, it'd be nice if they 
            // didn't both decide to throw to the same spot but instead considered their own position
            molotov_cell_score -= (2 * ivec2::euclidean_distance_squared(pyro.cell, cell) / MOLOTOV_RANGE_SQUARED);

            if (molotov_cell_score > best_molotov_cell_score) {
                best_molotov_cell = cell;
                best_molotov_cell_score = molotov_cell_score;
            }
        }
    } // End for each y

    if (best_molotov_cell_score == 0) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_MOLOTOV;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = best_molotov_cell;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = pyro_id;
    return input;
}

MatchInput bot_create_repair_building_input(const MatchState& state, const Bot& bot, EntityId building_id) {
    const Entity& building = state.entities.get_by_id(building_id);
    EntityId worker_id = bot_find_nearest_idle_worker(state, bot, building.cell);
    if (worker_id == ID_NULL) {
        EntityId nearest_goldmine_id = bot_find_best_entity(state, 
            // Filter
            [](const Entity& entity, EntityId entity_id) {
                return entity.type == ENTITY_GOLDMINE;
            }, 
            // Compare
            [&building](const Entity& a, const Entity& b) {
                return ivec2::manhattan_distance(a.cell, building.cell) < ivec2::manhattan_distance(b.cell, building.cell);
            });
        GOLD_ASSERT(nearest_goldmine_id != ID_NULL);
        worker_id = bot_pull_worker_off_gold(state, bot.player_id, nearest_goldmine_id);
    }
    if (worker_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_REPAIR;
    input.move.shift_command = 0;
    input.move.target_cell = ivec2(0, 0);
    input.move.target_id = building_id;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = worker_id;
    return input;
}

EntityId bot_find_first_entity(const MatchState& state, std::function<bool(const Entity&, EntityId)> filter_fn) {
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
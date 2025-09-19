#include "bot.h"

#include "core/logger.h"
#include "squad.h"
#include "scout.h"
#include "util.h"
#include "production.h"
#include "../lcg.h"

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed) {
    Bot bot;
    bot.player_id = player_id;

    // The bot takes a copy of the random seed, that way
    // it can make deterministic random decisions that will be
    // synced across each player's computer, but since the 
    // bot code will not be rerun during a replay, the bot will
    // not mess up replay playback
    bot.lcg_seed = lcg_seed;

    bot.should_surrender = true;
    bot.has_surrendered = false;

    bot.strategy = (BotStrategy)(lcg_rand(&bot.lcg_seed) % BOT_STRATEGY_COUNT);
    bot.goal = bot_goal_create(state, bot, bot_choose_opening_goal_type(bot));

    bot.scout_id = ID_NULL;
    bot.last_scout_time = 0;
    bot.scout_enemy_has_detectives = false;
    bot.scout_enemy_has_landmines = false;
    for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
        if (state.entities[goldmine_index].type == ENTITY_GOLDMINE) {
            bot.should_scout_goldmine[goldmine_index] = false;
        }
    }

    return bot;
}

MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    bot_scout_update(state, bot, match_time_minutes);

    // Strategy update
    if (bot_goal_should_be_abandoned(state, bot, bot.goal)) {
        bot.goal.desired_upgrade = 0;
        memset(bot.goal.desired_entities, 0, sizeof(bot.goal.desired_entities));
    }
    if (bot_goal_is_empty(bot.goal)) {
        bot.goal = bot_goal_create(state, bot, bot_choose_next_goal_type(state, bot));
    }
    if (bot_goal_is_met(state, bot, bot.goal)) {
        if (bot.goal.desired_squad_type != BOT_SQUAD_TYPE_NONE) {
            bot_squad_create_from_goal(state, bot, bot.goal);
        }
        bot.goal.desired_upgrade = 0;
        memset(bot.goal.desired_entities, 0, sizeof(bot.goal.desired_entities));
    }

    bot_handle_base_under_attack(state, bot);

    MatchInput production_input = bot_get_production_input(state, bot);
    if (production_input.type != MATCH_INPUT_NONE) {
        return production_input;
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

    MatchInput repair_input = bot_repair_burning_buildings(state, bot);
    if (repair_input.type != MATCH_INPUT_NONE) {
        return repair_input;
    }

    MatchInput scout_input = bot_scout(state, bot, match_time_minutes);
    if (scout_input.type != MATCH_INPUT_NONE) {
        return scout_input;
    }

    MatchInput rein_in_input = bot_rein_in_stray_units(state, bot);
    if (rein_in_input.type != MATCH_INPUT_NONE) {
        return rein_in_input;
    }

    MatchInput rally_input = bot_set_rally_points(state, bot);
    if (rally_input.type != MATCH_INPUT_NONE) {
        return rally_input;
    }

    MatchInput unload_input = bot_unload_unreserved_carriers(state, bot);
    if (unload_input.type != MATCH_INPUT_NONE) {
        return unload_input;
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

BotGoalType bot_choose_opening_goal_type(Bot& bot) {
    bool risky_opener = lcg_rand(&bot.lcg_seed) % 4 == 0;
    if (risky_opener) {
        switch (bot.strategy) {
            case BOT_STRATEGY_SALOON_COOP:
                return BOT_GOAL_BANDIT_RUSH;
            case BOT_STRATEGY_SALOON_WORKSHOP:
                return BOT_GOAL_EXPAND;
            case BOT_STRATEGY_BARRACKS:
                return BOT_GOAL_EXPAND;
            case BOT_STRATEGY_COUNT:
                GOLD_ASSERT(false);
                return BOT_GOAL_COUNT;
        }
    } else {
        return BOT_GOAL_BUNKER;
    }
}

BotGoalType bot_choose_next_goal_type(const MatchState& state, const Bot& bot) {
    // Count entities
    uint32_t entity_count[MAX_PLAYERS][ENTITY_TYPE_COUNT];
    memset(entity_count, 0, sizeof(entity_count));
    int army_size[MAX_PLAYERS];
    memset(army_size, 0, sizeof(army_size));
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.type == ENTITY_GOLDMINE ||
                entity.health == 0) {
            continue;
        }
        if (entity_is_building(entity.type) && 
                !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }

        entity_count[entity.player_id][entity.type]++;
        if (entity_is_unit(entity.type) && entity.type != ENTITY_MINER) {
            army_size[entity.player_id]++;
        }
    }

    int allied_army_size = 0;
    int enemy_army_size = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state.players[player_id].active) {
            continue;
        }
        if (state.players[player_id].team == state.players[bot.player_id].team) {
            allied_army_size += army_size[player_id];
        } else {
            enemy_army_size += army_size[player_id];
        }
    }

    // Count halls
    uint32_t player_mining_base_count[MAX_PLAYERS];
    memset(player_mining_base_count, 0, sizeof(player_mining_base_count));
    uint32_t unoccupied_goldmine_count = 0;
    for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
        const Entity& goldmine = state.entities[goldmine_index];
        if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
            continue;
        }

        EntityId surrounding_hall_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);
        if (surrounding_hall_id == ID_NULL) {
            unoccupied_goldmine_count++;
            continue;
        }

        const Entity& surrounding_hall = state.entities.get_by_id(surrounding_hall_id);
        player_mining_base_count[surrounding_hall.player_id]++;
    }
    uint32_t max_opponent_base_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state.players[player_id].active) {
            continue;
        }
        if (state.players[player_id].team == state.players[bot.player_id].team) {
            continue;
        }
        max_opponent_base_count = std::max(max_opponent_base_count, player_mining_base_count[player_id]);
    }

    // Count hall defenses
    std::unordered_map<uint32_t, uint32_t> hall_detective_defenses;
    std::unordered_map<uint32_t, uint32_t> hall_bunker_defenses;
    for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
        const Entity& hall = state.entities[hall_index];
        if (hall.type != ENTITY_HALL ||
                hall.player_id != bot.player_id || 
                !entity_is_selectable(hall)) {
            continue;
        }

        hall_detective_defenses[hall_index] = 0;
        hall_bunker_defenses[hall_index] = 0;
    }
    for (const BotSquad& squad : bot.squads) {
        uint32_t nearest_hall_index = INDEX_INVALID;
        for (auto it : hall_detective_defenses) {
            if (nearest_hall_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(state.entities[it.first].cell, squad.target_cell) <
                    ivec2::manhattan_distance(state.entities[nearest_hall_index].cell, squad.target_cell)) {
                nearest_hall_index = it.first;
            }
        }
        // nearest_hall_index should be INDEX_INVALID only when there are no halls
        if (nearest_hall_index == INDEX_INVALID) {
            continue;
        }

        if (bot_squad_can_defend_against_detectives(state, squad)) {
            hall_detective_defenses[nearest_hall_index]++;
        } 
        if (squad.type == BOT_SQUAD_TYPE_BUNKER) {
            hall_bunker_defenses[nearest_hall_index]++;
        }
    }

    // If we are not safe from detectives, make detective defense squad
    bool is_bot_safe_from_detectives = true;
    for (auto it : hall_detective_defenses) {
        if (it.second == 0) {
            is_bot_safe_from_detectives = false;
        }
    }
    if (bot.scout_enemy_has_detectives && !is_bot_safe_from_detectives) {
        return BOT_GOAL_DETECTIVE_DEFENSE;
    }

    if (bot.strategy == BOT_STRATEGY_BARRACKS || 
            (enemy_army_size > allied_army_size && allied_army_size < 4 * (int)player_mining_base_count[bot.player_id])) {
        bool bot_is_bunkered_up = true;
        uint32_t desired_bunker_count = 1;
        for (auto it : hall_bunker_defenses) {
            if (it.second < desired_bunker_count) {
                bot_is_bunkered_up = false;
                break;
            }
        }
        if (!bot_is_bunkered_up) {
            return BOT_GOAL_BUNKER;
        }
    }

    // If we are undefended, make defense squad
    // The tolerance the bot has of army differences gets bigger the more bases we have
    if (enemy_army_size > allied_army_size && allied_army_size < 4 * (int)player_mining_base_count[bot.player_id]) {
        return BOT_GOAL_DEFENSE;
    }
    
    // Expand
    uint32_t desired_base_count = std::max(2U, max_opponent_base_count);
    if (unoccupied_goldmine_count > 0 && player_mining_base_count[bot.player_id] < desired_base_count) {
        return BOT_GOAL_EXPAND;
    }

    // Landmines
    if (bot.strategy == BOT_STRATEGY_SALOON_WORKSHOP) {
        bool bot_has_landmine_squad = false;
        for (const BotSquad& squad : bot.squads) {
            if (squad.type == BOT_SQUAD_TYPE_LANDMINES) {
                bot_has_landmine_squad = true;
            }
        }
        if (!bot_has_landmine_squad) {
            return BOT_GOAL_LANDMINES;
        }
    }

    // If enemy has no base, then harass
    std::unordered_map<uint32_t, uint32_t> enemy_hall_defense_score = bot_get_enemy_hall_defense_scores(state, bot);
    if (enemy_hall_defense_score.empty()) {
        return BOT_GOAL_HARASS;
    }

    // If enemy has undefended base, then harass
    bool enemy_has_undefended_base = false;
    for (auto it : enemy_hall_defense_score) {
        if (it.second < 4) {
            enemy_has_undefended_base = true;
        }
    }
    if (enemy_has_undefended_base && bot.strategy != BOT_STRATEGY_BARRACKS) {
        return BOT_GOAL_HARASS;
    }

    // Otherwise push
    return BOT_GOAL_PUSH;
}

// Goal

BotGoal bot_goal_create(const MatchState& state, Bot& bot, BotGoalType goal_type) {
    BotGoal goal;
    goal.type = goal_type;
    memset(goal.desired_entities, 0, sizeof(goal.desired_entities));
    goal.desired_upgrade = 0;
    goal.desired_squad_type = BOT_SQUAD_TYPE_NONE;

    // Count unreserved entities
    int entity_count[ENTITY_TYPE_COUNT];
    memset(entity_count, 0, sizeof(entity_count));
    uint32_t bot_finished_base_count = 0;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (entity.player_id != bot.player_id ||
                !entity_is_selectable(entity) ||
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        entity_count[entity.type]++;
        if (entity.type == ENTITY_HALL && entity.mode == MODE_BUILDING_FINISHED) {
            bot_finished_base_count++;
        }
    }

    switch (goal.type) {
        case BOT_GOAL_BANDIT_RUSH: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_HARASS;
            goal.desired_entities[ENTITY_BANDIT] = 4;
            goal.desired_entities[ENTITY_WAGON] = 1;
            break;
        }
        case BOT_GOAL_BUNKER: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_BUNKER;
            goal.desired_entities[ENTITY_COWBOY] = 4;
            goal.desired_entities[ENTITY_BUNKER] = 1;
            if (bot.scout_enemy_has_detectives) {
                goal.desired_entities[ENTITY_BALLOON] = 1;
            }
            break;
        }
        case BOT_GOAL_EXPAND: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_NONE;
            goal.desired_entities[ENTITY_HALL] = entity_count[ENTITY_HALL] + 1;
            break;
        }
        case BOT_GOAL_LANDMINES: {
            goal.desired_entities[ENTITY_PYRO] = 1;
            goal.desired_squad_type = BOT_SQUAD_TYPE_LANDMINES;
            break;
        }
        case BOT_GOAL_DEFENSE: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_DEFENSE;
            goal.desired_entities[ENTITY_COWBOY] = 4;
            break;
        }
        case BOT_GOAL_DETECTIVE_DEFENSE: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_DEFENSE;
            goal.desired_entities[ENTITY_COWBOY] = 2;
            goal.desired_entities[ENTITY_BALLOON] = 1;
            break;
        }
        case BOT_GOAL_HARASS: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_HARASS;

            // Count enemy landmines
            uint32_t enemy_landmine_count = 0;
            if (bot.scout_enemy_has_landmines) {
                for (const Entity& entity : state.entities) {
                    if (entity.type == ENTITY_LANDMINE && 
                            entity_is_selectable(entity) &&
                            state.players[entity.player_id].team != state.players[bot.player_id].team) {
                        enemy_landmine_count++;
                    }
                }
            }

            // Choose harass type
            enum BotHarassType {
                BOT_HARASS_TYPE_SALOON_INFANTRY,
                BOT_HARASS_TYPE_SALOON_WORKSHOP,
                BOT_HARASS_TYPE_WAGON_DROP,
                BOT_HARASS_TYPE_JOCKEYS,
                BOT_HARASS_TYPE_DETECTIVES,
                BOT_HARASS_TYPE_PYROS,
                BOT_HARASS_TYPE_SOLDIERS,
                BOT_HARASS_TYPE_COUNT
            };
            BotHarassType highest_harass_type = BOT_HARASS_TYPE_COUNT;

            // If we don't have a second base yet, then choose infantry harass
            // This is so that we don't try to tech up too soon
            if (bot_finished_base_count < 2) {
                highest_harass_type = BOT_HARASS_TYPE_SALOON_INFANTRY;
            } else {
                int harass_type_scores[BOT_HARASS_TYPE_COUNT];
                static const int HARASS_TYPE_INVALID_FOR_STRATEGY = -1;
                memset(harass_type_scores, HARASS_TYPE_INVALID_FOR_STRATEGY, sizeof(harass_type_scores));

                // Score strategies based on existing units
                harass_type_scores[BOT_HARASS_TYPE_SALOON_INFANTRY] = entity_count[ENTITY_COWBOY] + entity_count[ENTITY_BANDIT];
                harass_type_scores[BOT_HARASS_TYPE_DETECTIVES] = entity_count[ENTITY_DETECTIVE] + (enemy_landmine_count / 2);

                if (bot.strategy == BOT_STRATEGY_SALOON_COOP) {
                    harass_type_scores[BOT_HARASS_TYPE_WAGON_DROP] = entity_count[ENTITY_COWBOY] + entity_count[ENTITY_BANDIT] + entity_count[ENTITY_SAPPER] + entity_count[ENTITY_WAGON];
                    harass_type_scores[BOT_HARASS_TYPE_JOCKEYS] = entity_count[ENTITY_JOCKEY]; 
                }

                if (bot.strategy == BOT_STRATEGY_SALOON_WORKSHOP) {
                    harass_type_scores[BOT_HARASS_TYPE_SALOON_WORKSHOP] = entity_count[ENTITY_COWBOY] + entity_count[ENTITY_BANDIT] + entity_count[ENTITY_SAPPER];
                    harass_type_scores[BOT_HARASS_TYPE_PYROS] = entity_count[ENTITY_PYRO];
                }

                if (bot.strategy == BOT_STRATEGY_BARRACKS) {
                    harass_type_scores[BOT_HARASS_TYPE_SOLDIERS] = entity_count[ENTITY_SOLDIER];
                }

                for (uint32_t harass_type = 0; harass_type < BOT_HARASS_TYPE_COUNT; harass_type++) {
                    if (harass_type_scores[harass_type] == HARASS_TYPE_INVALID_FOR_STRATEGY) {
                        continue;
                    } 
                    if (highest_harass_type == BOT_HARASS_TYPE_COUNT || harass_type_scores[harass_type] > harass_type_scores[highest_harass_type]) {
                        highest_harass_type = (BotHarassType)harass_type;
                    }
                }
                log_trace("BOT: highest scored harass type %u", highest_harass_type);

                // This assert shouldn't be necessary, but if it fails then the while loop below it will run forever
                GOLD_ASSERT(harass_type_scores[BOT_HARASS_TYPE_SALOON_INFANTRY] != HARASS_TYPE_INVALID_FOR_STRATEGY);

                // If all the entity counts are 0, choose a random harass type
                if (highest_harass_type == BOT_HARASS_TYPE_COUNT) {
                    highest_harass_type = (BotHarassType)(lcg_rand(&bot.lcg_seed) % BOT_HARASS_TYPE_COUNT);

                    // Ensure that the randomly chosen harass type is valid for this strategy
                    while (harass_type_scores[highest_harass_type] == HARASS_TYPE_INVALID_FOR_STRATEGY) {
                        highest_harass_type = (BotHarassType)((highest_harass_type + 1) % BOT_HARASS_TYPE_COUNT);
                    }
                    log_trace("BOT: chose random harass type %u", highest_harass_type);
                }
            }

            GOLD_ASSERT(highest_harass_type != BOT_HARASS_TYPE_COUNT);

            // Choose desired units based on the harass type
            switch (highest_harass_type) {
                case BOT_HARASS_TYPE_SALOON_INFANTRY: {
                    uint32_t infantry_count = 4 + (lcg_rand(&bot.lcg_seed) % 4);
                    goal.desired_entities[ENTITY_BANDIT] = lcg_rand(&bot.lcg_seed) % (infantry_count + 1);
                    goal.desired_entities[ENTITY_COWBOY] = infantry_count - goal.desired_entities[ENTITY_BANDIT];
                    break;
                }
                case BOT_HARASS_TYPE_SALOON_WORKSHOP: {
                    uint32_t infantry_count = 4 + (lcg_rand(&bot.lcg_seed) % 4);
                    goal.desired_entities[ENTITY_SAPPER] = lcg_rand(&bot.lcg_seed) % (infantry_count / 1);
                    goal.desired_entities[ENTITY_COWBOY] = infantry_count - goal.desired_entities[ENTITY_SAPPER];
                    goal.desired_entities[ENTITY_PYRO] = lcg_rand(&bot.lcg_seed) % 3;
                    break;
                }
                case BOT_HARASS_TYPE_WAGON_DROP: {
                    if (entity_count[ENTITY_WAGON] > 1) {
                        goal.desired_entities[ENTITY_WAGON] = 2;
                    } else {
                        goal.desired_entities[ENTITY_WAGON] = 1 + lcg_rand(&bot.lcg_seed) % 2;
                    }

                    uint32_t infantry_count = goal.desired_entities[ENTITY_WAGON] * entity_get_data(ENTITY_WAGON).garrison_capacity;
                    goal.desired_entities[ENTITY_BANDIT] = lcg_rand(&bot.lcg_seed) % (infantry_count + 1);
                    goal.desired_entities[ENTITY_COWBOY] = infantry_count - goal.desired_entities[ENTITY_BANDIT];
                    break;
                }
                case BOT_HARASS_TYPE_JOCKEYS: {
                    goal.desired_entities[ENTITY_JOCKEY] = 4 + (lcg_rand(&bot.lcg_seed) % 5);
                    break;
                }
                case BOT_HARASS_TYPE_DETECTIVES: {
                    goal.desired_entities[ENTITY_DETECTIVE] = 2 + (lcg_rand(&bot.lcg_seed) % 2);
                    break;
                }
                case BOT_HARASS_TYPE_PYROS: {
                    goal.desired_entities[ENTITY_PYRO] = 2;
                    break;
                }
                case BOT_HARASS_TYPE_SOLDIERS: {
                    goal.desired_entities[ENTITY_SOLDIER] = 2 + (lcg_rand(&bot.lcg_seed) % 5);
                    break;
                }
                case BOT_HARASS_TYPE_COUNT: {
                    GOLD_ASSERT(false);
                    break;
                }
            }
            break;
        }
        case BOT_GOAL_PUSH: {
            goal.desired_squad_type = BOT_SQUAD_TYPE_PUSH;

            uint32_t bot_mining_base_count = bot_get_mining_base_count(state, bot);
            uint32_t push_size = 16 + ((lcg_rand(&bot.lcg_seed) % 5) * bot_mining_base_count);

            switch (bot.strategy) {
                case BOT_STRATEGY_SALOON_COOP: {
                    goal.desired_entities[ENTITY_WAGON] = push_size / 8;
                    uint32_t wagon_capacity = goal.desired_entities[ENTITY_WAGON] * entity_get_data(ENTITY_WAGON).garrison_capacity;
                    goal.desired_entities[ENTITY_BANDIT] = lcg_rand(&bot.lcg_seed) % (wagon_capacity + 1);
                    goal.desired_entities[ENTITY_COWBOY] = wagon_capacity - goal.desired_entities[ENTITY_BANDIT];
                    uint32_t population_used_so_far = (goal.desired_entities[ENTITY_COWBOY] + goal.desired_entities[ENTITY_BANDIT] + (2 * goal.desired_entities[ENTITY_WAGON]));
                    if (population_used_so_far < push_size) {
                        goal.desired_entities[ENTITY_JOCKEY] = push_size - population_used_so_far;
                    }
                    if (bot.scout_enemy_has_detectives) {
                        goal.desired_entities[ENTITY_BALLOON] = 2;
                    }
                    break;
                }
                case BOT_STRATEGY_SALOON_WORKSHOP: {
                    goal.desired_entities[ENTITY_PYRO] = 2;
                    goal.desired_entities[ENTITY_SAPPER] = (lcg_rand(&bot.lcg_seed) % 5) * bot_mining_base_count;

                    uint32_t remaining_push_size = push_size - (goal.desired_entities[ENTITY_PYRO] + goal.desired_entities[ENTITY_SAPPER]);
                    goal.desired_entities[ENTITY_BANDIT] = lcg_rand(&bot.lcg_seed) % (remaining_push_size / 2);
                    goal.desired_entities[ENTITY_COWBOY] = remaining_push_size - goal.desired_entities[ENTITY_BANDIT];
                    break;
                }
                case BOT_STRATEGY_BARRACKS: {
                    goal.desired_entities[ENTITY_CANNON] = 2;
                    uint32_t remaining_push_size = push_size - goal.desired_entities[ENTITY_CANNON]; 
                    goal.desired_entities[ENTITY_BANDIT] = lcg_rand(&bot.lcg_seed) % (remaining_push_size / 3);
                    goal.desired_entities[ENTITY_SOLDIER] = remaining_push_size - goal.desired_entities[ENTITY_BANDIT];
                    break;
                }
                case BOT_STRATEGY_COUNT: {
                    GOLD_ASSERT(false);
                    break;
                }
            }

            if (bot.scout_enemy_has_landmines || bot.scout_enemy_has_detectives) {
                goal.desired_entities[ENTITY_BALLOON] = 2;
            }

            break;
        }
        case BOT_GOAL_COUNT: {
            GOLD_ASSERT(false);
        }
    }

    return goal;
}

bool bot_goal_should_be_abandoned(const MatchState& state, const Bot& bot, const BotGoal& goal) {
    if (goal.type == BOT_GOAL_BANDIT_RUSH) {
        EntityId wagon_id = bot_find_entity((BotFindEntityParams) {
            .state = state, 
            .filter = [&bot](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id && 
                            entity.type == ENTITY_WAGON && 
                            entity_is_selectable(entity);
            }
        });
        if (wagon_id == ID_NULL || bot_is_base_under_attack(state, bot)) {
            return true;
        }
    }

    return false;
}

bool bot_goal_is_empty(const BotGoal& goal) {
    if (goal.desired_upgrade != 0) {
        return false;
    }
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (goal.desired_entities[entity_type] > 0) {
            return false;
        }
    }
    return true;
}

bool bot_goal_is_met(const MatchState& state, const Bot& bot, const BotGoal& goal) {
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

    bool is_maxed_out = true;
    uint32_t population = match_get_player_population(state, bot.player_id);
    for (uint32_t unit_type = ENTITY_MINER; unit_type < ENTITY_HALL; unit_type++) {
        if (entity_count[unit_type] >= goal.desired_entities[unit_type]) {
            continue;
        }
        if (population + entity_get_data((EntityType)unit_type).unit_data.population_cost <= MATCH_MAX_POPULATION) {
            is_maxed_out = false;
        }
    }
    bool still_has_desired_buildings = false;
    for (uint32_t building_type = ENTITY_HALL; building_type < ENTITY_TYPE_COUNT; building_type++) {
        if (entity_count[building_type] < goal.desired_entities[building_type]) {
            still_has_desired_buildings = true;
        }
    }
    if (is_maxed_out && !still_has_desired_buildings) {
        return true;
    }

    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (entity_count[entity_type] < goal.desired_entities[entity_type]) {
            return false;
        }
    }

    return true;
}

// Helpers

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

void bot_handle_base_under_attack(const MatchState& state, Bot& bot) {
    // Prepare the goldmines under attack list with each goldmine
    std::unordered_map<uint32_t, uint32_t> hall_surrounding_goldmine;
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        if (state.entities[entity_index].type != ENTITY_GOLDMINE) {
            continue;
        }
        EntityId hall_id = bot_find_hall_surrounding_goldmine(state, bot, state.entities[entity_index]);
        uint32_t hall_index = state.entities.get_index_of(hall_id);
        hall_surrounding_goldmine[entity_index] = hall_index;
    }

    // Helper function to find the nearest hall to a given cell
    std::function<uint32_t(ivec2)> get_nearest_hall_index = [&state, &hall_surrounding_goldmine](ivec2 cell) {
        uint32_t nearest_goldmine_index = INDEX_INVALID;
        for (auto it : hall_surrounding_goldmine) {
            if (nearest_goldmine_index == INDEX_INVALID || 
                    ivec2::manhattan_distance(cell, state.entities[it.first].cell) <
                    ivec2::manhattan_distance(cell, state.entities[nearest_goldmine_index].cell)) {
                nearest_goldmine_index = it.first;
            }
        }
        GOLD_ASSERT(nearest_goldmine_index != INDEX_INVALID);

        return hall_surrounding_goldmine[nearest_goldmine_index];
    };

    // Mark each of the halls that are under attack
    std::vector<uint32_t> halls_under_attack;
    uint32_t hall_count = 0;
    for (const Entity& entity : state.entities) {
        if (!(entity_is_building(entity.type) || entity.type == ENTITY_MINER) ||
                !entity_is_selectable(entity) ||
                entity.player_id != bot.player_id) {
            continue;
        }

        EntityId nearby_enemy_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&state, &entity](const Entity& enemy, EntityId enemy_id) {
                return entity_is_unit(enemy.type) &&
                        entity_is_selectable(enemy) &&
                        (enemy.type == ENTITY_PYRO || entity_get_data(enemy.type).unit_data.damage != 0) &&
                        state.players[enemy.player_id].team != state.players[entity.player_id].team &&
                        match_is_entity_visible_to_player(state, enemy, entity.player_id) &&
                        ivec2::manhattan_distance(entity.cell, enemy.cell) < BOT_SQUAD_GATHER_DISTANCE;
            }
        });
        if (nearby_enemy_id != ID_NULL) {
            uint32_t nearest_hall_index = get_nearest_hall_index(entity.cell);
            if (nearest_hall_index != INDEX_INVALID) {
                halls_under_attack.push_back(nearest_hall_index);
            }
        }
        if (entity.type == ENTITY_HALL) {
            hall_count++;
        }
    }

    // For each goldmine under attack, determine how to respond
    for (uint32_t hall_index : halls_under_attack) {
        // Determine the strength of the attacking force
        uint32_t enemy_score = 0;
        for (const Entity& enemy : state.entities) {
            if (!entity_is_unit(enemy.type) ||
                    !entity_is_selectable(enemy) ||
                    state.players[enemy.player_id].team == state.players[bot.player_id].team ||
                    !match_is_entity_visible_to_player(state, enemy, bot.player_id) ||
                    ivec2::manhattan_distance(enemy.cell, state.entities[hall_index].cell) > BOT_SQUAD_GATHER_DISTANCE * 2) {
                continue;
            }

            uint32_t nearest_hall_index = get_nearest_hall_index(match_get_entity_target_cell(state, enemy));
            if (nearest_hall_index != hall_index) {
                continue;
            }

            enemy_score += bot_score_entity(enemy);
        }

        // Determine the strength of the defending force
        uint32_t defending_score = 0;
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const Entity& entity = state.entities[entity_index];
            EntityId entity_id = state.entities.get_id_of(entity_index);

            // Note that we're filtering out reserved entities here because we will count the squads separately
            if (entity.type == ENTITY_GOLDMINE ||
                    entity.type == ENTITY_MINER ||
                    (entity_is_building(entity.type) && entity.type != ENTITY_BUNKER) ||
                    !entity_is_selectable(entity) ||
                    entity.player_id != bot.player_id ||
                    bot_is_entity_reserved(bot, entity_id) ||
                    ivec2::manhattan_distance(entity.cell, state.entities[hall_index].cell) > BOT_SQUAD_GATHER_DISTANCE * 2) {
                continue;
            }

            defending_score += bot_score_entity(entity);
        }

        // Add any defending squads to the score
        for (const BotSquad& squad : bot.squads) {
            if (!(squad.type == BOT_SQUAD_TYPE_BUNKER || 
                    squad.type == BOT_SQUAD_TYPE_RESERVES ||
                    squad.type == BOT_SQUAD_TYPE_DEFENSE)) {
                continue;
            }

            // Ignore this squad if it's not close to the hall that is under attack
            if (ivec2::manhattan_distance(squad.target_cell, state.entities[hall_index].cell) > BOT_SQUAD_GATHER_DISTANCE * 2) {
                continue;
            }

            // Tally up defending score for each entity in the squad
            for (EntityId entity_id : squad.entities) {
                uint32_t entity_index = state.entities.get_index_of(entity_id);
                if (entity_index == INDEX_INVALID || 
                        !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }

                defending_score += bot_score_entity(state.entities[entity_index]);
            }
        }

        // If this base is already well defended, then don't do anything
        if (defending_score > enemy_score + 8) {
            continue;
        }

        // Otherwise, the base is not well defended, so let's see if there is anything we can do

        // First, see if we have any idle defense squads that we can flex to defend
        int nearest_idle_defending_squad_index = -1;
        for (int squad_index = 0; squad_index < bot.squads.size(); squad_index++) {
            const BotSquad& squad = bot.squads[squad_index];
            if (squad.type != BOT_SQUAD_TYPE_DEFENSE || 
                    ivec2::manhattan_distance(squad.target_cell, state.entities[hall_index].cell) < BOT_SQUAD_GATHER_DISTANCE ||
                    bot_squad_is_engaged(state, bot, squad)) {
                continue;
            }
            if (nearest_idle_defending_squad_index == -1 ||
                    ivec2::manhattan_distance(squad.target_cell, state.entities[hall_index].cell) <
                    ivec2::manhattan_distance(bot.squads[nearest_idle_defending_squad_index].target_cell, state.entities[hall_index].cell)) {
                nearest_idle_defending_squad_index = squad_index;
            }
        }
        if (nearest_idle_defending_squad_index != -1) {
            bot.squads[nearest_idle_defending_squad_index].target_cell = state.entities[hall_index].cell;
            continue;
        }
        
        // Next, check to see if we have any unreserved units to create a defense squad with
        BotSquad defend_squad;
        defend_squad.type = BOT_SQUAD_TYPE_RESERVES;
        defend_squad.target_cell = state.entities[hall_index].cell;
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const Entity& entity = state.entities[entity_index];
            EntityId entity_id = state.entities.get_id_of(entity_index);
            if (!entity_is_unit(entity.type) ||
                    entity.type == ENTITY_MINER ||
                    entity.type == ENTITY_WAGON ||
                    !entity_is_selectable(entity) ||
                    entity.player_id != bot.player_id ||
                    bot_is_entity_reserved(bot, entity_id)) {
                continue;
            }

            defend_squad.entities.push_back(entity_id);
        }
        if (!defend_squad.entities.empty()) {
            // Reserve all the entities
            for (EntityId entity_id : defend_squad.entities) {
                bot_reserve_entity(bot, entity_id);
            }

            bot.squads.push_back(defend_squad);
            continue;
        }

        // Next check attacking squads to see if they should turn around and defend
        for (BotSquad& squad : bot.squads) {
            if (!(squad.type == BOT_SQUAD_TYPE_PUSH || squad.type == BOT_SQUAD_TYPE_HARASS)) {
                continue;
            }

            // Count how many units are closer to the target than they are to the under-attack base
            uint32_t squad_units_closer_to_target = 0;
            uint32_t squad_units_closer_to_base = 0;
            for (EntityId entity_id : squad.entities) {
                uint32_t entity_index = state.entities.get_index_of(entity_id);
                if (entity_index == INDEX_INVALID) {
                    continue;
                }

                if (ivec2::manhattan_distance(state.entities[entity_index].cell, squad.target_cell) <
                        ivec2::manhattan_distance(state.entities[entity_index].cell, state.entities[hall_index].cell)) {
                    squad_units_closer_to_target++;
                } else {
                    squad_units_closer_to_base++;
                }
            }

            // If they are closer to the target, keep attacking the target
            if (squad_units_closer_to_target > squad_units_closer_to_base) {
                continue;
            }

            // Otherwise, switch targets to defend the base
            squad.type = BOT_SQUAD_TYPE_RESERVES;
            squad.target_cell = state.entities[hall_index].cell;
            continue;
        }

        // Finally if we're really desperate, use workers to attack
        // We can re-use the defend_squad for this
        GOLD_ASSERT(defend_squad.entities.empty());
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const Entity& entity = state.entities[entity_index];
            EntityId entity_id = state.entities.get_id_of(entity_index);
            if (entity.type != ENTITY_MINER ||
                    entity.player_id != bot.player_id ||
                    bot_is_entity_reserved(bot, entity_id) ||
                    entity.health == 0 ||
                    get_nearest_hall_index(entity.cell) != hall_index) {
                continue;
            }

            defend_squad.entities.push_back(entity_id);
        }
        if (!defend_squad.entities.empty()) {
            // Reserve all the entities
            for (EntityId entity_id : defend_squad.entities) {
                bot_reserve_entity(bot, entity_id);
            }

            bot.squads.push_back(defend_squad);
            continue;
        }

        // If there wasn't anything we could do, should we surrender?
        if (bot.should_surrender && 
                hall_count <= 1 &&
                defending_score <= enemy_score / 2) {
            bot.has_surrendered = true;
        }
    } // End for each goldmine under attack
}

uint32_t bot_get_mining_base_count(const MatchState& state, const Bot& bot) {
    uint32_t bot_mining_base_count = 0;
    for (const Entity& goldmine : state.entities) {
        if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
            continue;
        }
        EntityId surrounding_base_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);
        if (surrounding_base_id == ID_NULL) {
            continue;
        }
        if (state.entities.get_by_id(surrounding_base_id).player_id == bot.player_id) {
            bot_mining_base_count++;
        }
    }

    return bot_mining_base_count;
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

bool bot_is_rally_cell_valid(const MatchState& state, ivec2 rally_cell, int rally_margin) {
    for (int y = rally_cell.y - rally_margin; y < rally_cell.y + rally_margin + 1; y++) {
        for (int x = rally_cell.x - rally_margin; x < rally_cell.x + rally_margin + 1; x++) {
            ivec2 cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                return false;
            }
            if (map_is_tile_ramp(state.map, cell)) {
                return false;
            }
            Cell map_cell = map_get_cell(state.map, CELL_LAYER_GROUND, cell);
            if (map_cell.type == CELL_BUILDING || map_cell.type == CELL_GOLDMINE || map_cell.type == CELL_BLOCKED) {
                return false;
            }
        }
    }

    return true;
}

ivec2 bot_choose_building_rally_point(const MatchState& state, const Bot& bot, const Entity& building) {
    std::vector<ivec2> frontier;
    std::vector<bool> explored(state.map.width * state.map.height, false);
    frontier.push_back(building.cell);
    ivec2 fallback_rally_point = ivec2(-1, -1);

    while (!frontier.empty()) {
        ivec2 next = frontier[0];
        frontier.erase(frontier.begin());

        if (explored[next.x + (next.y * state.map.width)]) {
            continue;
        }

        static const int RALLY_MARGIN = 4;
        if (bot_is_rally_cell_valid(state, next, RALLY_MARGIN)) {
            return next;
        }
        if (fallback_rally_point.x == -1 && bot_is_rally_cell_valid(state, next, 2)) {
            fallback_rally_point = next;
        }

        explored[next.x + (next.y * state.map.width)] = true;
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(state.map, child)) {
                continue;
            }
            if (map_is_tile_ramp(state.map, child)) {
                continue;
            }
            if (map_get_tile(state.map, next).elevation != map_get_tile(state.map, child).elevation) {
                continue;
            }
            frontier.push_back(child);
        }
    }

    return fallback_rally_point;
}

MatchInput bot_unload_unreserved_carriers(const MatchState& state, const Bot& bot) {
    EntityId carrier_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& carrier, EntityId carrier_id) {
            return carrier.player_id == bot.player_id &&
                    entity_is_selectable(carrier) &&
                    !carrier.garrisoned_units.empty() &&
                    carrier.target.type == TARGET_NONE &&
                    !bot_is_entity_reserved(bot, carrier_id);
        }
    });
    if (carrier_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }
    
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_UNLOAD;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = state.entities.get_by_id(carrier_id).cell;
    input.move.entity_ids[0] = carrier_id;
    input.move.entity_count = 1;
    return input;
}

MatchInput bot_rein_in_stray_units(const MatchState& state, const Bot& bot) {
    MatchInput input = (MatchInput) { .type = MATCH_INPUT_NONE };

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        // Filter down to bot-owned, unreserved units
        if (!entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                entity.player_id != bot.player_id ||
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        // Don't interrupt units with targets
        if (!(entity.target.type == TARGET_ATTACK_ENTITY || entity.target.type == TARGET_NONE)) {
            continue;
        }

        // If the unit is attacking something, don't bother it
        uint32_t target_index = entity.target.type == TARGET_ATTACK_ENTITY ? state.entities.get_index_of(entity.target.id) : INDEX_INVALID;
        if (target_index != INDEX_INVALID && ivec2::manhattan_distance(entity.cell, state.entities[target_index].cell) < BOT_SQUAD_GATHER_DISTANCE) {
            continue;
        }

        // If this unit is already nearby an allied building, skip
        EntityId nearby_allied_building_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&entity](const Entity& building, EntityId building_id) {
                return entity_is_building(building.type) &&
                        entity_is_selectable(building) &&
                        entity.player_id == building.player_id &&
                        ivec2::manhattan_distance(entity.cell, building.cell) < BOT_SQUAD_GATHER_DISTANCE * 2;
            }
        });
        if (nearby_allied_building_id != ID_NULL) {
            continue;
        }

        // If we've gotten all the way down here, it's time to rein in the unit

        // But first let's check the input, if the input is none, then start a new retreat input
        if (input.type == MATCH_INPUT_NONE) {
            input = bot_return_entity_to_nearest_hall(state, bot, entity_id);
            continue;
        } 

        // Otherwise if the input is not none, then add this unit to it
        input.move.entity_ids[input.move.entity_count] = entity_id;
        input.move.entity_count++;
        if (input.move.entity_count == SELECTION_LIMIT) {
            break;
        }
    } // End for each entity

    return input;
}

MatchInput bot_repair_burning_buildings(const MatchState& state, const Bot& bot) {
    EntityId building_in_need_of_repair_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot](const Entity& building, EntityId building_id) {
            if (building.player_id != bot.player_id ||
                    building.mode != MODE_BUILDING_FINISHED ||
                    building.health > entity_get_data(building.type).max_health / 2) {
                return false;
            }

            EntityId existing_repairer = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&building_id](const Entity& repairer, EntityId repairer_id) {
                    return repairer.target.type == TARGET_REPAIR && repairer.target.id == building_id;
                }
            });
            if (existing_repairer != ID_NULL) {
                return false;
            }

            return true;
        }
    });
    if (building_in_need_of_repair_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    const Entity& building = state.entities.get_by_id(building_in_need_of_repair_id);
    EntityId repairer_id = bot_find_nearest_idle_worker(state, bot, building.cell);
    if (repairer_id == ID_NULL) {
        Target goldmine_target = match_entity_target_nearest_gold_mine(state, building);
        if (goldmine_target.type == TARGET_ENTITY) {
            repairer_id = bot_pull_worker_off_gold(state, bot, goldmine_target.id);
        }
    }
    if (repairer_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_REPAIR;
    input.move.shift_command = 0;
    input.move.target_cell = ivec2(0, 0);
    input.move.target_id = building_in_need_of_repair_id;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = repairer_id;
    return input;
}
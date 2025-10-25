#include "bot.h"

#include "lcg.h"
#include "upgrade.h"
#include "core/logger.h"

static const uint32_t BOT_DEFEND_COUNTERATTACK = 1;
static const uint32_t BOT_DEFEND_WITH_WORKERS = 2;

static const uint32_t BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES = 1;
static const uint32_t BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES = 2;

static const int BOT_UNIT_SCORE = 4;
static const int BOT_UNIT_SCORE_IN_BUNKER = 6;
static const int BOT_UNIT_SCORE_FULL_BUNKER = 4 * BOT_UNIT_SCORE_IN_BUNKER;
static const int BOT_SQUAD_GATHER_DISTANCE = 16;
static const uint32_t BOT_MAX_LANDMINES_PER_BASE = 6;
static const uint32_t BOT_HARASS_SQUAD_MAX_SIZE = 8;

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed) {
    Bot bot;

    bot.player_id = player_id;
    bot.lcg_seed = lcg_seed;
    bot.mode = (BotMode)(lcg_rand(&bot.lcg_seed) % 3);

    bot.unit_comp = BOT_UNIT_COMP_COWBOY_BANDIT;

    bot.scout_id = ID_NULL;
    bot.last_scout_time = 0;
    bot.scout_info = 0;

    return bot;
}

MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    if (bot.mode == BOT_MODE_SURRENDER) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    bot_scout_update(state, bot, match_time_minutes);

    // Strategy

    bool is_base_under_attack = bot_handle_base_under_attack(state, bot);
    bot_strategy_update(state, bot, is_base_under_attack);

    // Production

    MatchInput production_input = bot_get_production_input(state, bot, is_base_under_attack);
    if (production_input.type != MATCH_INPUT_NONE) {
        return production_input;
    }

    // Squad

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

    // Scout

    MatchInput scout_input = bot_scout(state, bot, match_time_minutes);
    if (scout_input.type != MATCH_INPUT_NONE) {
        return scout_input;
    }

    // Misc

    MatchInput build_cancel_input = bot_cancel_in_progress_buildings(state, bot);
    if (build_cancel_input.type != MATCH_INPUT_NONE) {
        return build_cancel_input;
    }

    MatchInput repair_input = bot_repair_burning_buildings(state, bot);
    if (repair_input.type != MATCH_INPUT_NONE) {
        return repair_input;
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

bool bot_has_surrendered(const Bot& bot) {
    return bot.mode == BOT_MODE_SURRENDER;
}

// STRATEGY

void bot_strategy_update(const MatchState& state, Bot& bot, bool is_base_under_attack) {
    // Count unreserved entities
    BotEntityCount unreserved_entity_count = bot_entity_count_empty();
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        if (entity.player_id != bot.player_id || 
                entity.health == 0 ||
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        unreserved_entity_count[entity.type]++;
    }

    // Create squads from desired squads
    uint32_t desired_squad_index = 0;
    while (desired_squad_index < bot.desired_squads.size()) {
        if (bot_entity_count_is_gte_to(unreserved_entity_count, bot.desired_squads[desired_squad_index].entities)) {
            bot_squad_create_from_entity_count(state, bot, bot.desired_squads[desired_squad_index].type, bot.desired_squads[desired_squad_index].entities);
            unreserved_entity_count = bot_entity_count_subtract(unreserved_entity_count, bot.desired_squads[desired_squad_index].entities);

            bot.desired_squads[desired_squad_index] = bot.desired_squads.back();
            bot.desired_squads.pop_back();
            continue;
        }

        desired_squad_index++;
    }

    // Get strategic metrics
    int allied_army_score = 0;
    int enemy_army_score = 0;
    uint32_t bot_miner_count = 0;
    int bot_unreserved_unit_score = 0;
    int bot_unit_count = 0;
    int bot_hall_count = 0;

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        if (entity.health == 0) {
            continue;
        }

        if (entity.type == ENTITY_HALL && entity.player_id == bot.player_id) {
            bot_hall_count++;
        }

        if (!entity_is_unit(entity.type)) {
            continue;
        }

        if (entity.player_id == bot.player_id) {
            bot_unit_count++;
        }

        if (entity.type == ENTITY_MINER && entity.player_id == bot.player_id) {
            bot_miner_count++;
        }
        if (entity.type == ENTITY_MINER) {
            continue;
        }

        if (entity.player_id == bot.player_id && !bot_is_entity_reserved(bot, entity_id)) {
            bot_unreserved_unit_score += bot_score_entity(state, bot, entity, entity_id);
        }

        if (state.players[entity.player_id].team == state.players[bot.player_id].team) {
            allied_army_score += bot_score_entity(state, bot, entity, entity_id);
        } else {
            enemy_army_score += bot_score_entity(state, bot, entity, entity_id);
        }
    }

    // Surrender
    if (bot_hall_count <= 1 && bot_unit_count == 0 && is_base_under_attack) {
        // TODO: Idea, have a surrender_timer that starts whenever the bot is clearly disadvantaged
        // If the surrender timer is allowed to countdown to 0, then the bot surrenders
        // If the bot regains an advantage, the surrender timer stops / resets
        bot.mode = BOT_MODE_SURRENDER;
    }

    // Check if we need to switch unit comps for detection
    if (bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES) ||
            bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES)) {
        switch (bot.unit_comp) {
            case BOT_UNIT_COMP_COWBOY_BANDIT:
            case BOT_UNIT_COMP_COWBOY_BANDIT_WAGON: {
                bot.unit_comp = BOT_UNIT_COMP_COWBOY_BANDIT_PYRO;
                break;
            }
            case BOT_UNIT_COMP_COWBOY_BANDIT_WAGON_JOCKEY: {
                bot.unit_comp = BOT_UNIT_COMP_SOLDIER_BANDIT;
                break;
            }
            default: {
                break;
            }
        }
    }

    switch (bot.mode) {
        case BOT_MODE_OPENER_BANDIT_RUSH: {
            // Abandon rush is wagon has died
            EntityId wagon_id = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&bot](const Entity& entity, EntityId entity_id) {
                    return entity.type == ENTITY_WAGON && entity_is_selectable(entity) && entity.player_id == bot.player_id;
                }
            });
            if (wagon_id == ID_NULL) {
                bot.mode = BOT_MODE_OPENER_BUNKER;
                break;
            }

            // If we have a squad, we can assume it is the rush squad
            // Switch to expand mode
            if (!bot.squads.empty()) {
                bot.mode = BOT_MODE_EXPAND;
                break;
            }

            // If we do not have a squad and we do not have a desired squad,
            // then create the desired rush squad
            if (bot.desired_squads.empty()) {
                BotDesiredSquad squad;
                squad.type = BOT_SQUAD_TYPE_ATTACK;
                squad.entities = bot_entity_count_empty();
                squad.entities[ENTITY_BANDIT] = 4;
                squad.entities[ENTITY_WAGON] = 1;
                bot.desired_squads.push_back(squad);
            }

            break;
        }
        case BOT_MODE_OPENER_BUNKER: {
            // If we're under attack, stop trying to make bunker and just defend with units
            if (is_base_under_attack) {
                bot.mode = BOT_MODE_ATTACK;
                break;
            }

            // If we have a squad, we can assume it is the bunker squad
            // Switch to expand mode
            if (!bot.squads.empty()) {
                bot.mode = BOT_MODE_EXPAND;
                break;
            }

            // If we do not have a squad and we do not have a desired squad,
            // then create the desired bunker squad
            if (bot.desired_squads.empty()) {
                BotDesiredSquad squad;
                squad.type = BOT_SQUAD_TYPE_DEFEND;
                squad.entities = bot_entity_count_empty();
                squad.entities[ENTITY_COWBOY] = 4;
                squad.entities[ENTITY_BUNKER] = 1;
                bot.desired_squads.push_back(squad);
            }

            break;
        }
        case BOT_MODE_EXPAND: {
            // If we have a hall-in progress, then switch back to attack mode
            EntityId in_progress_hall_id = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&bot](const Entity& entity, EntityId entity_id) {
                    return entity.type == ENTITY_HALL &&
                            entity.mode == MODE_BUILDING_IN_PROGRESS &&
                            entity.player_id == bot.player_id;
                }
            });
            if (in_progress_hall_id != ID_NULL) {
                bot.mode = BOT_MODE_ATTACK;
                break;
            }

            // If there are no more goldmines to expand to, then switch back to attack mode
            uint32_t next_hall_goldmine_index = bot_find_next_hall_goldmine_index(state, bot);
            if (next_hall_goldmine_index == INDEX_INVALID) {
                bot.mode = BOT_MODE_ATTACK;
                break;
            }

            // Secure goldmine area if enemy tries to stop us from expanding
            ivec2 goldmine_cell = state.entities[next_hall_goldmine_index].cell;
            if (!bot_is_area_safe(state, bot, goldmine_cell)) {
                bot_defend_location(state, bot, goldmine_cell, 0);
            }

            break;
        }
        case BOT_MODE_ATTACK: {
            // Count mining bases
            uint32_t mining_base_count[MAX_PLAYERS];
            memset(mining_base_count, 0, sizeof(mining_base_count));
            uint32_t bot_fully_saturated_base_count = 0;
            uint32_t unoccupied_goldmine_count = 0;
            for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
                const Entity& goldmine = state.entities[goldmine_index];
                if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
                    continue;
                }
                EntityId hall_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);
                if (hall_id == ID_NULL) {
                    unoccupied_goldmine_count++;
                    continue;
                }
                const Entity& hall = state.entities.get_by_id(hall_id);
                if (!bot_has_scouted_entity(state, bot, hall, hall_id)) {
                    continue;
                }
                mining_base_count[hall.player_id]++;
                EntityId goldmine_id = state.entities.get_id_of(goldmine_index);
                if (hall.player_id == bot.player_id && match_get_miners_on_gold(state, goldmine_id, bot.player_id) == MATCH_MAX_MINERS_ON_GOLD) {
                    bot_fully_saturated_base_count++;
                }
            }

            // Count max enemy mining base
            uint32_t max_enemy_mining_base_count = 0;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (!state.players[player_id].active ||
                        state.players[player_id].team == state.players[bot.player_id].team) {
                    continue;
                }
                max_enemy_mining_base_count = std::max(max_enemy_mining_base_count, mining_base_count[player_id]);
            }

            // Decide whether to expand
            uint32_t target_base_count = std::max(2U, max_enemy_mining_base_count);
            if (unoccupied_goldmine_count != 0 && 
                    mining_base_count[bot.player_id] < target_base_count && 
                    mining_base_count[bot.player_id] == bot_fully_saturated_base_count) {
                bot.mode = BOT_MODE_EXPAND;
                break;
            }
            
            // Decide whether to tech switch
            if (mining_base_count[bot.player_id] > 1 && 
                    mining_base_count[bot.player_id] == bot_fully_saturated_base_count &&
                    allied_army_score > 7 * BOT_UNIT_SCORE) {
                switch (bot.unit_comp) {
                    case BOT_UNIT_COMP_COWBOY_BANDIT: {
                        const uint32_t NEW_UNIT_COMP_COUNT = 4;
                        BotUnitComp new_unit_comps[NEW_UNIT_COMP_COUNT] = { 
                            BOT_UNIT_COMP_COWBOY_BANDIT_PYRO, 
                            BOT_UNIT_COMP_COWBOY_SAPPER_PYRO, 
                            BOT_UNIT_COMP_COWBOY_BANDIT_WAGON, 
                            BOT_UNIT_COMP_SOLDIER_BANDIT 
                        };
                        uint32_t new_unit_comp_roll = lcg_rand(&bot.lcg_seed) % NEW_UNIT_COMP_COUNT;

                        bot.unit_comp = new_unit_comps[new_unit_comp_roll];
                        break;
                    }
                    case BOT_UNIT_COMP_SOLDIER_BANDIT: {
                        // Soldier bandit is an intermediary unit comp before we get into soldier cannon
                        // So only switch into soldier cannon if we have some barracks setup already
                        BotEntityCount desired_buildings, desired_army_ratio;
                        bot_get_desired_buildings_and_army_ratio(state, bot, desired_buildings, desired_army_ratio);
                        if (unreserved_entity_count[ENTITY_BARRACKS] == desired_buildings[ENTITY_BARRACKS]) {
                            bot.unit_comp = BOT_UNIT_COMP_SOLDIER_CANNON;
                        }
                        break;
                    }
                    case BOT_UNIT_COMP_COWBOY_BANDIT_WAGON: {
                        // Switch from intermediate comp to full comp
                        BotEntityCount desired_buildings, desired_army_ratio;
                        bot_get_desired_buildings_and_army_ratio(state, bot, desired_buildings, desired_army_ratio);
                        if (unreserved_entity_count[ENTITY_COOP] == desired_buildings[ENTITY_COOP]) {
                            bot.unit_comp = BOT_UNIT_COMP_COWBOY_BANDIT_WAGON_JOCKEY;
                        }
                        break;
                    }
                    case BOT_UNIT_COMP_COWBOY_BANDIT_WAGON_JOCKEY:
                    case BOT_UNIT_COMP_COWBOY_BANDIT_PYRO:
                    case BOT_UNIT_COMP_COWBOY_SAPPER_PYRO:
                    case BOT_UNIT_COMP_SOLDIER_CANNON: {
                        // Don't switch out of these comps
                        break;
                    }
                }
            }
            
            // Determine desired army ratio
            BotEntityCount desired_buildings, desired_army_ratio;
            bot_get_desired_buildings_and_army_ratio(state, bot, desired_buildings, desired_army_ratio);

            // Request landmines squad
            if (desired_army_ratio[ENTITY_PYRO] != 0 && !bot_has_landmine_squad(bot) && !bot_has_desired_squad_of_type(bot, BOT_SQUAD_TYPE_LANDMINES)) {
                BotDesiredSquad squad;
                squad.type = BOT_SQUAD_TYPE_LANDMINES;
                squad.entities = bot_entity_count_empty();
                squad.entities[ENTITY_PYRO] = 1;
                bot.desired_squads.push_back(squad);
            }

            // Reinforce
            uint32_t attack_squad_index = 0;
            while (attack_squad_index < bot.squads.size()) {
                if (bot.squads[attack_squad_index].type == BOT_SQUAD_TYPE_ATTACK) {
                    break;
                }
                attack_squad_index++;
            }
            // If we have an attack squad
            if (attack_squad_index < bot.squads.size() && 
                    // And we have units to reinforce with
                    bot_unreserved_unit_score > 0 &&
                    // And the attack squad is big enough (i.e. not a harassment squad)
                    bot.squads[attack_squad_index].entities.size() > BOT_HARASS_SQUAD_MAX_SIZE &&
                    // And the attack squad is not retreating
                    !bot_squad_should_retreat(state, bot, bot.squads[attack_squad_index])) {
                // Then reinforce
                BotEntityCount reinforce_entity_count = bot_entity_count_empty();
                for (uint32_t entity_type = ENTITY_MINER + 1; entity_type < ENTITY_HALL; entity_type++) {
                    if (entity_type == ENTITY_WAGON && desired_army_ratio[ENTITY_WAGON] == 0) {
                        continue;
                    }
                    reinforce_entity_count[entity_type] = unreserved_entity_count[entity_type];
                    unreserved_entity_count[entity_type] = 0;
                }

                // bot_squad_create should reinforce the existing squad
                std::vector<EntityId> reinforcements = bot_create_entity_list_from_entity_count(state, bot, reinforce_entity_count);
                bot_squad_create(state, bot, BOT_SQUAD_TYPE_ATTACK, bot.squads[attack_squad_index].target_cell, reinforcements);
            }

            // Harass / Push
            const int least_defended_hall_score = bot_get_least_defended_enemy_hall_score(state, bot);
            const int bot_minimum_attack_threshold = least_defended_hall_score < BOT_UNIT_SCORE_FULL_BUNKER 
                                            ? BOT_UNIT_SCORE_FULL_BUNKER 
                                            : 32 * BOT_UNIT_SCORE;
            const int bot_attack_threshold = std::max(least_defended_hall_score + (BOT_UNIT_SCORE * 4), bot_minimum_attack_threshold);
            const bool should_attack = 
                    // Attack if we have a bigger army than our opponent
                    (bot_unreserved_unit_score > bot_attack_threshold && allied_army_score > enemy_army_score) ||
                    // Attack if we are maxed out
                    match_get_player_population(state, bot.player_id) >= 98 ||
                    // And if we have no miners and no money to get more, then attack with everything we've got
                    (bot_unreserved_unit_score > 0 && bot_miner_count == 0 && bot_get_effective_gold(state, bot) < entity_get_data(ENTITY_MINER).gold_cost);
            if (should_attack) {
                BotEntityCount attack_entity_count = bot_entity_count_empty();
                for (uint32_t entity_type = ENTITY_MINER + 1; entity_type < ENTITY_HALL; entity_type++) {
                    if (entity_type == ENTITY_WAGON && desired_army_ratio[ENTITY_WAGON] == 0) {
                        continue;
                    }
                    attack_entity_count[entity_type] = unreserved_entity_count[entity_type];
                }
                bot_squad_create_from_entity_count(state, bot, BOT_SQUAD_TYPE_ATTACK, attack_entity_count);
            }
            break;
        }
        case BOT_MODE_SURRENDER:
            break;
    }
}

bool bot_handle_base_under_attack(const MatchState& state, Bot& bot) {
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
    }

    // Defend under-attack bases
    for (uint32_t hall_index : halls_under_attack) {
        bot_defend_location(state, bot, state.entities[hall_index].cell, BOT_DEFEND_COUNTERATTACK | BOT_DEFEND_WITH_WORKERS);
    }

    return !halls_under_attack.empty();
}

bool bot_is_area_safe(const MatchState& state, const Bot& bot, ivec2 cell) {
    EntityId nearby_enemy_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &cell](const Entity& entity, EntityId entity_id) {
            return entity.type != ENTITY_GOLDMINE &&
                    entity_is_selectable(entity) &&
                    state.players[entity.player_id].team != state.players[bot.player_id].team &&
                    ivec2::manhattan_distance(entity.cell, cell) < BOT_SQUAD_GATHER_DISTANCE * 2;
        }
    });

    return nearby_enemy_id == ID_NULL;
}

void bot_defend_location(const MatchState& state, Bot& bot, ivec2 location, uint32_t options) {
    // Determine the strength of the attacking force
    uint32_t enemy_score = 0;
    for (uint32_t enemy_index = 0; enemy_index < state.entities.size(); enemy_index++) {
        const Entity& enemy = state.entities[enemy_index];
        EntityId enemy_id = state.entities.get_id_of(enemy_index);

        if (!entity_is_unit(enemy.type) ||
                !entity_is_selectable(enemy) ||
                state.players[enemy.player_id].team == state.players[bot.player_id].team) {
            continue;
        }

        ivec2 enemy_cell = enemy.cell;
        if (!match_is_target_invalid(state, enemy.target, enemy.player_id)) {
            enemy_cell = match_get_entity_target_cell(state, enemy);
        }
        if (ivec2::manhattan_distance(enemy_cell, location) > BOT_SQUAD_GATHER_DISTANCE * 2) {
            continue;
        }

        enemy_score += bot_score_entity(state, bot, enemy, enemy_id);
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
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        ivec2 entity_cell = entity.cell;
        if (!match_is_target_invalid(state, entity.target, entity.player_id)) {
            entity_cell = match_get_entity_target_cell(state, entity);
        }
        if (ivec2::manhattan_distance(entity.cell, location) > BOT_SQUAD_GATHER_DISTANCE * 2) {
            continue;
        }

        defending_score += bot_score_entity(state, bot, entity, entity_id);
    }

    // Add any defending squads to the score
    for (const BotSquad& squad : bot.squads) {
        if (!(squad.type == BOT_SQUAD_TYPE_DEFEND || 
                squad.type == BOT_SQUAD_TYPE_RESERVES)) {
            continue;
        }

        // Ignore this squad if it's not close to the hall that is under attack
        if (ivec2::manhattan_distance(squad.target_cell, location) > BOT_SQUAD_GATHER_DISTANCE * 2) {
            continue;
        }

        // Tally up defending score for each entity in the squad
        for (EntityId entity_id : squad.entities) {
            uint32_t entity_index = state.entities.get_index_of(entity_id);
            if (entity_index == INDEX_INVALID || 
                    !entity_is_selectable(state.entities[entity_index])) {
                continue;
            }

            defending_score += bot_score_entity(state, bot, state.entities[entity_index], entity_id);
        }
    }

    // If this location is already well defended, then don't do anything
    if (defending_score > enemy_score + 8) {
        return;
    }

    // Otherwise, the location is not well defended, so let's see if there is anything we can do

    // First, see if we have any idle defense squads that we can flex to defend
    int nearest_idle_defending_squad_index = -1;
    for (int squad_index = 0; squad_index < bot.squads.size(); squad_index++) {
        const BotSquad& squad = bot.squads[squad_index];
        if (squad.type != BOT_SQUAD_TYPE_DEFEND || 
                ivec2::manhattan_distance(squad.target_cell, location) < BOT_SQUAD_GATHER_DISTANCE ||
                bot_squad_is_engaged(state, bot, squad)) {
            continue;
        }

        // Don't send in a squad if it's far away, unless we feel pretty sure that this squad could cleanup the attack
        if (ivec2::manhattan_distance(squad.target_cell, location) > BOT_SQUAD_GATHER_DISTANCE * 2 &&
                squad.entities.size() < enemy_score + 8) {
            continue;
        }
        if (nearest_idle_defending_squad_index == -1 ||
                ivec2::manhattan_distance(squad.target_cell, location) <
                ivec2::manhattan_distance(bot.squads[nearest_idle_defending_squad_index].target_cell, location)) {
            nearest_idle_defending_squad_index = squad_index;
        }
    }
    if (nearest_idle_defending_squad_index != -1) {
        bot.squads[nearest_idle_defending_squad_index].target_cell = location;
        return;
    }
    
    // Next, check to see if we have any unreserved units to create a defense squad with
    std::vector<EntityId> defend_squad_entities;
    uint32_t defend_squad_score = 0;
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

        defend_squad_entities.push_back(entity_id);
        defend_squad_score += bot_score_entity(state, bot, entity, entity_id);
    }
    if (!defend_squad_entities.empty() && 
            (ivec2::manhattan_distance(bot_squad_get_center_point(state, defend_squad_entities), location) < BOT_SQUAD_GATHER_DISTANCE * 2 ||
            defend_squad_score + defending_score >= enemy_score)) {
        bot_squad_create(state, bot, BOT_SQUAD_TYPE_RESERVES, location, defend_squad_entities);
        return;
    }

    // Next check attacking squads to see if they should turn around and defend
    for (BotSquad& squad : bot.squads) {
        if (squad.type != BOT_SQUAD_TYPE_ATTACK) {
            continue;
        }
        // Don't send in a squad unless we feel pretty sure that this squad could cleanup the attack
        if (defending_score + bot_score_entity_list(state, bot, squad.entities) < enemy_score + (4 * BOT_UNIT_SCORE)) {
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
                    ivec2::manhattan_distance(state.entities[entity_index].cell, location)) {
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
        squad.target_cell = location;
        continue;
    }

    // Next, see if we can use unreserved units to mount a counterattack
    bool should_counterattack = (options & BOT_DEFEND_COUNTERATTACK) == BOT_DEFEND_COUNTERATTACK;
    int least_defended_enemy_hall_score = bot_get_least_defended_enemy_hall_score(state, bot);
    if (should_counterattack && least_defended_enemy_hall_score < BOT_UNIT_SCORE_FULL_BUNKER) {
        std::vector<EntityId> counterattack_squad_entities;
        int counterattack_squad_score = 0;
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

            counterattack_squad_entities.push_back(entity_id);
            counterattack_squad_score += bot_score_entity(state, bot, entity, entity_id);
        }

        if (!counterattack_squad_entities.empty() && counterattack_squad_score > least_defended_enemy_hall_score) {
            ivec2 counterattack_squad_target_cell = bot_squad_choose_attack_point(state, bot, bot_squad_get_center_point(state, counterattack_squad_entities));
            bot_squad_create(state, bot, BOT_SQUAD_TYPE_ATTACK, counterattack_squad_target_cell, counterattack_squad_entities);
        }
    }

    // Finally if we're really desperate, use workers to attack
    bool should_defend_with_workers = (options & BOT_DEFEND_WITH_WORKERS) == BOT_DEFEND_WITH_WORKERS;
    if (should_defend_with_workers) {
        std::vector<EntityId> worker_squad_entities;
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const Entity& entity = state.entities[entity_index];
            EntityId entity_id = state.entities.get_id_of(entity_index);
            if (entity.type != ENTITY_MINER ||
                    entity.player_id != bot.player_id ||
                    bot_is_entity_reserved(bot, entity_id) ||
                    entity.health == 0 ||
                    ivec2::manhattan_distance(entity.cell, location) > BOT_SQUAD_GATHER_DISTANCE) {
                continue;
            }

            worker_squad_entities.push_back(entity_id);
        }
        if (!worker_squad_entities.empty()) {
            bot_squad_create(state, bot, BOT_SQUAD_TYPE_RESERVES, location, worker_squad_entities);
            return;
        }
    }
}

// PRODUCTION

MatchInput bot_get_production_input(const MatchState& state, Bot& bot, bool is_base_under_attack) {
    MatchInput saturate_bases_input = bot_saturate_bases(state, bot);
    if (saturate_bases_input.type != MATCH_INPUT_NONE) {
        return saturate_bases_input;
    }

    if (bot_should_build_house(state, bot) && !is_base_under_attack) {
        return bot_build_building(state, bot, ENTITY_HOUSE);
    }

    // If we are expanding, place town hall
    // but only if it is safe to do so
    if (bot.mode == BOT_MODE_EXPAND) {
        EntityId in_progress_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&bot](const Entity& entity, EntityId entity_id) {
                if (entity.type == ENTITY_MINER &&
                        entity.target.type == TARGET_BUILD &&
                        entity.target.build.building_type == ENTITY_HALL &&
                        entity.player_id == bot.player_id &&
                        entity_is_selectable(entity)) {
                    return true;
                }
                if (entity.type == ENTITY_HALL &&
                        entity.mode == MODE_BUILDING_IN_PROGRESS &&
                        entity.player_id == bot.player_id) {
                    return true;
                }
                return false;
            }
        });
        if (in_progress_id == ID_NULL) {
            uint32_t goldmine_index = bot_find_next_hall_goldmine_index(state, bot);
            if (goldmine_index != INDEX_INVALID && bot_is_area_safe(state, bot, state.entities[goldmine_index].cell)) {
                return bot_build_building(state, bot, ENTITY_HALL);
            }
        }
    }

    // Count entities
    BotEntityCount entity_count = bot_entity_count_empty();
    BotEntityCount available_building_count = bot_entity_count_empty();
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        if (entity.player_id != bot.player_id || 
                entity.health == 0 ||
                bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        entity_count[entity.type]++;

        // Count available production buildings
        if (entity_is_building(entity.type) &&
                entity.mode == MODE_BUILDING_FINISHED &&
                entity.queue.empty()) {
            available_building_count[entity.type]++;
        }

        // Count in-progress entities
        if (entity.mode == MODE_BUILDING_FINISHED &&
                !entity.queue.empty() &&
                entity.queue.front().type == BUILDING_QUEUE_ITEM_UNIT) {
            entity_count[entity.queue.front().unit_type]++;
        }
        if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD) {
            entity_count[entity.target.build.building_type]++;
        }
    }

    // Research upgrades
    uint32_t desired_upgrade = bot_get_desired_upgrade(state, bot);
    if (desired_upgrade != 0) {
        EntityType building_type = bot_get_building_which_researches(desired_upgrade);
        if (available_building_count[building_type] != 0) {
            return bot_research_upgrade(state, bot, desired_upgrade);
        }
    }

    // Determine desired entities
    BotEntityCount desired_entities, desired_army_ratio;
    bot_get_desired_buildings_and_army_ratio(state, bot, desired_entities, desired_army_ratio);
    for (const BotDesiredSquad& squad : bot.desired_squads) {
        desired_entities = bot_entity_count_add(desired_entities, squad.entities);
    }

    // Determine if we have an available building
    bool is_available_building = false;
    for (uint32_t unit_type = ENTITY_MINER + 1; unit_type < ENTITY_HALL; unit_type++) {
        const bool bot_desires_unit = 
                    desired_entities[unit_type] > entity_count[unit_type] ||
                    (bot_should_use_continuous_production(bot) && desired_army_ratio[unit_type] != 0);
        if (!bot_desires_unit) {
            continue;
        }

        EntityType building_type = bot_get_building_which_trains((EntityType)unit_type);
        if (available_building_count[building_type] != 0) {
            is_available_building = true;
            break;
        }
    }

    // Train desired unit
    if (is_available_building) {
        while (true) {
            for (uint32_t unit_type = ENTITY_MINER + 1; unit_type < ENTITY_HALL; unit_type++) {
                if (desired_entities[unit_type] <= entity_count[unit_type]) {
                    continue;
                }

                EntityType building_type = bot_get_building_which_trains((EntityType)unit_type);
                if (available_building_count[building_type] == 0) {
                    continue;
                }

                return bot_train_unit(state, bot, (EntityType)unit_type);
            }

            // If we have an available building and we are not using continuous production,
            // then we should have returned an entity with the first pass through the unit types
            GOLD_ASSERT(bot_should_use_continuous_production(bot));

            desired_entities = bot_entity_count_add(desired_entities, desired_army_ratio);
        }
    }

    // Determine any other desired buildings based on building pre-reqs
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_LANDMINE; entity_type++) {
        if (desired_entities[entity_type] == 0) {
            continue;
        }

        EntityType prereq = bot_get_building_prereq((EntityType)entity_type);
        while (prereq != ENTITY_TYPE_COUNT && desired_entities[prereq] == 0) {
            desired_entities[prereq] = 1;
            prereq = bot_get_building_prereq((EntityType)prereq);
        }
    }

    // Build desired building
    if (!is_base_under_attack) {
        for (uint32_t building_type = ENTITY_HALL; building_type < ENTITY_TYPE_COUNT; building_type++) {
            if (desired_entities[building_type] > entity_count[building_type]) {
                return bot_build_building(state, bot, (EntityType)building_type);
            }
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

void bot_get_desired_buildings_and_army_ratio(const MatchState& state, const Bot& bot, BotEntityCount& desired_buildings, BotEntityCount& desired_army_ratio) {
    desired_buildings = bot_entity_count_empty();
    desired_army_ratio = bot_entity_count_empty();

    uint32_t mining_base_count = bot_get_mining_base_count(state, bot);

    switch (bot.unit_comp) {
        case BOT_UNIT_COMP_COWBOY_BANDIT: {
            desired_buildings[ENTITY_SALOON] = 2 * mining_base_count;

            desired_army_ratio[ENTITY_COWBOY] = 2;
            desired_army_ratio[ENTITY_BANDIT] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_BANDIT_PYRO: {
            desired_buildings[ENTITY_SALOON] = 2 * mining_base_count;
            desired_buildings[ENTITY_WORKSHOP] = 1;

            desired_army_ratio[ENTITY_COWBOY] = 3;
            desired_army_ratio[ENTITY_BANDIT] = 2;
            desired_army_ratio[ENTITY_PYRO] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_SAPPER_PYRO: {
            desired_buildings[ENTITY_SALOON] = mining_base_count + 1;
            desired_buildings[ENTITY_WORKSHOP] = mining_base_count;

            desired_army_ratio[ENTITY_COWBOY] = 3;
            desired_army_ratio[ENTITY_SAPPER] = 2;
            desired_army_ratio[ENTITY_PYRO] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_BANDIT_WAGON: {
            desired_buildings[ENTITY_SALOON] = 2 * mining_base_count;
            desired_buildings[ENTITY_COOP] = mining_base_count;

            desired_army_ratio[ENTITY_COWBOY] = 2;
            desired_army_ratio[ENTITY_BANDIT] = 2;
            desired_army_ratio[ENTITY_WAGON] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_BANDIT_WAGON_JOCKEY: {
            desired_buildings[ENTITY_COOP] = mining_base_count;
            desired_buildings[ENTITY_SALOON] = mining_base_count;

            desired_army_ratio[ENTITY_COWBOY] = 2;
            desired_army_ratio[ENTITY_BANDIT] = 2;
            desired_army_ratio[ENTITY_WAGON] = 1;
            desired_army_ratio[ENTITY_JOCKEY] = 4;
            break;
        }
        case BOT_UNIT_COMP_SOLDIER_BANDIT: {
            desired_buildings[ENTITY_BARRACKS] = 2 * mining_base_count;

            desired_army_ratio[ENTITY_SOLDIER] = 1;
            desired_army_ratio[ENTITY_BANDIT] = 1;

            if (bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES) || bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES)) {
                desired_buildings[ENTITY_WORKSHOP] = 1;
                desired_army_ratio[ENTITY_BALLOON] = 1;
            }
            break;
        }
        case BOT_UNIT_COMP_SOLDIER_CANNON: {
            desired_buildings[ENTITY_BARRACKS] = 2 * mining_base_count;

            desired_army_ratio[ENTITY_SOLDIER] = 8;
            desired_army_ratio[ENTITY_CANNON] = 1;

            if (bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES) || bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES)) {
                desired_buildings[ENTITY_WORKSHOP] = 1;
                desired_army_ratio[ENTITY_BALLOON] = 1;
            }
            break;
        }
    }
}

bool bot_should_use_continuous_production(const Bot& bot) {
    return bot.mode != BOT_MODE_OPENER_BANDIT_RUSH && bot.mode != BOT_MODE_OPENER_BUNKER;
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
                if (hall.rally_point.x == -1) {
                    bot.buildings_to_set_rally_points.push(hall_id);
                }

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
        building_location = bot_find_hall_location(state, bot);
    } else if (building_type == ENTITY_BUNKER) {
        building_location = bot_find_bunker_location(state, bot, hall_index);
    } else {
        building_location = bot_find_building_location(state, bot.player_id, state.entities[hall_index].cell + ivec2(1, 1), entity_get_data(building_type).cell_size);
    }
    if (building_location.x == -1) {
        log_warn("BOT: building location not found for building type %s", entity_get_data(building_type).name);
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

    bot.buildings_to_set_rally_points.push(building_id);

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = (uint32_t)unit_type;
    input.building_enqueue.building_count = 1;
    input.building_enqueue.building_ids[0] = building_id;
    return input;
}

uint32_t bot_get_desired_upgrade(const MatchState& state, const Bot& bot) {
    if (bot_has_landmine_squad(bot) && 
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_LANDMINES)) {
        return UPGRADE_LANDMINES;
    }

    if (bot.unit_comp == BOT_UNIT_COMP_SOLDIER_CANNON &&
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_BAYONETS)) {
        return UPGRADE_BAYONETS;
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
        case ENTITY_DETECTIVE:
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

uint32_t bot_find_next_hall_goldmine_index(const MatchState& state, const Bot& bot) {
    uint32_t existing_hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id, false);
    if (existing_hall_index == INDEX_INVALID) {
        return INDEX_INVALID;
    }

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
    
    return nearest_goldmine_index;
}

ivec2 bot_find_hall_location(const MatchState& state, const Bot& bot) {
    // If no goldmines are found, we will not build a hall
    uint32_t nearest_goldmine_index = bot_find_next_hall_goldmine_index(state, bot);
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
            ivec2 mine_exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, state.entities[nearest_goldmine_index].cell, entity_get_data(ENTITY_GOLDMINE).cell_size, 1, rally_cell, MAP_OPTION_IGNORE_MINERS);

            GOLD_ASSERT(mine_exit_cell.x != -1);
            std::vector<ivec2> path;
            map_pathfind(state.map, CELL_LAYER_GROUND, mine_exit_cell, rally_cell, 1, &path, MAP_OPTION_IGNORE_MINERS);
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
    ivec2 path_start_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, nearby_hall_cell, entity_get_data(ENTITY_HALL).cell_size, 1, nearest_enemy_building.cell, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_building.cell, entity_get_data(nearest_enemy_building.type).cell_size, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    if (path_start_cell.x == -1 || path_end_cell.x == -1) {
        return bot_find_building_location(state, bot.player_id, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }
    map_pathfind(state.map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, &path, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);

    if (path.empty()) {
        return bot_find_building_location(state, bot.player_id, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }

    ivec2 search_start_cell = path_start_cell;
    int path_index = 0;
    while (path_index < path.size() &&
            (ivec2::manhattan_distance(path[path_index], path_start_cell) < 17 &&
            map_get_pathing_region(state.map, path[path_index]) == map_get_pathing_region(state.map, path_start_cell))) {
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

// SQUAD

std::vector<EntityId> bot_create_entity_list_from_entity_count(const MatchState& state, const Bot& bot, BotEntityCount desired_entities) {
    std::vector<EntityId> entity_list;
    BotEntityCount list_entity_count = bot_entity_count_empty();

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
        if (list_entity_count[entity.type] >= desired_entities[entity.type]) {
            continue;
        }

        entity_list.push_back(entity_id);
        list_entity_count[entity.type]++;
    }

    return entity_list;
}

void bot_squad_create_from_entity_count(const MatchState& state, Bot& bot, BotSquadType squad_type, BotEntityCount desired_entities) {
    std::vector<EntityId> entity_list = bot_create_entity_list_from_entity_count(state, bot, desired_entities);

    // Release scout if it's in the squad
    for (EntityId entity_id : entity_list) {
        if (entity_id == bot.scout_id) {
            bot_release_scout(bot);
            break;
        }
    }

    ivec2 target_cell = ivec2(-1, -1);
    switch (squad_type) {
        case BOT_SQUAD_TYPE_ATTACK: {
            target_cell = bot_squad_choose_attack_point(state, bot, bot_squad_get_center_point(state, entity_list));
            break;
        }
        case BOT_SQUAD_TYPE_DEFEND: {
            target_cell = bot_squad_choose_defense_point(state, bot, entity_list);
            break;
        }
        case BOT_SQUAD_TYPE_LANDMINES: {
            const Entity& pyro = state.entities.get_by_id(entity_list[0]);
            EntityId nearest_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state,
                .filter = [&pyro](const Entity& hall, EntityId hall_id) {
                    return hall.type == ENTITY_HALL &&
                            hall.mode == MODE_BUILDING_FINISHED &&
                            hall.player_id == pyro.player_id;
                },
                .compare = bot_closest_manhattan_distance_to(pyro.cell)
            });
            if (nearest_hall_id == ID_NULL) {
                log_trace("BOT: Landmines squad found no nearby halls. Abandoning.");
                return;
            }
            target_cell = state.entities.get_by_id(nearest_hall_id).cell;
            break;
        }
        case BOT_SQUAD_TYPE_RESERVES: {
            GOLD_ASSERT(false);
            break;
        }
    }

    bot_squad_create(state, bot, squad_type, target_cell, entity_list);
}

void bot_squad_create(const MatchState& state, Bot& bot, BotSquadType type, ivec2 target_cell, std::vector<EntityId>& entities) {
    if (entities.empty()) {
        log_trace("BOT: bot_squad_create no entities in entities list. Abandoning squad.");
        return;
    }
    if (target_cell.x == -1) {
        log_trace("BOT: squad of type %u target cell is invalid. Abandoning squad.", type);
        return;
    }

    // Decide whether to create a new squad to reinforce an existing one
    uint32_t existing_squad_index;
    for (existing_squad_index = 0; existing_squad_index < bot.squads.size(); existing_squad_index++) {
        if (bot.squads[existing_squad_index].type == type &&
                ivec2::manhattan_distance(bot.squads[existing_squad_index].target_cell, target_cell) < BOT_SQUAD_GATHER_DISTANCE) {
            break;
        }
    }

    uint32_t squad_index;
    bool should_reinforce_existing_squad = (type == BOT_SQUAD_TYPE_ATTACK || type == BOT_SQUAD_TYPE_DEFEND) &&
                                            existing_squad_index < bot.squads.size();
    if (should_reinforce_existing_squad) {
        squad_index = existing_squad_index;
    } else {
        BotSquad squad;
        squad.type = type;
        squad.target_cell = target_cell;
        bot.squads.push_back(squad);
        squad_index = bot.squads.size() - 1;
    }

    BotSquad& squad = bot.squads[squad_index];

    // Reserve all entities
    if (should_reinforce_existing_squad) {
        log_trace("BOT: -- reinforcing squad with type %u target_cell %vi --", squad.type, &squad.target_cell);
    } else {
        log_trace("BOT: -- creating squad with type %u target_cell %vi --", squad.type, &squad.target_cell);
    }
    squad.entities.reserve(squad.entities.size() + entities.size());
    for (EntityId entity_id : entities) {
        squad.entities.push_back(entity_id);
        bot_reserve_entity(bot, entity_id);
    }
    log_trace("BOT: -- end squad --");
}

void bot_squad_dissolve(Bot& bot, BotSquad& squad) {
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

    // Retreat
    if (squad.type == BOT_SQUAD_TYPE_ATTACK && bot_squad_should_retreat(state, bot, squad)) {
        Cell cell = map_get_cell(state.map, CELL_LAYER_GROUND, squad.target_cell);
        log_trace("BOT: squad should retreat with cell %vi cell type %u", &squad.target_cell, cell.type);
        if (cell.type == CELL_BUILDING) {
            EntityId target_hall_id = cell.id;
            BotRetreatMemory memory = (BotRetreatMemory) {
                .entity_list = bot_squad_get_nearby_enemy_army(state, bot, squad),
                .desired_lead = 4 * BOT_UNIT_SCORE
            };

            auto existing_retreat_memory_it = bot.retreat_memory.find(target_hall_id);
            if (existing_retreat_memory_it != bot.retreat_memory.end() && 
                    bot_score_entity_list(state, bot, memory.entity_list) <= 
                    bot_score_entity_list(state, bot, existing_retreat_memory_it->second.entity_list)) {
                memory.desired_lead = existing_retreat_memory_it->second.desired_lead * 2;
            }

            bot.retreat_memory[target_hall_id] = memory;
        }
        return bot_squad_return_to_nearest_base(state, bot, squad);
    }

    // Check if there is a target nearby
    EntityId enemy_near_squad_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &squad](const Entity& enemy, EntityId enemy_id) {
            return enemy.type != ENTITY_GOLDMINE &&
                    state.players[enemy.player_id].team != state.players[bot.player_id].team &&
                    entity_is_selectable(enemy) &&
                    ivec2::manhattan_distance(enemy.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE;
        }
    });
    bool is_enemy_near_squad = enemy_near_squad_id != ID_NULL;

    // Check if squad should exit bunker
    if (squad.type == BOT_SQUAD_TYPE_DEFEND) {
        for (EntityId bunker_id : squad.entities) {
            const Entity& bunker = state.entities.get_by_id(bunker_id);

            // Filter down to only non-empty bunkers
            if (bunker.type != ENTITY_BUNKER || bunker.garrisoned_units.empty()) {
                continue;
            }

            // Check if an enemy is attacking nearby
            EntityId nearby_miner_under_attack_id = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&bunker](const Entity& miner, EntityId miner_id) {
                    return miner.player_id == bunker.player_id &&
                            entity_is_selectable(miner) &&
                            miner.type == ENTITY_MINER &&
                            miner.taking_damage_timer != 0 && 
                            ivec2::manhattan_distance(miner.cell, bunker.cell) < BOT_SQUAD_GATHER_DISTANCE * 2;
                }
            });

            // Determine if we are in range of that enemy
            int largest_range_in_bunker = 0;
            for (EntityId unit_id : bunker.garrisoned_units) {
                const Entity& unit = state.entities.get_by_id(unit_id);
                largest_range_in_bunker = std::max(largest_range_in_bunker, entity_get_data(unit.type).unit_data.range_squared);
            }
            EntityId enemy_in_range_of_bunker_id = bot_find_entity((BotFindEntityParams) {
                .state = state,
                .filter = [&state, &bunker, &largest_range_in_bunker](const Entity& enemy, EntityId enemy_id) {
                    return entity_is_unit(enemy.type) &&
                            state.players[enemy.player_id].team != state.players[bunker.player_id].team &&
                            entity_is_selectable(enemy) &&
                            ivec2::euclidean_distance_squared(bunker.cell, enemy.cell) <= largest_range_in_bunker;
                }
            });

            // If we're under attack, but we can't shoot the enemies, then exit the bunker to defend
            if (nearby_miner_under_attack_id != ID_NULL && enemy_in_range_of_bunker_id == ID_NULL) {
                // Change to reserves type so that they will be free after they finish defending against the attack
                squad.type = BOT_SQUAD_TYPE_RESERVES;
                squad.target_cell = state.entities.get_by_id(nearby_miner_under_attack_id).cell;

                // Remove bunker from squad
                for (int squad_entity_index = 0; squad_entity_index < squad.entities.size(); squad_entity_index++) {
                    if (squad.entities[squad_entity_index] == bunker_id) {
                        squad.entities[squad_entity_index] = squad.entities.back();
                        squad.entities.pop_back();
                        bot_release_entity(bot, bunker_id);
                    }
                }

                // Unload all units in bunker
                MatchInput unload_input;
                unload_input.type = MATCH_INPUT_UNLOAD;
                unload_input.unload.carrier_count = 1;
                unload_input.unload.carrier_ids[0] = bunker_id;
                return unload_input;
            }
        }
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
        if (nearby_enemy_id == ID_NULL || unit.type == ENTITY_BALLOON) {
            unengaged_units.push_back(unit_id);
            continue;
        }

        const Entity& nearby_enemy = state.entities.get_by_id(nearby_enemy_id);
        is_enemy_near_squad = true;

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

        // If this unit is a detective, then activate camo
        if (unit.type == ENTITY_DETECTIVE && !entity_check_flag(unit, ENTITY_FLAG_INVISIBLE)) {
            if (unit.energy < CAMO_ENERGY_COST) {
                squad.entities[squad_entity_index] = squad.entities[squad.entities.size() - 1];
                squad.entities.pop_back();
                bot_release_entity(bot, unit_id);
                return bot_return_entity_to_nearest_hall(state, bot, unit_id);
            }

            MatchInput input;
            input.type = MATCH_INPUT_CAMO;
            input.camo.unit_count = 1;
            input.camo.unit_ids[0] = unit_id;

            // Seee if there are any other un-cloaked detectives in the squad
            for (EntityId other_id : squad.entities) {
                if (other_id == unit_id) {
                    continue;
                }
                const Entity& other = state.entities.get_by_id(other_id);
                if (other.type == ENTITY_DETECTIVE && 
                        !entity_check_flag(other, ENTITY_FLAG_INVISIBLE) && 
                        other.energy >= CAMO_ENERGY_COST) {
                    input.camo.unit_ids[input.camo.unit_count] = other_id;
                    input.camo.unit_count++;
                    if (input.camo.unit_count == SELECTION_LIMIT) {
                        break;
                    }
                }
            }

            return input;
        }

        // If this unit is a miner which is currently mining, tell it to snap out of it and start attacking
        if (unit.type == ENTITY_MINER && unit.target.type == TARGET_ENTITY) {
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
            input.move.shift_command = 0;
            input.move.target_cell = nearby_enemy.cell;
            input.move.target_id = ID_NULL;
            input.move.entity_count = 1;
            input.move.entity_ids[0] = unit_id;

            // Check if other miners want to join this input
            for (EntityId other_id : squad.entities) {
                const Entity& other = state.entities.get_by_id(other_id);
                if (other.type != ENTITY_MINER ||
                        !entity_is_selectable(other) ||
                        other.target.type != TARGET_ENTITY ||
                        ivec2::manhattan_distance(other.cell, unit.cell) > BOT_SQUAD_GATHER_DISTANCE) {
                    continue;
                }

                input.move.entity_ids[input.move.entity_count] = other_id;
                input.move.entity_count++;
                if (input.move.entity_count == SELECTION_LIMIT) {
                    break;
                }
            }

            return input;
        }
    } // End for each infantry
    // End attack micro

    // If attacking pyros, make sure they have enough energy first
    if (squad.type == BOT_SQUAD_TYPE_ATTACK) {
        bool has_low_energy_pyro = false;
        for (EntityId unit_id : unengaged_units) {
            const Entity& unit = state.entities.get_by_id(unit_id);
            if (unit.type == ENTITY_PYRO && unit.energy < MOLOTOV_ENERGY_COST) {
                has_low_energy_pyro = true;
                break;
            }
        }
        if (has_low_energy_pyro) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

    // Unengaged infantry micro
    // This section mostly just sorts distant infantry into a list
    // But it also handles the pyro landmine placement
    std::vector<EntityId> distant_infantry;
    std::vector<EntityId> distant_cavalry;
    for (EntityId unit_id : unengaged_units) {
        const Entity& unit = state.entities.get_by_id(unit_id);

        // Skip units that are already garrisoning
        if (unit.target.type == TARGET_ENTITY) {
            bool is_garrison_target_in_squad = false;
            for (EntityId carrier_id : squad.entities) {
                if (unit.target.id == carrier_id && entity_get_data(state.entities.get_by_id(carrier_id).type).garrison_capacity != 0) {
                    is_garrison_target_in_squad = true;
                    break;
                }
            }
            if (is_garrison_target_in_squad) {
                continue;
            }
        } 

        // Pyro landmine placement
        // This means that landmine squads won't get a-moved to the target cell like other squads
        if (squad.type == BOT_SQUAD_TYPE_LANDMINES) {
            // If the player doesn't have the landmine upgrade, then skip this unit
            if (!match_player_has_upgrade(state, bot.player_id, UPGRADE_LANDMINES)) {
                continue;
            }

            // If we're already making a landmine or we don't have enough energy, skip this unit
            if (unit.target.type == TARGET_BUILD || unit.energy < entity_get_data(ENTITY_LANDMINE).gold_cost) {
                continue;
            }

            // First pass, find all allied halls
            std::unordered_map<uint32_t, uint32_t> hall_landmine_count;
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                if (state.entities[entity_index].type != ENTITY_HALL ||
                        state.entities[entity_index].player_id != bot.player_id) {
                    continue;
                }
                hall_landmine_count[entity_index] = 0;
            }

            // Second pass, find all landmines and associate them with the nearest hall
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                if (state.entities[entity_index].type != ENTITY_LANDMINE ||
                        state.entities[entity_index].player_id != bot.player_id) {
                    continue;
                }
                uint32_t nearest_hall_index = INDEX_INVALID;
                for (auto it : hall_landmine_count) {
                    if (nearest_hall_index == INDEX_INVALID ||
                            ivec2::manhattan_distance(state.entities[entity_index].cell, state.entities[it.first].cell) <
                            ivec2::manhattan_distance(state.entities[entity_index].cell, state.entities[nearest_hall_index].cell)) {
                        nearest_hall_index = it.first;
                    }
                }

                GOLD_ASSERT(nearest_hall_index != INDEX_INVALID);
                hall_landmine_count[nearest_hall_index]++;
            }

            // Find the nearest hall which still needs landmines
            uint32_t nearest_hall_index = INDEX_INVALID;
            for (auto it : hall_landmine_count) {
                if (it.second >= BOT_MAX_LANDMINES_PER_BASE) {
                    continue;
                }
                if (nearest_hall_index == INDEX_INVALID || 
                        ivec2::manhattan_distance(unit.cell, state.entities[it.first].cell) < 
                        ivec2::manhattan_distance(unit.cell, state.entities[nearest_hall_index].cell)) {
                    nearest_hall_index = it.first;
                }
            }

            // If there are no halls which still need landmines, then do nothing
            if (nearest_hall_index == INDEX_INVALID) {
                return (MatchInput) { .type = MATCH_INPUT_NONE };
            }

            // Find the nearest enemy base to determine where to place mines
            const Entity& hall = state.entities[nearest_hall_index];
            EntityId nearest_enemy_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state,
                .filter = [&state, &bot](const Entity& hall, EntityId hall_id) {
                    return hall.type == ENTITY_HALL &&
                            state.players[hall.player_id].team != state.players[bot.player_id].team &&
                            entity_is_selectable(hall);
                },
                .compare = bot_closest_manhattan_distance_to(hall.cell)
            });
            // If there is no enemy base, dissolve squad, there's no use for landmines anymore
            if (nearest_enemy_hall_id == ID_NULL) {
                bot_squad_dissolve(bot, squad);
                return (MatchInput) { .type = MATCH_INPUT_NONE };
            }

            // Pathfind to that base so that we place mines close to where the enemies would be coming in
            ivec2 enemy_base_cell = state.entities.get_by_id(nearest_enemy_hall_id).cell;
            std::vector<ivec2> path;
            map_pathfind(state.map, CELL_LAYER_GROUND, hall.cell, enemy_base_cell, 1, &path, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS | MAP_OPTION_AVOID_LANDMINES);

            // Find the place along the path that is within the base radius
            int path_index = 0;
            while (path_index < path.size() && ivec2::manhattan_distance(path[path_index], hall.cell) > BOT_SQUAD_GATHER_DISTANCE) {
                path_index++;
            }

            if (path_index >= path.size()) {
                bot_squad_dissolve(bot, squad);
                return (MatchInput) { .type = MATCH_INPUT_NONE };
            }

            MatchInput landmine_input;
            landmine_input.type = MATCH_INPUT_BUILD;
            landmine_input.build.shift_command = 0;
            landmine_input.build.building_type = ENTITY_LANDMINE;
            landmine_input.build.target_cell = path[path_index];
            landmine_input.build.entity_count = 1;
            landmine_input.build.entity_ids[0] = unit_id;
            return landmine_input;
        } // End if squad type is landmines

        // Detectives minesweeping, make detectives wait until private eye is finished
        if (bot_squad_is_detective_harass(state, squad) &&
                bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES) &&
                !match_player_has_upgrade(state, bot.player_id, UPGRADE_PRIVATE_EYE)) {
            continue;
        }

        // If the unit is far away, add it to the distant infantry/cavalry list
        if (ivec2::manhattan_distance(unit.cell, squad.target_cell) > BOT_SQUAD_GATHER_DISTANCE / 2 ||
                bot_squad_is_bunker_defense(state, squad)) {
            if (entity_get_data(unit.type).garrison_size == ENTITY_CANNOT_GARRISON) {
                distant_cavalry.push_back(unit_id);
            } else {
                distant_infantry.push_back(unit_id);
            }
        }
    } // End for each unit of unengaged units

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
                // If a distant infantry is close enough, don't bother transporting it
                if (!bot_squad_is_bunker_defense(state, squad) &&
                        ivec2::manhattan_distance(state.entities.get_by_id(infantry_id).cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE * 2) {
                    continue;
                }
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

            if (input.move.entity_count != 0) {
                return input;
            }
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
            map_pathfind(state.map, CELL_LAYER_GROUND, carrier.cell, infantry_center, 2, &path_to_infantry_center, MAP_OPTION_IGNORE_UNITS);

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
        if (ivec2::manhattan_distance(carrier.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE - 4) {
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

    if ((squad.type == BOT_SQUAD_TYPE_ATTACK || squad.type == BOT_SQUAD_TYPE_RESERVES) && !is_enemy_near_squad) {
        if (squad.type == BOT_SQUAD_TYPE_RESERVES) {
            bot_squad_dissolve(bot, squad);
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
        return bot_squad_return_to_nearest_base(state, bot, squad);
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

std::vector<EntityId> bot_squad_get_nearby_enemy_army(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    std::vector<EntityId> enemy_army;

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];

        // Filter down to enemy units or bunkers which the bot can see
        if (entity.type == ENTITY_GOLDMINE ||
                (entity_is_building(entity.type) && entity.type != ENTITY_BUNKER) ||
                state.players[entity.player_id].team == state.players[bot.player_id].team ||
                !entity_is_selectable(entity)) {
            continue;
        }

        // Filter out any units that are far away from the squad
        bool is_near_squad = false;
        for (EntityId squad_entity_id : squad.entities) {
            ivec2 squad_entity_cell = state.entities.get_by_id(squad_entity_id).cell;
            if (ivec2::manhattan_distance(entity.cell, squad_entity_cell) < BOT_SQUAD_GATHER_DISTANCE) {
                is_near_squad = true;
                break;
            }
        }
        if (!is_near_squad) {
            continue;
        }

        enemy_army.push_back(state.entities.get_id_of(entity_index));
    }

    return enemy_army;
}

bool bot_squad_should_retreat(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    // Score the enemy's army
    std::vector<EntityId> nearby_enemy_army = bot_squad_get_nearby_enemy_army(state, bot, squad);
    int enemy_army_score = bot_score_entity_list(state, bot, nearby_enemy_army);

    // Score the allied army
    int squad_score = 0;
    for (EntityId entity_id : squad.entities) {
        const Entity& entity = state.entities.get_by_id(entity_id);
        squad_score += bot_score_entity(state, bot, entity, entity_id);
    }

    if (squad_score < enemy_army_score) {
        log_trace("BOT: squad with size %u first entity %u is retreating. squad score %u enemy army score %u", squad.entities.size(), squad.entities[0], squad_score, enemy_army_score);
        log_trace("BOT: -- enemy army --");
        for (EntityId entity_id : nearby_enemy_army) {
            log_trace("%u %s", entity_id, entity_get_data(state.entities.get_by_id(entity_id).type).name);
        }
        log_trace("BOT: -- end --");
    }

    return squad_score < enemy_army_score;
}

MatchInput bot_squad_return_to_nearest_base(const MatchState& state, Bot& bot, BotSquad& squad) {
    // Find the nearest base to retreat to
    EntityId nearest_allied_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& hall, EntityId hall_id) {
            return hall.type == ENTITY_HALL && entity_is_selectable(hall) &&
                    hall.player_id == bot.player_id;
        },
        .compare = bot_closest_manhattan_distance_to(squad.target_cell)
    });

    // If there is no nearby base, just dissolve the army
    if (nearest_allied_hall_id == ID_NULL) {
        bot_squad_dissolve(bot, squad);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // To determine the retreat_cell, path from the nearby base
    std::vector<ivec2> path;
    ivec2 hall_cell = state.entities.get_by_id(nearest_allied_hall_id).cell;
    map_pathfind(state.map, CELL_LAYER_GROUND, hall_cell, squad.target_cell, 1, &path, MAP_OPTION_IGNORE_UNITS);

    ivec2 retreat_cell = hall_cell;
    if (!path.empty()) {
        retreat_cell = path[std::min((int)path.size() - 1, BOT_SQUAD_GATHER_DISTANCE)];
    } 

    // Build out the input
    MatchInput retreat_input;
    retreat_input.type = MATCH_INPUT_MOVE_CELL;
    retreat_input.move.shift_command = 0;
    retreat_input.move.target_id = ID_NULL;
    retreat_input.move.target_cell = retreat_cell;
    retreat_input.move.entity_count = 0;

    // Find entities to order to retreat
    while (!squad.entities.empty()) {
        bot_release_entity(bot, squad.entities.back());
        retreat_input.move.entity_ids[retreat_input.move.entity_count] = squad.entities.back();
        retreat_input.move.entity_count++;
        squad.entities.pop_back();

        if (retreat_input.move.entity_count == SELECTION_LIMIT) {
            break;
        }
    }

    return retreat_input;
}

int bot_get_molotov_cell_score(const MatchState& state, const Bot& bot, const Entity& pyro, ivec2 cell) {
    if (!map_is_cell_in_bounds(state.map, cell) ||
            map_get_cell(state.map, CELL_LAYER_GROUND, cell).type == CELL_BLOCKED || 
            map_get_tile(state.map, cell).sprite == SPRITE_TILE_WATER) {
        return 0;
    }

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
            existing_molotov_cells.push_back(projectile.target.to_ivec2() / TILE_SIZE);
        }
    }
    for (ivec2 existing_molotov_cell : existing_molotov_cells) {
        int distance = ivec2::manhattan_distance(cell, existing_molotov_cell);
        if (distance < PROJECTILE_MOLOTOV_FIRE_SPREAD) {
            score -= (PROJECTILE_MOLOTOV_FIRE_SPREAD - distance) * 5;
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

bool bot_squad_is_bunker_defense(const MatchState& state, const BotSquad& squad) {
    if (squad.type != BOT_SQUAD_TYPE_DEFEND) {
        return false;
    }
    for (EntityId entity_id : squad.entities) {
        if (state.entities.get_by_id(entity_id).type == ENTITY_BUNKER) {
            return true;
        }
    }
    return false;
}

bool bot_squad_is_detective_harass(const MatchState& state, const BotSquad& squad) {
    if (squad.type != BOT_SQUAD_TYPE_ATTACK) {
        return false;
    }
    for (EntityId entity_id : squad.entities) {
        if (state.entities.get_by_id(entity_id).type != ENTITY_DETECTIVE) {
            return false;
        }
    }
    return true;
}

bool bot_squad_can_defend_against_detectives(const MatchState& state, BotSquadType type, const std::vector<EntityId>& entities) {
    if (!(type == BOT_SQUAD_TYPE_DEFEND || 
            type == BOT_SQUAD_TYPE_RESERVES)) {
        return false;
    }

    bool has_detection = false;
    bool has_damage = false;

    for (EntityId entity_id : entities) {
        uint32_t entity_index = state.entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID) {
            continue;
        }

        if (!entity_is_unit(state.entities[entity_index].type)) {
            continue;
        }
        if (match_entity_has_detection(state, state.entities[entity_index])) {
            has_detection = true;
        }
        if (entity_get_data(state.entities[entity_index].type).unit_data.damage > 0) {
            has_damage = true;
        }
    }

    return has_detection && has_damage;
}

ivec2 bot_squad_get_center_point(const MatchState& state, const std::vector<EntityId>& entities) {
    struct CenterPoint {
        ivec2 sum;
        int size;
        ivec2 point() {
            return sum / size;
        }
    };
    std::vector<CenterPoint> center_points;

    for (EntityId entity_id : entities) {
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

ivec2 bot_squad_choose_attack_point(const MatchState& state, const Bot& bot, ivec2 squad_center) {
    EntityId nearest_building_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&state, &bot](const Entity& building, EntityId building_id) {
            return state.players[building.player_id].team !=
                    state.players[bot.player_id].team &&
                    entity_is_building(building.type) &&
                    entity_is_selectable(building) &&
                    bot_has_scouted_entity(state, bot, building, building_id);
        },
        .compare = [&squad_center](const Entity& a, const Entity& b) {
            if (a.type == ENTITY_HALL && b.type != ENTITY_HALL) {
                return true;
            }
            if (a.type != ENTITY_HALL && b.type == ENTITY_HALL) {
                return false;
            }
            return ivec2::manhattan_distance(a.cell, squad_center) <
                    ivec2::manhattan_distance(b.cell, squad_center);
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

    // Find the least defended hall
    std::unordered_map<uint32_t, int> enemy_hall_defense_score = bot_get_enemy_hall_defense_scores(state, bot);
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

ivec2 bot_squad_choose_defense_point(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entities) {
    // Determine if this squad provides detection
    bool is_detection_squad = bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES) && bot_squad_can_defend_against_detectives(state, BOT_SQUAD_TYPE_DEFEND, entities);

    std::unordered_map<uint32_t, uint32_t> hall_defenses;
    for (uint32_t hall_index = 0; hall_index < state.entities.size(); hall_index++) {
        const Entity& hall = state.entities[hall_index];
        if (hall.type != ENTITY_HALL || 
                !entity_is_selectable(hall) ||
                hall.player_id != bot.player_id) {
            continue;
        }

        hall_defenses[hall_index] = 0;
    }

    // If there's no halls, abandon the defense squad
    if (hall_defenses.empty()) {
        return ivec2(-1, -1);
    }

    for (const BotSquad& existing_squad : bot.squads) {
        if (existing_squad.type != BOT_SQUAD_TYPE_DEFEND) {
            continue;
        }
        if (is_detection_squad && !bot_squad_can_defend_against_detectives(state, existing_squad.type, existing_squad.entities)) {
            continue;
        }

        uint32_t nearest_hall_index = INDEX_INVALID;
        for (auto it : hall_defenses) {
            if (nearest_hall_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(state.entities[it.first].cell, existing_squad.target_cell) <
                    ivec2::manhattan_distance(state.entities[nearest_hall_index].cell, existing_squad.target_cell)) {
                nearest_hall_index = it.first;
            }
        }
        GOLD_ASSERT(nearest_hall_index != INDEX_INVALID);

        hall_defenses[nearest_hall_index]++;
    }

    uint32_t least_defended_hall_index = INDEX_INVALID;
    for (auto it : hall_defenses) {
        if (least_defended_hall_index == INDEX_INVALID ||
                it.second < hall_defenses[least_defended_hall_index]) {
            least_defended_hall_index = it.first;
        }
    }
    GOLD_ASSERT(least_defended_hall_index != INDEX_INVALID);

    const Entity& hall = state.entities[least_defended_hall_index];
    EntityId nearest_enemy_building_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&state, &bot](const Entity& entity, EntityId entity_id) {
            return entity_is_building(entity.type) &&
                    entity_is_selectable(entity) &&
                    state.players[entity.player_id].team != state.players[bot.player_id].team;
        },
        .compare = bot_closest_manhattan_distance_to(hall.cell)
    });
    if (nearest_enemy_building_id == ID_NULL) {
        return ivec2(-1, -1);
    }
    const Entity& nearest_enemy_building = state.entities.get_by_id(nearest_enemy_building_id);

    std::vector<ivec2> path;
    ivec2 path_start_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, hall.cell, entity_get_data(hall.type).cell_size, 1, nearest_enemy_building.cell, MAP_OPTION_IGNORE_UNITS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_building.cell, entity_get_data(nearest_enemy_building.type).cell_size, MAP_OPTION_IGNORE_UNITS);
    map_pathfind(state.map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, &path, MAP_OPTION_IGNORE_UNITS);

    if (path.empty()) {
        return path_start_cell;
    }
    return path[std::min((int)path.size() - 1, 7)];
}

bool bot_squad_is_engaged(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    EntityId enemy_near_squad_id = bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &squad](const Entity& enemy, EntityId enemy_id) {
            return enemy.type != ENTITY_GOLDMINE &&
                    state.players[enemy.player_id].team != state.players[bot.player_id].team &&
                    entity_is_selectable(enemy) &&
                    ivec2::manhattan_distance(enemy.cell, squad.target_cell) < BOT_SQUAD_GATHER_DISTANCE &&
                    match_is_entity_visible_to_player(state, enemy, bot.player_id);
        }
    });
    if (enemy_near_squad_id != ID_NULL) {
        return true;
    }

    for (EntityId entity_id : squad.entities) {
        uint32_t entity_index = state.entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID || state.entities[entity_index].health == 0) {
            continue;
        }

        const Entity& entity = state.entities[entity_index];
        EntityId nearby_enemy_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&state, &entity](const Entity& enemy, EntityId enemy_id) {
                return enemy.type != ENTITY_GOLDMINE &&
                        state.players[enemy.player_id].team != state.players[entity.player_id].team &&
                        entity_is_selectable(enemy) &&
                        ivec2::manhattan_distance(enemy.cell, entity.cell) < BOT_SQUAD_GATHER_DISTANCE &&
                        match_is_entity_visible_to_player(state, enemy, entity.player_id);
            }
        });
        if (nearby_enemy_id != ID_NULL) {
            return true;
        }
    }

    return false;
}

// SCOUT

void bot_scout_update(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    // Scout goal update, done before scout death check because bot_release_scout
    // will clear the scout goal
    uint32_t player_entity_counts[MAX_PLAYERS][ENTITY_TYPE_COUNT];
    memset(player_entity_counts, 0, sizeof(player_entity_counts));
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        // Ignore goldmines and un-selectables
        if (entity.type == ENTITY_GOLDMINE || 
                !entity_is_selectable(entity)) {
            continue;
        }
        // Ignore allied units
        if (state.players[entity.player_id].team == state.players[bot.player_id].team) {
            continue;
        }
        // Ignore units we can't see
        if (entity_is_unit(entity.type) && !match_is_entity_visible_to_player(state, entity, bot.player_id)) {
            continue;
        }
        // Ignore buildings we haven't scouted
        if (entity_is_building(entity.type) && !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }

        player_entity_counts[entity.player_id][entity.type]++;
    }

    // Check for scout death
    if (bot.scout_id != ID_NULL) {
        uint32_t scout_index = state.entities.get_index_of(bot.scout_id);
        if (scout_index == INDEX_INVALID) {
            bot_release_scout(bot);
        }
    }

    // Check for invisible units
    for (const Entity& entity : state.entities) {
        if (state.players[entity.player_id].team == state.players[bot.player_id].team) {
            continue;
        }
        if (entity.type == ENTITY_PYRO || 
                (entity.type == ENTITY_LANDMINE && 
                match_is_entity_visible_to_player(state, entity, bot.player_id))) {
            bot_scout_set_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES, true);
            break;
        }
        if (entity.type == ENTITY_SHERIFFS ||
                (entity.type == ENTITY_DETECTIVE && 
                match_is_entity_visible_to_player(state, entity, bot.player_id))) {
            bot_scout_set_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES, true);
            break;
        }
    }

    // Remove expired danger
    uint32_t danger_index = 0;
    while (danger_index < bot.scout_danger.size()) {
        if (bot_scout_danger_is_expired(state, bot.scout_danger[danger_index], match_time_minutes)) {
            bot.scout_danger[danger_index] = bot.scout_danger.back();
            bot.scout_danger.pop_back();
        } else {
            danger_index++;
        }
    }

    // Bunker scouting
    for (uint32_t bunker_index = 0; bunker_index < state.entities.size(); bunker_index++) {
        const Entity& bunker = state.entities[bunker_index];
        EntityId bunker_id = state.entities.get_id_of(bunker_index);
        if (bunker.type != ENTITY_BUNKER ||
                !entity_is_selectable(bunker) ||
                state.players[bunker.player_id].team == state.players[bot.player_id].team ||
                !bot_has_scouted_entity(state, bot, bunker, bunker_id)) {
            continue;
        }

        // Remember how many garrisoned units are in the bunker
        if (match_is_entity_visible_to_player(state, bunker, bot.player_id)) {
            bot.bunker_garrison_count_memory[bunker_id] = bunker.garrisoned_units.size();
        }

        // Add bunkers to danger list
        bool is_bunker_already_in_danger_list = false;
        for (const BotScoutDanger& danger : bot.scout_danger) {
            if (danger.type == BOT_SCOUT_DANGER_TYPE_BUNKER && danger.bunker_id == bunker_id) {
                is_bunker_already_in_danger_list = true;
            }
        }
        if (is_bunker_already_in_danger_list) {
            continue;
        }

        bot.scout_danger.push_back((BotScoutDanger) {
            .type = BOT_SCOUT_DANGER_TYPE_BUNKER,
            .bunker_id = bunker_id
        });
    }
}

MatchInput bot_scout(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    // If we have no scout, it means we're not scouting
    if (bot.scout_id == ID_NULL) {
        // Determine if we should begin scouting
        if (!bot_should_scout(state, bot, match_time_minutes)) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        // Determine entities to scout
        bot.entities_to_scout.clear();
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const Entity& entity = state.entities[entity_index];
            EntityId entity_id = state.entities.get_id_of(entity_index);

            // Filter out units
            if (entity_is_unit(entity.type)) {
                continue;
            }

            // Filter out allied buildings
            if (entity_is_building(entity.type) &&
                    state.players[entity.player_id].team == state.players[bot.player_id].team) {
                continue;
            }

            // Filter out houses and landmines
            if (entity.type == ENTITY_HOUSE || entity.type == ENTITY_LANDMINE) {
                continue;
            }

            // Filter out things we've already scouted
            if (bot_has_scouted_entity(state, bot, entity, entity_id)) {
                continue;
            }

            bot.entities_to_scout.push_back(entity_id);
        }
        if (bot.entities_to_scout.empty()) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        // Find a scout
        bot.scout_id = bot_find_best_entity((BotFindBestEntityParams) {
            .state = state, 
            .filter = [&bot](const Entity& entity, EntityId entity_id) {
                return entity.player_id == bot.player_id && 
                    (entity.type == ENTITY_WAGON || entity.type == ENTITY_MINER) &&
                    entity.garrison_id == ID_NULL &&
                    !(entity.target.type == TARGET_BUILD || entity.target.type == TARGET_REPAIR || entity.target.type == TARGET_UNLOAD) &&
                    entity.garrisoned_units.empty() && 
                    !bot_is_entity_reserved(bot, entity_id);
            },
            .compare = [](const Entity&a, const Entity& b) {
                if (a.type == ENTITY_WAGON && b.type != ENTITY_WAGON) {
                    return true;
                }
                return false;
            }
        });
        if (bot.scout_id == ID_NULL) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
        if (bot.mode != BOT_MODE_OPENER_BANDIT_RUSH) {
            bot_reserve_entity(bot, bot.scout_id);
        }
    }

    const Entity& scout = state.entities.get_by_id(bot.scout_id);

    // Remove scouted and dead entities from to-scout list
    {
        uint32_t entities_to_scout_index = 0;
        while (entities_to_scout_index < bot.entities_to_scout.size()) {
            EntityId entity_id = bot.entities_to_scout[entities_to_scout_index];
            uint32_t entity_index = state.entities.get_index_of(entity_id);
            if (entity_index == INDEX_INVALID || bot_has_scouted_entity(state, bot, state.entities[entity_index], entity_id)) {
                bot.entities_to_scout[entities_to_scout_index] = bot.entities_to_scout.back();
                bot.entities_to_scout.pop_back();
            } else {
                entities_to_scout_index++;
            }
        }
    }

    // Finish scout
    if (bot.entities_to_scout.empty()) {
        MatchInput input = bot_return_entity_to_nearest_hall(state, bot, bot.scout_id);
        bot_release_scout(bot);
        bot.last_scout_time = match_time_minutes;
        return input;
    }

    // If we are taking damage, flee 
    if (scout.taking_damage_timer != 0) {
        EntityId attacker_id = bot_find_best_entity((BotFindBestEntityParams) {
            .state = state,
            .filter = [&bot](const Entity& entity, EntityId entity_id) {
                return entity_is_unit(entity.type) &&
                        entity.health != 0 &&
                        entity.target.type == TARGET_ATTACK_ENTITY && 
                        entity.target.id == bot.scout_id;
            },
            .compare = bot_closest_manhattan_distance_to(scout.cell)
        });
        if (attacker_id != ID_NULL) {
            const Entity& attacker = state.entities.get_by_id(attacker_id);

            // Add a danger cell
            bool is_existing_danger_cell_nearby = false;
            for (const BotScoutDanger& danger : bot.scout_danger) {
                ivec2 danger_cell = bot_scout_danger_get_cell(state, danger);
                if (ivec2::manhattan_distance(danger_cell, attacker.cell) < BOT_SQUAD_GATHER_DISTANCE) {
                    is_existing_danger_cell_nearby = true;
                    break;
                }
            }
            if (!is_existing_danger_cell_nearby) {
                // Danger cell is being added as type of units
                // It's possible that the attacker is actually inside a bunker,
                // but we should never reach this part of the code when that happens because
                // that would mean that there is an existing bunker-type scout danger nearby
                static const uint32_t BOT_SCOUT_DANGER_UNITS_TTL = 5;
                bot.scout_danger.push_back((BotScoutDanger) {
                    .type = BOT_SCOUT_DANGER_TYPE_UNITS,
                    .units = (BotScoutDangerUnits) {
                        .cell = attacker.cell,
                        .expiration_time_minutes = match_time_minutes + BOT_SCOUT_DANGER_UNITS_TTL
                    }
                });
            }

            // If there is a base near the danger that we haven't scouted, then assume that it exists
            for (EntityId entity_to_scout_id : bot.entities_to_scout) {
                const Entity& entity_to_scout = state.entities.get_by_id(entity_to_scout_id);
                if (entity_to_scout.type != ENTITY_HALL || 
                        ivec2::manhattan_distance(entity_to_scout.cell, attacker.cell) > BOT_SQUAD_GATHER_DISTANCE) {
                    continue;
                }

                // Confirm that this base is the nearest base to the danger
                EntityId nearest_hall_to_danger_id = bot_find_best_entity((BotFindBestEntityParams) {
                    .state = state,
                    .filter = [&attacker](const Entity& hall, EntityId hall_id) {
                        return hall.type == ENTITY_HALL &&
                                entity_is_selectable(hall) &&
                                hall.player_id == attacker.player_id;
                    },
                    .compare = bot_closest_manhattan_distance_to(attacker.cell)
                });
                if (nearest_hall_to_danger_id != entity_to_scout_id) {
                    continue;
                }

                EntityId goldmine_nearest_to_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
                    .state = state,
                    .filter = [](const Entity& goldmine, EntityId goldmine_id) {
                        return goldmine.type == ENTITY_GOLDMINE;
                    },
                    .compare = bot_closest_manhattan_distance_to(state.entities.get_by_id(nearest_hall_to_danger_id).cell)
                });
                GOLD_ASSERT(goldmine_nearest_to_hall_id != ID_NULL);

                // Mark the entity as scouted even though we haven't technically seen it yet
                bot.is_entity_assumed_to_be_scouted[nearest_hall_to_danger_id] = true;
                bot.is_entity_assumed_to_be_scouted[goldmine_nearest_to_hall_id] = true;

                // Remove the hall and goldmine from the to-scout list
                uint32_t entities_to_scout_index = 0;
                while (entities_to_scout_index < bot.entities_to_scout.size()) {
                    if (bot.entities_to_scout[entities_to_scout_index] == nearest_hall_to_danger_id ||
                            bot.entities_to_scout[entities_to_scout_index] == goldmine_nearest_to_hall_id) {
                        bot.entities_to_scout[entities_to_scout_index] = bot.entities_to_scout.back();
                        bot.entities_to_scout.pop_back();
                    } else {
                        entities_to_scout_index++;
                    }
                }

                break;
            }

            return bot_unit_flee(state, bot, bot.scout_id);
        }
    }

    // Check for any danger along the path
    bool is_danger_along_path = false;
    static const uint32_t DANGER_RADIUS = 4;
    for (uint32_t path_index = 0; path_index < scout.path.size(); path_index += DANGER_RADIUS) {
        for (const BotScoutDanger& danger : bot.scout_danger) {
            ivec2 danger_cell = bot_scout_danger_get_cell(state, danger);
            if (ivec2::manhattan_distance(scout.path[path_index], danger_cell) < DANGER_RADIUS * 2 &&
                    map_get_tile(state.map, scout.path[path_index]).elevation == map_get_tile(state.map, danger_cell).elevation) {
                is_danger_along_path = true;
            }
        }
        if (is_danger_along_path) {
            break;
        }
    }

    // If there is danger, abandon scouting this target
    if (is_danger_along_path) {
        uint32_t entities_to_scout_index = 0;
        while (entities_to_scout_index < bot.entities_to_scout.size() &&
                    bot.entities_to_scout[entities_to_scout_index] != scout.target.id) {
            entities_to_scout_index++;
        }

        if (entities_to_scout_index < bot.entities_to_scout.size()) {
            bot.entities_to_scout[entities_to_scout_index] = bot.entities_to_scout.back();
            bot.entities_to_scout.pop_back();
        }

        // If we are moving towards our now-abandoned target, order the scout to stop
        // It will get a new order on the next iteration
        if (scout.target.type == TARGET_ENTITY) {
            MatchInput input;
            input.type = MATCH_INPUT_STOP;
            input.stop.entity_ids[0] = bot.scout_id;
            input.stop.entity_count = 1;
            return input;
        } else {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

    uint32_t closest_unscouted_entity_index = INDEX_INVALID;
    for (EntityId entity_id : bot.entities_to_scout) {
        uint32_t entity_index = state.entities.get_index_of(entity_id);
        if (closest_unscouted_entity_index == INDEX_INVALID || 
                ivec2::manhattan_distance(state.entities[entity_index].cell, scout.cell) <
                ivec2::manhattan_distance(state.entities[closest_unscouted_entity_index].cell, scout.cell)) {
            closest_unscouted_entity_index = entity_index;
        }
    }
    EntityId unscouted_entity_id = state.entities.get_id_of(closest_unscouted_entity_index);
    GOLD_ASSERT(unscouted_entity_id != ID_NULL);

    if (scout.target.type == TARGET_ENTITY && scout.target.id == unscouted_entity_id) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_ENTITY;
    input.move.shift_command = 0;
    input.move.target_id = unscouted_entity_id;
    input.move.target_cell = ivec2(0, 0);
    input.move.entity_ids[0] = bot.scout_id;
    input.move.entity_count = 1;
    return input;
}

void bot_release_scout(Bot& bot) {
    if (bot.scout_id == ID_NULL) {
        return;
    }
    if (bot_is_entity_reserved(bot, bot.scout_id)) {
        bot_release_entity(bot, bot.scout_id);
    }
    bot.scout_id = ID_NULL;
}

bool bot_should_scout(const MatchState& state, const Bot& bot, uint32_t match_time_minutes) {
    uint32_t next_scout_time;
    if (bot.last_scout_time == 0)  {
        next_scout_time = 0;
    } else if (match_time_minutes < 10) {
        next_scout_time = bot.last_scout_time + 2;
    } else {
        next_scout_time = bot.last_scout_time + 5;
    }

    return match_time_minutes >= next_scout_time;
}

bool bot_scout_danger_is_expired(const MatchState& state, const BotScoutDanger& danger, uint32_t match_time_mintutes) {
    switch (danger.type) {
        case BOT_SCOUT_DANGER_TYPE_UNITS: {
            return match_time_mintutes >= danger.units.expiration_time_minutes;
        }
        case BOT_SCOUT_DANGER_TYPE_BUNKER: {
            uint32_t bunker_index = state.entities.get_index_of(danger.bunker_id);
            return bunker_index == INDEX_INVALID || state.entities[bunker_index].health == 0;
        }
    }
}

ivec2 bot_scout_danger_get_cell(const MatchState& state, const BotScoutDanger& danger) {
    switch (danger.type) {
        case BOT_SCOUT_DANGER_TYPE_UNITS: {
            return danger.units.cell;
        }
        case BOT_SCOUT_DANGER_TYPE_BUNKER: {
            return state.entities.get_by_id(danger.bunker_id).cell;
        }
    }
}

bool bot_scout_check_info(const Bot& bot, uint32_t flag) {
    return (bot.scout_info & flag) == flag;
}

void bot_scout_set_info(Bot& bot, uint32_t flag, bool value) {
    if (value) {
        bot.scout_info |= flag;
    } else {
        bot.scout_info &= ~flag;
    }
}

// UTIL

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
    ivec2 target_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_size, state.entities.get_by_id(nearest_hall_id).cell, entity_get_data(ENTITY_HALL).cell_size, MAP_OPTION_IGNORE_UNITS);

    std::vector<ivec2> path_from_hall;
    map_pathfind(state.map, CELL_LAYER_GROUND, target_cell, entity.cell, entity_size, &path_from_hall, MAP_OPTION_IGNORE_UNITS);

    static const int DISTANCE_FROM_HALL = 8;
    if (!path_from_hall.empty()) {
        target_cell = path_from_hall[std::min((int)path_from_hall.size() - 1, DISTANCE_FROM_HALL)];
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

MatchInput bot_unit_flee(const MatchState& state, const Bot& bot, EntityId entity_id) {
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
    ivec2 target_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_size, state.entities.get_by_id(nearest_hall_id).cell, entity_get_data(ENTITY_HALL).cell_size, MAP_OPTION_IGNORE_UNITS);

    static const int FLEE_DISTANCE = 16;
    if (ivec2::manhattan_distance(entity.cell, target_cell) < FLEE_DISTANCE &&
            map_get_pathing_region(state.map, entity.cell) == map_get_pathing_region(state.map, target_cell)) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    std::vector<ivec2> path_from_hall;
    map_pathfind(state.map, CELL_LAYER_GROUND, target_cell, entity.cell, entity_size, &path_from_hall, MAP_OPTION_IGNORE_UNITS);

    if (path_from_hall.empty()) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = path_from_hall[std::min((int)path_from_hall.size() - 1, FLEE_DISTANCE)];
    input.move.target_id = ID_NULL;
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

MatchInput bot_cancel_in_progress_buildings(const MatchState& state, const Bot& bot) {
    for (uint32_t building_index = 0; building_index < state.entities.size(); building_index++) {
        const Entity& building = state.entities[building_index];
        EntityId building_id = state.entities.get_id_of(building_index);

        // Filter down to bot-owned, in-progress buildings 
        if (building.player_id != bot.player_id ||
                building.mode != MODE_BUILDING_IN_PROGRESS) {
            continue;
        }

        // Score how well-defended this building is
        int nearby_ally_score = 0;
        int nearby_enemy_score = 0;
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const Entity& entity = state.entities[entity_index];
            EntityId entity_id = state.entities.get_id_of(entity_index);

            if (!(entity.type == ENTITY_BUNKER || entity_is_unit(entity.type)) ||
                    entity.health == 0 ||
                    entity.mode == MODE_BUILDING_IN_PROGRESS ||
                    ivec2::manhattan_distance(entity.cell, building.cell) > BOT_SQUAD_GATHER_DISTANCE ||
                    !match_is_entity_visible_to_player(state, entity, bot.player_id)) {
                continue;
            }
            if (state.players[entity.player_id].team == state.players[bot.player_id].team) {
                nearby_ally_score += bot_score_entity(state, bot, entity, entity_id);
            } else {
                nearby_enemy_score += bot_score_entity(state, bot, entity, entity_id);
            }
        }

        if (nearby_enemy_score == 0) {
            continue;
        }
        if (building.health > 100 && nearby_enemy_score < 3 * BOT_UNIT_SCORE) {
            continue;
        }
        if (building.health > entity_get_data(building.type).max_health / 4 && nearby_ally_score > nearby_enemy_score) {
            continue;
        }

        MatchInput input;
        input.type = MATCH_INPUT_BUILD_CANCEL;
        input.build_cancel.building_id = building_id;
        return input;
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
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

MatchInput bot_set_rally_points(const MatchState& state, Bot& bot) {
    while (!bot.buildings_to_set_rally_points.empty()) {
        EntityId building_id = bot.buildings_to_set_rally_points.front();
        bot.buildings_to_set_rally_points.pop();
        const Entity& building = state.entities.get_by_id(building_id);

        // Choose rally point
        ivec2 rally_point;
        if (building.type == ENTITY_HALL) {
            EntityId goldmine_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state,
                .filter = [](const Entity& goldmine, EntityId goldmine_id) {
                    return goldmine.type == ENTITY_GOLDMINE;
                },
                .compare = bot_closest_manhattan_distance_to(building.cell)
            });

            rally_point = (state.entities.get_by_id(goldmine_id).cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
        } else {
            ivec2 rally_cell = bot_choose_building_rally_point(state, bot, building);
            if (rally_cell.x == -1) {
                continue;
            }
            rally_point = cell_center(rally_cell).to_ivec2();
        }

        if (building.rally_point == rally_point) {
            continue;
        }

        MatchInput rally_input;
        rally_input.type = MATCH_INPUT_RALLY;
        rally_input.rally.building_count = 1;
        rally_input.rally.building_ids[0] = building_id;
        rally_input.rally.rally_point = rally_point;
        return rally_input;
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
    return [cell](const Entity& a, const Entity& b) {
        return ivec2::manhattan_distance(a.cell, cell) < ivec2::manhattan_distance(b.cell, cell);
    };
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

int bot_score_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id) {
    if (entity_is_building(entity.type) && entity.type != ENTITY_BUNKER) {
        return 0;
    }
    // The miner is worth 1/4 of a military unit
    // Since the scores are ints, everything is multiplied by BOT_UNIT_SCORE to account for this
    switch (entity.type) {
        case ENTITY_MINER:
            return 1;
        case ENTITY_BUNKER: {
            int bunker_garrison_count;
            if (match_is_entity_visible_to_player(state, entity, bot.player_id)) {
                bunker_garrison_count = entity.garrisoned_units.size();
            } else if (bot_has_scouted_entity(state, bot, entity, entity_id)) {
                bunker_garrison_count = bot.bunker_garrison_count_memory.at(entity_id);
            } else {
                bunker_garrison_count = 0;
            }

            return bunker_garrison_count * BOT_UNIT_SCORE_IN_BUNKER;
        }
        case ENTITY_WAGON:
            return entity.garrisoned_units.size() * BOT_UNIT_SCORE;
        case ENTITY_CANNON:
            return 3 * BOT_UNIT_SCORE;
        default:
            return 1 * BOT_UNIT_SCORE;
    }
}

int bot_score_entity_list(const MatchState& state, const Bot& bot, const std::vector<EntityId>& entity_list) {
    int score = 0;

    for (EntityId entity_id : entity_list) {
        uint32_t entity_index = state.entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID) {
            continue;
        }
        score += bot_score_entity(state, bot, state.entities[entity_index], entity_id);
    }

    return score;
}

BotEntityCount bot_entity_count_empty() {
    BotEntityCount list;
    list.fill(0);
    return list;
}

uint32_t bot_entity_count_size(const BotEntityCount& count) {
    uint32_t size = 0;
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        size += count[entity_type];
    }
    return size;
}

bool bot_is_entity_count_empty(const BotEntityCount& count) {
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (count[entity_type] != 0) { 
            return false;
        }
    }
    return true;
}

BotEntityCount bot_entity_count_add(const BotEntityCount& left, const BotEntityCount& right) {
    BotEntityCount list = bot_entity_count_empty();
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        list[entity_type] = left[entity_type] + right[entity_type];
    }

    return list;
}

BotEntityCount bot_entity_count_subtract(const BotEntityCount& left, const BotEntityCount& right) {
    BotEntityCount list = bot_entity_count_empty();
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (right[entity_type] > left[entity_type]) {
            list[entity_type] = 0;
        } else {
            list[entity_type] = left[entity_type] - right[entity_type];
        }
    }

    return list;
}

bool bot_entity_count_is_gte_to(const BotEntityCount& list, const BotEntityCount& other) {
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (list[entity_type] < other[entity_type]) {
            return false;
        }
    }
    return true;
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
    if (bot.is_entity_assumed_to_be_scouted.find(entity_id) != bot.is_entity_assumed_to_be_scouted.end()) {
        return true;
    }
    // Otherwise, they have not scouted it
    return false;
}

bool bot_has_landmine_squad(const Bot& bot) {
    for (const BotSquad& squad : bot.squads) {
        if (squad.type == BOT_SQUAD_TYPE_LANDMINES) {
            return true;
        }
    }

    return false;
}

bool bot_has_desired_squad_of_type(const Bot& bot, BotSquadType type) {
    for (const BotDesiredSquad& squad : bot.desired_squads) {
        if (squad.type == type) {
            return true;
        }
    }

    return false;
}

std::unordered_map<uint32_t, int> bot_get_enemy_hall_defense_scores(const MatchState& state, const Bot& bot) {
    std::unordered_map<uint32_t, int> enemy_hall_defense_score;
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

    if (enemy_hall_defense_score.empty()) {
        return enemy_hall_defense_score;
    }

    // Second pass, find all hall defenses
    // (this way allows us to avoid O(n^2) because we don't have to search
    // all entities for every defense just to find the nearest hall)
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        // Filter out goldmines and miners
        if (entity.type == ENTITY_GOLDMINE || entity.type == ENTITY_MINER) {
            continue;
        }
        // Filter out allied units
        if (state.players[entity.player_id].team == 
                state.players[bot.player_id].team) {
            continue;
        }
        // Filter out buildings except for bunkers and landmines
        if (entity_is_building(entity.type) && 
                !(entity.type == ENTITY_BUNKER || 
                    entity.type == ENTITY_LANDMINE)) {
            continue;
        }
        // If it's a bunker, make sure that we've scouted it
        if (entity.type == ENTITY_BUNKER && !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }
        // If it's a landmine, make sure that we've scouted that they have landmines
        if (entity.type == ENTITY_LANDMINE && !bot_scout_check_info(bot, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES)) {
            continue;
        }

        uint32_t nearest_hall_index = INDEX_INVALID;
        for (auto it : enemy_hall_defense_score) {
            if (ivec2::manhattan_distance(entity.cell, state.entities[it.first].cell) < BOT_SQUAD_GATHER_DISTANCE * 2) {
                continue;
            }

            if (nearest_hall_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(entity.cell, state.entities[it.first].cell) <
                    ivec2::manhattan_distance(entity.cell, state.entities[nearest_hall_index].cell)) {
                nearest_hall_index = it.first;
            }
        }

        if (nearest_hall_index == INDEX_INVALID) {
            continue;
        }

        if (entity.type == ENTITY_LANDMINE) {
            enemy_hall_defense_score[nearest_hall_index] += 2;
        } else {
            enemy_hall_defense_score[nearest_hall_index] += bot_score_entity(state, bot, entity, entity_id);
        }
    }

    // Add retreat entries to defense score
    for (auto it : enemy_hall_defense_score) {
        EntityId hall_id = state.entities.get_id_of(it.first);
        auto retreat_memory_it = bot.retreat_memory.find(hall_id);
        if (retreat_memory_it == bot.retreat_memory.end()) {
            continue;
        }
        int retreat_memory_score = bot_score_entity_list(state, bot, retreat_memory_it->second.entity_list) + retreat_memory_it->second.desired_lead;
        enemy_hall_defense_score[it.first] = std::max(enemy_hall_defense_score[it.first], retreat_memory_score);
    }

    return enemy_hall_defense_score;
}

int bot_get_least_defended_enemy_hall_score(const MatchState& state, const Bot& bot) {
    std::unordered_map<uint32_t, int> defense_scores = bot_get_enemy_hall_defense_scores(state, bot);
    int least_defended_hall_score = -1;
    for (auto it : defense_scores) {
        if (least_defended_hall_score == -1) {
            least_defended_hall_score = it.second;
            continue;
        }
        least_defended_hall_score = std::min(least_defended_hall_score, it.second);
    }

    return least_defended_hall_score;
}

uint32_t bot_get_mining_base_count(const MatchState& state, const Bot& bot) {
    uint32_t mining_base_count = 0;
    for (const Entity& goldmine : state.entities) {
        if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
            continue;
        }
        EntityId hall_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);
        if (hall_id == ID_NULL) {
            continue;
        }
        const Entity& hall = state.entities.get_by_id(hall_id);
        if (hall.player_id == bot.player_id) {
            mining_base_count++;
        }
    }

    return mining_base_count;
}
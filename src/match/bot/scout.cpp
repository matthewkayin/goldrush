#include "scout.h"

#include "util.h"
#include "core/logger.h"

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
            bot.scout_enemy_has_landmines = true;
            break;
        }
        if (entity.type == ENTITY_SHERIFFS ||
                (entity.type == ENTITY_DETECTIVE && 
                match_is_entity_visible_to_player(state, entity, bot.player_id))) {
            bot.scout_enemy_has_detectives = true;
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

    // Add bunkers to danger list
    for (uint32_t bunker_index = 0; bunker_index < state.entities.size(); bunker_index++) {
        const Entity& bunker = state.entities[bunker_index];
        EntityId bunker_id = state.entities.get_id_of(bunker_index);
        if (bunker.type != ENTITY_BUNKER ||
                !entity_is_selectable(bunker) ||
                state.players[bunker.player_id].team == state.players[bot.player_id].team ||
                !bot_has_scouted_entity(state, bot, bunker, bunker_id)) {
            continue;
        }

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
        if (bot.goal.type != BOT_GOAL_BANDIT_RUSH) {
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
                bot.scout_assumed_entities[nearest_hall_to_danger_id] = true;
                bot.scout_assumed_entities[goldmine_nearest_to_hall_id] = true;
            }

            return bot_unit_flee(state, bot, bot.scout_id);
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

    // Check for any danger along the path
    bool is_danger_along_path = false;
    std::vector<ivec2> path;
    ivec2 scout_path_dest_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, scout.cell, 2, state.entities[closest_unscouted_entity_index].cell, entity_get_data(state.entities[closest_unscouted_entity_index].type).cell_size, MAP_IGNORE_UNITS | MAP_IGNORE_MINERS);
    map_pathfind(state.map, CELL_LAYER_GROUND, scout.cell, scout_path_dest_cell, 2, &path, MAP_IGNORE_UNITS | MAP_IGNORE_MINERS);
    static const uint32_t DANGER_RADIUS = 4;
    for (uint32_t path_index = 0; path_index < path.size(); path_index += DANGER_RADIUS) {
        for (const BotScoutDanger& danger : bot.scout_danger) {
            ivec2 danger_cell = bot_scout_danger_get_cell(state, danger);
            if (ivec2::manhattan_distance(path[path_index], danger_cell) < DANGER_RADIUS * 2 &&
                    map_get_tile(state.map, path[path_index]).elevation == map_get_tile(state.map, danger_cell).elevation) {
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
                    bot.entities_to_scout[entities_to_scout_index] != unscouted_entity_id) {
            entities_to_scout_index++;
        }
        GOLD_ASSERT(entities_to_scout_index < bot.entities_to_scout.size());

        bot.entities_to_scout[entities_to_scout_index] = bot.entities_to_scout.back();
        bot.entities_to_scout.pop_back();

        // If we are moving towards our now-abandoned target, order the scout to stop
        // It will get a new order on the next iteration
        if (scout.target.type == TARGET_ENTITY && scout.target.id == unscouted_entity_id) {
            MatchInput input;
            input.type = MATCH_INPUT_STOP;
            input.stop.entity_ids[0] = bot.scout_id;
            input.stop.entity_count = 1;
            return input;
        } else {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

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

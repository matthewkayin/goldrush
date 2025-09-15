#include "squad.h"

#include "util.h"
#include "scout.h"
#include "../upgrade.h"
#include "core/logger.h"

static const uint32_t BOT_SQUAD_LANDMINE_MAX = 6;

void bot_squad_create_from_goal(const MatchState& state, Bot& bot, const BotGoal& goal) {
    GOLD_ASSERT(goal.desired_squad_type != BOT_SQUAD_TYPE_NONE);

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
        if (squad_entity_count[entity.type] >= goal.desired_entities[entity.type]) {
            continue;
        }

        if (entity_id == bot.scout_id) {
            bot_release_scout(bot);
        }

        squad.entities.push_back(entity_id);
        squad_entity_count[entity.type]++;
    }

    if (squad.entities.empty()) {
        log_trace("BOT: no entities in squad, abandoning...");
        return;
    }

    squad.type = goal.desired_squad_type;
    squad.target_cell = ivec2(-1, -1);
    if (squad.type == BOT_SQUAD_TYPE_HARASS || squad.type == BOT_SQUAD_TYPE_PUSH) {
        squad.target_cell = bot_squad_choose_attack_point(state, bot, squad);

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
    } else if (squad.type == BOT_SQUAD_TYPE_LANDMINES) {
        const Entity& pyro = state.entities.get_by_id(squad.entities[0]);
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
            log_trace("BOT: landmines squad found no nearby halls. abandoning...");
            return;
        }
        squad.target_cell = state.entities.get_by_id(nearest_hall_id).cell;
    } else if (squad.type == BOT_SQUAD_TYPE_DEFENSE) {
        squad.target_cell = bot_squad_choose_defense_point(state, bot, squad);
        if (squad.target_cell.x == -1) {
            log_trace("BOT: no defense point found. Abandoning squad.");
            return;
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

    // Retreat
    if (squad.type == BOT_SQUAD_TYPE_PUSH && bot_squad_should_retreat(state, bot, squad)) {
        return bot_squad_return_to_nearest_base(state, bot, squad);
    }

    // Attack micro
    std::vector<EntityId> unengaged_units;
    bool is_enemy_near_squad = false;
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

    // If harassing or pushing with pyros, make sure they have enough energy first
    if (squad.type == BOT_SQUAD_TYPE_HARASS || squad.type == BOT_SQUAD_TYPE_PUSH) {
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
            continue;
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
                if (it.second >= BOT_SQUAD_LANDMINE_MAX) {
                    continue;
                }
                if (nearest_hall_index == INDEX_INVALID || 
                        ivec2::manhattan_distance(unit.cell, state.entities[it.first].cell) < 
                        ivec2::manhattan_distance(unit.cell, state.entities[nearest_hall_index].cell)) {
                    nearest_hall_index = it.first;
                }
            }

            // If there are no halls which still need landmines, then dissolve the squad
            if (nearest_hall_index == INDEX_INVALID) {
                bot_squad_dissolve(state, bot, squad);
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
                bot_squad_dissolve(state, bot, squad);
                return (MatchInput) { .type = MATCH_INPUT_NONE };
            }

            // Pathfind to that base so that we place mines close to where the enemies would be coming in
            ivec2 enemy_base_cell = state.entities.get_by_id(nearest_enemy_hall_id).cell;
            std::vector<ivec2> path;
            bot_pathfind_and_avoid_landmines(state, bot, enemy_base_cell, hall.cell, &path);

            // Find the place along the path that is within the base radius
            int path_index = 0;
            while (path_index < path.size() && ivec2::manhattan_distance(path[path_index], hall.cell) > BOT_SQUAD_GATHER_DISTANCE) {
                path_index++;
            }

            if (path_index >= path.size()) {
                bot_squad_dissolve(state, bot, squad);
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
                bot.scout_enemy_has_landmines && 
                !match_player_has_upgrade(state, bot.player_id, UPGRADE_PRIVATE_EYE)) {
            continue;
        }

        // If the unit is far away, add it to the distant infantry/cavalry list
        if (ivec2::manhattan_distance(unit.cell, squad.target_cell) > BOT_SQUAD_GATHER_DISTANCE / 2 ||
                squad.type == BOT_SQUAD_TYPE_BUNKER) {
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
                if (squad.type != BOT_SQUAD_TYPE_BUNKER && 
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
    bool squad_has_carriers = false;
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

        squad_has_carriers = true;

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

    if ((squad.type == BOT_SQUAD_TYPE_HARASS ||
            squad.type == BOT_SQUAD_TYPE_PUSH ||
            squad.type == BOT_SQUAD_TYPE_RESERVES) &&
            !squad_has_carriers && 
            distant_infantry.empty() && 
            !is_enemy_near_squad) {
        if (squad.type == BOT_SQUAD_TYPE_RESERVES) {
            bot_squad_dissolve(state, bot, squad);
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
        return bot_squad_return_to_nearest_base(state, bot, squad);
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

bool bot_squad_should_retreat(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    // Score the enemy's army
    uint32_t enemy_army_score = 0;
    for (const Entity& entity : state.entities) {
        // Filter down to enemy units or bunkers which the bot can see
        if (entity.type == ENTITY_GOLDMINE ||
                (entity_is_building(entity.type) && entity.type != ENTITY_BUNKER) ||
                state.players[entity.player_id].team == state.players[bot.player_id].team ||
                !entity_is_selectable(entity) || 
                !match_is_entity_visible_to_player(state, entity, bot.player_id)) {
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

        enemy_army_score += bot_score_entity(entity);
    }

    // Score the allied army
    uint32_t squad_score = 0;
    for (EntityId entity_id : squad.entities) {
        const Entity& entity = state.entities.get_by_id(entity_id);
        squad_score += bot_score_entity(entity);
    }

    return enemy_army_score > squad_score + 8;
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
        bot_squad_dissolve(state, bot, squad);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // To determine the retreat_cell, first path to the nearby base
    std::vector<ivec2> path;
    ivec2 hall_cell = state.entities.get_by_id(nearest_allied_hall_id).cell;
    map_pathfind(state.map, CELL_LAYER_GROUND, squad.target_cell, hall_cell, 1, &path, MAP_IGNORE_UNITS);

    // Then walk backwards on the path a little so that the units do not end up right up against the town hall
    int path_index = path.size() - 1;
    while (path_index >= 0 && ivec2::manhattan_distance(path[path_index], hall_cell) < BOT_SQUAD_GATHER_DISTANCE) {
        path_index--;
    }
    ivec2 retreat_cell = path_index < 0 ? hall_cell : path[path_index];

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

bool bot_squad_is_detective_harass(const MatchState& state, const BotSquad& squad) {
    if (squad.type != BOT_SQUAD_TYPE_HARASS) {
        return false;
    }
    for (EntityId entity_id : squad.entities) {
        if (state.entities.get_by_id(entity_id).type != ENTITY_DETECTIVE) {
            return false;
        }
    }
    return true;
}

bool bot_squad_can_defend_against_detectives(const MatchState& state, const BotSquad& squad) {
    if (!(squad.type == BOT_SQUAD_TYPE_BUNKER || 
            squad.type == BOT_SQUAD_TYPE_RESERVES)) {
        return false;
    }

    bool has_detection = false;
    bool has_damage = false;

    for (EntityId entity_id : squad.entities) {
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

ivec2 bot_squad_get_center(const MatchState& state, const BotSquad& squad) {
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

ivec2 bot_squad_choose_attack_point(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    ivec2 center_point = bot_squad_get_center(state, squad);
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
            if (a.type != ENTITY_HALL && b.type == ENTITY_HALL) {
                return false;
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

    // Find the least defended hall
    std::unordered_map<uint32_t, uint32_t> enemy_hall_defense_score = bot_get_enemy_hall_defense_scores(state, bot);
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

ivec2 bot_squad_choose_defense_point(const MatchState& state, const Bot& bot, const BotSquad& squad) {
    // Determine if this squad provides detection
    bool is_detection_squad = bot.scout_enemy_has_detectives && bot_squad_can_defend_against_detectives(state, squad);

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
        if (!(existing_squad.type == BOT_SQUAD_TYPE_DEFENSE ||
                existing_squad.type == BOT_SQUAD_TYPE_BUNKER)) {
            continue;
        }
        if (is_detection_squad && !bot_squad_can_defend_against_detectives(state, existing_squad)) {
            continue;
        }

        uint32_t nearest_hall_index = INDEX_INVALID;
        for (auto it : hall_defenses) {
            if (nearest_hall_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(state.entities[it.first].cell, squad.target_cell) <
                    ivec2::manhattan_distance(state.entities[nearest_hall_index].cell, squad.target_cell)) {
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
    ivec2 path_start_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, hall.cell, entity_get_data(hall.type).cell_size, 1, nearest_enemy_building.cell, MAP_IGNORE_UNITS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_building.cell, entity_get_data(nearest_enemy_building.type).cell_size, MAP_IGNORE_UNITS);
    map_pathfind(state.map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, &path, MAP_IGNORE_UNITS);

    return path.size() < 8 ? path_start_cell : path[7];
}

void bot_pathfind_and_avoid_landmines(const MatchState& state, const Bot& bot, ivec2 from, ivec2 to, std::vector<ivec2>* path) {
    static const int CELL_SIZE = 1;

    std::vector<MapPathNode> frontier;
    std::vector<MapPathNode> explored;
    std::vector<int> explored_indices(state.map.width * state.map.height, -1);

    frontier.push_back((MapPathNode) {
        .cost = fixed::from_int(0),
        .distance = fixed::from_int(ivec2::manhattan_distance(from, to)),
        .parent = -1,
        .cell = from
    });

    uint32_t closest_explored = 0;
    bool found_path = false;
    MapPathNode path_end;

    while (!frontier.empty()) {
        // Find the smallest path
        uint32_t smallest_index = 0;
        for (uint32_t i = 1; i < frontier.size(); i++) {
            if (frontier[i].score() < frontier[smallest_index].score()) {
                smallest_index = i;
            }
        }

        // Pop the smallest path
        MapPathNode smallest = frontier[smallest_index];
        frontier[smallest_index] = frontier.back();
        frontier.pop_back();

        // If it's the solution, return it
        if (smallest.cell == to) {
            found_path = true;
            path_end = smallest;
            break;
        }

        // Otherwise, add this tile to the explored list
        explored.push_back(smallest);
        explored_indices[smallest.cell.x + (smallest.cell.y * state.map.width)] = explored.size() - 1;
        if (explored[explored.size() - 1].distance < explored[closest_explored].distance) {
            closest_explored = explored.size() - 1;
        }

        if (explored.size() > 1999) {
            // log_warn("Pathfinding from %vi to %vi too long, going with closest explored...", &from, &to);
            break;
        }

        // Consider all children
        // Adjacent cells are considered first so that we can use information about them to inform whether or not a diagonal movement is allowed
        bool is_adjacent_direction_blocked[4] = { true, true, true, true };
        const int CHILD_DIRECTIONS[DIRECTION_COUNT] = { DIRECTION_NORTH, DIRECTION_EAST, DIRECTION_SOUTH, DIRECTION_WEST, 
                                                        DIRECTION_NORTHEAST, DIRECTION_SOUTHEAST, DIRECTION_SOUTHWEST, DIRECTION_NORTHWEST };
        for (int direction_index = 0; direction_index < DIRECTION_COUNT; direction_index++) {
            int direction = CHILD_DIRECTIONS[direction_index];
            MapPathNode child = (MapPathNode) {
                .cost = smallest.cost + (direction % 2 == 1 ? (fixed::from_int(3) / 2) : fixed::from_int(1)),
                .distance = fixed::from_int(ivec2::manhattan_distance(smallest.cell + DIRECTION_IVEC2[direction], to)),
                .parent = (int)explored.size() - 1,
                .cell = smallest.cell + DIRECTION_IVEC2[direction]
            };

            // Don't consider out of bounds children
            if (!map_is_cell_rect_in_bounds(state.map, child.cell, CELL_SIZE)) {
                continue;
            }

            // Don't consider if blocked, but only consider static obstacles, not buildings or units
            Cell map_ground_cell = map_get_cell(state.map, CELL_LAYER_GROUND, child.cell);
            if (map_ground_cell.type == CELL_BLOCKED || 
                    map_ground_cell.type == CELL_UNREACHABLE || 
                    (map_ground_cell.type >= CELL_DECORATION_1 && map_ground_cell.type <= CELL_DECORATION_5)) {
                continue;
            }

            // Skip cells that are too close to landmines
            bool is_cell_too_close_to_landmine = false;
            for (int y = child.cell.y - 1; y < child.cell.y + 2; y++) {
                for (int x = child.cell.x - 1; x < child.cell.x + 2; x++) {
                    ivec2 cell = ivec2(x, y);
                    if (!map_is_cell_in_bounds(state.map, cell)) {
                        continue;
                    }

                    Cell map_underground_cell = map_get_cell(state.map, CELL_LAYER_UNDERGROUND, cell);
                    if (map_underground_cell.type == CELL_BUILDING) {
                        is_cell_too_close_to_landmine = true;
                    }
                }
            }
            if (is_cell_too_close_to_landmine) {
                continue;
            }

            // Don't allow diagonal movement through cracks
            if (direction % 2 == 0) {
                is_adjacent_direction_blocked[direction / 2] = false;
            } else {
                int next_direction = direction + 1 == DIRECTION_COUNT ? 0 : direction + 1;
                int prev_direction = direction - 1;
                if (is_adjacent_direction_blocked[next_direction / 2] && is_adjacent_direction_blocked[prev_direction / 2]) {
                    continue;
                }
            }

            // Don't consider already explored children
            if (explored_indices[child.cell.x + (child.cell.y * state.map.width)] != -1) {
                continue;
            }

            uint32_t frontier_index;
            for (frontier_index = 0; frontier_index < frontier.size(); frontier_index++) {
                MapPathNode& frontier_node = frontier[frontier_index];
                if (frontier_node.cell == child.cell) {
                    break;
                }
            }
            // If it is in the frontier...
            if (frontier_index < frontier.size()) {
                // ...and the child represents a shorter version of the frontier path, then replace the frontier version with the shorter child
                if (child.score() < frontier[frontier_index].score()) {
                    frontier[frontier_index] = child;
                }
                continue;
            }
            // If it's not in the frontier, then add it to the frontier
            frontier.push_back(child);
        } // End for each child
    } // End while not frontier empty

    // Backtrack to build the path
    MapPathNode current = found_path ? path_end : explored[closest_explored];
    path->clear();
    path->reserve(current.cost.integer_part() + 1);
    while (current.parent != -1) {
        path->insert(path->begin(), current.cell);
        current = explored[current.parent];
    }
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
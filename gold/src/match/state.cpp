#include "match/state.h"

#include "core/logger.h"
#include "lcg.h"

static const uint32_t MATCH_PLAYER_STARTING_GOLD = 50;
static const uint32_t MATCH_GOLDMINE_STARTING_GOLD = 5000;
static const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION = 10;
static const uint32_t UNIT_HEALTH_REGEN_DURATION = 64;
static const int UNIT_BLOCKED_DURATION = 30;
static const int SMOKE_BOMB_THROW_RANGE_SQUARED = 36;

MatchState match_init(int32_t lcg_seed, Noise& noise, MatchPlayer players[MAX_PLAYERS]) {
    MatchState state;
    #ifdef GOLD_RAND_SEED
        lcg_seed = GOLD_RAND_SEED;
    #endif
    lcg_srand(lcg_seed);
    log_info("Set random seed to %i", lcg_seed);
    std::vector<ivec2> player_spawns;
    std::vector<ivec2> goldmine_cells;
    map_init(state.map, noise, player_spawns, goldmine_cells);
    memcpy(state.players, players, sizeof(state.players));

    // Generate goldmines
    for (ivec2 cell : goldmine_cells) {
        match_create_goldmine(state, cell, MATCH_GOLDMINE_STARTING_GOLD);
    }

    // Init players
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state.players[player_id].gold = MATCH_PLAYER_STARTING_GOLD;
        state.players[player_id].upgrades = 0;
        state.players[player_id].upgrades_in_progress = 0;

        state.fog[player_id] = std::vector<int>(state.map.width * state.map.height, FOG_HIDDEN);
        state.detection[player_id] = std::vector<int>(state.map.width * state.map.height, 0);

        if (state.players[player_id].active) {
            ivec2 town_hall_cell = map_get_player_town_hall_cell(state.map, player_spawns[player_id]);
            match_create_entity(state, ENTITY_MINER, town_hall_cell, player_id);
        }
    }

    state.is_fog_dirty = false;

    return state;
}

void match_handle_input(MatchState& state, const MatchInput& input) {
    switch (input.type) {
        case MATCH_INPUT_NONE:
            return;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR:
        case MATCH_INPUT_MOVE_UNLOAD: {
            // Determine the target index
            uint32_t target_index = INDEX_INVALID;
            if (input.type == MATCH_INPUT_MOVE_ENTITY || 
                    input.type == MATCH_INPUT_MOVE_ATTACK_ENTITY ||
                    input.type == MATCH_INPUT_MOVE_REPAIR) {
                target_index = state.entities.get_index_of(input.move.target_id);
                // Don't target a unit which is no longer selectable
                if (target_index != INDEX_INVALID && !entity_is_selectable(state.entities[target_index])) {
                    target_index = INDEX_INVALID;
                }
            }

            // Calculate group center
            ivec2 group_center;
            bool should_move_as_group = target_index == INDEX_INVALID;
            uint32_t unit_count = 0;
            if (should_move_as_group) {
                ivec2 group_min;
                ivec2 group_max;
                for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                    uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                        continue;
                    }

                    ivec2 entity_cell = state.entities[entity_index].cell;
                    if (unit_count == 0) {
                        group_min = entity_cell;
                        group_max = entity_cell;
                    } else {
                        group_min.x = std::min(group_min.x, entity_cell.x);
                        group_min.y = std::min(group_min.y, entity_cell.y);
                        group_max.x = std::max(group_max.x, entity_cell.x);
                        group_max.y = std::max(group_max.y, entity_cell.y);
                    }

                    unit_count++;
                }

                Rect group_rect = (Rect) { 
                    .x = group_min.x, .y = group_min.y,
                    .w = group_max.x - group_min.x, .h = group_max.y - group_min.y
                };
                group_center = ivec2(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));

                // Don't move as group if we're not in a group
                // Also don't move as a group if the target is inside the group rect (this allows units to converge in on a cell)
                if (unit_count < 2 || group_rect.has_point(input.move.target_cell)) {
                    should_move_as_group = false;
                }
            } // End calculate group center

            // Give each unit the move command
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                Entity& entity = state.entities[entity_index];

                // Set the unit's target
                Target target;
                target.type = (TargetType)input.type;
                if (target_index == INDEX_INVALID) {
                    target.cell = input.move.target_cell;
                    // If group-moving, use the group move cell, but only if the cell is valid
                    if (should_move_as_group) {
                        ivec2 group_move_cell = input.move.target_cell + (entity.cell - group_center);
                        if (map_is_cell_in_bounds(state.map, group_move_cell) && 
                                ivec2::manhattan_distance(group_move_cell, input.move.target_cell) <= 3 &&
                                map_get_tile(state.map, group_move_cell).elevation == map_get_tile(state.map, input.move.target_cell).elevation) {
                            target.cell = group_move_cell;
                        }
                    }
                // Ensure that units do not target themselves
                } else if (input.move.target_id == input.move.entity_ids[id_index]) {
                    target = (Target) { .type = TARGET_NONE };
                } else {
                    target.id = input.move.target_id;
                }

                if (!input.move.shift_command || (entity.target.type == TARGET_NONE && entity.target_queue.empty())) {
                    entity.target_queue.clear();
                    entity_set_target(entity, target);
                } else {
                    entity.target_queue.push_back(target);
                }
            } // End for each unit in move input
            break;
        } // End case MATCH_INPUT_MOVE
        case MATCH_INPUT_MOVE_SMOKE: {
            break;
        }
    }
}

void match_update(MatchState& state) {
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        match_entity_update(state, entity_index);
    }

    // Update remembered entities
    if (state.is_fog_dirty) {
        for (uint8_t team = 0; team < MAX_PLAYERS; team++) {
            // Remove any remembered entities (but only if the players can see that they should be removed)
            auto it = state.remembered_entities[team].begin();
            while (it != state.remembered_entities[team].end()) {
                if ((state.entities.get_index_of(it->first) == INDEX_INVALID || state.entities.get_by_id(it->first).health == 0) &&
                        match_is_cell_rect_revealed(state, team, it->second.cell, it->second.cell_size)) {
                    it = state.remembered_entities[team].erase(it);
                } else {
                    it++;
                }
            }
        }

        state.is_fog_dirty = false;
    }
}

// ENTITY

EntityId match_create_entity(MatchState& state, EntityType type, ivec2 cell, uint8_t player_id) {
    const EntityData& entity_data = entity_get_data(type);

    Entity entity;
    entity.type = type;
    entity.mode = MODE_UNIT_IDLE;
    entity.player_id = player_id;
    entity.flags = 0;

    entity.cell = cell;
    entity.position = entity_is_unit(type) 
                        ? entity_get_target_position(entity)
                        : fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = entity_is_unit(type)
                        ? entity_data.max_health
                        : (entity_data.max_health / 10);
    entity.target = (Target) { .type = TARGET_NONE };
    entity.pathfind_attempts = 0;
    entity.timer = 0;
    entity.rally_point = ivec2(-1, -1);

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = 0;
    entity.gold_mine_id = ID_NULL;

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;

    EntityId id = state.entities.push_back(entity);
    map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
        .type = entity_is_unit(type) ? CELL_UNIT : CELL_BUILDING,
        .id = id
    });
    match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, true);

    return id;
}

EntityId match_create_goldmine(MatchState& state, ivec2 cell, uint32_t gold_left) {
    Entity entity;
    entity.type = ENTITY_GOLDMINE;
    entity.player_id = PLAYER_NONE;
    entity.flags = 0;
    entity.mode = MODE_GOLDMINE;

    entity.cell = cell;
    entity.position = fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.garrison_id = ID_NULL;
    entity.gold_held = gold_left;

    EntityId id = state.entities.push_back(entity);
    map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_get_data(entity.type).cell_size, (Cell) {
        .type = CELL_GOLDMINE,
        .id = id
    });

    return id;
}

void match_entity_update(MatchState& state, uint32_t entity_index) {
    EntityId entity_id = state.entities.get_id_of(entity_index);
    Entity& entity = state.entities[entity_index];
    const EntityData& entity_data = entity_get_data(entity.type);

    bool update_finished = false;
    fixed movement_left = entity_is_unit(entity.type) ? entity_data.unit_data.speed : fixed::from_raw(0);
    while (!update_finished) {
        switch (entity.mode) {
            case MODE_UNIT_IDLE: {
                // Do nothing if unit is garrisoned
                if (entity.garrison_id != ID_NULL) {
                    update_finished = true;
                    break;
                }

                // If unit is idle, check target queue
                if (entity.target.type == TARGET_NONE && !entity.target_queue.empty()) {
                    entity_set_target(entity, entity.target_queue[0]);
                    entity.target_queue.erase(entity.target_queue.begin());
                }

                // If unit is idle, try to find a nearby target

                // If unit is still idle, do nothing
                if (entity.target.type == TARGET_NONE) {
                    // If soldier is idle, charge weapon
                    update_finished = true;
                    break;
                }

                if (match_is_target_invalid(state, entity.target)) {
                    entity.target = (Target) { .type = TARGET_NONE };
                    update_finished = true;
                    break;
                }

                // If mining, cache the current target's gold mine so that this unit returns to it later
                if (entity.type == ENTITY_MINER && entity.target.type == TARGET_ENTITY) {
                    const Entity& target = state.entities.get_by_id(entity.target.id);
                    if (target.type == ENTITY_GOLDMINE && target.gold_held != 0) {
                        entity.gold_mine_id = entity.target.id;
                    }
                }

                if (match_has_entity_reached_target(state, entity)) {
                    entity.mode = MODE_UNIT_MOVE_FINISHED;
                    break;
                }

                // Don't move if hold position or if garrisoned
                // We have to check garrisoned a second time here because previously we had allowed bunkered units to shoot
                if (entity_check_flag(entity, ENTITY_FLAG_HOLD_POSITION) || entity.garrison_id != ID_NULL) {
                    // Throw away targets if garrisoned. This prevents bunkered units from fixated on a target they can no longer reach
                    if (entity.garrison_id != ID_NULL) {
                        entity.target = (Target) { .type = TARGET_NONE };
                    }
                    update_finished = true;
                    break;
                }

                // Attempt to move and avoid ideal mine exit path

                // Pathfind
                map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, match_get_entity_target_cell(state, entity), entity_data.cell_size, &entity.path, match_is_entity_mining(state, entity));
                if (!entity.path.empty()) {
                    entity.pathfind_attempts = 0;
                    entity.mode = MODE_UNIT_MOVE;
                } else {
                    entity.pathfind_attempts++;
                    if (entity.pathfind_attempts >= 3) {
                        if (entity.target.type == TARGET_BUILD) {
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_CANT_BUILD);
                        }
                        entity.target = (Target) { .type = TARGET_NONE };
                        entity.pathfind_attempts = 0;
                        update_finished = true;
                        break;
                    } else {
                        entity.timer = UNIT_BLOCKED_DURATION;
                        entity.mode = MODE_UNIT_BLOCKED;
                        update_finished = true;
                        break;
                    }
                }
                break;
            }
            case MODE_UNIT_BLOCKED: {
                entity.timer--;
                if (entity.timer == 0) {
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_MOVE: {
                bool path_is_blocked = false;

                while (movement_left.raw_value > 0) {
                    // If the unit is not moving between tiles, then pop the next cell off the path
                    if (entity.position == entity_get_target_position(entity) && !entity.path.empty()) {
                        entity.direction = enum_from_ivec2_direction(entity.path[0] - entity.cell);
                        if (map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, entity.path[0], entity_data.cell_size, entity.cell, false)) {
                            path_is_blocked = true;
                            // breaks out of while movement left
                            break;
                        }

                        if (map_is_cell_rect_equal_to(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, entity_id)) {
                            map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY,
                                .id = ID_NULL
                            });
                        }
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, false);
                        entity.cell = entity.path[0];
                        map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                            .type = match_is_entity_mining(state, entity) ? CELL_MINER : CELL_UNIT,
                            .id = entity_id
                        });
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, true);
                        entity.path.erase(entity.path.begin());
                    }

                    // Step unit along movement
                    if (entity.position.distance_to(entity_get_target_position(entity)) > movement_left) {
                        entity.position += DIRECTION_FVEC2[entity.direction] * movement_left;
                        movement_left = fixed::from_raw(0);
                    } else {
                        movement_left -= entity.position.distance_to(entity_get_target_position(entity));
                        entity.position = entity_get_target_position(entity);
                        // On step finished
                        // Check to see if we triggered a mine
                        /*
                        TODO
                        for (entity_t& mine : state.entities) {
                            if (mine.type != ENTITY_LAND_MINE || mine.health == 0 || mine.mode != MODE_BUILDING_FINISHED || network_get_player(mine.player_id).team == network_get_player(entity.player_id).team ||
                                    std::abs(entity.cell.x - mine.cell.x) > 1 || std::abs(entity.cell.y - mine.cell.y) > 1) {
                                continue;
                            }
                            mine.animation = animation_create(ANIMATION_MINE_PRIME);
                            mine.timer = MINE_PRIME_DURATION;
                            mine.mode = MODE_MINE_PRIME;
                            entity_set_flag(mine, ENTITY_FLAG_INVISIBLE, false);
                        }
                        if (entity.target.type == TARGET_ATTACK_CELL) {
                            target_t attack_target = entity_target_nearest_enemy(state, entity);
                            if (attack_target.type != TARGET_NONE) {
                                entity.target = attack_target;
                                entity.path.clear();
                                entity.mode = entity_has_reached_target(state, entity) ? MODE_UNIT_MOVE_FINISHED : MODE_UNIT_IDLE;
                                // breaks out of while movement left > 0
                                break;
                            }
                        }
                        */
                        if (match_is_target_invalid(state, entity.target)) {
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.path.clear();
                            break;
                        }
                        if (match_has_entity_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_MOVE_FINISHED;
                            entity.path.clear();
                            // break out of while movement left
                            break;
                        }
                        if (entity.path.empty()) {
                            entity.mode = MODE_UNIT_IDLE;
                            // break out of while movement left
                            break;
                        }
                    }
                } // End while movement left

                if (path_is_blocked) {
                    bool try_walk_around_blocker = false;
                    bool is_entity_mining = match_is_entity_mining(state, entity);
                    if (is_entity_mining) {
                        Cell blocking_cell = map_get_cell(state.map, CELL_LAYER_GROUND, entity.path[0]);
                        if (blocking_cell.type == CELL_MINER) {
                            const Entity& blocker = state.entities.get_by_id(blocking_cell.id);
                            if (entity.direction == ((blocker.direction + 4) % DIRECTION_COUNT)) {
                                try_walk_around_blocker = true;
                            }
                        }
                    }
                    if (try_walk_around_blocker) {
                        entity.path.clear();
                        map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, match_get_entity_target_cell(state, entity), entity_data.cell_size, &entity.path, false);
                        update_finished = true;
                        break;
                    }

                    path_is_blocked = true;
                    entity.mode = MODE_UNIT_BLOCKED;
                    entity.timer = is_entity_mining ? 10 : UNIT_BLOCKED_DURATION;
                }

                update_finished = entity.mode != MODE_UNIT_MOVE_FINISHED;
                break;
            }
            case MODE_UNIT_MOVE_FINISHED: {
                entity.target = (Target) { .type = TARGET_NONE };
                entity.mode = MODE_UNIT_IDLE;
            }
            default:
                update_finished = true;
                break;
        }
    }

    // Update timers
    if (entity.cooldown_timer != 0) {
        entity.cooldown_timer--;
    }

    if (entity.taking_damage_counter != 0) {
        entity.taking_damage_timer--;
        if (entity.taking_damage_timer == 0) {
            entity.taking_damage_counter--;
            entity_set_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER, entity.taking_damage_counter == 0 ? false : !entity_check_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER));
            entity.taking_damage_timer = entity.taking_damage_counter == 0 ? 0 : MATCH_TAKING_DAMAGE_FLICKER_DURATION;
        } 
    }
    if (entity.health == entity_data.max_health) {
        entity.health_regen_timer = 0;
    }
    if (entity.health_regen_timer != 0) {
        entity.health_regen_timer--;
        if (entity.health_regen_timer == 0) {
            entity.health++;
            if (entity.health != entity_data.max_health) {
                entity.health_regen_timer = UNIT_HEALTH_REGEN_DURATION;
            }
        }
    }

    // Update animation
    if (entity_is_unit(entity.type)) {
        AnimationName expected_animation = entity_get_expected_animation(entity);
        if (entity.animation.name != expected_animation || !animation_is_playing(entity.animation)) {
            entity.animation = animation_create(expected_animation);
        }
        int prev_hframe = entity.animation.frame.x;
        animation_update(entity.animation);
        if (prev_hframe != entity.animation.frame.x) {
            if ((entity.mode == MODE_UNIT_REPAIR || entity.mode == MODE_UNIT_BUILD) && prev_hframe == 0) {
                match_event_play_sound(state, SOUND_HAMMER, entity.position.to_ivec2());
            } 
        }
    }
}

bool match_is_entity_visible_to_player(const MatchState& state, const Entity& entity, uint8_t player_id) {
    if (entity.garrison_id != ID_NULL) {
        return false;
    }

    uint8_t player_team = state.players[player_id].team;
    bool entity_is_invisible = entity_check_flag(entity, ENTITY_FLAG_INVISIBLE);
    int entity_cell_size = entity_get_data(entity.type).cell_size;

    for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size; y++) {
        for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size; x++) {
            if (state.fog[player_team][x + (y * state.map.width)] > 0 && 
                    (!entity_is_invisible || state.detection[player_team][x + (y * state.map.width)] == 1)) {
                return true;
            }
        }
    }

    return false;
}

bool match_is_target_invalid(const MatchState& state, const Target& target) {
    if (!(target.type == TARGET_ENTITY || target.type == TARGET_ATTACK_ENTITY || target.type == TARGET_REPAIR || target.type == TARGET_BUILD_ASSIST)) {
        return false;
    }

    uint32_t target_index = state.entities.get_index_of(target.id);
    if (target_index == INDEX_INVALID) {
        return true;
    }

    if (state.entities[target_index].type == ENTITY_GOLDMINE) {
        return false;
    }
    
    if (target.type == TARGET_BUILD_ASSIST) {
        return state.entities[target_index].health == 0 || state.entities[target_index].target.type != TARGET_BUILD;
    }

    return !entity_is_selectable(state.entities[target_index]);
}

bool match_has_entity_reached_target(const MatchState& state, const Entity& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return true;
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
            return entity.cell == entity.target.cell;
        case TARGET_BUILD: {
            /*
            if (entity.target.build.building_type == ENTITY_LAND_MINE) {
                return xy::manhattan_distance(entity.cell, entity.target.build.building_cell) == 1;
            }
            */
            return entity.cell == entity.target.build.unit_cell;
        }
        case TARGET_BUILD_ASSIST: {
            const Entity& builder = state.entities.get_by_id(entity.target.id);

            int building_size = entity_get_data(builder.target.build.building_type).cell_size;
            Rect building_rect = (Rect) {
                .x = builder.target.build.building_cell.x, .y = builder.target.build.building_cell.y,
                .w = building_size, .h = building_size
            };

            int unit_size = entity_get_data(entity.type).cell_size;
            Rect unit_rect = (Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = unit_size, .h = unit_size
            };

            return unit_rect.is_adjacent_to(building_rect);
        }
        case TARGET_UNLOAD:
            return entity.path.empty() && ivec2::manhattan_distance(entity.cell, entity.target.cell) < 3;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            const Entity& reference_entity = entity.garrison_id == ID_NULL ? entity : state.entities.get_by_id(entity.garrison_id);
            int reference_entity_size = entity_get_data(reference_entity.type).cell_size;
            Rect entity_rect = (Rect) {
                .x = reference_entity.cell.x, .y = reference_entity.cell.y,
                .w = reference_entity_size, .h = reference_entity_size
            };

            const Entity& target = state.entities.get_by_id(entity.target.id);
            int target_size = entity_get_data(target.type).cell_size;
            Rect target_rect = (Rect) {
                .x = target.cell.x, .y = target.cell.y,
                .w = target_size, .h = target_size
            };

            int entity_range_squared = entity_get_data(entity.type).unit_data.range_squared;
            return entity.target.type != TARGET_ATTACK_ENTITY || entity_range_squared == 1
                        ? entity_rect.is_adjacent_to(target_rect)
                        : Rect::euclidean_distance_squared_between(entity_rect, target_rect) <= entity_range_squared;
        }
        case TARGET_SMOKE: {
            return ivec2::euclidean_distance_squared(entity.cell, entity.target.cell) <= SMOKE_BOMB_THROW_RANGE_SQUARED;
        }
    }
}

ivec2 match_get_entity_target_cell(const MatchState& state, const Entity& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return entity.cell;
        case TARGET_BUILD: {
            /*
            TODO
            if (entity.target.build.building_type == ENTITY_LAND_MINE) {
                return map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), entity.target.build.building_cell, entity_cell_size(ENTITY_LAND_MINE), false);
            }
            */
            return entity.target.build.unit_cell;
        }
        case TARGET_BUILD_ASSIST: {
            const Entity& builder = state.entities.get_by_id(entity.target.id);
            return map_get_nearest_cell_around_rect(
                        state.map, 
                        CELL_LAYER_GROUND,
                        entity.cell, 
                        entity_get_data(entity.type).cell_size, 
                        builder.target.build.building_cell, 
                        entity_get_data(builder.target.build.building_type).cell_size, 
                        false, ivec2(-1, -1));
        }
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
        case TARGET_UNLOAD:
        case TARGET_SMOKE:
            return entity.target.cell;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            const Entity& target = state.entities.get_by_id(entity.target.id);
            ivec2 ignore_cell = ivec2(-1, -1);
            /*
            TODO
            if (target.type == ENTITY_GOLDMINE) {
                Target camp_target = entity_target_nearest_camp(state, entity);
                if (camp_target.type != TARGET_NONE) {
                    const entity_t& camp = state.entities.get_by_id(camp_target.id);
                    xy rally_cell = map_get_nearest_cell_around_rect(state, target.cell + xy(1, 1), 1, camp.cell, entity_cell_size(camp.type), true);
                    ignore_cell = entity_get_exit_cell(state, target.cell, entity_cell_size(target.type), entity_cell_size(ENTITY_MINER), rally_cell, true);
                }
            }
            */
            int entity_cell_size = entity_get_data(entity.type).cell_size;
            int target_cell_size = entity_get_data(target.type).cell_size;
            return map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_cell_size, target.cell, target_cell_size, match_is_entity_mining(state, entity), ignore_cell);
        }
    }
}

bool match_is_entity_mining(const MatchState& state, const Entity& entity) {
    if (entity.target.type != TARGET_ENTITY || entity.type != ENTITY_MINER) {
        return false;
    }

    const Entity& target = state.entities.get_by_id(entity.target.id);
    return (target.type == ENTITY_GOLDMINE && target.gold_held > 0);
}

// EVENTS

void match_event_play_sound(MatchState& state, SoundName sound, ivec2 position) {
    state.events.push_back((MatchEvent) {
        .type = MATCH_EVENT_SOUND,
        .sound = (MatchEventSound) {
            .position = position,
            .sound = sound
        }
    });
}

void match_event_alert(MatchState& state, MatchAlertType type, uint8_t player_id, ivec2 cell, int cell_size) {
    state.events.push_back((MatchEvent) {
        .type = MATCH_EVENT_ALERT,
        .alert = (MatchEventAlert) {
            .type = type,
            .player_id = player_id,
            .cell = cell,
            .cell_size = cell_size
        }
    });
}

void match_event_show_status(MatchState& state, uint8_t player_id, const char* message) {
    state.events.push_back((MatchEvent) {
        .type = MATCH_EVENT_STATUS,
        .status = (MatchEventStatus) {
            .player_id = player_id,
            .message = message
        }
    });
}

// FOG

int match_get_fog(const MatchState& state, uint8_t team, ivec2 cell) {
    return state.fog[team][cell.x + (cell.y * state.map.width)];
}

bool match_is_cell_rect_revealed(const MatchState& state, uint8_t team, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state.fog[team][x + (y * state.map.width)] > 0) {
                return true;
            }
        }
    }

    return false;
}

void match_fog_update(MatchState& state, uint8_t player_team, ivec2 cell, int cell_size, int sight, bool has_detection, bool increment) { 
    /*
    * This function does a raytrace from the cell center outwards to determine what this unit can see
    * Raytracing is done using Bresenham's Line Generation Algorithm (https://www.geeksforgeeks.org/bresenhams-line-generation-algorithm/)
    */

    ivec2 search_corners[4] = {
        cell - ivec2(sight, sight),
        cell + ivec2((cell_size - 1) + sight, -sight),
        cell + ivec2((cell_size - 1) + sight, (cell_size - 1) + sight),
        cell + ivec2(-sight, (cell_size - 1) + sight)
    };
    for (int search_index = 0; search_index < 4; search_index++) {
        ivec2 search_goal = search_corners[search_index + 1 == 4 ? 0 : search_index + 1];
        ivec2 search_step = DIRECTION_IVEC2[(search_index * 2) + 2 == DIRECTION_COUNT 
                                            ? DIRECTION_NORTH 
                                            : (search_index * 2) + 2];
        for (ivec2 line_end = search_corners[search_index]; line_end != search_goal; line_end += search_step) {
            ivec2 line_start;
            switch (cell_size) {
                case 1:
                    line_start = cell;
                    break;
                case 3:
                    line_start = cell + ivec2(1, 1);
                    break;
                case 2:
                case 4: {
                    ivec2 center_cell = cell_size == 2 ? cell : cell + ivec2(1, 1);
                    if (line_end.x < center_cell.x) {
                        line_start.x = center_cell.x;
                    } else if (line_end.x > center_cell.x + 1) {
                        line_start.x = center_cell.x + 1;
                    } else {
                        line_start.x = line_end.x;
                    }
                    if (line_end.y < center_cell.y) {
                        line_start.y = center_cell.y;
                    } else if (line_end.y > center_cell.y + 1) {
                        line_start.y = center_cell.y + 1;
                    } else {
                        line_start.y = line_end.y;
                    }
                    break;
                }
                default:
                    log_warn("cell size of %i not handled in map_fog_update", cell_size);
                    line_start = cell;
                    break;
            }

            // we want slope to be between 0 and 1
            // if "run" is greater than "rise" then m is naturally between 0 and 1, we will step with x in increments of 1 and handle y increments that are less than 1
            // if "rise" is greater than "run" (use_x_step is false) then we will swap x and y so that we can step with y in increments of 1 and handle x increments that are less than 1
            bool use_x_step = std::abs(line_end.x - line_start.x) >= std::abs(line_end.y - line_start.y);
            int slope = std::abs(2 * (use_x_step ? (line_end.y - line_start.y) : (line_end.x - line_start.x)));
            int slope_error = slope - std::abs((use_x_step ? (line_end.x - line_start.x) : (line_end.y - line_start.y)));
            ivec2 line_step;
            ivec2 line_opposite_step;
            if (use_x_step) {
                line_step = ivec2(1, 0) * (line_end.x >= line_start.x ? 1 : -1);
                line_opposite_step = ivec2(0, 1) * (line_end.y >= line_start.y ? 1 : -1);
            } else {
                line_step = ivec2(0, 1) * (line_end.y >= line_start.y ? 1 : -1);
                line_opposite_step = ivec2(1, 0) * (line_end.x >= line_start.x ? 1 : -1);
            }
            for (ivec2 line_cell = line_start; line_cell != line_end; line_cell += line_step) {
                if (!map_is_cell_in_bounds(state.map, line_cell) || ivec2::euclidean_distance_squared(line_start, line_cell) > sight * sight) {
                    break;
                }

                if (increment) {
                    if (state.fog[player_team][line_cell.x + (line_cell.y * state.map.width)] == FOG_HIDDEN) {
                        state.fog[player_team][line_cell.x + (line_cell.y * state.map.width)] = 1;
                    } else {
                        state.fog[player_team][line_cell.x + (line_cell.y * state.map.width)]++;
                    }
                    if (has_detection) {
                        state.detection[player_team][line_cell.x + (line_cell.y * state.map.width)]++;
                    }
                } else {
                    state.fog[player_team][line_cell.x + (line_cell.y * state.map.width)]--;
                    if (has_detection) {
                        state.detection[player_team][line_cell.x + (line_cell.y * state.map.width)]--;
                    }

                    // Remember revealed entities
                    Cell cell = map_get_cell(state.map, CELL_LAYER_GROUND, line_cell);
                    if (cell.type == CELL_BUILDING || cell.type == CELL_GOLDMINE) {
                        Entity& entity = state.entities.get_by_id(cell.id);
                        if (entity_is_selectable(entity)) {
                            state.remembered_entities[player_team][cell.id] = (RememberedEntity) {
                                .type = entity.type,
                                .frame = entity_get_animation_frame(entity),
                                .cell = entity.cell,
                                .cell_size = entity_get_data(entity.type).cell_size,
                                .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLDMINE ? 0 : state.players[entity.player_id].recolor_id
                            };
                        }
                    } // End if cell value < cell empty
                } // End if !increment

                if (map_get_tile(state.map, line_cell).elevation > map_get_tile(state.map, line_start).elevation) {
                    break;
                }

                slope_error += slope;
                if (slope_error >= 0) {
                    line_cell += line_opposite_step;
                    slope_error -= 2 * std::abs((use_x_step ? (line_end.x - line_start.x) : (line_end.y - line_start.y)));
                }
            } // End for each line cell in line
        } // End for each line end from corner to corner
    } // End for each search index

    state.is_fog_dirty = true;
}
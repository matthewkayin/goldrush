#include "match/state.h"

#include "core/logger.h"
#include "hotkey.h"
#include "lcg.h"

static const uint32_t MATCH_PLAYER_STARTING_GOLD = 500;
static const uint32_t MATCH_GOLDMINE_STARTING_GOLD = 50;
static const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION = 10;
static const uint32_t UNIT_HEALTH_REGEN_DURATION = 64;
static const uint32_t UNIT_BUILD_TICK_DURATION = 6;
static const uint32_t UNIT_REPAIR_RATE = 4;
static const uint32_t UNIT_IN_MINE_DURATION = 150;
static const uint32_t UNIT_MAX_GOLD_HELD = 5;
static const int UNIT_BLOCKED_DURATION = 30;
static const int SMOKE_BOMB_THROW_RANGE_SQUARED = 36;
static const uint32_t BUILDING_FADE_DURATION = 300;

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
            // Place town hall
            ivec2 town_hall_cell = map_get_player_town_hall_cell(state.map, player_spawns[player_id]);
            EntityId hall_id = match_create_entity(state, ENTITY_HALL, town_hall_cell, player_id);
            Entity& hall = state.entities.get_by_id(hall_id);
            hall.health = entity_get_data(hall.type).max_health;
            hall.mode = MODE_BUILDING_FINISHED;

            // Place miners
            Target goldmine_target = match_entity_target_nearest_gold_mine(state, hall);
            GOLD_ASSERT(goldmine_target.type != TARGET_NONE);
            Entity& mine = state.entities.get_by_id(goldmine_target.id);
            for (int index = 0; index < 3; index++) {
                ivec2 exit_cell = map_get_exit_cell(state.map, hall.cell, entity_get_data(hall.type).cell_size, entity_get_data(ENTITY_MINER).cell_size, mine.cell, false);
                match_create_entity(state, ENTITY_MINER, exit_cell, player_id);
            }
        }
    }

    state.is_fog_dirty = false;

    return state;
}

uint32_t match_get_player_population(const MatchState& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const Entity& entity : state.entities) {
        if (entity.player_id == player_id && entity_is_unit(entity.type)) {
            population += entity_get_data(entity.type).unit_data.population_cost;
        }
    }

    return population;
}

uint32_t match_get_player_max_population(const MatchState& state, uint8_t player_id) {
    uint32_t max_population = 0;
    for (const Entity& entity : state.entities) {
        if (entity.player_id == player_id && (entity.type == ENTITY_HALL || entity.type == ENTITY_HOUSE) && entity.mode == MODE_BUILDING_FINISHED) {
            max_population += 10;
        }
    }

    return max_population;
}

bool match_player_has_upgrade(const MatchState& state, uint8_t player_id, uint32_t upgrade) {
    return (state.players[player_id].upgrades & upgrade) == upgrade;
}

bool match_player_upgrade_is_available(const MatchState& state, uint8_t player_id, uint32_t upgrade) {
    return ((state.players[player_id].upgrades | state.players[player_id].upgrades) & upgrade) == 0;
}

void match_grant_player_upgrade(MatchState& state, uint8_t player_id, uint32_t upgrade) {
    state.players[player_id].upgrades |= upgrade;
}

bool match_does_player_meet_hotkey_requirements(const MatchState& state, uint8_t player_id, InputAction hotkey) {
    const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);

    switch (hotkey_info.requirements.type) {
        case HOTKEY_REQUIRES_NONE: {
            return true;
        }
        case HOTKEY_REQUIRES_BUILDING: {
            for (const Entity& entity : state.entities) {
                if (entity.player_id == player_id && entity.type == hotkey_info.requirements.building) {
                    return true;
                }
            }
            return false;
        }
        case HOTKEY_REQUIRES_UPGRADE: {
            return match_player_has_upgrade(state, player_id, hotkey_info.requirements.upgrade);
        }
    }
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
        case MATCH_INPUT_STOP:
        case MATCH_INPUT_DEFEND: {
            for (uint32_t index = 0; index < input.stop.entity_count; index++) {
                uint32_t entity_index = state.entities.get_index_of(input.stop.entity_ids[index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }

                Entity& entity = state.entities[entity_index];
                entity.path.clear();
                entity.target_queue.clear();
                entity_set_target(entity, (Target) { 
                    .type = TARGET_NONE
                });
                if (input.type == MATCH_INPUT_DEFEND) {
                    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, true);
                }
            }

            break;
        }
        case MATCH_INPUT_BUILD: {
            // Determine the list of viable builders
            std::vector<EntityId> builder_ids;
            for (uint32_t id_index = 0; id_index < input.build.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.build.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                builder_ids.push_back(input.build.entity_ids[id_index]);
            }

            // Assign the lead builder's target
            EntityId lead_builder_id = match_get_nearest_builder(state, builder_ids, input.build.target_cell);
            Entity& lead_builder = state.entities.get_by_id(lead_builder_id);
            int building_size = entity_get_data((EntityType)input.build.building_type).cell_size;
            Target build_target = (Target) {
                .type = TARGET_BUILD,
                .id = ID_NULL,
                .build = (TargetBuild) {
                    .unit_cell = input.build.building_type == ENTITY_LANDMINE 
                                    ? input.build.target_cell 
                                    : get_nearest_cell_in_rect(
                                        lead_builder.cell, 
                                        input.build.target_cell, 
                                        building_size),
                    .building_cell = input.build.target_cell,
                    .building_type = (EntityType)input.build.building_type
                }
            };
            if (!input.move.shift_command || (lead_builder.target.type == TARGET_NONE && lead_builder.target_queue.empty())) {
                lead_builder.target_queue.clear();
                entity_set_target(lead_builder, build_target);
            } else {
                lead_builder.target_queue.push_back(build_target);
            }

            // Assign the helpers' target
            if (input.build.building_type != ENTITY_LANDMINE && !input.build.shift_command) {
                for (EntityId builder_id : builder_ids) {
                    if (builder_id == lead_builder_id) {
                        continue;
                    } 
                    uint32_t builder_index = state.entities.get_index_of(builder_id);
                    if (builder_index == INDEX_INVALID || !entity_is_selectable(state.entities[builder_index])) {
                        continue;
                    }
                    entity_set_target(state.entities[builder_index], (Target) {
                        .type = TARGET_BUILD_ASSIST,
                        .id = lead_builder_id
                    });
                }
            }
            break;
        }
        case MATCH_INPUT_BUILD_CANCEL: {
            uint32_t building_index = state.entities.get_index_of(input.build_cancel.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                break;
            }

            const EntityData& building_data = entity_get_data(state.entities[building_index].type);
            uint32_t gold_refund = building_data.gold_cost - (((uint32_t)state.entities[building_index].health * building_data.gold_cost) / (uint32_t)building_data.max_health);
            state.players[state.entities[building_index].player_id].gold += gold_refund;

            // Tell the builder to stop building
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                if (state.entities[entity_index].target.type == TARGET_BUILD && state.entities[entity_index].target.id == input.build_cancel.building_id) {
                    Entity& builder = state.entities[entity_index];
                    const EntityData& builder_data = entity_get_data(builder.type);
                    builder.cell = builder.target.build.building_cell;
                    builder.position = entity_get_target_position(builder);
                    builder.target = (Target) {
                        .type = TARGET_NONE
                    };
                    builder.mode = MODE_UNIT_IDLE;
                    builder.target_queue.clear();
                    map_set_cell_rect(state.map, CELL_LAYER_GROUND, builder.cell, builder_data.cell_size, (Cell) {
                        .type = CELL_UNIT,
                        .id = state.entities.get_id_of(entity_index)
                    });
                    match_fog_update(state, state.players[builder.player_id].team, builder.cell, builder_data.cell_size, builder_data.sight, builder_data.has_detection, true);
                    break;
                }
            }

            // Destroy the building
            state.entities[building_index].health = 0;
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_enqueue.building_id);
            BuildingQueueItem item;
            item.type = (BuildingQueueItemType)input.building_enqueue.item_type;
            switch (item.type) {
                case BUILDING_QUEUE_ITEM_UNIT:
                    item.unit_type = (EntityType)input.building_enqueue.item_subtype;
                    break;
                case BUILDING_QUEUE_ITEM_UPGRADE: 
                    item.upgrade = input.building_enqueue.item_subtype;
                    break;
            }
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }

            Entity& building = state.entities[building_index];
            if (state.players[building.player_id].gold < building_queue_item_cost(item)) {
                return;
            }
            if (building.queue.size() == BUILDING_QUEUE_MAX) {
                return;
            }

            // Reject this enqueue if the upgrade is already being researched
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE && !match_player_upgrade_is_available(state, building.player_id, item.upgrade)) {
                return;
            }

            // Check if the player has the war wagon upgrade
            // TODO
            /*
            if (item.type == BUILDING_QUEUE_ITEM_UNIT && item.unit_type == ENTITY_WAGON && match_player_has_upgrade(state, building.player_id, UPGRADE_WAR_WAGON)) {
                item.unit_type = ENTITY_WAR_WAGON;
            }
            */

            // Mark upgrades as in-progress when we enqueue them
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state.players[building.player_id].upgrades_in_progress |= item.upgrade;
            }

            state.players[building.player_id].gold -= building_queue_item_cost(item);
            match_building_enqueue(state, building, item);
            break;
        }
        case MATCH_INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }

            Entity& building = state.entities[building_index];
            if (building.queue.empty()) {
                return;
            }

            uint32_t index = input.building_dequeue.index == BUILDING_DEQUEUE_POP_FRONT
                                    ? building.queue.size() - 1
                                    : input.building_dequeue.index;
            if (index >= building.queue.size()) {
                return;
            }

            state.players[building.player_id].gold += building_queue_item_cost(building.queue[index]);
            if (building.queue[index].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state.players[building.player_id].upgrades_in_progress &= ~building.queue[index].upgrade;
            }

            if (index == 0) {
                match_building_dequeue(state, building);
            } else {
                building.queue.erase(building.queue.begin() + index);
            }
            break;
        }
        case MATCH_INPUT_RALLY: {
            for (uint32_t id_index = 0; id_index < input.rally.building_count; id_index++) {
                uint32_t building_index = state.entities.get_index_of(input.rally.building_ids[id_index]);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                    continue;
                }

                state.entities[building_index].rally_point = input.rally.rally_point;
            }
            break;
        }
    }
}

void match_update(MatchState& state) {
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        match_entity_update(state, entity_index);
    }

    // Remove any dead entities
    {
        uint32_t entity_index = 0;
        while (entity_index < state.entities.size()) {
            if ((state.entities[entity_index].mode == MODE_UNIT_DEATH_FADE && !animation_is_playing(state.entities[entity_index].animation)) || 
                    (state.entities[entity_index].garrison_id != ID_NULL && state.entities[entity_index].health == 0) ||
                    (state.entities[entity_index].mode == MODE_BUILDING_DESTROYED && state.entities[entity_index].timer == 0)) {
                // Remove this entity's fog but only if they are not gold and not garrisoned
                if (state.entities[entity_index].player_id != PLAYER_NONE && state.entities[entity_index].garrison_id == ID_NULL) {
                    const EntityData& entity_data = entity_get_data(state.entities[entity_index].type);
                    match_fog_update(state, state.players[state.entities[entity_index].player_id].team, state.entities[entity_index].cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, false);
                }
                state.entities.remove_at(entity_index);
            } else {
                entity_index++;
            }
        }
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
    entity.mode = entity_is_unit(type) ? MODE_UNIT_IDLE : MODE_BUILDING_IN_PROGRESS;
    entity.player_id = player_id;
    entity.flags = 0;

    entity.cell = cell;
    entity.position = entity_is_unit(type) 
                        ? entity_get_target_position(entity)
                        : fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = entity_is_unit(type) || entity.type == ENTITY_LANDMINE
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

    // Check if entity should die
    if (entity_should_die(entity)) {
        if (entity_is_unit(entity.type)) {
            entity.mode = MODE_UNIT_DEATH;
            entity.animation = animation_create(entity_get_expected_animation(entity));
        } else {
            entity.mode = MODE_BUILDING_DESTROYED;
            entity.timer = BUILDING_FADE_DURATION;
            // TODO: entity.queue.clear();

            if (entity.type == ENTITY_LANDMINE) {
                map_set_cell_rect(state.map, CELL_LAYER_UNDERGROUND, entity.cell, entity_data.cell_size, (Cell) {
                    .type = CELL_EMPTY, .id = ID_NULL
                });
            } else {
                // Set building cells to empty
                // but don't override the miner cell
                for (int y = entity.cell.y; y < entity.cell.y + entity_data.cell_size; y++) {
                    for (int x = entity.cell.x; x < entity.cell.x + entity_data.cell_size; x++) {
                        ivec2 cell = ivec2(x, y);
                        if (map_get_cell(state.map, CELL_LAYER_GROUND, cell).id == entity_id) {
                            map_set_cell(state.map, CELL_LAYER_GROUND, cell, (Cell) {
                                .type = CELL_EMPTY,
                                .id = ID_NULL
                            });
                        }
                    }
                }
            }

            match_event_play_sound(state, entity_data.death_sound, entity.position.to_ivec2());
        }
    }
    // End if entity should die

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
                bool used_ideal_mining_path = false;
                if (entity.type == ENTITY_MINER && entity.target.type == TARGET_ENTITY) {
                    const Entity& mine = state.entities.get_by_id(entity.target.id);
                    if (mine.type == ENTITY_GOLDMINE) {
                        Target hall_target = match_entity_target_nearest_hall(state, entity);
                        if (hall_target.type == TARGET_ENTITY) {
                            const Entity& hall = state.entities.get_by_id(hall_target.id);
                            ivec2 rally_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, mine.cell + ivec2(1, 1), 1, hall.cell, entity_get_data(hall.type).cell_size, true);
                            ivec2 mine_exit_cell = map_get_exit_cell(state.map, mine.cell, entity_get_data(mine.type).cell_size, entity_data.cell_size, rally_cell, true);

                            GOLD_ASSERT(mine_exit_cell.x != -1);
                            std::vector<ivec2> mine_exit_path;
                            map_pathfind(state.map, CELL_LAYER_GROUND, mine_exit_cell, rally_cell, 1, &mine_exit_path, true);
                            mine_exit_path.push_back(mine_exit_cell);

                            map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, match_get_entity_target_cell(state, entity), 1, &entity.path, true, &mine_exit_path);
                            used_ideal_mining_path = true;
                        }
                    }
                }

                // Pathfind
                if (!used_ideal_mining_path) {
                    map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, match_get_entity_target_cell(state, entity), entity_data.cell_size, &entity.path, match_is_entity_mining(state, entity));
                }

                // Check path
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
                switch (entity.target.type) {
                    case TARGET_NONE:
                    case TARGET_ATTACK_CELL:
                    case TARGET_CELL: 
                    default: {
                        entity.target = (Target) { .type = TARGET_NONE };
                        entity.mode = MODE_UNIT_IDLE;
                        break;
                    }
                    case TARGET_BUILD: {
                        const EntityData& building_data = entity_get_data(entity.target.build.building_type);
                        bool can_build = true;

                        for (int y = entity.target.build.building_cell.y; y < entity.target.build.building_cell.y + building_data.cell_size; y++) {
                            for (int x = entity.target.build.building_cell.x; x < entity.target.build.building_cell.x + building_data.cell_size; x++) {
                                ivec2 cell = ivec2(x, y);
                                if ((cell != entity.cell && map_get_cell(state.map, CELL_LAYER_GROUND, cell).type != CELL_EMPTY) ||
                                        map_get_cell(state.map, CELL_LAYER_UNDERGROUND, cell).type != CELL_EMPTY) {
                                    can_build = false;
                                }
                            }
                        }
                        if (!can_build) {
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_CANT_BUILD);
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        if (state.players[entity.player_id].gold < building_data.gold_cost) {
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target_queue.clear();
                            break;
                        }

                        state.players[entity.player_id].gold -= building_data.gold_cost;

                        if (entity.target.build.building_type == ENTITY_LANDMINE) {
                            match_create_entity(state, entity.target.build.building_type, entity.target.build.building_cell, entity.player_id);
                            match_event_play_sound(state, SOUND_MINE_INSERT, cell_center(entity.target.build.building_cell).to_ivec2());

                            entity.direction = enum_direction_to_rect(entity.cell, entity.target.build.building_cell, building_data.cell_size);
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_TINKER_THROW;
                            entity.animation = animation_create(ANIMATION_UNIT_ATTACK);
                        } else {
                            log_trace("Starting build");
                            map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY, .id = ID_NULL
                            });
                            match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, false);
                            entity.target.id = match_create_entity(state, entity.target.build.building_type, entity.target.build.building_cell, entity.player_id);
                            entity.mode = MODE_UNIT_BUILD;
                            entity.timer = UNIT_BUILD_TICK_DURATION;

                            state.events.push_back((MatchEvent) {
                                .type = MATCH_EVENT_SELECTION_HANDOFF,
                                .selection_handoff = (MatchEventSelectionHandoff) {
                                    .player_id = entity.player_id,
                                    .to_deselect = entity_id,
                                    .to_select = entity.target.id
                                }
                            });
                        }

                        break;
                    }
                    case TARGET_BUILD_ASSIST: {
                        if (match_is_target_invalid(state, entity.target)) {
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                        }

                        Entity& builder = state.entities.get_by_id(entity.target.id);
                        if (builder.mode == MODE_UNIT_BUILD) {
                            entity.target = (Target) {
                                .type = TARGET_REPAIR,
                                .id = builder.target.id,
                            };
                            entity.mode = MODE_UNIT_REPAIR;
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            entity.direction = enum_direction_to_rect(entity.cell, builder.target.build.building_cell, entity_get_data(builder.target.build.building_type).cell_size);
                        }
                        break;
                    }
                    case TARGET_REPAIR:
                    case TARGET_ENTITY:
                    case TARGET_ATTACK_ENTITY: {
                        if (match_is_target_invalid(state, entity.target)) {
                            entity.target = (Target) {
                                .type = TARGET_NONE
                            };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        if (!match_has_entity_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        Entity& target = state.entities.get_by_id(entity.target.id);
                        const EntityData& target_data = entity_get_data(target.type);

                        // Return gold
                        if (entity.type == ENTITY_MINER && target.type == ENTITY_HALL && 
                                target.mode == MODE_BUILDING_FINISHED && entity.player_id == target.player_id &&
                                entity.gold_held != 0 && entity.target.type != TARGET_REPAIR) {
                            state.players[entity.player_id].gold += entity.gold_held;
                            entity.gold_held = 0;

                            // First clear entity's target
                            entity.target = (Target) { .type = TARGET_NONE };
                            // Then try to set its target based on the gold mine it just visited
                            if (entity.gold_mine_id != ID_NULL) {
                                // It's safe to do this because gold mines never get removed from the array
                                const Entity& gold_mine = state.entities.get_by_id(entity.gold_mine_id);
                                if (gold_mine.gold_held != 0) {
                                    entity.target = (Target) {
                                        .type = TARGET_ENTITY,
                                        .id = entity.gold_mine_id
                                    };
                                }
                            // If it doesn't have a last visited gold mine, then find a mine to visit
                            } else {
                                entity.target = match_entity_target_nearest_gold_mine(state, entity);
                            }

                            entity.mode = MODE_UNIT_IDLE;
                            update_finished = true;
                            break;
                        }
                        // End return gold

                        // Enter mine
                        if (entity.type == ENTITY_MINER && target.type == ENTITY_GOLDMINE && target.gold_held > 0) {
                            if (entity.gold_held != 0) {
                                entity.target = match_entity_target_nearest_hall(state, entity);
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            if (target.garrisoned_units.size() + entity_data.garrison_size <= target_data.garrison_capacity) {
                                target.garrisoned_units.push_back(entity_id);
                                entity.garrison_id = entity.target.id;
                                entity.mode = MODE_UNIT_IN_MINE;
                                entity.timer = UNIT_IN_MINE_DURATION;
                                entity.target = (Target) { .type = TARGET_NONE };
                                entity.gold_held = std::min(UNIT_MAX_GOLD_HELD, target.gold_held);
                                target.gold_held -= entity.gold_held;
                                map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                    .type = CELL_EMPTY, .id = ID_NULL
                                });
                            }

                            update_finished = true;
                            break;
                        }

                        // Begin repair
                        if (entity_is_building(target.type) && entity.type == ENTITY_MINER && 
                                state.players[entity.player_id].team == state.players[target.player_id].team &&
                                entity_is_building(target.type) && target.health < target_data.max_health) {
                            entity.mode = MODE_UNIT_REPAIR;
                            entity.direction = enum_direction_to_rect(entity.cell, target.cell, target_data.cell_size);
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            break;
                        }

                        entity.mode = MODE_UNIT_IDLE;
                        entity.target = (Target) {
                            .type = TARGET_NONE
                        };
                        update_finished = true;
                        break;
                    }
                }

                update_finished = update_finished || !(entity.mode == MODE_UNIT_MOVE && movement_left.raw_value > 0);
                break;
            }
            case MODE_UNIT_BUILD: {
                // This code handles the case where the building is destroyed while the unit is building it
                uint32_t building_index = state.entities.get_index_of(entity.target.id);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index]) || state.entities[building_index].mode != MODE_BUILDING_IN_PROGRESS) {
                    match_entity_stop_building(state, entity_id);
                    update_finished = true;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    // Building tick
                    Entity& building = state.entities[building_index];
                    int building_max_health = entity_get_data(building.type).max_health;

                    #ifdef GOLD_DEBUG_FAST_BUILD
                        building.health = std::min(building.health + 20, building_max_health);
                    #else
                        building.health++;
                    #endif
                    if (building.health == building_max_health) {
                        match_entity_building_finish(state, entity.target.id);
                    } else {
                        entity.timer = UNIT_BUILD_TICK_DURATION;
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_REPAIR: {
                // Stop repairing if the building is destroyed
                if (match_is_target_invalid(state, entity.target)) {
                    entity.target = (Target) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }

                Entity& target = state.entities.get_by_id(entity.target.id);
                int target_max_health = entity_get_data(target.type).max_health;
                if (target.health == target_max_health || state.players[entity.player_id].gold == 0) {
                    entity.target = (Target) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    target.health++;
                    target.health_regen_timer++;
                    if (target.health_regen_timer == UNIT_REPAIR_RATE) {
                        state.players[entity.player_id].gold--;
                        target.health_regen_timer = 0;
                    }
                    if (target.health == target_max_health) {
                        if (target.mode == MODE_BUILDING_IN_PROGRESS) {
                            match_entity_building_finish(state, entity.target.id);
                        } else {
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                        }
                    } else {
                        entity.timer = UNIT_BUILD_TICK_DURATION;
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_IN_MINE: {
                if (entity.timer != 0) {
                    entity.timer--;
                }
                if (entity.timer == 0) {
                    const EntityData& mine_data = entity_get_data(ENTITY_GOLDMINE);
                    const EntityData& hall_data = entity_get_data(ENTITY_HALL);

                    Entity& mine = state.entities.get_by_id(entity.garrison_id);
                    Target hall_target = match_entity_target_nearest_hall(state, entity);
                    uint32_t hall_index = hall_target.type == TARGET_NONE ? INDEX_INVALID : state.entities.get_index_of(hall_target.id);
                    ivec2 rally_cell = hall_target.type == TARGET_NONE 
                                        ? (mine.cell + ivec2(1, mine_data.cell_size)) 
                                        : map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, mine.cell + ivec2(1, 1), 1, state.entities[hall_index].cell, hall_data.cell_size, true);
                    ivec2 exit_cell = map_get_exit_cell(state.map, mine.cell, mine_data.cell_size, entity_data.cell_size, rally_cell, true);

                    if (exit_cell.x == -1) {
                        match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_MINE_EXIT_BLOCKED);
                    } else if (map_get_cell(state.map, CELL_LAYER_GROUND, exit_cell).type == CELL_EMPTY) {
                        // Remove unit from mine
                        for (uint32_t index = 0; index < mine.garrisoned_units.size(); index++) {
                            if (mine.garrisoned_units[index] == entity_id) {
                                mine.garrisoned_units.erase(mine.garrisoned_units.begin() + index);
                                break;
                            }
                        }

                        entity.garrison_id = ID_NULL;
                        entity.target = hall_target;
                        if (entity.target.type == TARGET_NONE) {
                            entity.gold_mine_id = ID_NULL;
                        }
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, false);
                        entity.cell = exit_cell;
                        ivec2 exit_from_cell = get_nearest_cell_in_rect(exit_cell, mine.cell, mine_data.cell_size);
                        entity.direction = enum_from_ivec2_direction(exit_cell - exit_from_cell);
                        entity.position = cell_center(exit_from_cell);
                        entity.mode = MODE_UNIT_MOVE;
                        map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                            .type = hall_target.type == TARGET_ENTITY
                                        ? CELL_MINER 
                                        : CELL_UNIT,
                            .id = entity_id
                        });
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, true);

                        if (mine.garrisoned_units.empty() && mine.gold_held == 0) {
                            mine.mode = MODE_GOLDMINE_COLLAPSED;
                            match_event_alert(state, MATCH_ALERT_TYPE_MINE_COLLAPSE, entity.player_id, mine.cell, mine_data.cell_size);
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_MINE_COLLAPSED);
                        }
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_BUILDING_FINISHED: {
                if (!entity.queue.empty() && entity.timer != 0) {
                    if (entity.timer == BUILDING_QUEUE_BLOCKED && !match_is_building_supply_blocked(state, entity)) {
                        entity.timer = building_queue_item_duration(entity.queue[0]);
                    } else if (entity.timer != BUILDING_QUEUE_BLOCKED && match_is_building_supply_blocked(state, entity)) {
                        entity.timer = BUILDING_QUEUE_BLOCKED;
                    }

                    if (entity.timer != BUILDING_QUEUE_BLOCKED && entity.timer != BUILDING_QUEUE_EXIT_BLOCKED) {
                        #ifdef GOLD_DEBUG_FAST_TRAIN
                            entity.timer = std::max((int)entity.timer - 10, 0);
                        #else
                            entity.timer--;
                        #endif
                    }

                    if ((entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) || 
                            entity.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                        ivec2 rally_cell = entity.rally_point.x == -1 
                                            ? entity.cell + ivec2(0, entity_data.cell_size)
                                            : entity.rally_point / TILE_SIZE;
                        ivec2 exit_cell = map_get_exit_cell(state.map, entity.cell, entity_data.cell_size, entity_get_data(entity.queue[0].unit_type).cell_size, rally_cell, false);
                        if (exit_cell.x == -1) {
                            if (entity.timer == 0) {
                                match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED);
                            }
                            entity.timer = BUILDING_QUEUE_EXIT_BLOCKED;
                            update_finished = true;
                            break;
                        } 

                        entity.timer = 0;
                        EntityId unit_id = match_create_entity(state, entity.queue[0].unit_type, exit_cell, entity.player_id);

                        // Rally unit
                        Entity& unit = state.entities.get_by_id(unit_id);
                        Cell rally_cell_value = map_get_cell(state.map, CELL_LAYER_GROUND, rally_cell);
                        if (unit.type == ENTITY_MINER && rally_cell_value.type == CELL_GOLDMINE) {
                            // Rally to gold
                            unit.target = (Target) {
                                .type = TARGET_ENTITY,
                                .id = rally_cell_value.id
                            };
                        } else {
                            // Rally to cell
                            unit.target = (Target) {
                                .type = TARGET_CELL,
                                .cell = rally_cell
                            };
                        }

                        // Create alert
                        match_event_alert(state, MATCH_ALERT_TYPE_UNIT, unit.player_id, unit.cell, entity_get_data(unit.type).cell_size);

                        match_building_dequeue(state, entity);
                    } else if (entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                        /*
                        TODO
                        match_grant_player_upgrade(state, entity.player_id, entity.queue[0].upgrade);

                        // Set all existing wagons to war wagons
                        if (entity.queue[0].upgrade == UPGRADE_WAR_WAGON) {
                            for (Entity& other_entity : state.entities) {
                                if (other_entity.player_id != entity.player_id) {
                                    continue;
                                }
                                if (other_entity.type == ENTITY_WAGON) {
                                    other_entity.type = ENTITY_WAR_WAGON;
                                } else if (entity_is_building(other_entity.type)) {
                                    for (BuildingQueueItem& other_item : other_entity.queue) {
                                        if (other_item.type == BUILDING_QUEUE_ITEM_UNIT && other_item.unit_type == ENTITY_WAGON) {
                                            other_item.unit_type = ENTITY_WAR_WAGON;
                                        }
                                    }
                                }
                            }
                        }

                        // Show status
                        state.events.push_back((MatchEvent) {
                            .type = MATCH_EVENT_RESEARCH_COMPLETE,
                            .research_complete = (MatchEventResearchComplete) {
                                .upgrade = entity.queue[0].upgrade,
                                .player_id = entity.player_id
                            }
                        });

                        // Create alert
                        match_event_alert(state, MATCH_ALERT_TYPE_RESEARCH, entity.player_id, entity.cell, entity_data.cell_size);

                        */
                        match_building_dequeue(state, entity);
                    }
                } 

                update_finished = true;
                break;
            }
            case MODE_BUILDING_DESTROYED: {
                if (entity.timer != 0) {
                    entity.timer--;
                }
                update_finished = true;
                break;
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
                    (!entity_is_invisible || state.detection[player_team][x + (y * state.map.width)] > 0)) {
                return true;
            }
        }
    }

    if (entity.mode == MODE_UNIT_MOVE) {
        ivec2 prev_cell = entity.cell - DIRECTION_IVEC2[entity.direction];
        for (int y = prev_cell.y; y < prev_cell.y + entity_cell_size; y++) {
            for (int x = prev_cell.x; x < prev_cell.x + entity_cell_size; x++) {
                if (state.fog[player_team][x + (y * state.map.width)] > 0 && 
                        (!entity_is_invisible || state.detection[player_team][x + (y * state.map.width)] > 0)) {
                    return true;
                }
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
            if (entity.target.build.building_type == ENTITY_LANDMINE) {
                return ivec2::manhattan_distance(entity.cell, entity.target.build.building_cell) == 1;
            }
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
            if (entity.target.build.building_type == ENTITY_LANDMINE) {
                return map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_get_data(entity.type).cell_size, entity.target.build.building_cell, entity_get_data(ENTITY_LANDMINE).cell_size, false);
            }
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
            int entity_cell_size = entity_get_data(entity.type).cell_size;
            int target_cell_size = entity_get_data(target.type).cell_size;
            ivec2 ignore_cell = ivec2(-1, -1);
            if (target.type == ENTITY_GOLDMINE) {
                Target hall_target = match_entity_target_nearest_hall(state, entity);
                if (hall_target.type != TARGET_NONE) {
                    const Entity& hall = state.entities.get_by_id(hall_target.id);
                    const EntityData& hall_data = entity_get_data(ENTITY_HALL);
                    ivec2 rally_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, target.cell + ivec2(1, 1), 1, hall.cell, hall_data.cell_size, true);
                    ignore_cell = map_get_exit_cell(state.map, target.cell, target_cell_size, entity_cell_size, rally_cell, true);
                }
            }
            return map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_cell_size, target.cell, target_cell_size, match_is_entity_mining(state, entity), ignore_cell);
        }
    }
}

bool match_is_entity_mining(const MatchState& state, const Entity& entity) {
    if (entity.target.type != TARGET_ENTITY || entity.type != ENTITY_MINER) {
        return false;
    }

    const Entity& target = state.entities.get_by_id(entity.target.id);
    return (target.type == ENTITY_GOLDMINE && target.gold_held > 0) ||
           (target.type == ENTITY_HALL && target.mode == MODE_BUILDING_FINISHED && 
                entity.player_id == target.player_id && entity.gold_held > 0);
}

EntityId match_get_nearest_builder(const MatchState& state, const std::vector<EntityId>& builders, ivec2 cell) {
    EntityId nearest_unit_id; 
    int nearest_unit_dist = -1;
    for (EntityId id : builders) {
        int selection_dist = ivec2::manhattan_distance(cell, state.entities.get_by_id(id).cell);
        if (nearest_unit_dist == -1 || selection_dist < nearest_unit_dist) {
            nearest_unit_id = id;
            nearest_unit_dist = selection_dist;
        }
    }

    return nearest_unit_id;
}

void match_entity_stop_building(MatchState& state, EntityId entity_id) {
    Entity& entity = state.entities.get_by_id(entity_id);
    const EntityData& entity_data = entity_get_data(entity.type);

    int building_size = entity_get_data(entity.target.build.building_type).cell_size;
    ivec2 exit_cell = entity.target.build.building_cell + ivec2(-1, 0);
    ivec2 search_corners[4] = { 
        entity.target.build.building_cell + ivec2(-1, building_size),
        entity.target.build.building_cell + ivec2(building_size, building_size),
        entity.target.build.building_cell + ivec2(building_size, -1),
        entity.target.build.building_cell + ivec2(-1, -1)
    };
    const Direction search_directions[4] = { DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_NORTH, DIRECTION_WEST };
    int search_index = 0;
    while (!map_is_cell_in_bounds(state.map, exit_cell) || map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, exit_cell, entity_data.cell_size)) {
        exit_cell += DIRECTION_IVEC2[search_directions[search_index]];
        if (exit_cell == search_corners[search_index]) {
            search_index++;
            if (search_index == 4) {
                search_index = 0;
                search_corners[0] += ivec2(-1, 1);
                search_corners[1] += ivec2(1, 1);
                search_corners[2] += ivec2(1, -1);
                search_corners[3] += ivec2(-1, -1);
            }
        }
    }

    entity.cell = exit_cell;
    entity.position = entity_get_target_position(entity);
    entity.target = (Target) {
        .type = TARGET_NONE
    };
    entity.mode = MODE_UNIT_IDLE;
    map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
        .type = CELL_UNIT, .id = entity_id
    });
    match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_data.has_detection, true); 
}

void match_entity_building_finish(MatchState& state, EntityId building_id) {
    Entity& building = state.entities.get_by_id(building_id);
    int building_cell_size = entity_get_data(building.type).cell_size;

    building.mode = MODE_BUILDING_FINISHED;

    // Show alert
    match_event_alert(state, MATCH_ALERT_TYPE_BUILDING, building.player_id, building.cell, building_cell_size);

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        Entity& entity = state.entities[entity_index];
        if (!entity_is_unit(entity.type)) {
            continue;
        }
        if (entity.target.id != building_id || !(entity.target.type == TARGET_BUILD || entity.target.type == TARGET_REPAIR)) {
            continue;
        }

        if (entity.target.type == TARGET_BUILD) {
            match_entity_stop_building(state, state.entities.get_id_of(entity_index));
            // If the unit was unable to stop building, notify the user that the exit is blocked
            if (entity.mode != MODE_UNIT_IDLE) {
                match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED);
            }
        } else if (entity.mode == MODE_UNIT_REPAIR) {
            entity.mode = MODE_UNIT_IDLE;
        }

        entity.target = building.type == ENTITY_HALL
                            ? match_entity_target_nearest_gold_mine(state, entity)
                            : (Target) { .type = TARGET_NONE };
    }
}

void match_building_enqueue(MatchState& state, Entity& building, BuildingQueueItem item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    building.queue.push_back(item);
    if (building.queue.size() == 1) {
        if (match_is_building_supply_blocked(state, building)) {
            if (building.timer != BUILDING_QUEUE_BLOCKED) {
                match_event_show_status(state, building.player_id, MATCH_UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(item);
        }
    }
}

void match_building_dequeue(MatchState& state, Entity& building) {
    GOLD_ASSERT(!building.queue.empty());

    building.queue.erase(building.queue.begin());
    if (building.queue.empty()) {
        building.timer = 0;
    } else {
        if (match_is_building_supply_blocked(state, building)) {
            if (building.timer != BUILDING_QUEUE_BLOCKED) {
                match_event_show_status(state, building.player_id, MATCH_UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(building.queue[0]);
        }
    }
}

bool match_is_building_supply_blocked(const MatchState& state, const Entity& building) {
    const BuildingQueueItem& item = building.queue[0];
    if (item.type == BUILDING_QUEUE_ITEM_UNIT) {
        uint32_t required_population = match_get_player_population(state, building.player_id) + entity_get_data(item.unit_type).unit_data.population_cost;
        if (match_get_player_max_population(state, building.player_id) < required_population) {
            return true;
        }
    }
    return false;
}

Target match_entity_target_nearest_gold_mine(const MatchState& state, const Entity& entity) {
    int entity_size = entity_get_data(entity.type).cell_size;
    int goldmine_size = entity_get_data(ENTITY_GOLDMINE).cell_size;
    Rect entity_rect = (Rect) { 
        .x = entity.cell.x, .y = entity.cell.y, 
        .w = entity_size, .h = entity_size
    };
    uint32_t nearest_index = INDEX_INVALID;
    ivec2 nearest_cell = ivec2(-1, -1);
    int nearest_dist = -1;

    for (uint32_t gold_index = 0; gold_index < state.entities.size(); gold_index++) {
        const Entity& gold = state.entities[gold_index];

        if (gold.type != ENTITY_GOLDMINE || gold.gold_held == 0 || match_get_fog(state, state.players[entity.player_id].team, gold.cell) == FOG_HIDDEN) {
            continue;
        }

        Rect gold_mine_rect = (Rect) { 
            .x = gold.cell.x, .y = gold.cell.y, 
            .w = goldmine_size, .h = goldmine_size
        };
        int gold_mine_dist = Rect::euclidean_distance_squared_between(entity_rect, gold_mine_rect);
        if (nearest_index == INDEX_INVALID || gold_mine_dist < nearest_dist) {
            nearest_index = gold_index;
            nearest_dist = gold_mine_dist;
        }
    }

    if (nearest_index == INDEX_INVALID) {
        return (Target) {
            .type = TARGET_NONE
        };
    }

    return (Target) {
        .type = TARGET_ENTITY,
        .id = state.entities.get_id_of(nearest_index)
    };
}

Target match_entity_target_nearest_hall(const MatchState& state, const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    Rect entity_rect = (Rect) { 
        .x = entity.cell.x, .y = entity.cell.y, 
        .w = entity_data.cell_size, .h = entity_data.cell_size
    };
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;

    for (uint32_t other_index = 0; other_index < state.entities.size(); other_index++) {
        const Entity& other = state.entities[other_index];

        if (other.player_id != entity.player_id || other.type != ENTITY_HALL || other.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        const EntityData& other_data = entity_get_data(other.type);
        Rect other_rect = (Rect) { 
            .x = other.cell.x, .y = other.cell.y, 
            .w = other_data.cell_size, .h = other_data.cell_size
        };

        int other_dist = Rect::euclidean_distance_squared_between(entity_rect, other_rect);
        if (nearest_enemy_index == INDEX_INVALID || other_dist < nearest_enemy_dist) {
            nearest_enemy_index = other_index;
            nearest_enemy_dist = other_dist;
        }
    }

    if (nearest_enemy_index == INDEX_INVALID) {
        return (Target) {
            .type = TARGET_NONE
        };
    }

    return (Target) {
        .type = TARGET_ENTITY,
        .id = state.entities.get_id_of(nearest_enemy_index)
    };
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

bool match_is_cell_rect_explored(const MatchState& state, uint8_t team, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state.fog[team][x + (y * state.map.width)] != FOG_HIDDEN) {
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
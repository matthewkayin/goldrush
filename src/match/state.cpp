#include "match/state.h"

#include "core/logger.h"
#include "render/render.h"
#include "hotkey.h"
#include "lcg.h"
#include "upgrade.h"
#include <algorithm>

static const uint32_t MATCH_PLAYER_STARTING_GOLD = 50;
static const uint32_t MATCH_GOLDMINE_STARTING_GOLD = 7500;
static const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION = 10;
static const uint32_t UNIT_HEALTH_REGEN_DURATION = 64;
static const uint32_t UNIT_HEALTH_REGEN_DELAY = 600;
static const uint32_t UNIT_ENERGY_REGEN_DURATION = 64;
static const uint32_t UNIT_BUILD_TICK_DURATION = 6;
static const uint32_t UNIT_REPAIR_RATE = 4;
static const uint32_t UNIT_IN_MINE_DURATION = 150;
static const uint32_t UNIT_MAX_GOLD_HELD = 5;
static const int UNIT_BLOCKED_DURATION = 30;
static const int MOLOTOV_RANGE_SQUARED = 49;
static const uint32_t BUILDING_FADE_DURATION = 300;
static const uint32_t ENTITY_BUNKER_FIRE_OFFSET = 10;
static const ivec2 BUNKER_PARTICLE_OFFSETS[4] = { ivec2(3, 23), ivec2(11, 26), ivec2(20, 25), ivec2(28, 23) };
static const ivec2 WAR_WAGON_DOWN_PARTICLE_OFFSETS[4] = { ivec2(14, 6), ivec2(17, 8), ivec2(21, 6), ivec2(24, 8) };
static const ivec2 WAR_WAGON_UP_PARTICLE_OFFSETS[4] = { ivec2(16, 20), ivec2(18, 22), ivec2(21, 20), ivec2(23, 22) };
static const ivec2 WAR_WAGON_RIGHT_PARTICLE_OFFSETS[4] = { ivec2(7, 18), ivec2(11, 19), ivec2(12, 20), ivec2(16, 18) };
static const int MINE_EXPLOSION_DAMAGE = 200;
static const fixed PROJECTILE_MOLOTOV_SPEED = fixed::from_int(4);
static const uint32_t PROJECTILE_MOLOTOV_FIRE_SPREAD = 3;
static const uint32_t FIRE_TTL = 30 * 30;
static const uint32_t ENTITY_FIRE_DAMAGE_COOLDOWN = 8;
static const uint32_t MINE_ARM_DURATION = 16;
static const uint32_t MINE_PRIME_DURATION = 6 * 6;
static const uint32_t FOG_REVEAL_DURATION = 60;
static const uint32_t MATCH_MAX_POPULATION = 100;
static const uint32_t STAKEOUT_ENERGY_BONUS = 60;

MatchState match_init(int32_t lcg_seed, Noise& noise, MatchPlayer players[MAX_PLAYERS]) {
    MatchState state;
    #ifdef GOLD_RAND_SEED
        state.lcg_seed = GOLD_RAND_SEED;
    #endif
    state.lcg_seed = lcg_seed;
    log_info("Set random seed to %i", lcg_seed);
    std::vector<ivec2> player_spawns;
    std::vector<ivec2> goldmine_cells;
    map_init(state.map, noise, &state.lcg_seed, player_spawns, goldmine_cells);
    memcpy(state.players, players, sizeof(state.players));

    state.fire_cells = std::vector<int>(state.map.width * state.map.height, 0);

    // Generate goldmines
    for (ivec2 cell : goldmine_cells) {
        match_create_goldmine(state, cell, MATCH_GOLDMINE_STARTING_GOLD);
    }

    // Init fog and detection for each team
    for (uint8_t team = 0; team < MAX_PLAYERS; team++) {
        state.fog[team] = std::vector<int>(state.map.width * state.map.height, FOG_HIDDEN);
        state.detection[team] = std::vector<int>(state.map.width * state.map.height, 0);
    }

    // Init players
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state.players[player_id].gold = MATCH_PLAYER_STARTING_GOLD;
        state.players[player_id].upgrades = 0;
        state.players[player_id].upgrades_in_progress = 0;

        if (state.players[player_id].active) {
            // Place town hall
            ivec2 town_hall_cell = map_get_player_town_hall_cell(state.map, player_spawns[player_id]);
            EntityId hall_id = match_create_entity(state, ENTITY_HALL, town_hall_cell, player_id);
            Entity& hall = state.entities.get_by_id(hall_id);
            const EntityData& hall_data = entity_get_data(hall.type);
            hall.health = hall_data.max_health;
            hall.mode = MODE_BUILDING_FINISHED;

            // Place miners
            Target goldmine_target = match_entity_target_nearest_gold_mine(state, hall);
            GOLD_ASSERT(goldmine_target.type != TARGET_NONE);
            Entity& mine = state.entities.get_by_id(goldmine_target.id);
            for (int index = 0; index < 3; index++) {
                ivec2 exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, hall.cell, hall_data.cell_size, entity_get_data(ENTITY_MINER).cell_size, mine.cell, false);
                match_create_entity(state, ENTITY_MINER, exit_cell, player_id);
            }

            // Place scout
            {
                const EntityData& mine_data = entity_get_data(ENTITY_GOLDMINE);
                ivec2 scout_cell = ivec2(-1, -1);
                if (hall.cell.y + hall_data.cell_size < mine.cell.y || mine.cell.y + mine_data.cell_size < hall.cell.y) {
                    // if y-aligned, place 
                    for (int x = hall.cell.x - 3; x < hall.cell.x + 4 + 4; x++) {
                        for (int y = hall.cell.y; y < hall.cell.y + 4; y++) {
                            ivec2 cell = ivec2(x, y);
                            if (!map_is_cell_rect_in_bounds(state.map, cell, 2) || map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, cell, 2)) {
                                continue;
                            }
                            scout_cell = cell;
                            break;
                        }
                        if (scout_cell.x != -1) {
                            break;
                        }
                    }
                } else {
                    for (int x = hall.cell.x; x < hall.cell.x + 4; x++) {
                        for (int y = hall.cell.y - 3; y < hall.cell.y + 4 + 4; y++) {
                            ivec2 cell = ivec2(x, y);
                            if (!map_is_cell_rect_in_bounds(state.map, cell, 2) || map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, cell, 2)) {
                                continue;
                            }
                            scout_cell = cell;
                            break;
                        }
                        if (scout_cell.x != -1) {
                            break;
                        }
                    }
                }
                GOLD_ASSERT(scout_cell.x != -1);
                match_create_entity(state, ENTITY_WAGON, scout_cell, player_id);
            }
        }
    }

    state.is_fog_dirty = false;

    return state;
}

uint32_t match_get_player_population(const MatchState& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const Entity& entity : state.entities) {
        if (entity.player_id == player_id && entity_is_unit(entity.type) && entity.health != 0) {
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

    return std::min(max_population, MATCH_MAX_POPULATION);
}

bool match_player_has_upgrade(const MatchState& state, uint8_t player_id, uint32_t upgrade) {
    return (state.players[player_id].upgrades & upgrade) == upgrade;
}

bool match_player_upgrade_is_available(const MatchState& state, uint8_t player_id, uint32_t upgrade) {
    return ((state.players[player_id].upgrades | state.players[player_id].upgrades_in_progress) & upgrade) == 0;
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
                if (entity.player_id == player_id && entity.type == hotkey_info.requirements.building && entity.mode == MODE_BUILDING_FINISHED) {
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
        case MATCH_INPUT_MOVE_MOLOTOV: {
            uint32_t thrower_index = INDEX_INVALID;
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t unit_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                if (unit_index == INDEX_INVALID || !entity_is_selectable(state.entities[unit_index])) {
                    continue;
                }
                if (state.entities[unit_index].energy < MOLOTOV_ENERGY_COST) {
                    continue;
                }
                if (thrower_index == INDEX_INVALID ||
                        ivec2::manhattan_distance(state.entities[unit_index].cell, input.move.target_cell) <
                        ivec2::manhattan_distance(state.entities[thrower_index].cell, input.move.target_cell)) {
                    thrower_index = unit_index;
                }
            }

            if (thrower_index == INDEX_INVALID) {
                return;
            }

            Target target = (Target) {
                .type = TARGET_MOLOTOV,
                .cell = input.move.target_cell
            };

            if (!input.move.shift_command || 
                    (state.entities[thrower_index].target.type == TARGET_NONE && state.entities[thrower_index].target_queue.empty())) {
                state.entities[thrower_index].target_queue.clear();
                entity_set_target(state.entities[thrower_index], target);
            } else {
                state.entities[thrower_index].target_queue.push_back(target);
            }

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
            const EntityData& building_data = entity_get_data((EntityType)input.build.building_type);
            for (uint32_t id_index = 0; id_index < input.build.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.build.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                builder_ids.push_back(input.build.entity_ids[id_index]);
            }

            // If there's no viable builders, don't build
            if (builder_ids.empty()) {
                return;
            }

            // Assign the lead builder's target
            EntityId lead_builder_id = match_get_nearest_builder(state, builder_ids, input.build.target_cell);
            Entity& lead_builder = state.entities.get_by_id(lead_builder_id);
            int building_size = building_data.cell_size;
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
                    match_fog_update(state, state.players[builder.player_id].team, builder.cell, builder_data.cell_size, builder_data.sight, match_entity_has_detection(state, builder), builder_data.cell_layer, true);
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
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE) {
                if ((item.upgrade == UPGRADE_LIGHT_ARMOR || item.upgrade == UPGRADE_HEAVY_ARMOR) && 
                        !match_player_upgrade_is_available(state, building.player_id, UPGRADE_LIGHT_ARMOR | UPGRADE_HEAVY_ARMOR)) {
                    return;
                } else if ((item.upgrade == UPGRADE_BLACK_POWDER || item.upgrade == UPGRADE_IRON_SIGHTS) && 
                        !match_player_upgrade_is_available(state, building.player_id, UPGRADE_BLACK_POWDER | UPGRADE_IRON_SIGHTS)) {
                    return;
                } else if (item.type == BUILDING_QUEUE_ITEM_UPGRADE && 
                        !match_player_upgrade_is_available(state, building.player_id, item.upgrade)) {
                    return;
                }
            }

            // Check if the player has the war wagon upgrade
            // TODO
            if (item.type == BUILDING_QUEUE_ITEM_UNIT && item.unit_type == ENTITY_WAGON && match_player_has_upgrade(state, building.player_id, UPGRADE_WAR_WAGON)) {
                item.unit_type = ENTITY_WAR_WAGON;
            }

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
        case MATCH_INPUT_SINGLE_UNLOAD: {
            uint32_t garrisoned_unit_index = state.entities.get_index_of(input.single_unload.entity_id);
            if (garrisoned_unit_index == INDEX_INVALID || 
                    state.entities[garrisoned_unit_index].health == 0 ||
                    state.entities[garrisoned_unit_index].garrison_id == ID_NULL) {
                return;
            }

            Entity& carrier = state.entities.get_by_id(state.entities[garrisoned_unit_index].garrison_id);
            match_entity_unload_unit(state, carrier, input.single_unload.entity_id);

            break;
        }
        case MATCH_INPUT_UNLOAD: {
            for (uint32_t id_index = 0; id_index < input.unload.carrier_count; id_index++) {
                uint32_t carrier_index = state.entities.get_index_of(input.unload.carrier_ids[id_index]);
                if (carrier_index == INDEX_INVALID || 
                        !entity_is_selectable(state.entities[carrier_index]) || 
                        state.entities[carrier_index].garrisoned_units.empty()) {
                    continue;
                }

                Entity& carrier = state.entities[carrier_index];
                match_entity_unload_unit(state, carrier, MATCH_ENTITY_UNLOAD_ALL);
            }
            break;
        }
        case MATCH_INPUT_CAMO:
        case MATCH_INPUT_DECAMO: {
            for (uint32_t id_index = 0; id_index < input.camo.unit_count; id_index++) {
                uint32_t unit_index = state.entities.get_index_of(input.camo.unit_ids[id_index]);
                if (unit_index == INDEX_INVALID || !entity_is_selectable(state.entities[unit_index])) {
                    continue;
                }

                Entity& unit = state.entities[unit_index];
                if (input.type == MATCH_INPUT_CAMO && unit.energy < CAMO_ENERGY_COST) {
                    continue;
                }
                if (input.type == MATCH_INPUT_CAMO) {
                    unit.energy -= CAMO_ENERGY_COST;
                }
                entity_set_flag(unit, ENTITY_FLAG_INVISIBLE, input.type == MATCH_INPUT_CAMO);
                match_event_play_sound(state, input.type == MATCH_INPUT_CAMO ? SOUND_CAMO_ON : SOUND_CAMO_OFF, unit.position.to_ivec2());
                unit.energy_regen_timer = UNIT_ENERGY_REGEN_DURATION;
            }
            break;
        }
    }
}

void match_update(MatchState& state) {
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        match_entity_update(state, entity_index);
    }

    // Update particles
    for (int particle_layer = 0; particle_layer < PARTICLE_LAYER_COUNT; particle_layer++) {
        uint32_t particle_index = 0;
        while (particle_index < state.particles[particle_layer].size()) {
            animation_update(state.particles[particle_layer][particle_index].animation);

            // On particle finish
            if (!animation_is_playing(state.particles[particle_layer][particle_index].animation)) {
                if (state.particles[particle_layer][particle_index].animation.name == ANIMATION_PARTICLE_SMOKE_START) {
                    state.particles[particle_layer][particle_index].animation = animation_create(ANIMATION_PARTICLE_SMOKE);
                } else if (state.particles[particle_layer][particle_index].animation.name == ANIMATION_PARTICLE_SMOKE) {
                    state.particles[particle_layer][particle_index].animation = animation_create(ANIMATION_PARTICLE_SMOKE_END);
                }
            }

            // If particle finished and is not playing an animation, then remove it
            if (!animation_is_playing(state.particles[particle_layer][particle_index].animation)) {
                state.particles[particle_layer].erase(state.particles[particle_layer].begin() + particle_index);
            } else {
                particle_index++;
            }
        }
    }

    // Update projectiles
    {
        uint32_t projectile_index = 0;
        while (projectile_index < state.projectiles.size()) {
            Projectile& projectile = state.projectiles[projectile_index];
            if (projectile.position.distance_to(projectile.target) <= PROJECTILE_MOLOTOV_SPEED) {
                // On projectile finish
                if (projectile.type == PROJECTILE_MOLOTOV) {
                    match_set_cell_on_fire(state, projectile.target.to_ivec2() / TILE_SIZE, projectile.target.to_ivec2() / TILE_SIZE);
                    // Check that it's actually on fire before playing the sound
                    if (match_is_cell_on_fire(state, projectile.target.to_ivec2() / TILE_SIZE)) {
                        match_event_play_sound(state, SOUND_MOLOTOV_IMPACT, projectile.target.to_ivec2());
                    }
                }
                state.projectiles.erase(state.projectiles.begin() + projectile_index);
            } else {
                projectile.position += ((projectile.target - projectile.position) * PROJECTILE_MOLOTOV_SPEED / projectile.position.distance_to(projectile.target));
                projectile_index++;
            }
        }
    }

    // Update fire
    {
        uint32_t fire_index = 0;
        while (fire_index < state.fires.size()) {
            animation_update(state.fires[fire_index].animation);

            // Start animation finished, enter prolonged burn and spread more flames
            if (state.fires[fire_index].animation.name == ANIMATION_FIRE_START && !animation_is_playing(state.fires[fire_index].animation)) {
                state.fires[fire_index].animation = animation_create(ANIMATION_FIRE_BURN);
                uint32_t fire_elevation = map_get_tile(state.map, state.fires[fire_index].cell).elevation;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 child_cell = state.fires[fire_index].cell + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(state.map, child_cell)) {
                        continue;
                    }
                    if (map_get_tile(state.map, child_cell).elevation != fire_elevation && !map_is_tile_ramp(state.map, child_cell)) {
                        continue;
                    }
                    match_set_cell_on_fire(state, child_cell, state.fires[fire_index].source);
                }
            // Fire is in prolonged burn, count down time to live
            } else if (state.fires[fire_index].animation.name == ANIMATION_FIRE_BURN) {
                state.fires[fire_index].time_to_live--;
            }
            // Time to live is 0, extinguish fire
            if (state.fires[fire_index].time_to_live == 0) {
                state.fire_cells[state.fires[fire_index].cell.x + (state.fires[fire_index].cell.y * state.map.width)] = 0;
                state.fires.erase(state.fires.begin() + fire_index);
            } else {
                fire_index++;
            }
        }
    }

    // Update fog reveals
    {
        uint32_t index = 0;
        while (index < state.fog_reveals.size()) {
            state.fog_reveals[index].timer--;
            if (state.fog_reveals[index].timer == 0) {
                match_fog_update(state, state.fog_reveals[index].team, state.fog_reveals[index].cell, state.fog_reveals[index].cell_size, state.fog_reveals[index].sight, false, CELL_LAYER_GROUND, false);
                state.fog_reveals.erase(state.fog_reveals.begin() + index);
            } else {
                index++;
            }
        }
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
                    match_fog_update(state, state.players[state.entities[entity_index].player_id].team, state.entities[entity_index].cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, state.entities[entity_index]), entity_data.cell_layer, false);
                }
                const EntityData& entity_data = entity_get_data(state.entities[entity_index].type);
                log_trace("Removing entity %s ID %u player id %u", entity_data.name, state.entities.get_id_of(entity_index), state.entities[entity_index].player_id);
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
    entity.energy = entity_is_unit(type) ? match_entity_get_max_energy(state, entity) / 4 : 0;
    entity.target = (Target) { .type = TARGET_NONE };
    entity.pathfind_attempts = 0;
    entity.timer = entity_is_unit(type) || entity.type == ENTITY_LANDMINE
                        ? 0
                        : (entity_data.max_health - entity.health);
    entity.rally_point = ivec2(-1, -1);

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = 0;
    entity.gold_mine_id = ID_NULL;

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;
    entity.fire_damage_timer = 0;
    entity.energy_regen_timer = 0;

    if (entity.type == ENTITY_LANDMINE) {
        entity.timer = MINE_ARM_DURATION;
        entity.mode = MODE_MINE_ARM;
    } else if (entity.type == ENTITY_SOLDIER) {
        entity_set_flag(entity, ENTITY_FLAG_CHARGED, true);
    }

    EntityId id = state.entities.push_back(entity);
    map_set_cell_rect(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
        .type = entity_is_unit(type) ? CELL_UNIT : CELL_BUILDING,
        .id = id
    });
    match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, true);

    log_trace("Created entity %s ID %u player %u cell %vi", entity_data.name, id, player_id, &cell);

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

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;

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
            entity.gold_mine_id = ID_NULL;
            entity.mode = entity.type == ENTITY_BALLOON ? MODE_UNIT_BALLOON_DEATH_START : MODE_UNIT_DEATH;
            entity.animation = animation_create(entity_get_expected_animation(entity));
            entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, false);
        } else {
            entity.mode = MODE_BUILDING_DESTROYED;
            entity.timer = BUILDING_FADE_DURATION;
            entity.queue.clear();

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
        }

        match_event_play_sound(state, entity_data.death_sound, entity.position.to_ivec2());

        if (!entity_is_unit(entity.type)) {
            // If it's a building, release garrisoned units
            // If it's a unit, it will release them once its death animation is over
            match_entity_release_garrisoned_units_on_death(state, entity);

            // Also if it's a building, turn off the burning flag
            entity_set_flag(entity, ENTITY_FLAG_ON_FIRE, false);
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
                    const Entity& carrier = state.entities.get_by_id(entity.garrison_id);
                    if (!(carrier.type == ENTITY_BUNKER || carrier.type == ENTITY_WAR_WAGON)) {
                        update_finished = true;
                        break;
                    }
                }

                // If unit is idle, check target queue
                if (entity.target.type == TARGET_NONE && !entity.target_queue.empty()) {
                    entity_set_target(entity, entity.target_queue[0]);
                    entity.target_queue.erase(entity.target_queue.begin());
                }

                // If unit is idle, try to find a nearby target
                if (entity.target.type == TARGET_NONE && entity.type != ENTITY_MINER && 
                        entity_data.unit_data.damage != 0 && 
                        !(entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE))) {
                    entity.target = match_entity_target_nearest_enemy(state, entity.garrison_id == ID_NULL ? entity : state.entities.get_by_id(entity.garrison_id));
                }

                // If unit is still idle, do nothing
                if (entity.target.type == TARGET_NONE) {
                    // If soldier is idle, charge weapon
                    if (entity.type == ENTITY_SOLDIER && !entity_check_flag(entity, ENTITY_FLAG_CHARGED)) {
                        entity.mode = MODE_UNIT_SOLDIER_CHARGE;
                    }
                    update_finished = true;
                    break;
                }

                if (match_is_target_invalid(state, entity.target, entity.player_id)) {
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
                if (entity.gold_mine_id != ID_NULL) {
                    const Entity& mine = state.entities.get_by_id(entity.gold_mine_id);
                    if (mine.gold_held == 0) {
                        entity.gold_mine_id = ID_NULL;
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
                            ivec2 mine_exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, mine.cell, entity_get_data(mine.type).cell_size, entity_data.cell_size, rally_cell, true);

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
                    map_pathfind(state.map, entity_data.cell_layer, entity.cell, match_get_entity_target_cell(state, entity), entity_data.cell_size, &entity.path, match_is_entity_mining(state, entity));
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
                        if (map_is_cell_rect_occupied(state.map, entity_data.cell_layer, entity.path[0], entity_data.cell_size, entity.cell, false)) {
                            path_is_blocked = true;
                            // breaks out of while movement left
                            break;
                        }

                        if (map_is_cell_rect_equal_to(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, entity_id)) {
                            map_set_cell_rect(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY,
                                .id = ID_NULL
                            });
                        }
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, false);
                        entity.cell = entity.path[0];
                        map_set_cell_rect(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                            .type = match_is_entity_mining(state, entity) ? CELL_MINER : CELL_UNIT,
                            .id = entity_id
                        });
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, true);
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
                        for (Entity& mine : state.entities) {
                            if (mine.type != ENTITY_LANDMINE || mine.health == 0 || mine.mode != MODE_BUILDING_FINISHED || 
                                    state.players[mine.player_id].team == state.players[entity.player_id].team ||
                                    std::abs(entity.cell.x - mine.cell.x) > 1 || std::abs(entity.cell.y - mine.cell.y) > 1) {
                                continue;
                            }
                            mine.animation = animation_create(ANIMATION_MINE_PRIME);
                            mine.timer = MINE_PRIME_DURATION;
                            mine.mode = MODE_MINE_PRIME;
                            entity_set_flag(mine, ENTITY_FLAG_INVISIBLE, false);
                        }
                        if (entity.target.type == TARGET_ATTACK_CELL) {
                            Target attack_target = match_entity_target_nearest_enemy(state, entity);
                            if (attack_target.type != TARGET_NONE) {
                                entity.target = attack_target;
                                entity.path.clear();
                                entity.mode = match_has_entity_reached_target(state, entity) ? MODE_UNIT_MOVE_FINISHED : MODE_UNIT_IDLE;
                                // breaks out of while movement left > 0
                                break;
                            }
                        }
                        if (match_is_target_invalid(state, entity.target, entity.player_id)) {
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
                        Cell blocking_cell = map_get_cell(state.map, entity_data.cell_layer, entity.path[0]);
                        if (blocking_cell.type == CELL_MINER) {
                            const Entity& blocker = state.entities.get_by_id(blocking_cell.id);
                            if (entity.direction == ((blocker.direction + 4) % DIRECTION_COUNT)) {
                                try_walk_around_blocker = true;
                            }
                        }
                    }
                    if (try_walk_around_blocker) {
                        entity.path.clear();
                        map_pathfind(state.map, entity_data.cell_layer, entity.cell, match_get_entity_target_cell(state, entity), entity_data.cell_size, &entity.path, false);
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
                    case TARGET_CELL: {
                        entity.target = (Target) { .type = TARGET_NONE };
                        entity.mode = MODE_UNIT_IDLE;
                        break;
                    }
                    case TARGET_UNLOAD: {
                        if (!match_has_entity_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        match_entity_unload_unit(state, entity, MATCH_ENTITY_UNLOAD_ALL);
                        entity.mode = MODE_UNIT_IDLE;
                        entity.target = (Target) { .type = TARGET_NONE };
                        break;
                    }
                    case TARGET_MOLOTOV: {
                        if (!match_has_entity_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        if (entity.energy < MOLOTOV_ENERGY_COST) {
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (Target) { .type = TARGET_NONE };
                            break;
                        }

                        entity.energy -= MOLOTOV_ENERGY_COST;
                        entity.direction = enum_direction_to_rect(entity.cell, entity.target.cell, 1);
                        entity.mode = MODE_UNIT_PYRO_THROW;
                        entity.animation = animation_create(ANIMATION_UNIT_ATTACK);
                        match_event_play_sound(state, SOUND_THROW, entity.position.to_ivec2());
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

                        bool building_costs_energy = (building_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY;
                        if (building_costs_energy) {
                            if (entity.energy < building_data.gold_cost) {
                                match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                                entity.target = (Target) { .type = TARGET_NONE };
                                entity.mode = MODE_UNIT_IDLE;
                                entity.target_queue.clear();
                                break;
                            }
                            entity.energy -= building_data.gold_cost;
                        } else {
                            if (state.players[entity.player_id].gold < building_data.gold_cost) {
                                match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                                entity.target = (Target) { .type = TARGET_NONE };
                                entity.mode = MODE_UNIT_IDLE;
                                entity.target_queue.clear();
                                break;
                            }
                            state.players[entity.player_id].gold -= building_data.gold_cost;
                        }

                        if (entity.target.build.building_type == ENTITY_LANDMINE) {
                            match_create_entity(state, entity.target.build.building_type, entity.target.build.building_cell, entity.player_id);
                            match_event_play_sound(state, SOUND_MINE_INSERT, cell_center(entity.target.build.building_cell).to_ivec2());

                            entity.direction = enum_direction_to_rect(entity.cell, entity.target.build.building_cell, building_data.cell_size);
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_PYRO_THROW;
                            entity.animation = animation_create(ANIMATION_UNIT_ATTACK);
                        } else {
                            map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY, .id = ID_NULL
                            });
                            match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, false);
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
                        if (match_is_target_invalid(state, entity.target, entity.player_id)) {
                            entity.target = (Target) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                        }

                        Entity& builder = state.entities.get_by_id(entity.target.id);
                        if (builder.mode == MODE_UNIT_BUILD) {
                            entity.target = (Target) {
                                .type = TARGET_REPAIR,
                                .id = builder.target.id,
                            };
                            entity.mode = MODE_UNIT_BUILD_ASSIST;
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            entity.direction = enum_direction_to_rect(entity.cell, builder.target.build.building_cell, entity_get_data(builder.target.build.building_type).cell_size);
                        }
                        break;
                    }
                    case TARGET_REPAIR:
                    case TARGET_ENTITY:
                    case TARGET_ATTACK_ENTITY: {
                        if (match_is_target_invalid(state, entity.target, entity.player_id)) {
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

                        // Sapper explosion
                        if (entity.target.type == TARGET_ATTACK_ENTITY && entity.type == ENTITY_SAPPER) {
                            match_entity_explode(state, entity_id);
                            update_finished = true;
                            break;
                        }

                        // Begin attack
                        if (entity.target.type == TARGET_ATTACK_ENTITY && entity_data.unit_data.damage != 0) {
                            if (entity.cooldown_timer != 0) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            // Don't attack sky units unless this unit is ranged
                            if (target_data.cell_layer == CELL_LAYER_SKY && (entity_data.unit_data.range_squared == 1 || entity.type == ENTITY_CANNON)) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            // Check min range
                            Rect entity_rect = (Rect) {
                                .x = entity.cell.x, .y = entity.cell.y,
                                .w = entity_data.cell_size, .h = entity_data.cell_size
                            };
                            Rect target_rect = (Rect) {
                                .x = target.cell.x, .y = target.cell.y,
                                .w = target_data.cell_size, .h = target_data.cell_size
                            };
                            bool attack_with_bayonets = false;
                            if (Rect::euclidean_distance_squared_between(entity_rect, target_rect) < entity_data.unit_data.min_range_squared && target_data.cell_layer != CELL_LAYER_SKY) {
                                if (entity.type == ENTITY_SOLDIER && match_player_has_upgrade(state, entity.player_id, UPGRADE_BAYONETS)) {
                                    attack_with_bayonets = true;
                                } else {
                                    entity.direction = enum_direction_to_rect(entity.cell, target.cell, target_data.cell_size);
                                    entity.mode = MODE_UNIT_IDLE;
                                    update_finished = true;
                                    break;
                                }
                            }

                            if (entity.type == ENTITY_SOLDIER && !attack_with_bayonets && !entity_check_flag(entity, ENTITY_FLAG_CHARGED)) {
                                entity.mode = MODE_UNIT_SOLDIER_CHARGE;
                                update_finished = true;
                                break;
                            }

                            // Attack inside bunker
                            if (entity.garrison_id != ID_NULL) {
                                Entity& carrier = state.entities.get_by_id(entity.garrison_id);
                                // Don't attack during bunker cooldown or if this is a melee unit
                                if (carrier.cooldown_timer != 0 || entity_data.unit_data.range_squared == 1) {
                                    update_finished = true;
                                    break;
                                }

                                carrier.cooldown_timer = ENTITY_BUNKER_FIRE_OFFSET;
                            }

                            // Begin attack windup
                            entity.direction = enum_direction_to_rect(entity.cell, target.cell, target_data.cell_size);
                            entity.mode = entity.type == ENTITY_SOLDIER && !attack_with_bayonets
                                            ? MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP
                                            : MODE_UNIT_ATTACK_WINDUP;
                            update_finished = true;
                            break;
                        }

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

                        // Garrison
                        if (entity.player_id == target.player_id && entity_data.garrison_size != ENTITY_CANNOT_GARRISON && 
                                entity.target.type != TARGET_REPAIR && (entity_is_unit(target.type) || target.mode == MODE_BUILDING_FINISHED) &&
                                match_get_entity_garrisoned_occupancy(state, target) + entity_data.garrison_size <= target_data.garrison_capacity) {
                            target.garrisoned_units.push_back(entity_id);
                            entity.garrison_id = entity.target.id;
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (Target) { .type = TARGET_NONE };
                            map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY, .id = ID_NULL
                            });
                            match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, false);
                            match_event_play_sound(state, SOUND_GARRISON_IN, target.position.to_ivec2());
                            update_finished = true;
                            break;
                        }

                        // Begin repair
                        if (entity_is_building(target.type) && entity.type == ENTITY_MINER && 
                                state.players[entity.player_id].team == state.players[target.player_id].team &&
                                entity_is_building(target.type) && target.health < target_data.max_health) {
                            entity.mode = target.mode == MODE_BUILDING_IN_PROGRESS ? MODE_UNIT_BUILD_ASSIST : MODE_UNIT_REPAIR;
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

                    #ifdef GOLD_DEBUG_FAST_BUILD
                        int building_max_health = entity_get_data(building.type).max_health;
                        building.health = std::min(building.health + 20, building_max_health);
                        building.timer = building.timer >= 20 ? building.timer - 20 : 0; 
                    #else
                        building.health++;
                        building.timer--;
                    #endif
                    if (building.timer == 0) {
                        match_entity_building_finish(state, entity.target.id);
                    } else {
                        entity.timer = UNIT_BUILD_TICK_DURATION;
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_BUILD_ASSIST: 
            case MODE_UNIT_REPAIR: {
                // Stop repairing if the building is destroyed
                if (match_is_target_invalid(state, entity.target, entity.player_id)) {
                    entity.target = (Target) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }

                Entity& target = state.entities.get_by_id(entity.target.id);
                int target_max_health = entity_get_data(target.type).max_health;
                if ((entity.mode == MODE_UNIT_REPAIR && target.health == target_max_health) || 
                        (entity.mode == MODE_UNIT_BUILD_ASSIST && target.mode == MODE_BUILDING_FINISHED) || 
                        state.players[entity.player_id].gold == 0) {
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
                    if (entity.mode == MODE_UNIT_BUILD_ASSIST) {
                        target.timer--;
                    }
                    target.health_regen_timer++;
                    if (target.health_regen_timer == UNIT_REPAIR_RATE) {
                        state.players[entity.player_id].gold--;
                        target.health_regen_timer = 0;
                    }
                    if (target.health > target_max_health / 4) {
                        entity_set_flag(target, ENTITY_FLAG_ON_FIRE, false);
                    }
                    if (entity.mode == MODE_UNIT_BUILD_ASSIST && target.timer == 0) {
                        match_entity_building_finish(state, entity.target.id);
                    } else if (entity.mode == MODE_UNIT_REPAIR && target.health == target_max_health) {
                        entity.target = (Target) { .type = TARGET_NONE };
                        entity.mode = MODE_UNIT_IDLE;
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
                    ivec2 exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, mine.cell, mine_data.cell_size, entity_data.cell_size, rally_cell, false);

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
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, false);
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
                        match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, true);

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
            case MODE_UNIT_ATTACK_WINDUP:
            case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP: {
                if (match_is_target_invalid(state, entity.target, entity.player_id)) {
                    entity.target = (Target) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                if (!animation_is_playing(entity.animation)) {
                    match_entity_attack_target(state, entity_id, state.entities.get_by_id(entity.target.id));
                    if (entity.mode == MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP) {
                        entity_set_flag(entity, ENTITY_FLAG_CHARGED, false);
                        entity.mode = MODE_UNIT_SOLDIER_CHARGE;
                    } else {
                        entity.cooldown_timer = entity_data.unit_data.attack_cooldown;
                        entity.mode = MODE_UNIT_IDLE;
                    }

                    // If garrisoned, re-asses targets
                    if (entity.garrison_id != ID_NULL) {
                        entity.target = match_entity_target_nearest_enemy(state, entity);
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_SOLDIER_CHARGE: {
                if (!animation_is_playing(entity.animation)) {
                    entity_set_flag(entity, ENTITY_FLAG_CHARGED, true);
                    entity.mode = MODE_UNIT_IDLE;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_PYRO_THROW: {
                if (!animation_is_playing(entity.animation)) {
                    entity.target = (Target) { .type = TARGET_NONE };
                    entity.mode = MODE_UNIT_IDLE;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_DEATH: {
                if (!animation_is_playing(entity.animation)) {
                    map_set_cell_rect(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                        .type = CELL_EMPTY, .id = ID_NULL
                    });
                    match_entity_release_garrisoned_units_on_death(state, entity);
                    entity.mode = MODE_UNIT_DEATH_FADE;
                }
                update_finished = true;
                break;
            }
            case MODE_UNIT_BALLOON_DEATH_START: {
                if (!animation_is_playing(entity.animation)) {
                    entity.mode = MODE_UNIT_BALLOON_DEATH;
                    entity.animation = animation_create(entity_get_expected_animation(entity));
                    entity.timer = ENTITY_BALLOON_DEATH_DURATION;
                }
                update_finished = true;
                break;
            }
            case MODE_UNIT_BALLOON_DEATH: {
                entity.timer--;
                if (entity.timer == 0) {
                    map_set_cell_rect(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                        .type = CELL_EMPTY, .id = ID_NULL
                    });
                    match_entity_release_garrisoned_units_on_death(state, entity);
                    state.particles[PARTICLE_LAYER_GROUND].push_back((Particle) {
                        .sprite = SPRITE_PARTICLE_CANNON_EXPLOSION,
                        .animation = animation_create(ANIMATION_PARTICLE_CANNON_EXPLOSION),
                        .vframe = 0,
                        .position = entity.position.to_ivec2()
                    });
                    match_event_play_sound(state, SOUND_EXPLOSION, entity.position.to_ivec2());
                    entity.mode = MODE_UNIT_DEATH_FADE;
                }
                update_finished = true;
                break;
            }
            case MODE_MINE_ARM: {
                entity.timer--;
                if (entity.timer == 0) {
                    entity.mode = MODE_BUILDING_FINISHED;
                    entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, true);
                }
                update_finished = true;
                break;
            }
            case MODE_MINE_PRIME: {
                entity.timer--;
                if (entity.timer == 0) {
                    match_entity_explode(state, entity_id);
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
                        ivec2 exit_cell = map_get_exit_cell(state.map, entity_data.cell_layer, entity.cell, entity_data.cell_size, entity_get_data(entity.queue[0].unit_type).cell_size, rally_cell, false);
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

    // Check for fire
    // If entity is moving, check for fire based on its previous cell
    if (entity.type != ENTITY_GOLDMINE && 
            entity_data.cell_layer != CELL_LAYER_SKY && 
            entity.type != ENTITY_BUNKER &&
            entity.health != 0 && entity.garrison_id == ID_NULL) {
        ivec2 entity_fire_cell = entity.cell;
        if (entity_is_unit(entity.type) && entity.mode == MODE_UNIT_MOVE) {
            entity_fire_cell = entity.cell - DIRECTION_IVEC2[entity.direction];
        }
        if (entity_is_building(entity.type) && entity.mode == MODE_BUILDING_FINISHED && entity.health < entity_data.max_health / 4) {
            entity_set_flag(entity, ENTITY_FLAG_ON_FIRE, true);
        }
        if (match_is_cell_rect_on_fire(state, entity_fire_cell, entity_data.cell_size) || entity_check_flag(entity, ENTITY_FLAG_ON_FIRE)) {
            if (entity.fire_damage_timer != 0) {
                entity.fire_damage_timer--;
            }
            if (entity.fire_damage_timer == 0) {
                entity.health--;
                entity.fire_damage_timer = ENTITY_FIRE_DAMAGE_COOLDOWN;
            } 
            if (entity_is_building(entity.type)) {
                entity_set_flag(entity, ENTITY_FLAG_ON_FIRE, true);
            }
        } else {
            entity.fire_damage_timer = 0;
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
    if (entity_is_unit(entity.type) && entity.health_regen_timer != 0) {
        entity.health_regen_timer--;
        if (entity.health_regen_timer == 0) {
            entity.health++;
            if (entity.health != entity_data.max_health) {
                entity.health_regen_timer = UNIT_HEALTH_REGEN_DURATION;
            }
        }
    }

    if (entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) {
        if (entity.energy_regen_timer != 0) {
            entity.energy_regen_timer--;
        }
        if (entity.energy_regen_timer == 0) {
            entity.energy_regen_timer = UNIT_ENERGY_REGEN_DURATION;
            entity.energy--;
            if (entity.energy == 0) {
                entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, false);
                match_event_play_sound(state, SOUND_CAMO_OFF, entity.position.to_ivec2());
            }
        }
    } else if (entity_is_unit(entity.type) && entity.energy < match_entity_get_max_energy(state, entity)) {
        if (entity.energy_regen_timer != 0) {
            entity.energy_regen_timer--;
        }
        if (entity.energy_regen_timer == 0) {
            entity.energy_regen_timer = UNIT_ENERGY_REGEN_DURATION;
            entity.energy++;
        }
    }

    // Update animation
    AnimationName expected_animation = entity_get_expected_animation(entity);
    if (entity.animation.name != expected_animation || !animation_is_playing(entity.animation)) {
        entity.animation = animation_create(expected_animation);
    }
    int prev_hframe = entity.animation.frame.x;
    animation_update(entity.animation);
    if (prev_hframe != entity.animation.frame.x) {
        if ((entity.mode == MODE_UNIT_REPAIR || entity.mode == MODE_UNIT_BUILD) && prev_hframe == 0) {
            match_event_play_sound(state, SOUND_HAMMER, entity.position.to_ivec2());
        } else if (entity.mode == MODE_UNIT_PYRO_THROW && entity.animation.frame.x == 6) {
            if (entity.target.type == TARGET_MOLOTOV) {
                state.projectiles.push_back((Projectile) {
                    .type = PROJECTILE_MOLOTOV,
                    .position = entity.position + ivec2(DIRECTION_IVEC2[entity.direction] * 6),
                    .target = cell_center(entity.target.cell)
                });
            }
        } else if (entity.mode == MODE_MINE_PRIME) {
            match_event_play_sound(state, SOUND_MINE_PRIME, entity.position.to_ivec2());
        }
    }
}

bool match_is_entity_visible_to_player(const MatchState& state, const Entity& entity, uint8_t player_id) {
    if (entity.garrison_id != ID_NULL) {
        return false;
    }

    uint8_t player_team = state.players[player_id].team;
    if (entity.type != ENTITY_GOLDMINE && state.players[entity.player_id].team == player_team) {
        return true;
    }

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

bool match_is_target_invalid(const MatchState& state, const Target& target, uint8_t player_id) {
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

    if (!entity_is_selectable(state.entities[target_index])) {
        return true;
    }

    if (target.type == TARGET_ATTACK_ENTITY && 
            entity_check_flag(state.entities[target_index], ENTITY_FLAG_INVISIBLE) && 
            !match_is_entity_visible_to_player(state, state.entities[target_index], player_id)) {
        return true;
    }

    return false;
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

            if (entity.target.type != TARGET_ATTACK_ENTITY && (entity.type == ENTITY_BALLOON || target.type == ENTITY_BALLOON)) {
                return entity.cell == match_get_entity_target_cell(state, entity);
            }

            int entity_range_squared = entity_get_data(entity.type).unit_data.range_squared;
            return entity.target.type != TARGET_ATTACK_ENTITY || entity_range_squared == 1
                        ? entity_rect.is_adjacent_to(target_rect)
                        : Rect::euclidean_distance_squared_between(entity_rect, target_rect) <= entity_range_squared;
        }
        case TARGET_MOLOTOV: {
            return ivec2::euclidean_distance_squared(entity.cell, entity.target.cell) <= MOLOTOV_RANGE_SQUARED;
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
        case TARGET_MOLOTOV:
            return entity.target.cell;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            const Entity& target = state.entities.get_by_id(entity.target.id);
            int entity_cell_size = entity_get_data(entity.type).cell_size;
            int target_cell_size = entity_get_data(target.type).cell_size;
            if (entity.type == ENTITY_BALLOON || target.type == ENTITY_BALLOON) {
                switch (target_cell_size) {
                    case 3:
                    case 4:
                        return target.cell + ivec2(1, 1);
                    default:
                        return target.cell;
                }
            }
            ivec2 ignore_cell = ivec2(-1, -1);
            if (target.type == ENTITY_GOLDMINE) {
                Target hall_target = match_entity_target_nearest_hall(state, entity);
                if (hall_target.type != TARGET_NONE) {
                    const Entity& hall = state.entities.get_by_id(hall_target.id);
                    const EntityData& hall_data = entity_get_data(ENTITY_HALL);
                    ivec2 rally_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, target.cell + ivec2(1, 1), 1, hall.cell, hall_data.cell_size, true);
                    ignore_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, target.cell, target_cell_size, entity_cell_size, rally_cell, true);
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
           (target.type == ENTITY_HALL && target.mode == MODE_BUILDING_FINISHED && entity.gold_mine_id != ID_NULL &&
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
    match_fog_update(state, state.players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, match_entity_has_detection(state, entity), entity_data.cell_layer, true); 
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
        } else if (entity.mode == MODE_UNIT_REPAIR || entity.mode == MODE_UNIT_BUILD_ASSIST) {
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
    for (int y = entity.cell.y - 16; y < entity.cell.y + 16; y++) {
        for (int x = entity.cell.x - 16; x < entity.cell.x + 16; x++) {
            ivec2 cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state.map, cell)) {
                continue;
            }
            Cell value = map_get_cell(state.map, CELL_LAYER_GROUND, cell);
            if (value.type == CELL_GOLDMINE && state.entities.get_by_id(value.id).gold_held != 0) {
                return (Target) {
                    .type = TARGET_ENTITY,
                    .id = value.id
                };
            }
        }
    }

    return (Target) { .type = TARGET_NONE };
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

Target match_entity_target_nearest_enemy(const MatchState& state, const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    Rect entity_rect = (Rect) { 
        .x = entity.cell.x, .y = entity.cell.y, 
        .w = entity_data.cell_size, .h = entity_data.cell_size
    };
    Rect entity_sight_rect = (Rect) {
        .x = entity.cell.x - entity_data.sight,
        .y = entity.cell.y - entity_data.sight,
        .w = 2 * entity_data.sight,
        .h = 2 * entity_data.sight
    };
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;
    uint32_t nearest_attack_priority;

    for (uint32_t other_index = 0; other_index < state.entities.size(); other_index++) {
        const Entity& other = state.entities[other_index];
        const EntityData& other_data = entity_get_data(other.type);

        // Goldmines are not enemies
        if (other.type == ENTITY_GOLDMINE) {
            continue;
        }
        // Allies are not enemies
        if (state.players[other.player_id].team == state.players[entity.player_id].team) {
            continue;
        } 
        // Don't attack non-selectable entities
        if (!entity_is_selectable(other)) {
            continue;
        }
        // Don't attack entities that the player can't see
        if (!match_is_entity_visible_to_player(state, other, entity.player_id)) {
            continue;
        }
        // Don't attack entities that this unit can't see
        Rect other_rect = (Rect) { 
            .x = other.cell.x, .y = other.cell.y, 
            .w = other_data.cell_size, .h = other_data.cell_size
        };
        if (!entity_sight_rect.intersects(other_rect)) {
            continue;
        }

        int other_dist = Rect::euclidean_distance_squared_between(entity_rect, other_rect);
        uint32_t other_attack_priority = other_data.attack_priority;
        if (nearest_enemy_index == INDEX_INVALID || other_attack_priority > nearest_attack_priority || (other_dist < nearest_enemy_dist && other_attack_priority == nearest_attack_priority)) {
            nearest_enemy_index = other_index;
            nearest_enemy_dist = other_dist;
            nearest_attack_priority = other_attack_priority;
        }
    }

    if (nearest_enemy_index == INDEX_INVALID) {
        return (Target) {
            .type = TARGET_NONE
        };
    }

    return (Target) {
        .type = TARGET_ATTACK_ENTITY,
        .id = state.entities.get_id_of(nearest_enemy_index)
    };
}

void match_entity_attack_target(MatchState& state, EntityId attacker_id, Entity& defender) {
    const Entity& attacker = state.entities.get_by_id(attacker_id);
    const EntityData& attacker_data = entity_get_data(attacker.type);
    const EntityData& defender_data = entity_get_data(defender.type);

    // Calculate damage
    bool attack_with_bayonets = attacker.type == ENTITY_SOLDIER && attacker.mode == MODE_UNIT_ATTACK_WINDUP;

    // Calculate accuracy
    int accuracy = attack_with_bayonets ? 100 : match_entity_get_accuracy(state, attacker);
    if (defender.mode == MODE_UNIT_MOVE) {
        accuracy -= match_entity_get_evasion(state, defender); 
    }
    if (defender.type == ENTITY_LANDMINE && defender.mode == MODE_MINE_PRIME) {
        accuracy = 0;
    }
    if (entity_get_elevation(attacker, state.map) < entity_get_elevation(defender, state.map)) {
        accuracy /= 2;
    } 

    // Bunker particle
    if (attacker.garrison_id != ID_NULL) {
        Entity& carrier = state.entities.get_by_id(attacker.garrison_id);
        int particle_index = lcg_rand(&state.lcg_seed) % 4;
        ivec2 particle_position;
        if (carrier.type == ENTITY_BUNKER) {
            particle_position = (carrier.cell * TILE_SIZE) + BUNKER_PARTICLE_OFFSETS[particle_index];
        } else if (carrier.type == ENTITY_WAR_WAGON) {
            const SpriteInfo& wagon_sprite_info = render_get_sprite_info(SPRITE_UNIT_WAR_WAGON);
            particle_position = carrier.position.to_ivec2() - (ivec2(wagon_sprite_info.frame_width, wagon_sprite_info.frame_height) / 2);
            if (carrier.direction == DIRECTION_SOUTH) {
                particle_position += WAR_WAGON_DOWN_PARTICLE_OFFSETS[particle_index];
            } else if (carrier.direction == DIRECTION_NORTH) {
                particle_position += WAR_WAGON_UP_PARTICLE_OFFSETS[particle_index];
            } else {
                ivec2 offset = WAR_WAGON_RIGHT_PARTICLE_OFFSETS[particle_index];
                if (carrier.direction > DIRECTION_SOUTH) {
                    offset.x = wagon_sprite_info.frame_width - offset.x;
                }
                particle_position += offset;
            }
        }

        state.particles[PARTICLE_LAYER_GROUND].push_back((Particle) {
            .sprite = SPRITE_PARTICLE_BUNKER_FIRE,
            .animation = animation_create(ANIMATION_PARTICLE_BUNKER_COWBOY),
            .vframe = 0,
            .position = particle_position
        });
    }

    // Fog reveal
    if (entity_get_elevation(attacker, state.map) > entity_get_elevation(defender, state.map) &&
            !entity_check_flag(attacker, ENTITY_FLAG_INVISIBLE)) {
        FogReveal reveal = (FogReveal) {
            .team = state.players[defender.player_id].team,
            .cell = attacker.cell,
            .cell_size = attacker_data.cell_size,
            .sight = 3,
            .timer = FOG_REVEAL_DURATION
        };
        match_fog_update(state, reveal.team, reveal.cell, reveal.cell_size, reveal.sight, false, CELL_LAYER_GROUND, true);
        state.fog_reveals.push_back(reveal);
    }

    bool attack_missed = accuracy < lcg_rand(&state.lcg_seed) % 100;
    bool attack_is_melee = attack_with_bayonets || attacker_data.unit_data.range_squared == 1;
    if (attack_missed && attack_is_melee) {
        return;
    }

    // Hit position will be the location of the particle
    // It will also determine who the attack hits if the attack misses
    Rect defender_rect = entity_get_rect(defender);
    ivec2 hit_position;
    if (attack_missed) {
        // Chooses a hit position for the particle in a donut
        int hit_x = lcg_rand(&state.lcg_seed) % (TILE_SIZE * 2);
        int hit_y = lcg_rand(&state.lcg_seed) % (TILE_SIZE * 2);
        if (hit_x >= TILE_SIZE) {
            hit_x += TILE_SIZE;
        }
        if (hit_y >= TILE_SIZE) {
            hit_y += TILE_SIZE;
        }

        hit_position = ivec2(defender_rect.x - TILE_SIZE + hit_x, defender_rect.y - TILE_SIZE + hit_y);
    } else {
        hit_position = ivec2(
            defender_rect.x + (defender_rect.w / 4) + (lcg_rand(&state.lcg_seed) % (defender_rect.w / 2)),
            defender_rect.y + (defender_rect.h / 4) + (lcg_rand(&state.lcg_seed) % (defender_rect.h / 2)));
    }

    // Play sound
    if (attack_missed && attacker.type != ENTITY_CANNON) {
        match_event_play_sound(state, SOUND_RICOCHET, hit_position);
    } 
    SoundName attack_sound = attack_with_bayonets
                                ? SOUND_SWORD
                                : attacker_data.unit_data.attack_sound;
    match_event_play_sound(state, attack_sound, hit_position);

    // Create particle effect
    if (attacker.type == ENTITY_CANNON) {
        state.particles[PARTICLE_LAYER_GROUND].push_back((Particle) {
            .sprite = SPRITE_PARTICLE_CANNON_EXPLOSION,
            .animation = animation_create(ANIMATION_PARTICLE_CANNON_EXPLOSION),
            .vframe = 0,
            .position = hit_position
        });
    } else if (attacker.type == ENTITY_COWBOY || (attacker.type == ENTITY_SOLDIER && !attack_with_bayonets) || attacker.type == ENTITY_DETECTIVE || attacker.type == ENTITY_JOCKEY) {
        state.particles[defender_data.cell_layer == CELL_LAYER_SKY ? PARTICLE_LAYER_SKY : PARTICLE_LAYER_GROUND].push_back((Particle) {
            .sprite = SPRITE_PARTICLE_SPARKS,
            .animation = animation_create(ANIMATION_PARTICLE_SPARKS),
            .vframe = lcg_rand(&state.lcg_seed) % 3,
            .position = hit_position
        });
    }

    if (attacker.type == ENTITY_CANNON) {
        // Check which enemies we hit
        int attacker_damage = match_entity_get_damage(state, attacker);
        Rect full_damage_rect = (Rect) {
            .x = hit_position.x - (TILE_SIZE / 2),
            .y = hit_position.y - (TILE_SIZE / 2),
            .w = TILE_SIZE, .h = TILE_SIZE
        };
        Rect splash_damage_rect = (Rect) {
            .x = full_damage_rect.x - (TILE_SIZE / 2),
            .y = full_damage_rect.y - (TILE_SIZE / 2),
            .w = full_damage_rect.w + TILE_SIZE,
            .h = full_damage_rect.h + TILE_SIZE,
        };
        for (Entity& entity : state.entities) {
            if (entity.type == ENTITY_GOLDMINE || !entity_is_selectable(entity)) {
                continue;
            }

            // To check if we've hit this enemy, first check the splash damage rect
            // We have to check both, but since the splash rect is bigger, we know that 
            // if they're outside of it, they will be outside of the full damage rect as well
            Rect entity_rect = entity_get_rect(entity);
            if (entity_rect.intersects(splash_damage_rect)) {
                int damage = attacker_damage; 
                // Half damage if they are only within splash damage range
                if (!entity_rect.intersects(full_damage_rect)) {
                    damage /= 2;
                }
                damage = std::max(1, damage - entity_get_data(entity.type).armor);

                entity.health = std::max(0, entity.health - damage);
                match_entity_on_attack(state, attacker_id, entity);
            }
        }
    } else if (!attack_missed) {
        int attacker_damage = attack_with_bayonets ? SOLDIER_BAYONET_DAMAGE : match_entity_get_damage(state, attacker);
        int damage = std::max(1, attacker_damage - match_entity_get_armor(state, defender));
        defender.health = std::max(0, defender.health - damage);
        match_entity_on_attack(state, attacker_id, defender);
    }
}

void match_entity_on_attack(MatchState& state, EntityId attacker_id, Entity& defender) {
    const Entity& attacker = state.entities.get_by_id(attacker_id);
    const EntityData& defender_data = entity_get_data(defender.type);

    // Alerts / taking damage flicker
    if (attacker.player_id != defender.player_id) {
        if (defender.taking_damage_counter == 0) {
            match_event_alert(state, MATCH_ALERT_TYPE_ATTACK, defender.player_id, defender.cell, defender_data.cell_size);
        }
    }
    defender.taking_damage_counter = 3;
    if (defender.taking_damage_timer == 0) {
        defender.taking_damage_timer = MATCH_TAKING_DAMAGE_FLICKER_DURATION;
    }

    // Health regen timer
    if (entity_is_unit(defender.type)) {
        defender.health_regen_timer = UNIT_HEALTH_REGEN_DURATION + UNIT_HEALTH_REGEN_DELAY;
    }

    // Make defender attack back
    if (entity_is_unit(defender.type) && defender.mode == MODE_UNIT_IDLE &&
            defender.target.type == TARGET_NONE && defender_data.unit_data.damage != 0 &&
            !(defender.type == ENTITY_DETECTIVE && entity_check_flag(defender, ENTITY_FLAG_INVISIBLE)) &&
            defender.player_id != attacker.player_id && match_is_entity_visible_to_player(state, attacker, defender.player_id)) {
        defender.target = (Target) {
            .type = TARGET_ATTACK_ENTITY,
            .id = attacker_id
        };
    }
}

uint32_t match_get_entity_garrisoned_occupancy(const MatchState& state, const Entity& entity) {
    uint32_t occupancy = 0;
    for (EntityId id : entity.garrisoned_units) {
        occupancy += entity_get_data(state.entities.get_by_id(id).type).garrison_size;
    }
    return occupancy;
}

void match_entity_unload_unit(MatchState& state, Entity& carrier, EntityId garrisoned_unit_id) {
    const EntityData& carrier_data = entity_get_data(carrier.type);
    uint32_t index = 0;
    while (index < carrier.garrisoned_units.size()) {
        if (garrisoned_unit_id == MATCH_ENTITY_UNLOAD_ALL || carrier.garrisoned_units[index] == garrisoned_unit_id) {
            Entity& garrisoned_unit = state.entities.get_by_id(carrier.garrisoned_units[index]);
            const EntityData& garrisoned_unit_data = entity_get_data(garrisoned_unit.type);

            // Find the exit cell
            ivec2 exit_cell = map_get_exit_cell(state.map, CELL_LAYER_GROUND, carrier.cell, carrier_data.cell_size, garrisoned_unit_data.cell_size, carrier.cell + ivec2(0, carrier_data.cell_size), false);
            if (exit_cell.x == -1) {
                if (entity_is_building(carrier.type)) {
                    match_event_show_status(state, garrisoned_unit.player_id, MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED);
                }
                return;
            }

            // Place the unit in the world
            garrisoned_unit.cell = exit_cell;
            garrisoned_unit.position = entity_get_target_position(garrisoned_unit);
            map_set_cell_rect(state.map, CELL_LAYER_GROUND, garrisoned_unit.cell, garrisoned_unit_data.cell_size, (Cell) {
                .type = CELL_UNIT, .id = carrier.garrisoned_units[index]
            });
            match_fog_update(state, state.players[garrisoned_unit.player_id].team, garrisoned_unit.cell, garrisoned_unit_data.cell_size, garrisoned_unit_data.sight, match_entity_has_detection(state, garrisoned_unit), garrisoned_unit_data.cell_layer, true);
            garrisoned_unit.mode = MODE_UNIT_IDLE;
            garrisoned_unit.target = (Target) { .type = TARGET_NONE };
            garrisoned_unit.garrison_id = ID_NULL;
            garrisoned_unit.gold_mine_id = ID_NULL;

            // Remove the unit from the garrisoned units list
            carrier.garrisoned_units.erase(carrier.garrisoned_units.begin() + index);

            match_event_play_sound(state, SOUND_GARRISON_OUT, carrier.position.to_ivec2());
        } else {
            index++;
        }
    }
}

void match_entity_release_garrisoned_units_on_death(MatchState& state, Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    for (EntityId garrisoned_unit_id : entity.garrisoned_units) {
        Entity& garrisoned_unit = state.entities.get_by_id(garrisoned_unit_id);
        const EntityData& garrisoned_unit_data = entity_get_data(garrisoned_unit.type);
        // place garrisoned units inside former-self
        bool unit_is_placed = false;
        for (int x = entity.cell.x; x < entity.cell.x + entity_data.cell_size; x++) {
            for (int y = entity.cell.y; y < entity.cell.y + entity_data.cell_size; y++) {
                if (!map_is_cell_rect_occupied(state.map, CELL_LAYER_GROUND, ivec2(x, y), garrisoned_unit_data.cell_size)) {
                    garrisoned_unit.cell = ivec2(x, y);
                    garrisoned_unit.position = entity_get_target_position(garrisoned_unit);
                    garrisoned_unit.garrison_id = ID_NULL;
                    garrisoned_unit.mode = MODE_UNIT_IDLE;
                    garrisoned_unit.target = (Target) { .type = TARGET_NONE };
                    map_set_cell_rect(state.map, CELL_LAYER_GROUND, garrisoned_unit.cell, garrisoned_unit_data.cell_size, (Cell) {
                        .type = CELL_UNIT, .id = garrisoned_unit_id
                    });
                    match_fog_update(state, state.players[garrisoned_unit.player_id].team, garrisoned_unit.cell, garrisoned_unit_data.cell_size, garrisoned_unit_data.sight, match_entity_has_detection(state, garrisoned_unit), garrisoned_unit_data.cell_layer, true);
                    unit_is_placed = true;
                    break;
                }
            }
            if (unit_is_placed) {
                break;
            }
        }

        if (!unit_is_placed) {
            log_warn("Unable to place garrisoned unit.");
        }
    }
}

void match_entity_explode(MatchState& state, EntityId entity_id) {
    Entity& entity = state.entities.get_by_id(entity_id);
    const EntityData& entity_data = entity_get_data(entity.type);

    // Apply damage
    Rect explosion_rect = (Rect) { 
        .x = (entity.cell.x - 1) * TILE_SIZE,
        .y = (entity.cell.y - 1) * TILE_SIZE,
        .w = TILE_SIZE * 3,
        .h = TILE_SIZE * 3
    };
    int explosion_damage = entity.type == ENTITY_SAPPER ? entity_data.unit_data.damage : MINE_EXPLOSION_DAMAGE;
    for (uint32_t defender_index = 0; defender_index < state.entities.size(); defender_index++) {
        if (defender_index == state.entities.get_index_of(entity_id)) {
            continue;
        }
        Entity& defender = state.entities[defender_index];
        if (defender.type == ENTITY_GOLDMINE || !entity_is_selectable(defender)) {
            continue;
        }

        Rect defender_rect = entity_get_rect(defender);
        if (explosion_rect.intersects(defender_rect)) {
            int defender_armor = entity_get_data(defender.type).armor;
            int damage = std::max(1, explosion_damage - defender_armor);
            defender.health = std::max(defender.health - damage, 0);
            match_entity_on_attack(state, entity_id, defender);
        }
    }

    // Kill the entity
    entity.health = 0;
    if (entity.type == ENTITY_SAPPER) {
        entity.target = (Target) { .type = TARGET_NONE };
        entity.mode = MODE_UNIT_DEATH_FADE;
        entity.animation = animation_create(ANIMATION_UNIT_DEATH_FADE);
        map_set_cell_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
            .type = CELL_EMPTY, .id = ID_NULL
        });
    } else {
        entity.mode = MODE_BUILDING_DESTROYED;
        entity.timer = BUILDING_FADE_DURATION;
        map_set_cell(state.map, CELL_LAYER_UNDERGROUND, entity.cell, (Cell) { .type = CELL_EMPTY, .id = ID_NULL });
    }

    match_event_play_sound(state, SOUND_EXPLOSION, entity.position.to_ivec2());

    // Create particle
    state.particles[PARTICLE_LAYER_GROUND].push_back((Particle) {
        .sprite = SPRITE_PARTICLE_EXPLOSION,
        .animation = animation_create(ANIMATION_PARTICLE_EXPLOSION),
        .vframe = 0,
        .position = entity.type == ENTITY_SAPPER ? entity.position.to_ivec2() : cell_center(entity.cell).to_ivec2()
    });
}

uint32_t match_entity_get_max_energy(const MatchState& state, const Entity& entity) {
    if (!entity_is_unit(entity.type)) {
        return 0;
    }

    uint32_t max_energy = entity_get_data(entity.type).unit_data.max_energy;
    if (entity.type == ENTITY_DETECTIVE && match_player_has_upgrade(state, entity.player_id, UPGRADE_STAKEOUT)) {
        max_energy += STAKEOUT_ENERGY_BONUS;
    }
    return max_energy;
}

bool match_entity_has_detection(const MatchState& state, const Entity& entity) {
    if (entity.type == ENTITY_DETECTIVE && match_player_has_upgrade(state, entity.player_id, UPGRADE_PRIVATE_EYE)) {
        return true;
    }
    return entity_get_data(entity.type).has_detection;
}

int match_entity_get_damage(const MatchState& state, const Entity& entity) {
    if (!entity_is_unit(entity.type)) {
        return 0;
    }

    const EntityData& entity_data = entity_get_data(entity.type);
    int damage = entity_data.unit_data.damage;
    if (entity.type != ENTITY_CANNON && entity_data.unit_data.range_squared != 1 && damage != 0 && match_player_has_upgrade(state, entity.player_id, UPGRADE_BLACK_POWDER)) {
        damage += 1;
    }

    return damage;
}

int match_entity_get_armor(const MatchState& state, const Entity& entity) {
    int armor = entity_get_data(entity.type).armor;
    if (entity_is_unit(entity.type) && match_player_has_upgrade(state, entity.player_id, UPGRADE_HEAVY_ARMOR)) {
        armor += 1;
    }
    return armor;
}

int match_entity_get_accuracy(const MatchState& state, const Entity& entity) {
    if (!entity_is_unit(entity.type)) {
        return 0;
    }

    const EntityData& entity_data = entity_get_data(entity.type);
    int accuracy = entity_data.unit_data.accuracy;
    if (entity.type != ENTITY_CANNON && entity_data.unit_data.range_squared != 1 && accuracy != 0 && match_player_has_upgrade(state, entity.player_id, UPGRADE_IRON_SIGHTS)) {
        accuracy += 10;
    }

    return accuracy;
}

int match_entity_get_evasion(const MatchState& state, const Entity& entity) {
    if (!entity_is_unit(entity.type)) {
        return 0;
    }

    int evasion = entity_get_data(entity.type).unit_data.evasion;
    if (match_player_has_upgrade(state, entity.player_id, UPGRADE_LIGHT_ARMOR)) {
        evasion += 10;
    }
    return evasion;
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

void match_fog_update(MatchState& state, uint8_t player_team, ivec2 cell, int cell_size, int sight, bool has_detection, CellLayer cell_layer, bool increment) {
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
                            ivec2 frame = entity_get_animation_frame(entity);
                            if (entity.type == ENTITY_GOLDMINE && frame.x == 1) {
                                frame.x = 0;
                            }
                            state.remembered_entities[player_team][cell.id] = (RememberedEntity) {
                                .type = entity.type,
                                .frame = frame,
                                .cell = entity.cell,
                                .cell_size = entity_get_data(entity.type).cell_size,
                                .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLDMINE ? 0 : state.players[entity.player_id].recolor_id
                            };
                        }
                    } // End if cell value < cell empty
                } // End if !increment

                if (map_get_tile(state.map, line_cell).elevation > map_get_tile(state.map, line_start).elevation && cell_layer != CELL_LAYER_SKY) {
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

// FIRE

bool match_is_cell_on_fire(const MatchState& state, ivec2 cell) {
    return state.fire_cells[cell.x + (cell.y * state.map.width)] == 1;
}

bool match_is_cell_rect_on_fire(const MatchState& state, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state.fire_cells[x + (y * state.map.width)] == 1) {
                return true;
            }
        }
    }

    return false;
}

void match_set_cell_on_fire(MatchState& state, ivec2 cell, ivec2 source) {
    if (match_is_cell_on_fire(state, cell)) {
        return;
    }
    if (map_get_cell(state.map, CELL_LAYER_GROUND, cell).type == CELL_BLOCKED) {
        return;
    }
    if (map_get_tile(state.map, cell).sprite == SPRITE_TILE_WATER) {
        return;
    }
    if (ivec2::manhattan_distance(cell, source) > PROJECTILE_MOLOTOV_FIRE_SPREAD ||
        (cell.x == source.x && std::abs(cell.y - source.y) >= PROJECTILE_MOLOTOV_FIRE_SPREAD) ||
        (cell.y == source.y && std::abs(cell.x - source.x) >= PROJECTILE_MOLOTOV_FIRE_SPREAD)) {
        return;
    }
    state.fires.push_back((Fire) {
        .cell = cell,
        .source = source,
        .time_to_live = FIRE_TTL,
        .animation = animation_create(ANIMATION_FIRE_START)
    });
    state.fire_cells[cell.x + (cell.y * state.map.width)] = 1;
}
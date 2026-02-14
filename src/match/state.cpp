#include "state.h"

#include "core/logger.h"
#include "match/upgrade.h"
#include "match/lcg.h"
#include "render/render.h"
#include "profile/profile.h"

static const uint32_t MATCH_PLAYER_STARTING_GOLD = 50;
static const uint32_t MATCH_GOLDMINE_STARTING_GOLD = 7500;
static const uint32_t UNIT_ENERGY_REGEN_DURATION = 64;
static const uint32_t UNIT_REPAIR_RATE = 4;
static const uint32_t UNIT_IN_MINE_DURATION = 150;
static const uint32_t UNIT_MAX_GOLD_HELD = 5;
static const int UNIT_BLOCKED_DURATION = 30;
static const uint32_t BUILDING_FADE_DURATION = 300;
static const uint32_t ENTITY_BUNKER_FIRE_OFFSET = 10;
static const ivec2 BUNKER_PARTICLE_OFFSETS[4] = { ivec2(3, 23), ivec2(11, 26), ivec2(20, 25), ivec2(28, 23) };
static const ivec2 WAR_WAGON_DOWN_PARTICLE_OFFSETS[4] = { ivec2(14, 6), ivec2(17, 8), ivec2(21, 6), ivec2(24, 8) };
static const ivec2 WAR_WAGON_UP_PARTICLE_OFFSETS[4] = { ivec2(16, 20), ivec2(18, 22), ivec2(21, 20), ivec2(23, 22) };
static const ivec2 WAR_WAGON_RIGHT_PARTICLE_OFFSETS[4] = { ivec2(7, 18), ivec2(11, 19), ivec2(12, 20), ivec2(16, 18) };
static const int MINE_EXPLOSION_DAMAGE = 200;
static const fixed PROJECTILE_MOLOTOV_SPEED = fixed::from_int(8);
static const uint32_t FIRE_TTL = 30U * 60U;
static const uint32_t ENTITY_FIRE_DAMAGE_COOLDOWN = 8;
static const uint32_t MINE_ARM_DURATION = 16;
static const uint32_t MINE_PRIME_DURATION = 6 * 6;
static const uint32_t FOG_REVEAL_DURATION = 60;
static const fixed BLEED_SPEED_PERCENTAGE = fixed::from_int_and_raw_decimal(0, 192);
static const uint32_t MATCH_LOW_GOLD_THRESHOLD = 1000;

MatchState* match_init(int32_t lcg_seed, MatchPlayer players[MAX_PLAYERS], MatchInitMapParams map_params) {
    MatchState* state = new MatchState();

    // LCG seed
    #ifdef GOLD_RAND_SEED
        state->lcg_seed = GOLD_RAND_SEED;
    #endif
        state->lcg_seed = lcg_seed;
    log_info("Set random seed to %i", lcg_seed);

    // Players
    memcpy(state->players, players, sizeof(state->players));

    // Fog and detection
    const int map_width = map_params.type == MATCH_INIT_MAP_FROM_NOISE
        ? map_params.noise.noise->width
        : map_params.copy.map->width;
    const int map_height = map_params.type == MATCH_INIT_MAP_FROM_NOISE
        ? map_params.noise.noise->height
        : map_params.copy.map->height;
    for (uint8_t team = 0; team < MAX_PLAYERS; team++) {
        for (int index = 0; index < map_width * map_height; index++) {
            state->fog[team][index] = FOG_HIDDEN;
            state->detection[team][index] = 0;
        }
    }

    // Map
    if (map_params.type == MATCH_INIT_MAP_FROM_NOISE) {
        std::vector<ivec2> map_spawn_points;
        std::vector<ivec2> goldmine_cells;
        map_init_generate(state->map, map_params.noise.type, map_params.noise.noise, &lcg_seed, map_spawn_points, goldmine_cells);

        // Init goldmines
        for (ivec2 cell : goldmine_cells) {
            entity_goldmine_create(state, cell, MATCH_GOLDMINE_STARTING_GOLD);
        }

        // Init player spawns
        match_spawn_players(state, map_spawn_points);
    } else if (map_params.type == MATCH_INIT_MAP_FROM_COPY) {
        memcpy(&state->map, map_params.copy.map, sizeof(state->map));
    }
    map_calculate_unreachable_cells(state->map);
    map_init_regions(state->map);

    memset(state->padding, 0, sizeof(state->padding));
    memset(state->remembered_entity_count, 0, sizeof(state->remembered_entity_count));
    memset(state->remembered_entities, 0, sizeof(state->remembered_entities));
    memset(state->fire_cells, 0, sizeof(state->fire_cells));

    state->is_fog_dirty = false;

    return state;
}

void match_free(MatchState* state) {
    delete state;
}

void match_spawn_players(MatchState* state, const std::vector<ivec2>& map_spawn_points) {
    // Determine player spawns
    ivec2 player_spawns[MAX_PLAYERS];
    uint32_t team_player_count[MAX_PLAYERS];
    bool is_spawn_point_available[MAX_PLAYERS];
    memset(team_player_count, 0, sizeof(team_player_count));
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        player_spawns[player_id] = ivec2(-1, -1);
        team_player_count[state->players[player_id].team]++;
        is_spawn_point_available[player_id] = true;
    }

    uint32_t spawn_point_index = (uint32_t)(lcg_rand(&state->lcg_seed) % MAX_PLAYERS);
    while (true) {
        // Find the biggest team without a spawn point
        uint32_t biggest_team = MAX_PLAYERS;
        for (uint32_t team = 0; team < MAX_PLAYERS; team++) {
            if (team_player_count[team] == 0) {
                continue;
            }
            if (biggest_team == MAX_PLAYERS || team_player_count[team] > team_player_count[biggest_team]) {
                biggest_team = team;
            }
        }

        // If no team found, then exit
        if (biggest_team == MAX_PLAYERS) {
            break;
        }

        uint32_t team_spawn_point_index = spawn_point_index;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (!state->players[player_id].active || state->players[player_id].team != biggest_team) {
                continue;
            }

            while (!is_spawn_point_available[team_spawn_point_index]) {
                team_spawn_point_index = (team_spawn_point_index + 1) % MAX_PLAYERS;
            }

            player_spawns[player_id] = map_spawn_points[team_spawn_point_index];
            is_spawn_point_available[team_spawn_point_index] = false;
        }

        team_player_count[biggest_team] = 0;
        spawn_point_index = (spawn_point_index + 2) % MAX_PLAYERS;
    }

    #ifdef GOLD_DEBUG
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            GOLD_ASSERT(!(state->players[player_id].active && player_spawns[player_id].x == -1));
        }
    #endif

    // Init players
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state->players[player_id].gold = MATCH_PLAYER_STARTING_GOLD;
        state->players[player_id].gold_mined_total = 0;
        state->players[player_id].upgrades = 0;
        state->players[player_id].upgrades_in_progress = 0;

        if (state->players[player_id].active) {
            // Place town hall
            ivec2 town_hall_cell = map_get_player_town_hall_cell(state->map, player_spawns[player_id]);
            EntityId hall_id = entity_create(state, ENTITY_HALL, town_hall_cell, player_id);
            Entity& hall = state->entities.get_by_id(hall_id);
            const EntityData& hall_data = entity_get_data(hall.type);
            hall.health = hall_data.max_health;
            hall.mode = MODE_BUILDING_FINISHED;

            // Place miners
            Target goldmine_target = entity_target_nearest_goldmine(state, hall);
            GOLD_ASSERT(goldmine_target.type != TARGET_NONE);
            Entity& mine = state->entities.get_by_id(goldmine_target.id);
            for (int index = 0; index < 3; index++) {
                ivec2 exit_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, hall.cell, hall_data.cell_size, entity_get_data(ENTITY_MINER).cell_size, mine.cell, 0);
                entity_create(state, ENTITY_MINER, exit_cell, player_id);
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
                            if (!map_is_cell_rect_in_bounds(state->map, cell, 2) || map_is_cell_rect_occupied(state->map, CELL_LAYER_GROUND, cell, 2)) {
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
                            if (!map_is_cell_rect_in_bounds(state->map, cell, 2) || map_is_cell_rect_occupied(state->map, CELL_LAYER_GROUND, cell, 2)) {
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
                entity_create(state, ENTITY_WAGON, scout_cell, player_id);
            }
        }
    }
}

uint32_t match_get_player_population(const MatchState* state, uint8_t player_id) {
    uint32_t population = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id == player_id && entity_is_unit(entity.type) && entity.health != 0) {
            population += entity_get_data(entity.type).unit_data.population_cost;
        }
    }

    return population;
}

uint32_t match_get_player_max_population(const MatchState* state, uint8_t player_id) {
    uint32_t max_population = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id == player_id && (entity.type == ENTITY_HALL || entity.type == ENTITY_HOUSE) && entity.mode == MODE_BUILDING_FINISHED) {
            max_population += 10;
        }
    }

    return std::min(max_population, MATCH_MAX_POPULATION);
}

bool match_player_has_upgrade(const MatchState* state, uint8_t player_id, uint32_t upgrade) {
    return (state->players[player_id].upgrades & upgrade) == upgrade;
}

bool match_player_upgrade_is_available(const MatchState* state, uint8_t player_id, uint32_t upgrade) {
    return ((state->players[player_id].upgrades | state->players[player_id].upgrades_in_progress) & upgrade) == 0;
}

void match_grant_player_upgrade(MatchState* state, uint8_t player_id, uint32_t upgrade) {
    state->players[player_id].upgrades |= upgrade;

    // Grant detection to all detectives immediately, otherwise it will mess up the detection map
    if (upgrade == UPGRADE_PRIVATE_EYE) {
        const EntityData& entity_data = entity_get_data(ENTITY_DETECTIVE);
        for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
            const Entity& entity = state->entities[entity_index];
            if (entity.player_id != player_id || entity.type != ENTITY_DETECTIVE || entity.health == 0) {
                continue;
            }

            // De-increment the detective's vision without detection
            match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, false, entity_data.cell_layer, false);
            // Then re-increment it with detection
            match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, true, entity_data.cell_layer, true);
        }
    }
}

uint32_t match_get_miners_on_gold(const MatchState* state, EntityId goldmine_id, uint8_t player_id) {
    uint32_t miner_count = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& miner = state->entities[entity_index];
        if (miner.type == ENTITY_MINER && miner.player_id == player_id && miner.goldmine_id == goldmine_id) {
            miner_count++;
        }
    }
    return miner_count;
}

void match_handle_input(MatchState* state, const MatchInput& input) {
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
                target_index = state->entities.get_index_of(input.move.target_id);
                // Don't target a unit which is no longer selectable
                if (target_index != INDEX_INVALID && !entity_is_selectable(state->entities[target_index])) {
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
                    uint32_t entity_index = state->entities.get_index_of(input.move.entity_ids[id_index]);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(state->entities[entity_index])) {
                        continue;
                    }

                    ivec2 entity_cell = state->entities[entity_index].cell;
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
                uint32_t entity_index = state->entities.get_index_of(input.move.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_can_be_given_orders(state, state->entities[entity_index])) {
                    continue;
                }
                Entity& entity = state->entities[entity_index];

                // Set the unit's target
                Target target = target_none();
                target.type = (TargetType)input.type;
                if (target_index == INDEX_INVALID) {
                    target.cell = input.move.target_cell;
                    // If group-moving, use the group move cell, but only if the cell is valid
                    if (should_move_as_group) {
                        ivec2 group_move_cell = input.move.target_cell + (entity.cell - group_center);
                        if (map_is_cell_in_bounds(state->map, group_move_cell) && 
                                ivec2::manhattan_distance(group_move_cell, input.move.target_cell) <= 3 &&
                                map_get_tile(state->map, group_move_cell).elevation == map_get_tile(state->map, input.move.target_cell).elevation &&
                                    !(!map_is_cell_blocked(map_get_cell(state->map, CELL_LAYER_GROUND, input.move.target_cell)) && 
                                    map_is_cell_blocked(map_get_cell(state->map, CELL_LAYER_GROUND, group_move_cell)))) {
                            target.cell = group_move_cell;
                        }
                    }
                // Ensure that units do not target themselves
                } else if (input.move.target_id == input.move.entity_ids[id_index]) {
                    target = target_none();
                } else {
                    target.id = input.move.target_id;
                }

                if (!input.move.shift_command || (entity.target.type == TARGET_NONE && entity.target_queue.empty())) {
                    entity_clear_target_queue(state, entity);
                    entity_set_target(state, entity, target);
                } else if (entity.target_queue.is_full()) {
                    match_event_show_status(state, entity.player_id, "Command queue is full.");
                } else {
                    entity.target_queue.push(target);
                }
            } // End for each unit in move input
            break;
        } // End case MATCH_INPUT_MOVE
        case MATCH_INPUT_MOVE_MOLOTOV: {
            uint32_t thrower_index = INDEX_INVALID;
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t unit_index = state->entities.get_index_of(input.move.entity_ids[id_index]);
                if (unit_index == INDEX_INVALID || !entity_is_selectable(state->entities[unit_index])) {
                    continue;
                }
                if (state->entities[unit_index].energy < MOLOTOV_ENERGY_COST) {
                    continue;
                }
                if (thrower_index == INDEX_INVALID ||
                        ivec2::manhattan_distance(state->entities[unit_index].cell, input.move.target_cell) <
                        ivec2::manhattan_distance(state->entities[thrower_index].cell, input.move.target_cell)) {
                    thrower_index = unit_index;
                }
            }

            if (thrower_index == INDEX_INVALID) {
                return;
            }

            Target target = target_molotov(input.move.target_cell);

            if (!input.move.shift_command || 
                    (state->entities[thrower_index].target.type == TARGET_NONE && state->entities[thrower_index].target_queue.empty())) {
                entity_clear_target_queue(state, state->entities[thrower_index]);
                entity_set_target(state, state->entities[thrower_index], target);
            } else {
                state->entities[thrower_index].target_queue.push(target);
            }

            break;
        }
        case MATCH_INPUT_STOP:
        case MATCH_INPUT_DEFEND: {
            for (uint32_t index = 0; index < input.stop.entity_count; index++) {
                uint32_t entity_index = state->entities.get_index_of(input.stop.entity_ids[index]);
                if (entity_index == INDEX_INVALID || !entity_can_be_given_orders(state, state->entities[entity_index])) {
                    continue;
                }

                Entity& entity = state->entities[entity_index];
                entity.path.clear();
                entity_clear_target_queue(state, entity);
                entity_set_target(state, entity, target_none());
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
                uint32_t entity_index = state->entities.get_index_of(input.build.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_can_be_given_orders(state, state->entities[entity_index])) {
                    continue;
                }
                builder_ids.push_back(input.build.entity_ids[id_index]);
            }

            // If there's no viable builders, don't build
            if (builder_ids.empty()) {
                return;
            }

            // Get the lead builder
            EntityId lead_builder_id = match_get_nearest_builder(state, builder_ids, input.build.target_cell);
            Entity& lead_builder = state->entities.get_by_id(lead_builder_id);

            // Make sure the player has enough gold / energy to build
            const bool building_costs_energy = (building_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY;
            const bool can_afford_building = 
                (building_costs_energy && lead_builder.energy >= building_data.gold_cost) ||
                (!building_costs_energy && state->players[lead_builder.player_id].gold >= building_data.gold_cost);
            if (!can_afford_building) {
                match_event_show_status(state, lead_builder.player_id, building_costs_energy ? MATCH_UI_STATUS_NOT_ENOUGH_ENERGY : MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                break;
            }

            // Charge the cost of the building
            if (building_costs_energy) {
                lead_builder.energy -= building_data.gold_cost;
            } else {
                state->players[lead_builder.player_id].gold -= building_data.gold_cost;
                log_debug("PLAYER %u build gold %u", lead_builder.player_id, state->players[lead_builder.player_id].gold);
            }

            // Assign the lead builder's target
            int building_size = building_data.cell_size;
            Target build_target = target_build((TargetBuild) {
                .unit_cell = input.build.building_type == ENTITY_LANDMINE 
                                ? input.build.target_cell 
                                : get_nearest_cell_in_rect(
                                    lead_builder.cell, 
                                    input.build.target_cell, 
                                    building_size),
                .building_cell = input.build.target_cell,
                .building_type = (EntityType)input.build.building_type
            });
            if (!input.move.shift_command || (lead_builder.target.type == TARGET_NONE && lead_builder.target_queue.empty())) {
                entity_clear_target_queue(state, lead_builder);
                entity_set_target(state, lead_builder, build_target);
            } else {
                lead_builder.target_queue.push(build_target);
            }

            // Assign the helpers' target
            if (input.build.building_type != ENTITY_LANDMINE && !input.build.shift_command) {
                for (EntityId builder_id : builder_ids) {
                    if (builder_id == lead_builder_id) {
                        continue;
                    } 
                    Entity& builder = state->entities.get_by_id(builder_id);
                    entity_clear_target_queue(state, builder);
                    entity_set_target(state, builder, target_build_assist(lead_builder_id));
                }
            }
            break;
        }
        case MATCH_INPUT_BUILD_CANCEL: {
            uint32_t building_index = state->entities.get_index_of(input.build_cancel.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state->entities[building_index])) {
                break;
            }

            const EntityData& building_data = entity_get_data(state->entities[building_index].type);
            uint32_t gold_refund = building_data.gold_cost - (((uint32_t)state->entities[building_index].health * building_data.gold_cost) / (uint32_t)building_data.max_health);
            state->players[state->entities[building_index].player_id].gold += gold_refund;

            // Tell the builder to stop building
            for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
                if (state->entities[entity_index].target.type == TARGET_BUILD && state->entities[entity_index].target.id == input.build_cancel.building_id) {
                    Entity& builder = state->entities[entity_index];
                    const EntityData& builder_data = entity_get_data(builder.type);
                    builder.cell = builder.target.build.building_cell;
                    builder.position = entity_get_target_position(builder);
                    builder.target = target_none();
                    builder.mode = MODE_UNIT_IDLE;
                    entity_clear_target_queue(state, builder);
                    map_set_cell_rect(state->map, CELL_LAYER_GROUND, builder.cell, builder_data.cell_size, (Cell) {
                        .type = CELL_UNIT,
                        .id = state->entities.get_id_of(entity_index)
                    });
                    match_fog_update(state, state->players[builder.player_id].team, builder.cell, builder_data.cell_size, builder_data.sight, entity_has_detection(state, builder), builder_data.cell_layer, true);
                    break;
                }
            }

            // Destroy the building
            state->entities[building_index].health = 0;
            break;
        }
        case MATCH_INPUT_BUILDING_ENQUEUE: {
            // Choose the best building to enqueue out of the selection
            uint32_t building_index = INDEX_INVALID;
            uint32_t shortest_building_queue_duration = 0;
            for (int index = 0; index < input.building_enqueue.building_count; index++) {
                uint32_t candidate_index = state->entities.get_index_of(input.building_enqueue.building_ids[index]);
                if (candidate_index == INDEX_INVALID ||
                        !entity_is_selectable(state->entities[candidate_index]) ||
                        state->entities[candidate_index].queue.size() == BUILDING_QUEUE_MAX) {
                    continue;
                }

                uint32_t building_queue_duration = state->entities[candidate_index].timer;
                for (uint32_t queue_index = 1; queue_index < state->entities[candidate_index].queue.size(); queue_index++) {
                    building_queue_duration += building_queue_item_duration(state->entities[candidate_index].queue[queue_index]);
                }

                if (building_index == INDEX_INVALID || 
                        building_queue_duration < shortest_building_queue_duration) {
                    building_index = candidate_index;
                    shortest_building_queue_duration = building_queue_duration;
                }
            }
            if (building_index == INDEX_INVALID) {
                return;
            }

            Entity& building = state->entities[building_index];
            GOLD_ASSERT(building.mode == MODE_BUILDING_FINISHED);

            // Parse the building queue item
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

            // Make sure the player can afford the item
            if (state->players[building.player_id].gold < building_queue_item_cost(item)) {
                return;
            }

            // Reject this enqueue if the upgrade is already being researched
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE && !match_player_upgrade_is_available(state, building.player_id, item.upgrade)) {
                return;
            }

            // Mark upgrades as in-progress when we enqueue them
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state->players[building.player_id].upgrades_in_progress |= item.upgrade;
            }

            state->players[building.player_id].gold -= building_queue_item_cost(item);
            log_debug("PLAYER %u building queue item gold %u", building.player_id, state->players[building.player_id].gold);
            entity_building_enqueue(state, building, item);
            break;
        }
        case MATCH_INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state->entities.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state->entities[building_index])) {
                return;
            }

            Entity& building = state->entities[building_index];
            if (building.queue.empty()) {
                return;
            }

            uint32_t index = input.building_dequeue.index == BUILDING_DEQUEUE_POP_FRONT
                                    ? (uint32_t)building.queue.size() - 1
                                    : input.building_dequeue.index;
            if (index >= building.queue.size()) {
                return;
            }

            state->players[building.player_id].gold += building_queue_item_cost(building.queue[index]);
            if (building.queue[index].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state->players[building.player_id].upgrades_in_progress &= ~building.queue[index].upgrade;
            }

            if (index == 0) {
                entity_building_dequeue(state, building);
            } else {
                building.queue.remove_at_ordered(index);
            }
            break;
        }
        case MATCH_INPUT_RALLY: {
            for (uint32_t id_index = 0; id_index < input.rally.building_count; id_index++) {
                uint32_t building_index = state->entities.get_index_of(input.rally.building_ids[id_index]);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state->entities[building_index])) {
                    continue;
                }

                state->entities[building_index].rally_point = input.rally.rally_point;
            }
            break;
        }
        case MATCH_INPUT_SINGLE_UNLOAD: {
            uint32_t garrisoned_unit_index = state->entities.get_index_of(input.single_unload.entity_id);
            if (garrisoned_unit_index == INDEX_INVALID || 
                    state->entities[garrisoned_unit_index].health == 0 ||
                    state->entities[garrisoned_unit_index].garrison_id == ID_NULL) {
                return;
            }

            Entity& carrier = state->entities.get_by_id(state->entities[garrisoned_unit_index].garrison_id);
            entity_unload_unit(state, carrier, input.single_unload.entity_id);

            break;
        }
        case MATCH_INPUT_UNLOAD: {
            for (uint32_t id_index = 0; id_index < input.unload.carrier_count; id_index++) {
                uint32_t carrier_index = state->entities.get_index_of(input.unload.carrier_ids[id_index]);
                if (carrier_index == INDEX_INVALID || 
                        !entity_is_selectable(state->entities[carrier_index]) || 
                        state->entities[carrier_index].garrisoned_units.empty()) {
                    continue;
                }

                Entity& carrier = state->entities[carrier_index];
                entity_unload_unit(state, carrier, ENTITY_UNLOAD_ALL);
            }
            break;
        }
        case MATCH_INPUT_CAMO:
        case MATCH_INPUT_DECAMO: {
            for (uint32_t id_index = 0; id_index < input.camo.unit_count; id_index++) {
                uint32_t unit_index = state->entities.get_index_of(input.camo.unit_ids[id_index]);
                if (unit_index == INDEX_INVALID || !entity_is_selectable(state->entities[unit_index])) {
                    continue;
                }

                Entity& unit = state->entities[unit_index];
                if (input.type == MATCH_INPUT_CAMO && unit.energy < CAMO_ENERGY_COST) {
                    continue;
                }
                if (input.type == MATCH_INPUT_CAMO) {
                    unit.energy -= CAMO_ENERGY_COST;
                }
                entity_set_flag(unit, ENTITY_FLAG_INVISIBLE, input.type == MATCH_INPUT_CAMO);
                match_event_play_sound(state, input.type == MATCH_INPUT_CAMO ? SOUND_CAMO_ON : SOUND_CAMO_OFF, unit.position.to_ivec2());
                unit.energy_regen_timer = entity_get_energy_regen_duration(state, unit);
            }
            break;
        }
        case MATCH_INPUT_PATROL: {
            for (uint32_t id_index = 0; id_index < input.patrol.unit_count; id_index++) {
                EntityId entity_id = input.patrol.unit_ids[id_index];
                uint32_t entity_index = state->entities.get_index_of(entity_id);
                if (entity_index == INDEX_INVALID || 
                        !entity_can_be_given_orders(state, state->entities[entity_index])) {
                    continue;
                }

                entity_set_target(state, state->entities[entity_index], target_patrol(input.patrol.target_cell_a, input.patrol.target_cell_b));
            }
            break;
        }
        case MATCH_INPUT_TYPE_COUNT: {
            GOLD_ASSERT(false);
            break;
        }
    }
}

void match_update(MatchState* state) {
    ZoneScoped;
    
    // Update entities
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        entity_update(state, entity_index);
    }

    // Update particles
    {
        uint32_t particle_index = 0;
        while (particle_index < state->particles.size()) {
            animation_update(state->particles[particle_index].animation);

            // On particle finish, remove particle
            if (!animation_is_playing(state->particles[particle_index].animation)) {
                state->particles.remove_at_unordered(particle_index);
            } else {
                particle_index++;
            }
        }
    }

    // Update projectiles
    {
        uint32_t projectile_index = 0;
        while (projectile_index < state->projectiles.size()) {
            Projectile& projectile = state->projectiles[projectile_index];
            if (projectile.position.distance_to(projectile.target) <= PROJECTILE_MOLOTOV_SPEED) {
                // On projectile finish
                if (projectile.type == PROJECTILE_MOLOTOV) {
                    match_set_cell_on_fire(state, projectile.target.to_ivec2() / TILE_SIZE, projectile.target.to_ivec2() / TILE_SIZE);
                    // Check that it's actually on fire before playing the sound
                    if (match_is_cell_on_fire(state, projectile.target.to_ivec2() / TILE_SIZE)) {
                        match_event_play_sound(state, SOUND_MOLOTOV_IMPACT, projectile.target.to_ivec2());
                    }
                }
                state->projectiles.remove_at_unordered(projectile_index);
            } else {
                projectile.position += ((projectile.target - projectile.position) * PROJECTILE_MOLOTOV_SPEED / projectile.position.distance_to(projectile.target));
                projectile_index++;
            }
        }
    }

    // Update fire
    {
        uint32_t fire_index = 0;
        while (fire_index < state->fires.size()) {
            animation_update(state->fires[fire_index].animation);

            // Start animation finished, enter prolonged burn and spread more flames
            if (state->fires[fire_index].animation.name == ANIMATION_FIRE_START && !animation_is_playing(state->fires[fire_index].animation)) {
                state->fires[fire_index].animation = animation_create(ANIMATION_FIRE_BURN);
                uint32_t fire_elevation = map_get_tile(state->map, state->fires[fire_index].cell).elevation;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 child_cell = state->fires[fire_index].cell + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(state->map, child_cell)) {
                        continue;
                    }
                    if (map_get_tile(state->map, child_cell).elevation != fire_elevation && !map_is_tile_ramp(state->map, child_cell)) {
                        continue;
                    }
                    match_set_cell_on_fire(state, child_cell, state->fires[fire_index].source);
                }
            // Fire is in prolonged burn, count down time to live
            } else if (state->fires[fire_index].animation.name == ANIMATION_FIRE_BURN) {
                state->fires[fire_index].time_to_live--;
            }
            // Time to live is 0, extinguish fire
            if (state->fires[fire_index].time_to_live == 0) {
                state->fire_cells[(size_t)(state->fires[fire_index].cell.x + (state->fires[fire_index].cell.y * state->map.width))] = 0;
                state->fires.remove_at_unordered(fire_index);
            } else {
                fire_index++;
            }
        }
    }

    // Update fog reveals
    {
        uint32_t index = 0;
        while (index < state->fog_reveals.size()) {
            state->fog_reveals[index].timer--;
            if (state->fog_reveals[index].timer == 0) {
                match_fog_update(state, state->fog_reveals[index].team, state->fog_reveals[index].cell, state->fog_reveals[index].cell_size, state->fog_reveals[index].sight, false, CELL_LAYER_GROUND, false);
                state->fog_reveals.remove_at_unordered(index);
            } else {
                index++;
            }
        }
    }

    // Remove any dead entities
    {
        uint32_t entity_index = 0;
        while (entity_index < state->entities.size()) {
            if ((state->entities[entity_index].mode == MODE_UNIT_DEATH_FADE && !animation_is_playing(state->entities[entity_index].animation)) || 
                    (state->entities[entity_index].garrison_id != ID_NULL && state->entities[entity_index].health == 0) ||
                    (state->entities[entity_index].mode == MODE_BUILDING_DESTROYED && state->entities[entity_index].timer == 0)) {
                // Remove this entity's fog but only if they are not gold and not garrisoned
                if (state->entities[entity_index].player_id != PLAYER_NONE && state->entities[entity_index].garrison_id == ID_NULL) {
                    const EntityData& entity_data = entity_get_data(state->entities[entity_index].type);
                    // Decrementing non-detection fog only because entities should clear their detection when they begin death
                    match_fog_update(state, state->players[state->entities[entity_index].player_id].team, state->entities[entity_index].cell, entity_data.cell_size, entity_data.sight, false, entity_data.cell_layer, false);
                }
                // Remove this entity from garrisoned list if they are garrisoned
                if (state->entities[entity_index].garrison_id != ID_NULL) {
                    Entity& carrier = state->entities.get_by_id(state->entities[entity_index].garrison_id);
                    EntityId entity_id = state->entities.get_id_of(entity_index);
                    uint32_t garrison_index;
                    for (garrison_index = 0; garrison_index < carrier.garrisoned_units.size(); garrison_index++) {
                        if (carrier.garrisoned_units[garrison_index] == entity_id) {
                            break;
                        }
                    }
                    GOLD_ASSERT(garrison_index != carrier.garrisoned_units.size());
                    carrier.garrisoned_units.remove_at_ordered(garrison_index);
                    state->entities[entity_index].garrison_id = ID_NULL;
                }
                const EntityData& entity_data = entity_get_data(state->entities[entity_index].type);
                log_info("Removing entity %s ID %u player id %u", entity_data.name, state->entities.get_id_of(entity_index), state->entities[entity_index].player_id);
                state->entities.remove_at(entity_index);
            } else {
                entity_index++;
            }
        }
    }

    // Update remembered entities
    if (state->is_fog_dirty) {
        for (uint8_t team = 0; team < MAX_PLAYERS; team++) {
            // Remove any remembered entities (but only if the players can see that they should be removed)
            uint8_t remembered_entity_index = 0;
            while (remembered_entity_index < state->remembered_entity_count[team]) {
                const RememberedEntity& remembered_entity = state->remembered_entities[team][remembered_entity_index];
                uint32_t entity_index = state->entities.get_index_of(remembered_entity.entity_id);
                if ((entity_index == INDEX_INVALID || state->entities[entity_index].health == 0) && 
                        match_is_cell_rect_revealed(state, team, remembered_entity.cell, entity_get_data(remembered_entity.type).cell_size)) {
                    // Remove remembered entity
                    state->remembered_entities[team][remembered_entity_index] = state->remembered_entities[team][state->remembered_entity_count[team] - 1];
                    state->remembered_entity_count[team]--;
                } else {
                    remembered_entity_index++;
                }
            }
        }

        state->is_fog_dirty = false;
    }
}

EntityId match_find_entity(const MatchState* state, std::function<bool(const Entity& entity, EntityId entity_id)> filter) {
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        EntityId entity_id = state->entities.get_id_of(entity_index);
        if (filter(entity, entity_id)) {
            return entity_id;
        }
    }

    return ID_NULL;
}

EntityId match_find_best_entity(const MatchState* state, const MatchFindBestEntityParams& params) {
    uint32_t best_entity_index = INDEX_INVALID;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        EntityId entity_id = state->entities.get_id_of(entity_index);

        if (!params.filter(entity, entity_id)) {
            continue;
        }
        if (best_entity_index == INDEX_INVALID || params.compare(entity, state->entities[best_entity_index])) {
            best_entity_index = entity_index;
        }
    }

    if (best_entity_index == INDEX_INVALID) {
        return ID_NULL;
    }

    return state->entities.get_id_of(best_entity_index);
}

std::function<bool(const Entity& a, const Entity& b)> match_compare_closest_manhattan_distance_to(ivec2 cell) {
    return [cell](const Entity& a, const Entity& b) {
        return ivec2::manhattan_distance(a.cell, cell) < ivec2::manhattan_distance(b.cell, cell);
    };
}

EntityList match_find_entities(const MatchState* state, std::function<bool(const Entity& entity, EntityId entity_id)> filter) {
    EntityList entity_list;

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        EntityId entity_id = state->entities.get_id_of(entity_index);

        if (!filter(entity, entity_id)) {
            continue;
        }
        if (entity_list.is_full()) {
            log_warn("match_find_entities, entity_list is full.");
            break;
        }
        entity_list.push_back(entity_id);
    }

    return entity_list;
}

EntityId match_get_nearest_builder(const MatchState* state, const std::vector<EntityId>& builders, ivec2 cell) {
    EntityId nearest_unit_id; 
    int nearest_unit_dist = -1;
    for (EntityId id : builders) {
        int selection_dist = ivec2::manhattan_distance(cell, state->entities.get_by_id(id).cell);
        if (nearest_unit_dist == -1 || selection_dist < nearest_unit_dist) {
            nearest_unit_id = id;
            nearest_unit_dist = selection_dist;
        }
    }

    return nearest_unit_id;
}

bool match_is_target_invalid(const MatchState* state, const Target& target, uint8_t player_id) {
    if (!(target.type == TARGET_ENTITY || target.type == TARGET_ATTACK_ENTITY || target.type == TARGET_REPAIR || target.type == TARGET_BUILD_ASSIST)) {
        return false;
    }

    uint32_t target_index = state->entities.get_index_of(target.id);
    if (target_index == INDEX_INVALID) {
        return true;
    }

    if (state->entities[target_index].type == ENTITY_GOLDMINE) {
        return false;
    }
    
    if (target.type == TARGET_BUILD_ASSIST) {
        return state->entities[target_index].health == 0 || state->entities[target_index].target.type != TARGET_BUILD;
    }

    if (!entity_is_selectable(state->entities[target_index])) {
        return true;
    }

    if (target.type == TARGET_ATTACK_ENTITY && 
            entity_check_flag(state->entities[target_index], ENTITY_FLAG_INVISIBLE) && 
            !entity_is_visible_to_player(state, state->entities[target_index], player_id)) {
        return true;
    }

    return false;
}

bool match_player_has_buildings(const MatchState* state, uint8_t player_id) {
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id == player_id && 
                entity_is_building(entity.type) && 
                entity.type != ENTITY_LANDMINE &&
                entity.mode != MODE_BUILDING_DESTROYED) {
            return true;
        }
    }

    return false;
}

bool match_player_has_entities(const MatchState* state, uint8_t player_id) {
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id == player_id &&
                entity.type != ENTITY_LANDMINE &&
                entity.health != 0) {
            return true;
        }
    }

    return false;
}

uint32_t match_team_find_remembered_entity_index(const MatchState* state, uint8_t team, EntityId entity_id) {
    for (uint32_t index = 0; index < state->remembered_entity_count[team]; index++) {
        if (state->remembered_entities[team][index].entity_id == entity_id) {
            return index;
        }
    }

    return MATCH_ENTITY_NOT_REMEMBERED;
}

bool match_team_remembers_entity(const MatchState* state, uint8_t team, EntityId entity_id) {
    for (uint32_t index = 0; index < state->remembered_entity_count[team]; index++) {
        if (state->remembered_entities[team][index].entity_id == entity_id) {
            return true;
        }
    }

    return false;
}

// ENTITY

EntityId entity_create(MatchState* state, EntityType type, ivec2 cell, uint8_t player_id) {
    const EntityData& entity_data = entity_get_data(type);

    if (state->entities.is_full()) {
        log_warn("entity_create cannot create entity: entities is full");
        return ID_NULL;
    }

    Entity entity;
    entity.type = type;
    entity.mode = entity_is_unit(type) ? MODE_UNIT_IDLE : MODE_BUILDING_IN_PROGRESS;
    entity.player_id = player_id;
    memset(entity.padding, 0, sizeof(entity.padding));
    entity.flags = 0;

    entity.cell = cell;
    entity.position = entity_is_unit(type) 
                        ? entity_get_target_position(entity)
                        : fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = entity_is_unit(type) || entity.type == ENTITY_LANDMINE
                        ? entity_data.max_health
                        : (entity_data.max_health / 10);
    entity.energy = entity_is_unit(type) ? entity_data.unit_data.max_energy / 4 : 0;
    entity.target = target_none();
    entity.pathfind_attempts = 0;
    entity.timer = entity_is_unit(type) || entity.type == ENTITY_LANDMINE
                        ? 0
                        : (uint32_t)(entity_data.max_health - entity.health);
    entity.attack_move_cell = ivec2(-1, -1);
    entity.rally_point = ivec2(-1, -1);

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = 0;
    entity.goldmine_id = ID_NULL;

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;
    entity.fire_damage_timer = 0;
    entity.energy_regen_timer = 0;
    entity.bleed_timer = 0;
    entity.bleed_damage_timer = 0;
    entity.bleed_animation = animation_create(ANIMATION_PARTICLE_BLEED);

    if (entity.type == ENTITY_LANDMINE) {
        entity.timer = MINE_ARM_DURATION;
        entity.mode = MODE_MINE_ARM;
    } else if (entity.type == ENTITY_SOLDIER) {
        entity_set_flag(entity, ENTITY_FLAG_CHARGED, true);
    }

    EntityId id = state->entities.push_back(entity);
    map_set_cell_rect(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
        .type = entity_is_unit(type) ? CELL_UNIT : CELL_BUILDING,
        .id = id
    });
    match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, true);

    if (entity_is_building(type)) {
        map_calculate_unreachable_cells(state->map);
    }

    log_info("Created entity %s ID %u player %u cell <%i, %i>", entity_data.name, id, player_id, cell.x, cell.y);

    return id;
}

EntityId entity_goldmine_create(MatchState* state, ivec2 cell, uint32_t gold_left) {
    if (state->entities.is_full()) {
        log_warn("entity_create cannot create entity: entities is full");
        return ID_NULL;
    }

    Entity entity;
    entity.type = ENTITY_GOLDMINE;
    entity.mode = gold_left != 0 
        ? MODE_GOLDMINE
        : MODE_GOLDMINE_COLLAPSED;
    entity.player_id = PLAYER_NONE;
    memset(entity.padding, 0, sizeof(entity.padding));
    entity.flags = 0;

    entity.cell = cell;
    entity.position = fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = 0;
    entity.energy = 0;
    entity.target = target_none();
    entity.attack_move_cell = ivec2(-1, -1);
    entity.rally_point = ivec2(-1, -1);
    entity.pathfind_attempts = 0;
    entity.timer = 0;
    entity.animation = animation_create(ANIMATION_UNIT_IDLE);
    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = gold_left;
    entity.goldmine_id = ID_NULL;

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;
    entity.fire_damage_timer = 0;
    entity.energy_regen_timer = 0;
    entity.bleed_timer = 0;
    entity.bleed_damage_timer = 0;
    entity.bleed_animation = animation_create(ANIMATION_PARTICLE_BLEED);

    EntityId id = state->entities.push_back(entity);
    map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_get_data(entity.type).cell_size, (Cell) {
        .type = CELL_GOLDMINE,
        .id = id
    });

    return id;
}

void entity_update(MatchState* state, uint32_t entity_index) {
    ZoneScoped;

    EntityId entity_id = state->entities.get_id_of(entity_index);
    Entity& entity = state->entities[entity_index];
    const EntityData& entity_data = entity_get_data(entity.type);

    // Check if entity should die
    if (entity_should_die(entity)) {
        if (entity_is_unit(entity.type)) {
            entity.goldmine_id = ID_NULL;
            entity.mode = entity.type == ENTITY_BALLOON ? MODE_UNIT_BALLOON_DEATH_START : MODE_UNIT_DEATH;
            entity.animation = animation_create(entity_get_expected_animation(entity));
            entity.bleed_timer = 0;
            entity.bleed_damage_timer = 0;
            entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, false);

            // Clear the unit's target
            // This is so that we refund any buildings
            if (entity.target.type == TARGET_BUILD) {
                entity_refund_target_build(state, entity, entity.target);
            }
            entity_clear_target_queue(state, entity);

            if (entity_has_detection(state, entity) && entity.garrison_id == ID_NULL) {
                // Remove this units detection
                match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, true, entity_data.cell_layer, false);
                // Then re-increment fog with non-detection so that we can still see death fade
                match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, false, entity_data.cell_layer, true);
            }
        } else {
            entity.mode = MODE_BUILDING_DESTROYED;
            entity.timer = BUILDING_FADE_DURATION;

            // Make sure that any building upgrades are marked as re-available
            for (uint32_t building_queue_index = 0; building_queue_index < entity.queue.size(); building_queue_index++) {
                const BuildingQueueItem& item = entity.queue[building_queue_index];
                if (item.type == BUILDING_QUEUE_ITEM_UPGRADE) {
                    state->players[entity.player_id].upgrades_in_progress &= ~item.upgrade;
                }
            }
            entity.queue.clear();

            if (entity.type == ENTITY_LANDMINE) {
                map_set_cell_rect(state->map, CELL_LAYER_UNDERGROUND, entity.cell, entity_data.cell_size, (Cell) {
                    .type = CELL_EMPTY, .id = ID_NULL
                });
            } else {
                // Set building cells to empty
                // but don't override the miner cell
                for (int y = entity.cell.y; y < entity.cell.y + entity_data.cell_size; y++) {
                    for (int x = entity.cell.x; x < entity.cell.x + entity_data.cell_size; x++) {
                        ivec2 cell = ivec2(x, y);
                        if (map_get_cell(state->map, CELL_LAYER_GROUND, cell).id == entity_id) {
                            map_set_cell(state->map, CELL_LAYER_GROUND, cell, (Cell) {
                                .type = CELL_EMPTY,
                                .id = ID_NULL
                            });
                        }
                    }
                }
                map_calculate_unreachable_cells(state->map);
            }
        }

        match_event_play_sound(state, entity_data.death_sound, entity.position.to_ivec2());

        if (!entity_is_unit(entity.type)) {
            // If it's a building, release garrisoned units
            // If it's a unit, it will release them once its death animation is over
            entity_release_garrisoned_units_on_death(state, entity);

            // Also if it's a building, turn off the burning flag
            entity_set_flag(entity, ENTITY_FLAG_ON_FIRE, false);
        }

        // Check if player has lost
        if (entity.type != ENTITY_LANDMINE &&
                ((entity_is_building(entity.type) && 
                    !match_player_has_buildings(state, entity.player_id)) ||
                (!entity_is_building(entity.type) && 
                    !match_player_has_entities(state, entity.player_id)))) {
            state->players[entity.player_id].active = false;
            state->events.push((MatchEvent) {
                .type = MATCH_EVENT_PLAYER_DEFEATED,
                .player_defeated = (MatchEventPlayerDefeated) {
                    .player_id = entity.player_id
                }
            });
        }
    }
    // End if entity should die

    bool update_finished = false;
    fixed movement_left = entity_get_speed(state, entity); 
    if (entity.bleed_timer != 0) {
        movement_left = movement_left * BLEED_SPEED_PERCENTAGE;
    }
    while (!update_finished) {
        switch (entity.mode) {
            case MODE_UNIT_IDLE: {
                // Do nothing if unit is garrisoned
                if (entity.garrison_id != ID_NULL) {
                    const Entity& carrier = state->entities.get_by_id(entity.garrison_id);
                    if (!(carrier.type == ENTITY_BUNKER || 
                            (carrier.type == ENTITY_WAGON && match_player_has_upgrade(state, entity.player_id, UPGRADE_WAR_WAGON)))) {
                        update_finished = true;
                        break;
                    }
                }

                // If unit is idle, check target queue
                if (entity.target.type == TARGET_NONE && !entity.target_queue.empty()) {
                    entity_set_target(state, entity, entity.target_queue[0]);
                    entity.target_queue.pop();
                }

                // If unit is idle, try to find a nearby target
                if ((entity.target.type == TARGET_NONE || entity.target.type == TARGET_PATROL) && 
                        entity.type != ENTITY_MINER && 
                        !(entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) &&
                        entity_data.unit_data.damage != 0) {
                    Target nearest_enemy_target = entity_target_nearest_enemy(state, entity); 
                    if (nearest_enemy_target.type != TARGET_NONE) {
                        entity.target = nearest_enemy_target;
                    }
                }

                // If unit is attacking, check if there's a higher priority target nearby
                if (entity.target.type == TARGET_ATTACK_ENTITY && !entity_check_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY)) {
                    Target attack_target = entity_target_nearest_enemy(state, entity);
                    if (attack_target.type == TARGET_ATTACK_ENTITY && attack_target.id != entity.target.id) {
                        const Entity& attack_target_entity = state->entities.get_by_id(attack_target.id);

                        uint32_t entity_target_index = state->entities.get_index_of(entity.target.id);

                            // If entity target is invalid, then go with the new target
                        if ( entity_target_index == INDEX_INVALID || 
                                // If new target is higher priority than old target, then go with the new target
                                entity_get_target_attack_priority(entity, attack_target_entity) > 
                                entity_get_target_attack_priority(entity, state->entities[entity_target_index]) ||
                                // For sappers, if the new target is closer, then go with the new target
                                (entity.type == ENTITY_SAPPER && 
                                    ivec2::manhattan_distance(entity.cell, state->entities[entity_target_index].cell) <
                                    ivec2::manhattan_distance(entity.cell, attack_target_entity.cell))) {
                            entity.target = attack_target;
                        }
                    }
                }

                // If entity has a target attack cell, then move to it
                if (entity.target.type == TARGET_NONE && entity.attack_move_cell.x != -1) {
                    entity.target = target_attack_cell(entity.attack_move_cell);
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

                // Set patrol target cell
                if (entity.target.type == TARGET_PATROL) {
                    int distance_to_a = ivec2::manhattan_distance(entity.cell, entity.target.patrol.cell_a);
                    int distance_to_b = ivec2::manhattan_distance(entity.cell, entity.target.patrol.cell_b);
                    entity.target.cell = distance_to_a < distance_to_b
                        ? entity.target.patrol.cell_b
                        : entity.target.patrol.cell_a;
                }

                if (entity_is_target_invalid(state, entity)) {
                    entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, false);
                    entity.target = target_none();
                    update_finished = true;
                    break;
                }

                // If mining, cache the current target's gold mine so that this unit returns to it later
                if (entity.type == ENTITY_MINER && entity.target.type == TARGET_ENTITY) {
                    const Entity& target = state->entities.get_by_id(entity.target.id);
                    if (target.type == ENTITY_GOLDMINE && target.gold_held != 0) {
                        entity.goldmine_id = entity.target.id;
                    }
                }
                if (entity.goldmine_id != ID_NULL) {
                    const Entity& mine = state->entities.get_by_id(entity.goldmine_id);
                    if (mine.gold_held == 0) {
                        entity.goldmine_id = ID_NULL;
                    }
                }

                if (entity_has_reached_target(state, entity)) {
                    entity.mode = MODE_UNIT_MOVE_FINISHED;
                    break;
                }

                // Don't move if hold position or if garrisoned
                // We have to check garrisoned a second time here because previously we had allowed bunkered units to shoot
                if (entity_check_flag(entity, ENTITY_FLAG_HOLD_POSITION) || entity.garrison_id != ID_NULL) {
                    // Throw away targets if garrisoned. This prevents bunkered units from fixated on a target they can no longer reach
                    if (entity.garrison_id != ID_NULL) {
                        entity.target = target_none();
                    }
                    update_finished = true;
                    break;
                }

                // Pathfind
                {
                    MapPath mine_exit_path;
                    entity_get_mining_path_to_avoid(state, entity, &mine_exit_path);

                    uint32_t pathfind_options = 0;
                    if (entity_is_mining(state, entity)) {
                        pathfind_options |= MAP_OPTION_IGNORE_MINERS;
                        pathfind_options |= MAP_OPTION_NO_REGION_PATH;
                    }
                    if (entity.target.type == TARGET_CELL && entity.pathfind_attempts == 0) {
                        pathfind_options |= MAP_OPTION_ALLOW_PATH_SQUIRRELING;
                    }
                    map_pathfind(state->map, entity_data.cell_layer, entity.cell, entity_get_target_cell(state, entity), entity_data.cell_size, pathfind_options, &mine_exit_path, &entity.path);
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
                            entity_refund_target_build(state, entity, entity.target);
                        }
                        entity.attack_move_cell = ivec2(-1, -1);
                        entity.target = target_none();
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
                        entity.direction = enum_from_ivec2_direction(entity.path.back() - entity.cell);

                        // Miner - detect traffic jam and try to walk around entity
                        if (entity_is_mining(state, entity) &&
                                map_is_cell_rect_occupied(state->map, entity_data.cell_layer, entity.path.back(), entity_data.cell_size, entity.cell, 0) &&
                                entity_is_blocker_walking_towards_entity(state, entity)) {

                            uint32_t target_index = state->entities.get_index_of(entity.target.id);
                            if (target_index != INDEX_INVALID) {
                                const Entity& target = state->entities[target_index];
                                int target_size = entity_get_data(target.type).cell_size;

                                ivec2 target_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, target.cell, target_size, 0);

                                // Pathfind without ignoring miners to see if we can walk around
                                MapPath mine_exit_path;
                                entity_get_mining_path_to_avoid(state, entity, &mine_exit_path);
                                map_pathfind(state->map, entity_data.cell_layer, entity.cell, target_cell, entity_data.cell_size, MAP_OPTION_NO_REGION_PATH, &mine_exit_path, &entity.path);

                                // If no path was generated, then just consider ourselves blocked
                                if (entity.path.empty()) {
                                    path_is_blocked = true;
                                    // breaks out of while movement left
                                    break;
                                }

                                // Otherwise, orient towards the new path and try to keep walking
                                // The code below this block should double-check that path.back() is not blocked
                                entity.direction = enum_from_ivec2_direction(entity.path.back() - entity.cell);
                            }
                        }

                        if (map_is_cell_rect_occupied(state->map, entity_data.cell_layer, entity.path.back(), entity_data.cell_size, entity.cell, 0)) {
                            path_is_blocked = true;
                            // breaks out of while movement left
                            break;
                        }

                        if (map_is_cell_rect_equal_to(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, entity_id)) {
                            map_set_cell_rect(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY,
                                .id = ID_NULL
                            });
                        }
                        match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, false);
                        entity.cell = entity.path.back();
                        map_set_cell_rect(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                            .type = entity_is_mining(state, entity) ? CELL_MINER : CELL_UNIT,
                            .id = entity_id
                        });
                        match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, true);
                        entity.path.pop_back();
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
                        if (entity_data.cell_layer == CELL_LAYER_GROUND) {
                            for (uint32_t mine_index = 0; mine_index < state->entities.size(); mine_index++) {
                                Entity& mine = state->entities[mine_index];
                                if (mine.type != ENTITY_LANDMINE || mine.health == 0 || mine.mode != MODE_BUILDING_FINISHED || 
                                        state->players[mine.player_id].team == state->players[entity.player_id].team ||
                                        std::abs(entity.cell.x - mine.cell.x) > 1 || std::abs(entity.cell.y - mine.cell.y) > 1) {
                                    continue;
                                }
                                mine.animation = animation_create(ANIMATION_MINE_PRIME);
                                mine.timer = MINE_PRIME_DURATION;
                                mine.mode = MODE_MINE_PRIME;
                                entity_set_flag(mine, ENTITY_FLAG_INVISIBLE, false);
                            }
                        }
                        if (entity.target.type == TARGET_ATTACK_CELL || entity.target.type == TARGET_PATROL) {
                            Target attack_target = entity_target_nearest_enemy(state, entity);
                            if (attack_target.type != TARGET_NONE) {
                                entity.target = attack_target;
                                entity.path.clear();
                                entity.mode = entity_has_reached_target(state, entity) ? MODE_UNIT_MOVE_FINISHED : MODE_UNIT_IDLE;
                                // breaks out of while movement left > 0
                                break;
                            }
                        }
                        if (entity_is_target_invalid(state, entity)) {
                            entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, false);
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = target_none();
                            entity.path.clear();
                            break;
                        }
                        if (entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_MOVE_FINISHED;
                            entity.path.clear();
                            // break out of while movement left
                            break;
                        }
                        // If our path is no longer close to the target entity, then clear path and go into idle to trigger a repath
                        if (!entity.path.empty() && entity.path.size() < 8 &&
                                (entity.target.type == TARGET_ENTITY || entity.target.type == TARGET_ATTACK_ENTITY)) {
                            ivec2 target_cell = entity_get_target_cell(state, entity);
                            if (ivec2::manhattan_distance(entity.path[0], target_cell) > 8) {
                                entity.mode = MODE_UNIT_IDLE;
                                entity.path.clear();
                                // break out of while movement left
                                break;
                            }
                        }
                        if (entity.path.empty()) {
                            entity.mode = MODE_UNIT_IDLE;
                            // break out of while movement left
                            break;
                        }
                    }
                } // End while movement left

                if (path_is_blocked) {
                    entity.mode = MODE_UNIT_BLOCKED;
                    entity.timer = entity_is_mining(state, entity) ? 10 : UNIT_BLOCKED_DURATION;
                }

                update_finished = entity.mode != MODE_UNIT_MOVE_FINISHED;
                break;
            }
            case MODE_UNIT_MOVE_FINISHED: {
                switch (entity.target.type) {
                    case TARGET_NONE:
                    case TARGET_ATTACK_CELL:
                    case TARGET_CELL:
                    case TARGET_PATROL: {
                        entity.target = target_none();
                        entity.attack_move_cell = ivec2(-1, -1);
                        entity.mode = MODE_UNIT_IDLE;
                        break;
                    }
                    case TARGET_UNLOAD: {
                        if (!entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        entity_unload_unit(state, entity, ENTITY_UNLOAD_ALL);
                        entity.mode = MODE_UNIT_IDLE;
                        entity.target = target_none();
                        break;
                    }
                    case TARGET_MOLOTOV: {
                        if (!entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        if (entity.energy < MOLOTOV_ENERGY_COST) {
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = target_none();
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
                                if ((cell != entity.cell && map_get_cell(state->map, CELL_LAYER_GROUND, cell).type != CELL_EMPTY) ||
                                        map_get_cell(state->map, CELL_LAYER_UNDERGROUND, cell).type != CELL_EMPTY) {
                                    can_build = false;
                                }
                            }
                        }
                        if (!can_build) {
                            match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_CANT_BUILD);
                            entity_refund_target_build(state, entity, entity.target);
                            entity.target = target_none();
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        if (entity.target.build.building_type == ENTITY_LANDMINE) {
                            entity_create(state, entity.target.build.building_type, entity.target.build.building_cell, entity.player_id);
                            match_event_play_sound(state, SOUND_MINE_INSERT, cell_center(entity.target.build.building_cell).to_ivec2());

                            entity.direction = enum_direction_to_rect(entity.cell, entity.target.build.building_cell, building_data.cell_size);
                            entity.target = target_none();
                            entity.mode = MODE_UNIT_PYRO_THROW;
                            entity.animation = animation_create(ANIMATION_UNIT_ATTACK);
                        } else {
                            map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY, .id = ID_NULL
                            });
                            match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, false);
                            EntityId building_id = entity_create(state, entity.target.build.building_type, entity.target.build.building_cell, entity.player_id);
                            if (building_id != ID_NULL) {
                                entity.target.id = building_id;
                                entity.mode = MODE_UNIT_BUILD;
                                entity.timer = UNIT_BUILD_TICK_DURATION;

                                state->events.push((MatchEvent) {
                                    .type = MATCH_EVENT_SELECTION_HANDOFF,
                                    .selection_handoff = (MatchEventSelectionHandoff) {
                                        .player_id = entity.player_id,
                                        .to_deselect = entity_id,
                                        .to_select = entity.target.id
                                    }
                                });
                            } else {
                                entity.target = target_none();
                                match_event_show_status(state, entity.player_id, "Cannot create building. Entity limit reached.");
                            }
                        }

                        break;
                    }
                    case TARGET_BUILD_ASSIST: {
                        if (entity_is_target_invalid(state, entity)) {
                            entity.target = target_none();
                            entity.mode = MODE_UNIT_IDLE;
                        }

                        Entity& builder = state->entities.get_by_id(entity.target.id);
                        if (builder.mode == MODE_UNIT_BUILD) {
                            entity.target = target_repair(builder.target.id);
                            entity.mode = MODE_UNIT_BUILD_ASSIST;
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            entity.direction = enum_direction_to_rect(entity.cell, builder.target.build.building_cell, entity_get_data(builder.target.build.building_type).cell_size);
                        }
                        break;
                    }
                    case TARGET_REPAIR:
                    case TARGET_ENTITY:
                    case TARGET_ATTACK_ENTITY: {
                        if (entity_is_target_invalid(state, entity)) {
                            entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, false);
                            entity.target = target_none();
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        if (!entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        Entity& target = state->entities.get_by_id(entity.target.id);
                        const EntityData& target_data = entity_get_data(target.type);

                        // Sapper explosion
                        if (entity.target.type == TARGET_ATTACK_ENTITY && entity.type == ENTITY_SAPPER && target_data.cell_layer != CELL_LAYER_SKY) {
                            entity_explode(state, entity_id);
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
                            if (target_data.cell_layer == CELL_LAYER_SKY && (entity_get_range_squared(state, entity) == 1 || entity.type == ENTITY_CANNON)) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            // Check min range
                            bool attack_with_bayonets = false;
                            if (entity_is_target_within_min_range(entity, target)) {
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
                                Entity& carrier = state->entities.get_by_id(entity.garrison_id);
                                // Don't attack during bunker cooldown or if this is a melee unit
                                if (carrier.cooldown_timer != 0 || entity_get_range_squared(state, entity) == 1) {
                                    update_finished = true;
                                    break;
                                }

                                carrier.cooldown_timer = ENTITY_BUNKER_FIRE_OFFSET;
                            }

                            // Begin attack windup
                            entity.direction = enum_direction_to_rect(entity.cell, target.cell, target_data.cell_size);
                            if (entity.type == ENTITY_SOLDIER && !attack_with_bayonets) {
                                entity.mode = MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP;
                            } else {
                                entity.mode = MODE_UNIT_ATTACK_WINDUP;
                            }
                            update_finished = true;
                            break;
                        }

                        // Return gold
                        if (entity.type == ENTITY_MINER && target.type == ENTITY_HALL && 
                                target.mode == MODE_BUILDING_FINISHED && entity.player_id == target.player_id &&
                                entity.gold_held != 0 && entity.target.type != TARGET_REPAIR) {
                            state->players[entity.player_id].gold += entity.gold_held;
                            state->players[entity.player_id].gold_mined_total += entity.gold_held;
                            entity.gold_held = 0;

                            // First clear entity's target
                            entity.target = target_none();
                            // Then try to set its target based on the gold mine it just visited
                            if (entity.goldmine_id != ID_NULL) {
                                // It's safe to do this because gold mines never get removed from the array
                                const Entity& gold_mine = state->entities.get_by_id(entity.goldmine_id);
                                if (gold_mine.gold_held != 0) {
                                    entity.target = target_entity(entity.goldmine_id);
                                }
                            // If it doesn't have a last visited gold mine, then find a mine to visit
                            } else {
                                entity.target = entity_target_nearest_goldmine(state, entity);
                            }

                            entity.mode = MODE_UNIT_IDLE;
                            update_finished = true;
                            break;
                        }
                        // End return gold

                        // Enter mine
                        if (entity.type == ENTITY_MINER && target.type == ENTITY_GOLDMINE && target.gold_held > 0) {
                            if (entity.gold_held != 0) {
                                entity.target = entity_target_nearest_hall(state, entity);
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            if (target.garrisoned_units.size() + entity_data.garrison_size <= target_data.garrison_capacity) {
                                target.garrisoned_units.push_back(entity_id);
                                entity.garrison_id = entity.target.id;
                                entity.mode = MODE_UNIT_IN_MINE;
                                entity.timer = UNIT_IN_MINE_DURATION;
                                entity.target = target_none();
                                entity.gold_held = std::min(UNIT_MAX_GOLD_HELD, target.gold_held);
                                target.gold_held -= entity.gold_held;
                                map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                    .type = CELL_EMPTY, .id = ID_NULL
                                });

                                if (target.gold_held < MATCH_LOW_GOLD_THRESHOLD && target.gold_held + entity.gold_held >= MATCH_LOW_GOLD_THRESHOLD) {
                                    match_event_alert(state, MATCH_ALERT_TYPE_MINE_RUNNING_LOW, entity.player_id, target.cell, target_data.cell_size);
                                    match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_MINE_RUNNING_LOW);
                                }
                            }

                            update_finished = true;
                            break;
                        }

                        // Garrison
                        if (entity.player_id == target.player_id && entity_data.garrison_size != ENTITY_CANNOT_GARRISON && 
                                entity.target.type != TARGET_REPAIR && (entity_is_unit(target.type) || target.mode == MODE_BUILDING_FINISHED) &&
                                entity_get_garrisoned_occupancy(state, target) + entity_data.garrison_size <= target_data.garrison_capacity) {
                            target.garrisoned_units.push_back(entity_id);
                            entity.garrison_id = entity.target.id;
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = target_none();
                            map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                                .type = CELL_EMPTY, .id = ID_NULL
                            });
                            match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, false);
                            match_event_play_sound(state, SOUND_GARRISON_IN, target.position.to_ivec2());
                            update_finished = true;
                            break;
                        }

                        // Begin repair
                        if (entity_is_building(target.type) && entity.type == ENTITY_MINER && 
                                state->players[entity.player_id].team == state->players[target.player_id].team &&
                                entity_is_building(target.type) && target.health < target_data.max_health) {
                            entity.mode = target.mode == MODE_BUILDING_IN_PROGRESS ? MODE_UNIT_BUILD_ASSIST : MODE_UNIT_REPAIR;
                            entity.direction = enum_direction_to_rect(entity.cell, target.cell, target_data.cell_size);
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            break;
                        }

                        entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, false);
                        entity.mode = MODE_UNIT_IDLE;
                        entity.target = target_none();
                        update_finished = true;
                        break;
                    }
                    case TARGET_TYPE_COUNT:
                        GOLD_ASSERT(false);
                        break;
                }

                update_finished = update_finished || !(entity.mode == MODE_UNIT_MOVE && movement_left.raw_value > 0);
                break;
            }
            case MODE_UNIT_BUILD: {
                // This code handles the case where the building is destroyed while the unit is building it
                uint32_t building_index = state->entities.get_index_of(entity.target.id);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state->entities[building_index]) || state->entities[building_index].mode != MODE_BUILDING_IN_PROGRESS) {
                    entity_stop_building(state, entity_id);
                    update_finished = true;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    // Building tick
                    Entity& building = state->entities[building_index];

                    building.health++;
                    building.timer--;
                    if (building.timer == 0) {
                        entity_building_finish(state, entity.target.id);
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
                if (entity_is_target_invalid(state, entity)) {
                    entity.target = target_none();
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }

                Entity& target = state->entities.get_by_id(entity.target.id);
                const EntityData& target_data = entity_get_data(target.type);
                int target_max_health = target_data.max_health;
                if ((entity.mode == MODE_UNIT_REPAIR && target.health == target_max_health) || 
                        (entity.mode == MODE_UNIT_BUILD_ASSIST && target.mode == MODE_BUILDING_FINISHED) || 
                        state->players[entity.player_id].gold == 0) {
                    entity.target = target_none();
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
                        state->players[entity.player_id].gold--;
                        target.health_regen_timer = 0;
                    }
                    if (target.health > target_max_health / 4 && !match_is_cell_rect_on_fire(state, target.cell, target_data.cell_size)) {
                        entity_set_flag(target, ENTITY_FLAG_ON_FIRE, false);
                    }
                    if (entity.mode == MODE_UNIT_BUILD_ASSIST && target.timer == 0) {
                        entity_building_finish(state, entity.target.id);
                    } else if (entity.mode == MODE_UNIT_REPAIR && target.health == target_max_health) {
                        entity.target = target_none();
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
                    Entity& mine = state->entities.get_by_id(entity.garrison_id);

                    Target nearest_hall_target = entity_target_nearest_hall(state, entity);
                    Target entity_next_target = entity.goldmine_id == ID_NULL 
                        ? entity.target 
                        : nearest_hall_target;
                    uint32_t target_index = entity_next_target.type == TARGET_ENTITY ? state->entities.get_index_of(entity_next_target.id) : INDEX_INVALID;
                    ivec2 rally_cell; 
                    if (entity_next_target.type == TARGET_NONE) {
                        rally_cell = mine.cell + ivec2(1, mine_data.cell_size);
                    } else if (target_index != INDEX_INVALID && state->entities[target_index].type == ENTITY_HALL && entity.goldmine_id != ID_NULL) {
                        const EntityData& hall_data = entity_get_data(ENTITY_HALL);
                        rally_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, mine.cell + ivec2(1, 1), 1, state->entities[target_index].cell, hall_data.cell_size, MAP_OPTION_IGNORE_MINERS);
                    } else {
                        rally_cell = entity_get_target_cell(state, entity);
                    }
                    
                    // Avoid exiting onto the mine entrance path
                    ivec2 exit_ignore_cell = ivec2(-1, -1);
                    if (nearest_hall_target.type == TARGET_ENTITY) {
                        const Entity& hall = state->entities.get_by_id(nearest_hall_target.id);
                        exit_ignore_cell = map_get_ideal_mine_entrance_cell(state->map, mine.cell, hall.cell);
                    }
                    
                    ivec2 exit_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, mine.cell, mine_data.cell_size, entity_data.cell_size, rally_cell, 0, exit_ignore_cell);

                    if (exit_cell.x == -1) {
                        match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_MINE_EXIT_BLOCKED);
                    } else if (map_get_cell(state->map, CELL_LAYER_GROUND, exit_cell).type == CELL_EMPTY) {
                        // Remove unit from mine
                        for (uint32_t index = 0; index < mine.garrisoned_units.size(); index++) {
                            if (mine.garrisoned_units[index] == entity_id) {
                                mine.garrisoned_units.remove_at_unordered(index);
                                break;
                            }
                        }

                        entity.garrison_id = ID_NULL;
                        entity.target = entity_next_target;
                        if (entity.target.type == TARGET_NONE) {
                            entity.goldmine_id = ID_NULL;
                        }
                        match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, false);
                        entity.cell = exit_cell;
                        ivec2 exit_from_cell = get_nearest_cell_in_rect(exit_cell, mine.cell, mine_data.cell_size);
                        entity.direction = enum_from_ivec2_direction(exit_cell - exit_from_cell);
                        entity.position = cell_center(exit_from_cell);
                        entity.mode = MODE_UNIT_MOVE;
                        map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
                            .type = entity.goldmine_id != ID_NULL && entity.target.type == TARGET_ENTITY
                                        ? CELL_MINER 
                                        : CELL_UNIT,
                            .id = entity_id
                        });
                        match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, true);

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
                if (entity_is_target_invalid(state, entity) || !state->players[entity.player_id].active) {
                    entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, false);
                    entity.target = target_none();
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                if (!animation_is_playing(entity.animation)) {
                    entity_attack_target(state, entity_id);
                    if (entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) {
                        entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, false);
                        match_event_play_sound(state, SOUND_CAMO_OFF, entity.position.to_ivec2());
                    }
                    if (entity.mode == MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP) {
                        entity_set_flag(entity, ENTITY_FLAG_CHARGED, false);
                        entity.mode = MODE_UNIT_SOLDIER_CHARGE;
                    } else {
                        entity.cooldown_timer = entity_data.unit_data.attack_cooldown + lcg_rand(&state->lcg_seed) % 4;
                        entity.mode = MODE_UNIT_IDLE;
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
                    entity.target = target_none();
                    entity.mode = MODE_UNIT_IDLE;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_DEATH: {
                if (!animation_is_playing(entity.animation)) {
                    map_set_cell_rect(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                        .type = CELL_EMPTY, .id = ID_NULL
                    });
                    entity_release_garrisoned_units_on_death(state, entity);
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
                    map_set_cell_rect(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, (Cell) {
                        .type = CELL_EMPTY, .id = ID_NULL
                    });
                    entity_release_garrisoned_units_on_death(state, entity);
                    state->particles.push_back((Particle) {
                        .layer = PARTICLE_LAYER_GROUND,
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
                    entity_explode(state, entity_id);
                }
                update_finished = true;
                break;
            }
            case MODE_BUILDING_FINISHED: {
                if (!entity.queue.empty() && entity.timer != 0) {
                    if (entity.timer == BUILDING_QUEUE_BLOCKED && !entity_building_is_supply_blocked(state, entity)) {
                        entity.timer = building_queue_item_duration(entity.queue[0]);
                    } else if (entity.timer != BUILDING_QUEUE_BLOCKED && entity_building_is_supply_blocked(state, entity)) {
                        entity.timer = BUILDING_QUEUE_BLOCKED;
                    }

                    if (entity.timer != BUILDING_QUEUE_BLOCKED && entity.timer != BUILDING_QUEUE_EXIT_BLOCKED) {
                        entity.timer--;
                    }

                    if ((entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) || 
                            entity.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                        ivec2 rally_cell = entity.rally_point.x == -1 
                                            ? entity.cell + ivec2(0, entity_data.cell_size)
                                            : entity.rally_point / TILE_SIZE;
                        ivec2 ignore_cell = ivec2(-1, -1);
                        if (entity.type == ENTITY_HALL && entity.rally_point.x != -1) {
                            Cell map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, entity.rally_point / TILE_SIZE);
                            if (map_cell.type == CELL_GOLDMINE) {
                                EntityId goldmine_id = map_cell.id;
                                const Entity& goldmine = state->entities.get_by_id(goldmine_id);
                                ignore_cell = map_get_ideal_mine_exit_path_rally_cell(state->map, goldmine.cell, entity.cell);
                            }
                        }
                        ivec2 exit_cell = map_get_exit_cell(state->map, entity_data.cell_layer, entity.cell, entity_data.cell_size, entity_get_data(entity.queue[0].unit_type).cell_size, rally_cell, 0, ignore_cell);
                        if (exit_cell.x == -1) {
                            if (entity.timer == 0) {
                                match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED);
                            }
                            entity.timer = BUILDING_QUEUE_EXIT_BLOCKED;
                            update_finished = true;
                            break;
                        } 

                        entity.timer = 0;
                        EntityId unit_id = entity_create(state, entity.queue[0].unit_type, exit_cell, entity.player_id);

                        // Rally unit
                        Entity& unit = state->entities.get_by_id(unit_id);
                        Cell rally_cell_value = map_get_cell(state->map, CELL_LAYER_GROUND, rally_cell);
                        if (unit.type == ENTITY_MINER && rally_cell_value.type == CELL_GOLDMINE) {
                            // Rally to gold
                            unit.target = target_entity(rally_cell_value.id);
                        } else {
                            // Rally to cell
                            unit.target = target_cell(rally_cell);
                        }

                        // Create alert
                        match_event_alert(state, MATCH_ALERT_TYPE_UNIT, unit.player_id, unit.cell, entity_get_data(unit.type).cell_size);

                        entity_building_dequeue(state, entity);
                    } else if (entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                        match_grant_player_upgrade(state, entity.player_id, entity.queue[0].upgrade);

                        // Show status
                        state->events.push((MatchEvent) {
                            .type = MATCH_EVENT_RESEARCH_COMPLETE,
                            .research_complete = (MatchEventResearchComplete) {
                                .upgrade = entity.queue[0].upgrade,
                                .player_id = entity.player_id
                            }
                        });

                        // Create alert
                        match_event_alert(state, MATCH_ALERT_TYPE_RESEARCH, entity.player_id, entity.cell, entity_data.cell_size);

                        entity_building_dequeue(state, entity);
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
                entity_on_damage_taken(entity);
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
            entity.taking_damage_timer = entity.taking_damage_counter == 0 ? 0 : UNIT_TAKING_DAMAGE_FLICKER_DURATION;
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

    const bool does_entity_regen_energy = 
        !(entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && !match_player_has_upgrade(state, entity.player_id, UPGRADE_STAKEOUT));
    if (entity_is_unit(entity.type) && entity.energy < entity_data.unit_data.max_energy && does_entity_regen_energy) {
        if (entity.energy_regen_timer != 0) {
            entity.energy_regen_timer--;
        }
        if (entity.energy_regen_timer == 0) {
            entity.energy_regen_timer = entity_get_energy_regen_duration(state, entity);
            entity.energy++;
        }
    }

    // Bleed
    if (entity.bleed_timer != 0 && entity.health != 0) {
        entity.bleed_timer--;
        if (entity.bleed_timer != 0) {
            animation_update(entity.bleed_animation);
            if (entity.bleed_damage_timer != 0) {
                entity.bleed_damage_timer--;
            }
            if (entity.bleed_damage_timer == 0) {
                entity.health--;
                entity.bleed_damage_timer = BLEED_DAMAGE_RATE;
                entity_on_damage_taken(entity);
            }
        }
    }

    // Update animation
    AnimationName expected_animation = entity_get_expected_animation(entity);
    if (entity.animation.name != expected_animation || (!animation_is_playing(entity.animation) && expected_animation != ANIMATION_UNIT_IDLE)) {
        entity.animation = animation_create(expected_animation);
    }
    int prev_hframe = entity.animation.frame.x;
    animation_update(entity.animation);
    if (prev_hframe != entity.animation.frame.x) {
        if ((entity.mode == MODE_UNIT_REPAIR || entity.mode == MODE_UNIT_BUILD) && prev_hframe == 0) {
            match_event_play_sound(state, SOUND_HAMMER, entity.position.to_ivec2());
        } else if (entity.mode == MODE_UNIT_PYRO_THROW && entity.animation.frame.x == 6) {
            if (entity.target.type == TARGET_MOLOTOV) {
                state->projectiles.push_back((Projectile) {
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

SpriteName entity_get_sprite(const MatchState* state, const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);

    if (entity.mode == MODE_BUILDING_DESTROYED) {
        if (entity.type == ENTITY_BUNKER) {
            return SPRITE_BUILDING_DESTROYED_BUNKER;
        }
        if (entity.type == ENTITY_LANDMINE) {
            return SPRITE_BUILDING_DESTROYED_MINE;
        }
        switch (entity_data.cell_size) {
            case 2:
                return SPRITE_BUILDING_DESTROYED_2;
            case 3:
                return SPRITE_BUILDING_DESTROYED_3;
            case 4:
                return SPRITE_BUILDING_DESTROYED_4;
            default:
                GOLD_ASSERT_MESSAGE(false, "Destroyed sprite needed for building of this size");
                return SPRITE_BUILDING_DESTROYED_2;
        }
    }
    if (entity.mode == MODE_UNIT_BUILD || entity.mode == MODE_UNIT_REPAIR || entity.mode == MODE_UNIT_BUILD_ASSIST) {
        return SPRITE_MINER_BUILDING;
    }
    if (entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) {
        return SPRITE_UNIT_DETECTIVE_INVISIBLE;
    }
    if (entity.type == ENTITY_WAGON && match_player_has_upgrade(state, entity.player_id, UPGRADE_WAR_WAGON)) {
        return SPRITE_UNIT_WAR_WAGON;
    }
    return entity_data.sprite;
}

SpriteName entity_get_icon(const MatchState* state, EntityType type, uint8_t player_id) {
    if (type == ENTITY_WAGON && match_player_has_upgrade(state, player_id, UPGRADE_WAR_WAGON)) {
        return SPRITE_BUTTON_ICON_WAR_WAGON;
    }
    return entity_get_data(type).icon;
}

fixed entity_get_speed(const MatchState* state, const Entity& entity) {
    if (!entity_is_unit(entity.type)) {
        return fixed::from_raw(0);
    }
    if (entity.type == ENTITY_BALLOON && match_player_has_upgrade(state, entity.player_id, UPGRADE_TAILWIND)) {
        return fixed::from_int_and_raw_decimal(0, 128);
    }
    return entity_get_data(entity.type).unit_data.speed;    
}

bool entity_has_detection(const MatchState* state, const Entity& entity) {
    if (entity.type == ENTITY_DETECTIVE && match_player_has_upgrade(state, entity.player_id, UPGRADE_PRIVATE_EYE)) {
        return true;
    }
    return entity_get_data(entity.type).has_detection;
}

uint32_t entity_get_energy_regen_duration(const MatchState* state, const Entity& entity) {
    if (entity.type == ENTITY_DETECTIVE && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) {
        if (match_player_has_upgrade(state, entity.player_id, UPGRADE_STAKEOUT)) {
            return UNIT_ENERGY_REGEN_DURATION * 2;
        } else {
            return 0;
        }
    }
    return UNIT_ENERGY_REGEN_DURATION;
}

int entity_get_armor(const MatchState* state, const Entity& entity) {
    int armor = entity_get_data(entity.type).armor;
    if (entity.type == ENTITY_WAGON && match_player_has_upgrade(state, entity.player_id, UPGRADE_WAR_WAGON)) {
        armor += 2;
    }
    return armor;
}

int entity_get_range_squared(const MatchState* state, const Entity& entity) {
    if (!entity_is_unit(entity.type)) {
        return 0;
    }

    int range_squared = entity_get_data(entity.type).unit_data.range_squared;
    if (entity.type == ENTITY_COWBOY && match_player_has_upgrade(state, entity.player_id, UPGRADE_IRON_SIGHTS)) {
        range_squared = 25;
    }
    return range_squared;
}

bool entity_is_selectable(const Entity& entity) {
    if (entity.type == ENTITY_GOLDMINE) {
        return true;
    }

    return !(
        entity.health == 0 ||
        entity.mode == MODE_UNIT_BUILD ||
        entity.garrison_id != ID_NULL
    );
}

bool entity_can_be_given_orders(const MatchState* state, const Entity& entity) {
    return !( 
        entity.health == 0 || 
        entity.mode == MODE_UNIT_BUILD ||
        (entity.garrison_id != ID_NULL && !entity_is_in_mine(state, entity))
    );
}

uint32_t entity_get_elevation(const Entity& entity, const Map& map) {
    uint32_t elevation = map_get_tile(map, entity.cell).elevation;
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size; y++) {
        for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size; x++) {
            elevation = std::max(elevation, (uint32_t)map_get_tile(map, ivec2(x, y)).elevation);
        }
    }

    if (entity.mode == MODE_UNIT_MOVE) {
        ivec2 unit_prev_cell = entity.cell - DIRECTION_IVEC2[entity.direction];
        for (int y = unit_prev_cell.y; y < unit_prev_cell.y + entity_cell_size; y++) {
            for (int x = unit_prev_cell.x; x < unit_prev_cell.x + entity_cell_size; x++) {
                elevation = std::max(elevation, (uint32_t)map_get_tile(map, ivec2(x, y)).elevation);
            }
        }
    }
    
    return elevation;
}

Rect entity_get_rect(const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    Rect rect = (Rect) {
        .x = entity.position.x.integer_part(),
        .y = entity.position.y.integer_part(),
        .w = entity_data.cell_size * TILE_SIZE,
        .h = entity_data.cell_size * TILE_SIZE
    };
    if (entity_is_unit(entity.type)) {
        rect.x -= rect.w / 2;
        rect.y -= rect.h / 2;
    }
    if (entity.type == ENTITY_BALLOON) {
        rect.y -= 32;
        rect.h += 16;
    }

    return rect;
}

fvec2 entity_get_target_position(const Entity& entity) {
    int unit_size = entity_get_data(entity.type).cell_size * TILE_SIZE;
    return fvec2((entity.cell * TILE_SIZE) + ivec2(unit_size / 2, unit_size / 2));
}

bool entity_check_flag(const Entity& entity, uint32_t flag) {
    return (entity.flags & flag) == flag;
}

void entity_set_flag(Entity& entity, uint32_t flag, bool value) {
    if (value) {
        entity.flags |= flag;
    } else {
        entity.flags &= ~flag;
    }
}

AnimationName entity_get_expected_animation(const Entity& entity) {
    if (entity.type == ENTITY_BALLOON) {
        switch (entity.mode) {
            case MODE_UNIT_MOVE:
                return ANIMATION_BALLOON_MOVE;
            case MODE_UNIT_BALLOON_DEATH_START:
                return ANIMATION_BALLOON_DEATH_START;
            case MODE_UNIT_BALLOON_DEATH:
                return ANIMATION_BALLOON_DEATH;
            default: 
                return ANIMATION_UNIT_IDLE;
        }
    } else if (entity.type == ENTITY_CANNON) {
        switch (entity.mode) {
            case MODE_UNIT_MOVE:
                return ANIMATION_UNIT_MOVE_CANNON;
            case MODE_UNIT_ATTACK_WINDUP:
                return ANIMATION_CANNON_ATTACK;
            case MODE_UNIT_DEATH:
                return ANIMATION_CANNON_DEATH;
            case MODE_UNIT_DEATH_FADE:
                return ANIMATION_CANNON_DEATH_FADE;
            default:
                return ANIMATION_UNIT_IDLE;
        }
    } else if (entity.type == ENTITY_SMITH) {
        if (entity.queue.empty() && entity.animation.name != ANIMATION_UNIT_IDLE && entity.animation.name != ANIMATION_SMITH_END) {
            return ANIMATION_SMITH_END;
        } else if (entity.animation.name == ANIMATION_SMITH_END && !animation_is_playing(entity.animation)) {
            return ANIMATION_UNIT_IDLE;
        } else if (!entity.queue.empty() && entity.animation.name == ANIMATION_UNIT_IDLE) {
            return ANIMATION_SMITH_BEGIN;
        } else if (entity.animation.name == ANIMATION_SMITH_BEGIN && !animation_is_playing(entity.animation)) {
            return ANIMATION_SMITH_LOOP;
        } else {
            return entity.animation.name;
        }
    }

    switch (entity.mode) {
        case MODE_UNIT_MOVE:
            return ANIMATION_UNIT_MOVE;
        case MODE_UNIT_BUILD:
        case MODE_UNIT_BUILD_ASSIST:
        case MODE_UNIT_REPAIR:
            return ANIMATION_UNIT_BUILD;
        case MODE_UNIT_ATTACK_WINDUP:
        case MODE_UNIT_PYRO_THROW:
            return ANIMATION_UNIT_ATTACK;
        case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP:
            return ANIMATION_SOLDIER_RANGED_ATTACK;
        case MODE_UNIT_SOLDIER_CHARGE:
            return ANIMATION_SOLDIER_CHARGE;
        case MODE_UNIT_DEATH:
            return ANIMATION_UNIT_DEATH;
        case MODE_UNIT_DEATH_FADE:
            return ANIMATION_UNIT_DEATH_FADE;
        case MODE_MINE_PRIME:
            return ANIMATION_MINE_PRIME;
        case MODE_BUILDING_FINISHED: {
            if (entity.type == ENTITY_WORKSHOP && !entity.queue.empty() && !(entity.timer == BUILDING_QUEUE_BLOCKED || entity.timer == BUILDING_QUEUE_EXIT_BLOCKED)) {
                return ANIMATION_WORKSHOP;
            } else {
                return ANIMATION_UNIT_IDLE;
            }
        }
        default:
            return ANIMATION_UNIT_IDLE;
    }
}

ivec2 entity_get_animation_frame(const Entity& entity) {
    if (entity_is_unit(entity.type)) {
        ivec2 frame = entity.animation.frame;

        if (entity.type == ENTITY_BALLOON && entity.mode == MODE_UNIT_DEATH_FADE) {
            // A bit hacky, this will just be an empty frame
            return ivec2(2, 2);
        }

        if (entity.mode == MODE_UNIT_BUILD) {
            frame.y = 2;
        } else if (entity.animation.name == ANIMATION_UNIT_DEATH || entity.animation.name == ANIMATION_UNIT_DEATH_FADE || 
                        entity.animation.name == ANIMATION_CANNON_DEATH || entity.animation.name == ANIMATION_CANNON_DEATH_FADE || 
                        entity.animation.name == ANIMATION_BALLOON_DEATH_START || entity.animation.name == ANIMATION_BALLOON_DEATH) {
            frame.y = entity.direction == DIRECTION_NORTH ? 1 : 0;
        } else if (entity.direction == DIRECTION_NORTH) {
            frame.y = 1;
        } else if (entity.direction == DIRECTION_SOUTH) {
            frame.y = 0;
        } else {
            frame.y = 2;
        }

        if (entity.gold_held && (entity.animation.name == ANIMATION_UNIT_MOVE || entity.animation.name == ANIMATION_UNIT_IDLE)) {
            frame.y += 3;
        }
        if (entity.type == ENTITY_SAPPER && entity.target.type == TARGET_ATTACK_ENTITY) {
            frame.y += 3;
        }

        return frame;
    } else if (entity_is_building(entity.type)) {
        if (entity.mode == MODE_BUILDING_DESTROYED || entity.mode == MODE_MINE_ARM) {
            return ivec2(0, 0);
        }
        if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
            int max_health = entity_get_data(entity.type).max_health;
            return ivec2((3 * (max_health - entity.timer)) / max_health, 0);
        } 
        if (entity.mode == MODE_MINE_PRIME) {
            return entity.animation.frame;
        }
        if (entity.animation.name != ANIMATION_UNIT_IDLE && entity.type != ENTITY_SMITH) {
            return entity.animation.frame;
        }
        // Building finished frame
        return ivec2(3, 0);
    } else {
        // Gold
        if (entity.mode == MODE_GOLDMINE_COLLAPSED) {
            return ivec2(2, 0);
        }
        if (!entity.garrisoned_units.empty()) {
            return ivec2(1, 0);
        }
        return ivec2(0, 0);
    }
}

Rect entity_goldmine_get_block_building_rect(ivec2 cell) {
    return (Rect) { .x = cell.x - 4, .y = cell.y - 4, .w = 11, .h = 11 };
}

bool entity_is_mining(const MatchState* state, const Entity& entity) {
    if (entity.target.type != TARGET_ENTITY || entity.type != ENTITY_MINER) {
        return false;
    }

    const Entity& target = state->entities.get_by_id(entity.target.id);
    return (target.type == ENTITY_GOLDMINE && target.gold_held > 0) ||
           (target.type == ENTITY_HALL && target.mode == MODE_BUILDING_FINISHED && entity.goldmine_id != ID_NULL &&
                entity.player_id == target.player_id && entity.gold_held > 0);
}

bool entity_is_in_mine(const MatchState* state, const Entity& entity) {
    return entity.garrison_id != ID_NULL && state->entities.get_by_id(entity.garrison_id).type == ENTITY_GOLDMINE;
}

bool entity_is_idle_miner(const Entity& entity) {
    return entity.type == ENTITY_MINER && entity.mode == MODE_UNIT_IDLE &&
            entity.target.type == TARGET_NONE && entity.target_queue.empty() && 
            entity_is_selectable(entity);
}

void entity_get_mining_path_to_avoid(const MatchState* state, const Entity& entity, MapPath* path) {
    if (!entity_is_mining(state, entity)) {
        return;
    }

    Target hall_target = entity_target_nearest_hall(state, entity);
    if (hall_target.type != TARGET_ENTITY) {
        return;
    }
    Target goldmine_target = entity_target_nearest_goldmine(state, entity);
    if (goldmine_target.type != TARGET_ENTITY) {
        return;
    }
    const Entity& goldmine = state->entities.get_by_id(goldmine_target.id);
    const Entity& hall = state->entities.get_by_id(hall_target.id);

    if (entity.target.type == TARGET_ENTITY && entity.target.id == goldmine_target.id) {
        // We're entering the goldmine, so avoid the mine exit path
        map_get_ideal_mine_exit_path(state->map, goldmine.cell, hall.cell, path);
    } else if (entity.target.type == TARGET_ENTITY && entity.target.id == hall_target.id) {
        // We're leaving the goldmine, so avoid the mine entrance path
        map_get_ideal_mine_entrance_path(state->map, goldmine.cell, hall.cell, path);
    }

    if (path->size() > 8) {
        path->clear();
        return;
    }
}

bool entity_is_blocker_walking_towards_entity(const MatchState* state, const Entity& entity) {
    Cell blocking_cell = map_get_cell(state->map, entity_get_data(entity.type).cell_layer, entity.path.back());
    if (blocking_cell.type != CELL_MINER) {
        return false;
    }

    const Entity& blocker = state->entities.get_by_id(blocking_cell.id);
    return entity.direction == ((blocker.direction + 4) % DIRECTION_COUNT);
}

bool entity_is_visible_to_player(const MatchState* state, const Entity& entity, uint8_t player_id) {
    if (entity.garrison_id != ID_NULL) {
        return false;
    }

    uint8_t player_team = state->players[player_id].team;
    if (entity.type != ENTITY_GOLDMINE && state->players[entity.player_id].team == player_team) {
        return true;
    }

    bool entity_is_invisible = entity_check_flag(entity, ENTITY_FLAG_INVISIBLE);
    int entity_cell_size = entity_get_data(entity.type).cell_size;

    for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size; y++) {
        for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size; x++) {
            if (state->fog[player_team][x + (y * state->map.width)] > 0 && 
                    (!entity_is_invisible || state->detection[player_team][x + (y * state->map.width)] > 0)) {
                return true;
            }
        }
    }

    if (entity.mode == MODE_UNIT_MOVE) {
        ivec2 prev_cell = entity.cell - DIRECTION_IVEC2[entity.direction];
        for (int y = prev_cell.y; y < prev_cell.y + entity_cell_size; y++) {
            for (int x = prev_cell.x; x < prev_cell.x + entity_cell_size; x++) {
                if (state->fog[player_team][x + (y * state->map.width)] > 0 && 
                        (!entity_is_invisible || state->detection[player_team][x + (y * state->map.width)] > 0)) {
                    return true;
                }
            }
        }
    }

    return false;
}

void entity_set_target(MatchState* state, Entity& entity, Target target) {
    GOLD_ASSERT(entity.mode != MODE_UNIT_BUILD);

    // If the entity is not building but is en-route to build something, refund it before setting target
    if (entity.target.type == TARGET_BUILD) {
        entity_refund_target_build(state, entity, entity.target);
    }

    entity.target = target;
    entity.path.clear();
    entity.goldmine_id = ID_NULL;
    entity.attack_move_cell = target.type == TARGET_ATTACK_CELL ? target.cell : ivec2(-1, -1);
    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, false);
    entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, target.type == TARGET_ATTACK_ENTITY);

    if (entity.mode != MODE_UNIT_MOVE && entity.mode != MODE_UNIT_IN_MINE) {
        // Abandon current behavior in favor of new order
        entity.timer = 0;
        entity.pathfind_attempts = 0;
        entity.mode = MODE_UNIT_IDLE;
    }
    if (entity.mode == MODE_UNIT_IN_MINE) {
        // Force an early exit of the mine
        entity.timer = 0;
    }
}

void entity_clear_target_queue(MatchState* state, Entity& entity) {
    for (uint32_t target_queue_index = 0; target_queue_index < entity.target_queue.size(); target_queue_index++) {
        const Target& target = entity.target_queue[target_queue_index];
        if (target.type == TARGET_BUILD) {
            entity_refund_target_build(state, entity, target);
        }
    }

    entity.target_queue.clear();
}

void entity_refund_target_build(MatchState* state, Entity& entity, const Target& target) {
    GOLD_ASSERT(target.type == TARGET_BUILD);

    const EntityData& entity_data = entity_get_data(entity.type);
    const EntityData& building_data = entity_get_data(target.build.building_type);
    const bool building_costs_energy = (building_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY;
    if (building_costs_energy) {
        entity.energy = std::min(entity.energy + building_data.gold_cost, entity_data.unit_data.max_energy);
    } else {
        state->players[entity.player_id].gold += building_data.gold_cost;
    }
}

bool entity_is_target_invalid(const MatchState* state, const Entity& entity) {
    if (match_is_target_invalid(state, entity.target, entity.player_id)) {
        return true;
    }

    // Abandon target if it is within min range so that we can hopefully acquire a better one
    // But if they have the bayonets upgrade, then don't do this so that they can use bayonets
    if (entity.target.type == TARGET_ATTACK_ENTITY &&
            entity_is_target_within_min_range(entity, state->entities.get_by_id(entity.target.id)) &&
            !(entity.type == ENTITY_SOLDIER && match_player_has_upgrade(state, entity.player_id, UPGRADE_BAYONETS))) {
        return true;
    }

    return false;
}

bool entity_has_reached_target(const MatchState* state, const Entity& entity) {
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
            const Entity& builder = state->entities.get_by_id(entity.target.id);

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
            const Entity& target = state->entities.get_by_id(entity.target.id);
            return entity_is_target_in_range(state, entity, target, entity.target.type);
        }
        case TARGET_MOLOTOV: {
            return ivec2::euclidean_distance_squared(entity.cell, entity.target.cell) <= MOLOTOV_RANGE_SQUARED;
        }
        case TARGET_PATROL:
            return false;
        case TARGET_TYPE_COUNT:
            GOLD_ASSERT(false);
            return false;
    }
}

bool entity_is_target_in_range(const MatchState* state, const Entity& entity, const Entity& target, TargetType target_type) {
    const Entity& reference_entity = entity.garrison_id == ID_NULL ? entity : state->entities.get_by_id(entity.garrison_id);
    int reference_entity_size = entity_get_data(reference_entity.type).cell_size;
    Rect entity_rect = (Rect) {
        .x = reference_entity.cell.x, .y = reference_entity.cell.y,
        .w = reference_entity_size, .h = reference_entity_size
    };

    int target_size = entity_get_data(target.type).cell_size;
    Rect target_rect = (Rect) {
        .x = target.cell.x, .y = target.cell.y,
        .w = target_size, .h = target_size
    };

    if (entity.target.type != TARGET_ATTACK_ENTITY && (entity.type == ENTITY_BALLOON || target.type == ENTITY_BALLOON)) {
        return entity.cell == entity_get_target_cell(state, entity);
    }

    int entity_range_squared = entity_get_range_squared(state, entity);
    return target_type != TARGET_ATTACK_ENTITY || entity_range_squared == 1
                ? entity_rect.is_adjacent_to(target_rect)
                : Rect::euclidean_distance_squared_between(entity_rect, target_rect) <= entity_range_squared;
}

bool entity_is_target_within_min_range(const Entity& entity, const Entity& target) {
    const EntityData& entity_data = entity_get_data(entity.type);
    const EntityData& target_data = entity_get_data(target.type);

    // Min range is ignored when the target is in the sky
    // Min range is also ignored when the attacking entity is garrisoned
    if (entity.garrison_id != ID_NULL || 
            target_data.cell_layer == CELL_LAYER_SKY) {
        return false;
    }

    Rect entity_rect = (Rect) {
        .x = entity.cell.x, .y = entity.cell.y,
        .w = entity_data.cell_size, .h = entity_data.cell_size
    };
    Rect target_rect = (Rect) {
        .x = target.cell.x, .y = target.cell.y,
        .w = target_data.cell_size, .h = target_data.cell_size
    };

    return Rect::euclidean_distance_squared_between(entity_rect, target_rect) < entity_data.unit_data.min_range_squared;
}

ivec2 entity_get_target_cell_helper(const MatchState* state, const Entity& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return entity.cell;
        case TARGET_BUILD: {
            if (entity.target.build.building_type == ENTITY_LANDMINE) {
                return map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_get_data(entity.type).cell_size, entity.target.build.building_cell, entity_get_data(ENTITY_LANDMINE).cell_size, 0);
            }
            return entity.target.build.unit_cell;
        }
        case TARGET_BUILD_ASSIST: {
            const Entity& builder = state->entities.get_by_id(entity.target.id);
            return map_get_nearest_cell_around_rect(
                        state->map, 
                        CELL_LAYER_GROUND,
                        entity.cell, 
                        entity_get_data(entity.type).cell_size, 
                        builder.target.build.building_cell, 
                        entity_get_data(builder.target.build.building_type).cell_size, 
                        0, ivec2(-1, -1));
        }
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
        case TARGET_UNLOAD:
        case TARGET_MOLOTOV: 
        case TARGET_PATROL: {
            return entity.target.cell;
        }
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            const Entity& target = state->entities.get_by_id(entity.target.id);
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
                Target hall_target = entity_target_nearest_hall(state, entity);
                if (hall_target.type != TARGET_NONE) {
                    const Entity& hall = state->entities.get_by_id(hall_target.id);
                    const EntityData& hall_data = entity_get_data(ENTITY_HALL);
                    ivec2 rally_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, target.cell + ivec2(1, 1), 1, hall.cell, hall_data.cell_size, MAP_OPTION_IGNORE_MINERS);
                    ignore_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, target.cell, target_cell_size, entity_cell_size, rally_cell, MAP_OPTION_IGNORE_MINERS);
                }
            }
            return map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_cell_size, target.cell, target_cell_size, entity_is_mining(state, entity) ? MAP_OPTION_IGNORE_MINERS : 0, ignore_cell);
        }
        case TARGET_TYPE_COUNT:
            GOLD_ASSERT(false);
            return ivec2(-1, -1);
    }
}

ivec2 entity_get_target_cell(const MatchState* state, const Entity& entity) {
    ivec2 target_cell = entity_get_target_cell_helper(state, entity);
    if (entity.type == ENTITY_BALLOON && target_cell.x != -1 && target_cell.y < 2) {
        target_cell.y = 2;
    }
    return target_cell;
}

Target entity_target_nearest_goldmine(const MatchState* state, const Entity& entity) {
    EntityId goldmine_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [](const Entity& goldmine, EntityId /*entity_id*/) {
            return goldmine.type == ENTITY_GOLDMINE && goldmine.gold_held != 0;
        },
        .compare = match_compare_closest_manhattan_distance_to(entity.cell)
    });
    if (goldmine_id != ID_NULL) {
        return target_entity(goldmine_id);
    }

    return target_none();
}

Target entity_target_nearest_hall(const MatchState* state, const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    Rect entity_rect = (Rect) { 
        .x = entity.cell.x, .y = entity.cell.y, 
        .w = entity_data.cell_size, .h = entity_data.cell_size
    };
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;

    for (uint32_t other_index = 0; other_index < state->entities.size(); other_index++) {
        const Entity& other = state->entities[other_index];

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
        return target_none();
    }

    return target_entity(state->entities.get_id_of(nearest_enemy_index));
}

uint32_t entity_get_target_attack_priority(const Entity& entity, const Entity& target) {
    // Sappers don't care about attack priority, they treat everyone the same and explode the nearest target
    if (entity.type == ENTITY_SAPPER) {
        return 1;
    }
    if (target.type == ENTITY_BUNKER && !target.garrisoned_units.empty()) {
        return 2;
    }
    const EntityData& target_data = entity_get_data(target.type);
    if ((entity.type == ENTITY_MINER || entity.type == ENTITY_BANDIT) && target_data.cell_layer == CELL_LAYER_SKY) {
        return 0;
    }
    return target_data.attack_priority;
}

Target entity_target_nearest_enemy(const MatchState* state, const Entity& entity) {
    const Entity& reference_entity = entity.garrison_id == ID_NULL ? entity : state->entities.get_by_id(entity.garrison_id);
    const EntityData& reference_entity_data = entity_get_data(reference_entity.type);
    // This radius makes it so that enemies can target farther than they themselves can see
    // But the enemies player will still need to have vision of the unit before the entity
    // attacks it. This prevents entities from just standing there while their friends are
    // in a fight
    static const int ENTITY_TARGET_RADIUS = 16;
    Rect entity_rect = (Rect) { 
        .x = reference_entity.cell.x, .y = reference_entity.cell.y, 
        .w = reference_entity_data.cell_size, .h = reference_entity_data.cell_size
    };
    Rect entity_sight_rect = (Rect) {
        .x = reference_entity.cell.x - ENTITY_TARGET_RADIUS,
        .y = reference_entity.cell.y - ENTITY_TARGET_RADIUS,
        .w = 2 * ENTITY_TARGET_RADIUS,
        .h = 2 * ENTITY_TARGET_RADIUS
    };
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;
    uint32_t nearest_attack_priority;

    for (uint32_t other_index = 0; other_index < state->entities.size(); other_index++) {
        const Entity& other = state->entities[other_index];
        const EntityData& other_data = entity_get_data(other.type);

        // Goldmines are not enemies
        if (other.type == ENTITY_GOLDMINE) {
            continue;
        }
        // Allies are not enemies
        if (state->players[other.player_id].team == state->players[entity.player_id].team) {
            continue;
        } 
        // Don't attack non-selectable entities
        if (!entity_is_selectable(other)) {
            continue;
        }
        // Don't attack entities that the player can't see
        if (!entity_is_visible_to_player(state, other, entity.player_id)) {
            continue;
        }
        // Don't attack entities that are within min range
        if (entity_is_target_within_min_range(entity, other) && 
                !(entity.type == ENTITY_SOLDIER && match_player_has_upgrade(state, entity.player_id, UPGRADE_BAYONETS))) {
            continue;
        }
        // If garrisoned, don't attack entities that are outside of our range
        if (entity.garrison_id != ID_NULL && !entity_is_target_in_range(state, entity, other, TARGET_ATTACK_ENTITY)) {
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
        uint32_t other_attack_priority = entity_get_target_attack_priority(entity, other); 
        if (nearest_enemy_index == INDEX_INVALID || other_attack_priority > nearest_attack_priority || (other_dist < nearest_enemy_dist && other_attack_priority == nearest_attack_priority)) {
            nearest_enemy_index = other_index;
            nearest_enemy_dist = other_dist;
            nearest_attack_priority = other_attack_priority;
        }
    }

    if (nearest_enemy_index == INDEX_INVALID) {
        return target_none();
    }

    return target_attack_entity(state->entities.get_id_of(nearest_enemy_index));
}

void entity_attack_target(MatchState* state, EntityId attacker_id) {
    Entity& attacker = state->entities.get_by_id(attacker_id);
    Entity& defender = state->entities.get_by_id(attacker.target.id);
    const EntityData& attacker_data = entity_get_data(attacker.type);
    const EntityData& defender_data = entity_get_data(defender.type);

    // Calculate damage
    bool attack_with_bayonets = attacker.type == ENTITY_SOLDIER && attacker.mode == MODE_UNIT_ATTACK_WINDUP;

    // Calculate accuracy
    int accuracy = attack_with_bayonets ? 100 : attacker_data.unit_data.accuracy;
    if (defender.mode == MODE_UNIT_MOVE) {
        accuracy -= defender_data.unit_data.evasion;
    }
    if (defender.type == ENTITY_LANDMINE && defender.mode == MODE_MINE_PRIME) {
        accuracy = 0;
    }
    if (entity_get_elevation(attacker, state->map) < entity_get_elevation(defender, state->map)) {
        accuracy /= 2;
    } 

    // Bunker particle
    if (attacker.garrison_id != ID_NULL) {
        Entity& carrier = state->entities.get_by_id(attacker.garrison_id);
        int particle_index = lcg_rand(&state->lcg_seed) % 4;
        ivec2 particle_position;
        if (carrier.type == ENTITY_BUNKER) {
            particle_position = (carrier.cell * TILE_SIZE) + BUNKER_PARTICLE_OFFSETS[particle_index];
        } else if (carrier.type == ENTITY_WAGON) {
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

        state->particles.push_back((Particle) {
            .layer = PARTICLE_LAYER_GROUND,
            .sprite = SPRITE_PARTICLE_BUNKER_FIRE,
            .animation = animation_create(ANIMATION_PARTICLE_BUNKER_COWBOY),
            .vframe = 0,
            .position = particle_position
        });
    }

    // Fog reveal
    if (entity_get_elevation(attacker, state->map) > entity_get_elevation(defender, state->map) &&
            !entity_check_flag(attacker, ENTITY_FLAG_INVISIBLE)) {
        FogReveal reveal = (FogReveal) {
            .team = state->players[defender.player_id].team,
            .cell = attacker.cell,
            .cell_size = attacker_data.cell_size,
            .sight = 3,
            .timer = FOG_REVEAL_DURATION
        };
        match_fog_update(state, reveal.team, reveal.cell, reveal.cell_size, reveal.sight, false, CELL_LAYER_GROUND, true);
        state->fog_reveals.push_back(reveal);
    }

    int accuracy_roll = lcg_rand(&state->lcg_seed) % 100;
    bool attack_missed = accuracy < accuracy_roll;
    bool attack_is_melee = attack_with_bayonets || entity_get_range_squared(state, attacker) == 1;
    if (attack_missed && attack_is_melee) {
        return;
    }

    // Hit position will be the location of the particle
    // It will also determine who the attack hits if the attack misses
    Rect defender_rect = entity_get_rect(defender);
    ivec2 hit_position;
    if (attack_missed) {
        // Chooses a hit position for the particle in a donut
        int hit_x = lcg_rand(&state->lcg_seed) % (TILE_SIZE * 2);
        int hit_y = lcg_rand(&state->lcg_seed) % (TILE_SIZE * 2);
        if (hit_x >= TILE_SIZE) {
            hit_x += TILE_SIZE;
        }
        if (hit_y >= TILE_SIZE) {
            hit_y += TILE_SIZE;
        }

        hit_position = ivec2(defender_rect.x - TILE_SIZE + hit_x, defender_rect.y - TILE_SIZE + hit_y);
    } else {
        hit_position.x = defender_rect.x + (defender_rect.w / 4) + (lcg_rand(&state->lcg_seed) % (defender_rect.w / 2));
        hit_position.y = defender_rect.y + (defender_rect.h / 4) + (lcg_rand(&state->lcg_seed) % (defender_rect.h / 2));
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
        state->particles.push_back((Particle) {
            .layer = PARTICLE_LAYER_GROUND,
            .sprite = SPRITE_PARTICLE_CANNON_EXPLOSION,
            .animation = animation_create(ANIMATION_PARTICLE_CANNON_EXPLOSION),
            .vframe = 0,
            .position = hit_position
        });
    } else if (attacker.type == ENTITY_COWBOY || (attacker.type == ENTITY_SOLDIER && !attack_with_bayonets) || attacker.type == ENTITY_DETECTIVE || attacker.type == ENTITY_JOCKEY) {
        state->particles.push_back((Particle) {
            .layer = defender_data.cell_layer == CELL_LAYER_SKY
                ? PARTICLE_LAYER_SKY
                : PARTICLE_LAYER_GROUND,
            .sprite = SPRITE_PARTICLE_SPARKS,
            .animation = animation_create(ANIMATION_PARTICLE_SPARKS),
            .vframe = lcg_rand(&state->lcg_seed) % 3,
            .position = hit_position
        });
    }

    // Deal damage
    if (!attack_missed) {
        int attacker_damage = attack_with_bayonets ? SOLDIER_BAYONET_DAMAGE : attacker_data.unit_data.damage;
        int damage = std::max(1, attacker_damage - entity_get_armor(state, defender));
        defender.health = std::max(0, defender.health - damage);
        if (attacker.type == ENTITY_BANDIT && match_player_has_upgrade(state, attacker.player_id, UPGRADE_SERRATED_KNIVES) && entity_is_unit(defender.type)) {
            defender.bleed_timer = BLEED_DURATION;
        }
        entity_on_attack(state, attacker_id, defender);
    }

    // Cannon splash damage, happens regardless of miss
    if (attacker.type == ENTITY_CANNON) {
        Rect splash_damage_rect = (Rect) {
            .x = hit_position.x - TILE_SIZE,
            .y = hit_position.y - TILE_SIZE,
            .w = TILE_SIZE * 2,
            .h = TILE_SIZE * 2
        };
        const int splash_damage = attacker_data.unit_data.damage / 2;

        for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
            Entity& entity = state->entities[entity_index];
            if (entity.type == ENTITY_GOLDMINE || !entity_is_selectable(entity)) {
                continue;
            }

            Rect entity_rect = entity_get_rect(entity);
            if (entity_rect.intersects(splash_damage_rect)) {
                int damage = std::max(1, splash_damage - entity_get_armor(state, entity));
                entity.health = std::max(0, entity.health - damage);
                entity_on_attack(state, attacker_id, entity);
            }
        }
    }
}

void entity_on_attack(MatchState* state, EntityId attacker_id, Entity& defender) {
    const Entity& attacker = state->entities.get_by_id(attacker_id);
    const EntityData& defender_data = entity_get_data(defender.type);

    // Alerts / taking damage flicker
    if (attacker.player_id != defender.player_id) {
        if (defender.taking_damage_counter == 0) {
            match_event_alert(state, MATCH_ALERT_TYPE_ATTACK, defender.player_id, defender.cell, defender_data.cell_size);
        }
    }

    entity_on_damage_taken(defender);

    // Make defender attack back
    if (entity_is_unit(defender.type) && defender.mode == MODE_UNIT_IDLE &&
            defender.target.type == TARGET_NONE && defender_data.unit_data.damage != 0 &&
            !(defender.type == ENTITY_DETECTIVE && entity_check_flag(defender, ENTITY_FLAG_INVISIBLE)) &&
            defender.player_id != attacker.player_id && entity_is_visible_to_player(state, attacker, defender.player_id)) {
        defender.target = target_attack_entity(attacker_id);
    }
}

uint32_t entity_get_garrisoned_occupancy(const MatchState* state, const Entity& entity) {
    uint32_t occupancy = 0;
    for (uint32_t garrisoned_units_index = 0; garrisoned_units_index < entity.garrisoned_units.size(); garrisoned_units_index++) {
        EntityId entity_id = entity.garrisoned_units[garrisoned_units_index];
        occupancy += entity_get_data(state->entities.get_by_id(entity_id).type).garrison_size;
    }
    return occupancy;
}

void entity_unload_unit(MatchState* state, Entity& carrier, EntityId garrisoned_unit_id) {
    const EntityData& carrier_data = entity_get_data(carrier.type);
    uint32_t index = 0;
    while (index < carrier.garrisoned_units.size()) {
        if (garrisoned_unit_id == ENTITY_UNLOAD_ALL || carrier.garrisoned_units[index] == garrisoned_unit_id) {
            Entity& garrisoned_unit = state->entities.get_by_id(carrier.garrisoned_units[index]);
            const EntityData& garrisoned_unit_data = entity_get_data(garrisoned_unit.type);

            // Find the exit cell
            ivec2 exit_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, carrier.cell, carrier_data.cell_size, garrisoned_unit_data.cell_size, carrier.cell + ivec2(0, carrier_data.cell_size), 0);
            if (exit_cell.x == -1) {
                if (entity_is_building(carrier.type)) {
                    match_event_show_status(state, garrisoned_unit.player_id, MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED);
                }
                return;
            }

            // Place the unit in the world
            garrisoned_unit.cell = exit_cell;
            garrisoned_unit.position = entity_get_target_position(garrisoned_unit);
            map_set_cell_rect(state->map, CELL_LAYER_GROUND, garrisoned_unit.cell, garrisoned_unit_data.cell_size, (Cell) {
                .type = CELL_UNIT, .id = carrier.garrisoned_units[index]
            });
            match_fog_update(state, state->players[garrisoned_unit.player_id].team, garrisoned_unit.cell, garrisoned_unit_data.cell_size, garrisoned_unit_data.sight, entity_has_detection(state, garrisoned_unit), garrisoned_unit_data.cell_layer, true);
            garrisoned_unit.mode = MODE_UNIT_IDLE;
            garrisoned_unit.target = target_none();
            garrisoned_unit.garrison_id = ID_NULL;
            garrisoned_unit.goldmine_id = ID_NULL;

            // Remove the unit from the garrisoned units list
            // Choosing to use remove_at_ordered for this so that it looks good in the UI, and because there's only 4 elements anyways
            carrier.garrisoned_units.remove_at_ordered(index);

            match_event_play_sound(state, SOUND_GARRISON_OUT, carrier.position.to_ivec2());
        } else {
            index++;
        }
    }
}

void entity_release_garrisoned_units_on_death(MatchState* state, Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    for (uint32_t garrisoned_units_index = 0; garrisoned_units_index < entity.garrisoned_units.size(); garrisoned_units_index++) {
        EntityId garrisoned_unit_id = entity.garrisoned_units[garrisoned_units_index];
        Entity& garrisoned_unit = state->entities.get_by_id(garrisoned_unit_id);
        const EntityData& garrisoned_unit_data = entity_get_data(garrisoned_unit.type);
        // place garrisoned units inside former-self
        bool unit_is_placed = false;
        for (int x = entity.cell.x; x < entity.cell.x + entity_data.cell_size; x++) {
            for (int y = entity.cell.y; y < entity.cell.y + entity_data.cell_size; y++) {
                if (!map_is_cell_rect_occupied(state->map, CELL_LAYER_GROUND, ivec2(x, y), garrisoned_unit_data.cell_size)) {
                    garrisoned_unit.cell = ivec2(x, y);
                    garrisoned_unit.position = entity_get_target_position(garrisoned_unit);
                    garrisoned_unit.garrison_id = ID_NULL;
                    garrisoned_unit.mode = MODE_UNIT_IDLE;
                    garrisoned_unit.target = target_none();
                    map_set_cell_rect(state->map, CELL_LAYER_GROUND, garrisoned_unit.cell, garrisoned_unit_data.cell_size, (Cell) {
                        .type = CELL_UNIT, .id = garrisoned_unit_id
                    });
                    match_fog_update(state, state->players[garrisoned_unit.player_id].team, garrisoned_unit.cell, garrisoned_unit_data.cell_size, garrisoned_unit_data.sight, entity_has_detection(state, garrisoned_unit), garrisoned_unit_data.cell_layer, true);
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

void entity_explode(MatchState* state, EntityId entity_id) {
    Entity& entity = state->entities.get_by_id(entity_id);
    const EntityData& entity_data = entity_get_data(entity.type);

    // Apply damage
    Rect explosion_rect = (Rect) { 
        .x = (entity.cell.x - 1) * TILE_SIZE,
        .y = (entity.cell.y - 1) * TILE_SIZE,
        .w = TILE_SIZE * 3,
        .h = TILE_SIZE * 3
    };
    int explosion_damage = entity.type == ENTITY_SAPPER ? entity_data.unit_data.damage : MINE_EXPLOSION_DAMAGE;
    for (uint32_t defender_index = 0; defender_index < state->entities.size(); defender_index++) {
        if (defender_index == state->entities.get_index_of(entity_id)) {
            continue;
        }
        Entity& defender = state->entities[defender_index];
        if (defender.type == ENTITY_GOLDMINE || 
                !entity_is_selectable(defender) ||
                entity_get_data(defender.type).cell_layer == CELL_LAYER_SKY) {
            continue;
        }

        Rect defender_rect = entity_get_rect(defender);
        if (explosion_rect.intersects(defender_rect)) {
            int defender_armor = entity_get_armor(state, defender); 
            int damage = std::max(1, explosion_damage - defender_armor);
            defender.health = std::max(defender.health - damage, 0);
            entity_on_attack(state, entity_id, defender);
        }
    }

    // Kill the entity
    entity.health = 0;
    if (entity.type == ENTITY_SAPPER) {
        entity.target = target_none();
        entity.mode = MODE_UNIT_DEATH_FADE;
        entity.animation = animation_create(ANIMATION_UNIT_DEATH_FADE);
        map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
            .type = CELL_EMPTY, .id = ID_NULL
        });
    } else {
        entity.mode = MODE_BUILDING_DESTROYED;
        entity.timer = BUILDING_FADE_DURATION;
        map_set_cell(state->map, CELL_LAYER_UNDERGROUND, entity.cell, (Cell) { .type = CELL_EMPTY, .id = ID_NULL });
    }

    match_event_play_sound(state, SOUND_EXPLOSION, entity.position.to_ivec2());

    // Create particle
    state->particles.push_back((Particle) {
        .layer = PARTICLE_LAYER_GROUND,
        .sprite = SPRITE_PARTICLE_EXPLOSION,
        .animation = animation_create(ANIMATION_PARTICLE_EXPLOSION),
        .vframe = 0,
        .position = entity.type == ENTITY_SAPPER ? entity.position.to_ivec2() : cell_center(entity.cell).to_ivec2()
    });
}

bool entity_should_die(const Entity& entity) {
    if (entity.health != 0) {
        return false;
    }

    if (entity_is_unit(entity.type)) {
        if (entity.mode == MODE_UNIT_DEATH || entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_UNIT_BALLOON_DEATH_START || entity.mode == MODE_UNIT_BALLOON_DEATH) {
            return false;
        }
        if (entity.garrison_id != ID_NULL) {
            return false;
        }

        return true;
    } else if (entity_is_building(entity.type)) {
        return entity.mode != MODE_BUILDING_DESTROYED;
    }

    return false;
}

void entity_on_damage_taken(Entity& entity) {
    entity.taking_damage_counter = 3;
    if (entity.taking_damage_timer == 0) {
        entity.taking_damage_timer = UNIT_TAKING_DAMAGE_FLICKER_DURATION;
    }

    // Health regen timer
    if (entity_is_unit(entity.type)) {
        entity.health_regen_timer = UNIT_HEALTH_REGEN_DURATION + UNIT_HEALTH_REGEN_DELAY;
    }
}

void entity_stop_building(MatchState* state, EntityId entity_id) {
    Entity& entity = state->entities.get_by_id(entity_id);
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
    while (!map_is_cell_in_bounds(state->map, exit_cell) || map_is_cell_rect_occupied(state->map, CELL_LAYER_GROUND, exit_cell, entity_data.cell_size)) {
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
    entity.target = target_none();
    entity.mode = MODE_UNIT_IDLE;
    map_set_cell_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_data.cell_size, (Cell) {
        .type = CELL_UNIT, .id = entity_id
    });
    match_fog_update(state, state->players[entity.player_id].team, entity.cell, entity_data.cell_size, entity_data.sight, entity_has_detection(state, entity), entity_data.cell_layer, true); 
}

void entity_building_finish(MatchState* state, EntityId building_id) {
    Entity& building = state->entities.get_by_id(building_id);
    int building_cell_size = entity_get_data(building.type).cell_size;

    building.mode = MODE_BUILDING_FINISHED;

    // Show alert
    match_event_alert(state, MATCH_ALERT_TYPE_BUILDING, building.player_id, building.cell, building_cell_size);

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        Entity& entity = state->entities[entity_index];

        if (!entity_is_unit(entity.type)) {
            continue;
        }
        if (!(entity.mode == MODE_UNIT_BUILD || entity.mode == MODE_UNIT_REPAIR|| entity.mode == MODE_UNIT_BUILD_ASSIST) || entity.target.id != building_id) {
            continue;
        }

        if (entity.target.type == TARGET_BUILD) {
            entity_stop_building(state, state->entities.get_id_of(entity_index));
            // If the unit was unable to stop building, notify the user that the exit is blocked
            if (entity.mode != MODE_UNIT_IDLE) {
                match_event_show_status(state, entity.player_id, MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED);
            }
        } else {
            entity.mode = MODE_UNIT_IDLE;
        }

        entity.target = building.type == ENTITY_HALL
                            ? entity_target_nearest_goldmine(state, entity)
                            : target_none();
    }
}

void entity_building_enqueue(MatchState* state, Entity& building, BuildingQueueItem item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    building.queue.push_back(item);
    if (building.queue.size() == 1) {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.timer != BUILDING_QUEUE_BLOCKED) {
                match_event_show_status(state, building.player_id, MATCH_UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(item);
        }
    }
}

void entity_building_dequeue(MatchState* state, Entity& building) {
    GOLD_ASSERT(!building.queue.empty());

    building.queue.remove_at_ordered(0);
    if (building.queue.empty()) {
        building.timer = 0;
    } else {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.timer != BUILDING_QUEUE_BLOCKED) {
                match_event_show_status(state, building.player_id, MATCH_UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(building.queue[0]);
        }
    }
}

bool entity_building_is_supply_blocked(const MatchState* state, const Entity& building) {
    const BuildingQueueItem& item = building.queue[0];
    if (item.type == BUILDING_QUEUE_ITEM_UNIT) {
        if (state->entities.is_full()) {
            return true;
        }
        uint32_t required_population = match_get_player_population(state, building.player_id) + entity_get_data(item.unit_type).unit_data.population_cost;
        if (match_get_player_max_population(state, building.player_id) < required_population) {
            return true;
        }
    }
    return false;
}

// TARGET

Target target_none() {
    Target target;
    target.type = TARGET_NONE;
    target.id = ID_NULL;
    target.cell = ivec2(-1, -1);
    target.build.unit_cell = ivec2(-1, -1);
    target.build.building_cell = ivec2(-1, -1);
    target.build.building_type = ENTITY_TYPE_COUNT;
    return target;
}

Target target_cell(ivec2 cell) {
    Target target = target_none();
    target.type = TARGET_CELL;
    target.cell = cell;
    return target;
}

Target target_entity(EntityId entity_id) {
    Target target = target_none();
    target.type = TARGET_ENTITY;
    target.id = entity_id;
    return target;
}

Target target_attack_cell(ivec2 cell) {
    Target target = target_none();
    target.type = TARGET_ATTACK_CELL;
    target.cell = cell;
    return target;
}

Target target_attack_entity(EntityId entity_id) {
    Target target = target_none();
    target.type = TARGET_ATTACK_ENTITY;
    target.id = entity_id;
    return target;
}

Target target_repair(EntityId entity_id) {
    Target target = target_none();
    target.type = TARGET_REPAIR;
    target.id = entity_id;
    return target;
}

Target target_unload(ivec2 cell) {
    Target target = target_none();
    target.type = TARGET_UNLOAD;
    target.cell = cell;
    return target;
}

Target target_molotov(ivec2 cell) {
    Target target = target_none();
    target.type = TARGET_MOLOTOV;
    target.cell = cell;
    return target;
}

Target target_build(TargetBuild target_build) {
    Target target = target_none();
    target.type = TARGET_BUILD;
    target.build = target_build;
    return target;
}

Target target_build_assist(EntityId entity_id) {
    Target target = target_none();
    target.type = TARGET_BUILD_ASSIST;
    target.id = entity_id;
    return target;
}

Target target_patrol(ivec2 cell_a, ivec2 cell_b) {
    Target target = target_none();
    target.type = TARGET_PATROL;
    target.patrol.cell_a = cell_a;
    target.patrol.cell_b = cell_b;
    return target;
}

// BUILDING QUEUE

uint32_t building_queue_item_duration(const BuildingQueueItem& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return entity_get_data(item.unit_type).train_duration * 60;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return upgrade_get_data(item.upgrade).research_duration * 60;
        }
    }
}

uint32_t building_queue_item_cost(const BuildingQueueItem& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return entity_get_data(item.unit_type).gold_cost;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return upgrade_get_data(item.upgrade).gold_cost;
        }
    }
}

uint32_t building_queue_population_cost(const BuildingQueueItem& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return entity_get_data(item.unit_type).unit_data.population_cost;
        }
        default:
            return 0;
    }
}

// EVENTS

void match_event_play_sound(MatchState* state, SoundName sound, ivec2 position) {
    state->events.push((MatchEvent) {
        .type = MATCH_EVENT_SOUND,
        .sound = (MatchEventSound) {
            .position = position,
            .sound = sound
        }
    });
}

void match_event_alert(MatchState* state, MatchAlertType type, uint8_t player_id, ivec2 cell, int cell_size) {
    state->events.push((MatchEvent) {
        .type = MATCH_EVENT_ALERT,
        .alert = (MatchEventAlert) {
            .type = type,
            .player_id = player_id,
            .cell = cell,
            .cell_size = cell_size
        }
    });
}

void match_event_show_status(MatchState* state, uint8_t player_id, const char* message) {
    state->events.push((MatchEvent) {
        .type = MATCH_EVENT_STATUS,
        .status = (MatchEventStatus) {
            .player_id = player_id,
            .message = message
        }
    });
}

// FOG

int match_get_fog(const MatchState* state, uint8_t team, ivec2 cell) {
    return state->fog[team][cell.x + (cell.y * state->map.width)];
}

bool match_is_cell_rect_revealed(const MatchState* state, uint8_t team, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state->fog[team][x + (y * state->map.width)] > 0) {
                return true;
            }
        }
    }

    return false;
}

bool match_is_cell_rect_explored(const MatchState* state, uint8_t team, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state->fog[team][x + (y * state->map.width)] != FOG_HIDDEN) {
                return true;
            }
        }
    }

    return false;
}

void match_fog_update(MatchState* state, uint32_t player_team, ivec2 cell, int cell_size, int sight, bool has_detection, CellLayer cell_layer, bool increment) {
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
                if (!map_is_cell_in_bounds(state->map, line_cell) || ivec2::euclidean_distance_squared(line_start, line_cell) > sight * sight) {
                    break;
                }

                if (increment) {
                    if (state->fog[player_team][line_cell.x + (line_cell.y * state->map.width)] == FOG_HIDDEN) {
                        state->fog[player_team][line_cell.x + (line_cell.y * state->map.width)] = 1;
                    } else {
                        state->fog[player_team][line_cell.x + (line_cell.y * state->map.width)]++;
                    }
                    if (has_detection) {
                        state->detection[player_team][line_cell.x + (line_cell.y * state->map.width)]++;
                    }
                } else {
                    state->fog[player_team][line_cell.x + (line_cell.y * state->map.width)]--;
                    if (has_detection) {
                        state->detection[player_team][line_cell.x + (line_cell.y * state->map.width)]--;
                    }

                    // Remember revealed entities
                    Cell map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, line_cell);
                    // landmines are not shown in remembered entities so don't add them to this list, maybe
                    if (map_cell.type == CELL_BUILDING || map_cell.type == CELL_GOLDMINE) {
                        Entity& entity = state->entities.get_by_id(map_cell.id);
                        if (entity_is_selectable(entity) && entity.type != ENTITY_LANDMINE) {
                            ivec2 frame = entity_get_animation_frame(entity);
                            if (entity.type == ENTITY_GOLDMINE && frame.x == 1) {
                                frame.x = 0;
                            }
                            uint32_t remembered_entity_index = match_team_find_remembered_entity_index(state, player_team, map_cell.id);
                            if (remembered_entity_index == MATCH_ENTITY_NOT_REMEMBERED) {
                                GOLD_ASSERT(state->remembered_entity_count[player_team] < MATCH_MAX_REMEMBERED_ENTITIES);
                                remembered_entity_index = state->remembered_entity_count[player_team];
                                state->remembered_entity_count[player_team]++;
                            }
                            state->remembered_entities[player_team][remembered_entity_index] = (RememberedEntity) {
                                .entity_id = map_cell.id,
                                .type = entity.type,
                                .frame = frame,
                                .cell = entity.cell,
                                .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLDMINE ? 0 : state->players[entity.player_id].recolor_id
                            };
                        }
                    } // End if cell value < cell empty
                } // End if !increment

                if (map_get_tile(state->map, line_cell).elevation > map_get_tile(state->map, line_start).elevation && cell_layer != CELL_LAYER_SKY) {
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

    state->is_fog_dirty = true;
}

// FIRE

bool match_is_cell_on_fire(const MatchState* state, ivec2 cell) {
    return state->fire_cells[cell.x + (cell.y * state->map.width)] == 1;
}

bool match_is_cell_rect_on_fire(const MatchState* state, ivec2 cell, int cell_size) {
    for (int y = cell.y; y < cell.y + cell_size; y++) {
        for (int x = cell.x; x < cell.x + cell_size; x++) {
            if (state->fire_cells[x + (y * state->map.width)] == 1) {
                return true;
            }
        }
    }

    return false;
}

void match_set_cell_on_fire(MatchState* state, ivec2 cell, ivec2 source) {
    if (match_is_cell_on_fire(state, cell)) {
        return;
    }
    if (map_get_cell(state->map, CELL_LAYER_GROUND, cell).type == CELL_BLOCKED) {
        return;
    }
    if (map_is_tile_water(state->map, cell)) {
        return;
    }
    if (ivec2::manhattan_distance(cell, source) > PROJECTILE_MOLOTOV_FIRE_SPREAD ||
        (cell.x == source.x && std::abs(cell.y - source.y) >= PROJECTILE_MOLOTOV_FIRE_SPREAD) ||
        (cell.y == source.y && std::abs(cell.x - source.x) >= PROJECTILE_MOLOTOV_FIRE_SPREAD)) {
        return;
    }
    state->fires.push_back((Fire) {
        .cell = cell,
        .source = source,
        .time_to_live = FIRE_TTL,
        .animation = animation_create(ANIMATION_FIRE_START)
    });
    state->fire_cells[cell.x + (cell.y * state->map.width)] = 1;
}
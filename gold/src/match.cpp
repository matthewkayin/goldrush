#include "match.h"

#include "engine.h"
#include "network.h"
#include "logger.h"
#include "lcg.h"
#include <algorithm>

static const uint32_t PLAYER_STARTING_GOLD = 450;

const uint32_t MATCH_TAKING_DAMAGE_TIMER_DURATION = 30;
const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION = 10;
static const fixed PROJECTILE_SMOKE_SPEED = fixed::from_int(4);
const int PARTICLE_SMOKE_CELL_SIZE = 7;

const std::unordered_map<uint32_t, upgrade_data_t> UPGRADE_DATA = {
    { UPGRADE_WAR_WAGON, (upgrade_data_t) {
            .name = "Wagon Armor",
            .ui_button = UI_BUTTON_RESEARCH_WAR_WAGON,
            .gold_cost = 300,
            .research_duration = 50
    }},
    { UPGRADE_EXPLOSIVES, (upgrade_data_t) {
            .name = "Explosives",
            .ui_button = UI_BUTTON_RESEARCH_EXPLOSIVES,
            .gold_cost = 400,
            .research_duration = 75
    }},
    { UPGRADE_BAYONETS, (upgrade_data_t) {
            .name = "Bayonets",
            .ui_button = UI_BUTTON_RESEARCH_BAYONETS,
            .gold_cost = 200,
            .research_duration = 50
    }},
    { UPGRADE_SMOKE, (upgrade_data_t) {
            .name = "Smoke Bombs",
            .ui_button = UI_BUTTON_RESEARCH_SMOKE,
            .gold_cost = 300,
            .research_duration = 60
    }}
};

match_state_t match_init(int32_t lcg_seed, const noise_t& noise) {
    match_state_t state;

    lcg_srand(lcg_seed);
    log_trace("match_init: set random seed to %i", lcg_seed);

    map_init(state, noise);
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        // Determine player spawn
        #ifdef GOLD_NEAR_SPAWN
            if (player_id == 0) {
                for (entity_t& entity : state.entities) {
                    if (entity.type == ENTITY_GOLD_MINE) {
                        state.map_player_spawns[player_id] = entity.cell + xy(6, 0);
                        break;
                    }
                }
            }
        #endif
        xy player_spawn = state.map_player_spawns[player_id];
        #ifdef GOLD_NEAR_SPAWN
            if (player_id == 1) {
                player_spawn = state.map_player_spawns[0] + xy(0, 16);
            }
        #endif
        
        state.player_gold[player_id] = PLAYER_STARTING_GOLD;
        state.player_upgrades[player_id] = 0;
        state.player_upgrades_in_progress[player_id] = 0;
        state.map_fog[player_id] = std::vector<int>(state.map_width * state.map_height, FOG_HIDDEN);
        state.map_detection[player_id] = std::vector<int>(state.map_width * state.map_height, 0);

        #ifdef GOLD_DEBUG_FOG_DISABLED
            state.map_fog[player_id] = std::vector<int>(state.map_width * state.map_height, FOG_EXPLORED);
            for (uint32_t index = 0; index < state.entities.size(); index++) {
                entity_t& entity = state.entities[index];
                if (entity.type == ENTITY_GOLD_MINE) {
                    state.remembered_entities[player_id][state.entities.get_id_of(index)] = (remembered_entity_t) {
                        .sprite_params = (render_sprite_params_t) {
                            .sprite = entity_get_sprite(entity),
                            .frame = entity_get_animation_frame(entity),
                            .position = entity.position.to_xy(),
                            .options = 0,
                            .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLD_MINE ? (uint8_t)RECOLOR_NONE : network_get_player(entity.player_id).recolor_id
                        },
                        .cell = entity.cell,
                        .cell_size = entity_cell_size(entity.type)
                    };
                }
            }
        #endif

        entity_create(state, ENTITY_WAGON, player_id, player_spawn + xy(1, 0));
        entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(0, 0));
        entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(0, 1));
        entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(3, 0));
        entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(3, 1));
    }

    return state;
}

// INPUTS

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD:
        case INPUT_MOVE_SMOKE: {
            memcpy(out_buffer + out_buffer_length, &input.move.shift_command, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.move.target_id, sizeof(entity_id));
            out_buffer_length += sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_ids, input.move.entity_count * sizeof(entity_id));
            out_buffer_length += input.move.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            memcpy(out_buffer + out_buffer_length, &input.stop.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.stop.entity_ids, input.stop.entity_count * sizeof(entity_id));
            out_buffer_length += input.stop.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD: {
            memcpy(out_buffer + out_buffer_length, &input.build.shift_command, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.building_type, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);
            
            memcpy(out_buffer + out_buffer_length, &input.build.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.build.entity_ids, input.build.entity_count * sizeof(entity_id));
            out_buffer_length += input.build.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD_CANCEL: {
            memcpy(out_buffer + out_buffer_length, &input.build_cancel, sizeof(input_build_cancel_t));
            out_buffer_length += sizeof(input_build_cancel_t);
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_enqueue, sizeof(input_building_enqueue_t));
            out_buffer_length += sizeof(input_building_enqueue_t);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_dequeue, sizeof(input_building_dequeue_t));
            out_buffer_length += sizeof(input_building_dequeue_t);
            break;
        }
        case INPUT_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.unload.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.unload.entity_ids, input.unload.entity_count * sizeof(entity_id));
            out_buffer_length += input.unload.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_SINGLE_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.single_unload, sizeof(input_single_unload_t));
            out_buffer_length += sizeof(input_single_unload_t);
            break;
        }
        case INPUT_RALLY: {
            memcpy(out_buffer + out_buffer_length, &input.rally.rally_point, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.rally.building_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);
            
            memcpy(out_buffer + out_buffer_length, &input.rally.building_ids, input.rally.building_count * sizeof(entity_id));
            out_buffer_length += input.rally.building_count * sizeof(entity_id);
            break;
        }
        case INPUT_EXPLODE: {
            memcpy(out_buffer + out_buffer_length, &input.explode.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.explode.entity_ids, input.explode.entity_count * sizeof(entity_id));
            out_buffer_length += input.explode.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_CHAT: {
            memcpy(out_buffer + out_buffer_length, &input.chat.message_length, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.chat.message, input.chat.message_length);
            out_buffer_length += input.chat.message_length;
            break;
        }
        default:
            break;
    }
}

input_t match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head) {
    input_t input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: 
        case INPUT_MOVE_SMOKE: {
            memcpy(&input.move.shift_command, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.move.target_id, in_buffer + in_buffer_head, sizeof(entity_id));
            in_buffer_head += sizeof(entity_id);

            memcpy(&input.move.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.move.entity_ids, in_buffer + in_buffer_head, input.move.entity_count * sizeof(entity_id));
            in_buffer_head += input.move.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            memcpy(&input.stop.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.stop.entity_ids, in_buffer + in_buffer_head, input.stop.entity_count * sizeof(entity_id));
            in_buffer_head += input.stop.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD: {
            memcpy(&input.build.shift_command, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.building_type, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.build.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.build.entity_ids, in_buffer + in_buffer_head, input.build.entity_count * sizeof(entity_id));
            in_buffer_head += input.build.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD_CANCEL: {
            memcpy(&input.build_cancel, in_buffer + in_buffer_head, sizeof(input_build_cancel_t));
            in_buffer_head += sizeof(input_build_cancel_t);
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            memcpy(&input.building_enqueue, in_buffer + in_buffer_head, sizeof(input_building_enqueue_t));
            in_buffer_head += sizeof(input_building_enqueue_t);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            memcpy(&input.building_dequeue, in_buffer + in_buffer_head, sizeof(input_building_dequeue_t));
            in_buffer_head += sizeof(input_building_dequeue_t);
            break;
        }
        case INPUT_UNLOAD: {
            memcpy(&input.unload.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.unload.entity_ids, in_buffer + in_buffer_head, input.unload.entity_count * sizeof(entity_id));
            in_buffer_head += input.unload.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_SINGLE_UNLOAD: {
            memcpy(&input.single_unload, in_buffer + in_buffer_head, sizeof(input_single_unload_t));
            in_buffer_head += sizeof(input_single_unload_t);
            break;
        }
        case INPUT_RALLY: {
            memcpy(&input.rally.rally_point, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.rally.building_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.rally.building_ids, in_buffer + in_buffer_head, input.rally.building_count * sizeof(entity_id));
            in_buffer_head += input.rally.building_count * sizeof(entity_id);
            break;
        }
        case INPUT_EXPLODE: {
            memcpy(&input.explode.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.explode.entity_ids, in_buffer + in_buffer_head, input.explode.entity_count * sizeof(entity_id));
            in_buffer_head += input.explode.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_CHAT: {
            memcpy(&input.chat.message_length, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.chat.message, in_buffer + in_buffer_head, input.chat.message_length);
            in_buffer_head += input.chat.message_length;
            break;
        }
        default:
            break;
    }

    return input;
}

void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            // Determine the target index
            uint32_t target_index = INDEX_INVALID;
            if (input.type == INPUT_MOVE_ENTITY || input.type == INPUT_MOVE_ATTACK_ENTITY || input.type == INPUT_MOVE_REPAIR) {
                target_index = state.entities.get_index_of(input.move.target_id);
                if (target_index != INDEX_INVALID && !entity_is_selectable(state.entities[target_index])) {
                    target_index = INDEX_INVALID;
                }
            }

            // Calculate group center
            xy group_center;
            bool should_move_as_group = target_index == INDEX_INVALID;
            uint32_t unit_count = 0;
            if (should_move_as_group) {
                xy group_min;
                xy group_max;
                for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                    uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                        continue;
                    }

                    xy entity_cell = state.entities[entity_index].cell;
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

                SDL_Rect group_rect = (SDL_Rect) { 
                    .x = group_min.x, .y = group_min.y,
                    .w = group_max.x - group_min.x, .h = group_max.y - group_min.y
                };
                group_center = xy(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));

                // Don't move as group if we're not in a group
                // Also don't move as a group if the target is inside the group rect (this allows units to converge in on a cell)
                if (unit_count < 2 || sdl_rect_has_point(group_rect, input.move.target_cell)) {
                    should_move_as_group = false;
                }
            } // End calculate group center

            // Give each unit the move command
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                entity_t& entity = state.entities[entity_index];

                // Set the unit's target
                target_t target;
                target.type = (TargetType)input.type;
                if (target_index == INDEX_INVALID) {
                    target.cell = input.move.target_cell;
                    // If group-moving, use the group move cell, but only if the cell is valid
                    if (should_move_as_group) {
                        xy group_move_cell = input.move.target_cell + (entity.cell - group_center);
                        if (map_is_cell_in_bounds(state, group_move_cell) && 
                                xy::manhattan_distance(group_move_cell, input.move.target_cell) <= 3 &&
                                map_get_tile(state, group_move_cell).elevation == map_get_tile(state, input.move.target_cell).elevation) {
                            target.cell = group_move_cell;
                        }
                    }
                } else if (target.type == TARGET_REPAIR) {
                    target.id = input.move.target_id;
                    target.repair = (target_repair_t) {
                        .health_repaired = 0
                    };
                } else if (input.move.target_id == input.move.entity_ids[id_index]) {
                    target = (target_t) { .type = TARGET_NONE };
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
        } // End handle INPUT_MOVE
        case INPUT_MOVE_SMOKE: {
            uint32_t smoke_thrower_index = INDEX_INVALID;
            bool all_units_are_dead = true;
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t unit_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                if (unit_index == INDEX_INVALID || !entity_is_selectable(state.entities[unit_index])) {
                    continue;
                }
                all_units_are_dead = false;
                if (state.entities[unit_index].cooldown_timer != 0) {
                    continue;
                }
                if (smoke_thrower_index == INDEX_INVALID || xy::manhattan_distance(state.entities[unit_index].cell, input.move.target_cell) < 
                                                            xy::manhattan_distance(state.entities[smoke_thrower_index].cell, input.move.target_cell)) {
                    smoke_thrower_index = unit_index;                                            
                }
            }

            if (smoke_thrower_index == INDEX_INVALID) {
                if (!all_units_are_dead) {
                    match_event_show_status(state, player_id, UI_STATUS_SMOKE_COOLDOWN);
                }
                return;
            }

            target_t smoke_target = (target_t) {
                .type = TARGET_SMOKE,
                .cell = input.move.target_cell
            };

            if (!input.move.shift_command || (state.entities[smoke_thrower_index].target.type == TARGET_NONE && state.entities[smoke_thrower_index].target_queue.empty())) {
                state.entities[smoke_thrower_index].target_queue.clear();
                entity_set_target(state.entities[smoke_thrower_index], smoke_target);
            } else {
                state.entities[smoke_thrower_index].target_queue.push_back(smoke_target);
            }
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            for (uint8_t id_index = 0; id_index < input.stop.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.stop.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                entity_t& entity = state.entities[entity_index];

                entity.path.clear();
                entity.target_queue.clear();
                entity_set_target(entity, (target_t) {
                    .type = TARGET_NONE
                });
                if (input.type == INPUT_DEFEND) {
                    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, true);
                }
            }
            break;
        }
        case INPUT_BUILD: {
            // Determine the list of viable builders
            std::vector<entity_id> builder_ids;
            for (uint32_t id_index = 0; id_index < input.build.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.build.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                builder_ids.push_back(input.build.entity_ids[id_index]);
            }

            // Assign the lead builder's target
            entity_id lead_builder_id = match_get_nearest_builder(state, builder_ids, input.build.target_cell);
            entity_t& lead_builder = state.entities.get_by_id(lead_builder_id);
            target_t build_target = (target_t) {
                .type = TARGET_BUILD,
                .id = ID_NULL,
                .build = (target_build_t) {
                    .unit_cell = input.build.building_type == ENTITY_LAND_MINE ? input.build.target_cell : get_nearest_cell_in_rect(lead_builder.cell, input.build.target_cell, entity_cell_size((EntityType)input.build.building_type)),
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
            if (input.build.building_type != ENTITY_LAND_MINE && !input.build.shift_command) {
                for (entity_id builder_id : builder_ids) {
                    if (builder_id == lead_builder_id) {
                        continue;
                    } 
                    uint32_t builder_index = state.entities.get_index_of(builder_id);
                    if (builder_index == INDEX_INVALID || !entity_is_selectable(state.entities[builder_index])) {
                        continue;
                    }
                    entity_set_target(state.entities.get_by_id(builder_id), (target_t) {
                        .type = TARGET_BUILD_ASSIST,
                        .id = lead_builder_id
                    });
                }
            }
            break;
        }
        case INPUT_BUILD_CANCEL: {
            uint32_t building_index = state.entities.get_index_of(input.build_cancel.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                break;
            }

            const entity_data_t& building_data = ENTITY_DATA.at(state.entities[building_index].type);
            uint32_t gold_refund = building_data.gold_cost - (((uint32_t)state.entities[building_index].health * building_data.gold_cost) / (uint32_t)building_data.max_health);
            state.player_gold[state.entities[building_index].player_id] += gold_refund;

            // Tell the builder to stop building
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                if (state.entities[entity_index].target.type == TARGET_BUILD && state.entities[entity_index].target.id == input.build_cancel.building_id) {
                    entity_t& builder = state.entities[entity_index];
                    builder.cell = builder.target.build.building_cell;
                    builder.position = entity_get_target_position(builder);
                    builder.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    builder.mode = MODE_UNIT_IDLE;
                    builder.target_queue.clear();
                    map_set_cell_rect(state, builder.cell, entity_cell_size(builder.type), state.entities.get_id_of(entity_index));
                    map_fog_update(state, builder.player_id, builder.cell, entity_cell_size(builder.type), ENTITY_DATA.at(builder.type).sight, true, ENTITY_DATA.at(builder.type).has_detection);
                    break;
                }
            }

            // Destroy the building
            state.entities[building_index].health = 0;
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_enqueue.building_id);
            building_queue_item_t item = input.building_enqueue.item;
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }
            if (state.player_gold[player_id] < building_queue_item_cost(item)) {
                return;
            }
            if (state.entities[building_index].queue.size() == BUILDING_QUEUE_MAX) {
                return;
            }
            // Reject this enqueue if the upgrade is already being researched
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE && !match_player_upgrade_is_available(state, player_id, item.upgrade)) {
                return;
            }

            // Check if player has the war wagon upgrade
            if (item.type == BUILDING_QUEUE_ITEM_UNIT && item.unit_type == ENTITY_WAGON && match_player_has_upgrade(state, player_id, UPGRADE_WAR_WAGON)) {
                item.unit_type = ENTITY_WAR_WAGON;
            }

            // Mark upgrades as in-progress when we enqueue them
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state.player_upgrades_in_progress[player_id] |= item.upgrade;
            }

            state.player_gold[player_id] -= building_queue_item_cost(input.building_enqueue.item);
            entity_building_enqueue(state, state.entities[building_index], input.building_enqueue.item);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }
            entity_t& building = state.entities[building_index];
            if (building.queue.empty()) {
                return;
            }

            uint32_t index = input.building_dequeue.index == BUILDING_DEQUEUE_POP_FRONT
                                    ? building.queue.size() - 1
                                    : input.building_dequeue.index;
            if (index >= building.queue.size()) {
                return;
            }
            
            state.player_gold[player_id] += building_queue_item_cost(building.queue[index]);
            if (building.queue[index].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state.player_upgrades_in_progress[building.player_id] &= ~building.queue[index].upgrade;
            }

            if (index == 0) {
                entity_building_dequeue(state, building);
            } else {
                building.queue.erase(building.queue.begin() + index);
            }
            break;
        }
        case INPUT_UNLOAD: {
            for (uint32_t input_index = 0; input_index < input.unload.entity_count; input_index++) {
                uint32_t carrier_index = state.entities.get_index_of(input.unload.entity_ids[input_index]);
                if (carrier_index == INDEX_INVALID || !entity_is_selectable(state.entities[carrier_index]) || state.entities[carrier_index].garrisoned_units.empty()) {
                    continue;
                }

                entity_t& carrier = state.entities[carrier_index];
                entity_unload_unit(state, carrier, ENTITY_UNLOAD_ALL);
            }
            break;
        }
        case INPUT_SINGLE_UNLOAD: {
            uint32_t garrisoned_unit_index = state.entities.get_index_of(input.single_unload.unit_id);
            if (garrisoned_unit_index == INDEX_INVALID || state.entities[garrisoned_unit_index].health == 0 || state.entities[garrisoned_unit_index].garrison_id == ID_NULL) {
                return;
            }
            entity_t& garrisoned_unit = state.entities[garrisoned_unit_index];
            entity_id carrier_id = garrisoned_unit.garrison_id;
            entity_t& carrier = state.entities.get_by_id(carrier_id);
            entity_unload_unit(state, carrier, input.single_unload.unit_id);
            break;
        }
        case INPUT_RALLY: {
            for (uint32_t id_index = 0; id_index < input.rally.building_count; id_index++) {
                uint32_t building_index = state.entities.get_index_of(input.rally.building_ids[id_index]);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                    continue;
                }
                state.entities[building_index].rally_point = input.rally.rally_point;
            }
            break;
        }
        case INPUT_EXPLODE: {
            // Determine all the exploding entities up front
            // This is because if an entity has died in the four frames since this input was sent, we don't want it to explode
            // But otherwise we want them all to explode at once and if we handle them sequentially then one might kill the other before it gets the chance to explode
            entity_id entities_to_explode[SELECTION_LIMIT];
            uint32_t entity_count = 0;
            for (uint32_t id_index = 0; id_index < input.explode.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.explode.entity_ids[id_index]);
                if (entity_index != INDEX_INVALID && entity_is_selectable(state.entities[entity_index])) {
                    entities_to_explode[entity_count] = input.explode.entity_ids[id_index];
                    entity_count++;
                }
            }
            for (uint32_t explode_index = 0; explode_index < entity_count; explode_index++) {
                entity_explode(state, entities_to_explode[explode_index]);
            }
            break;
        }
        case INPUT_CHAT: {
            match_event_t event;
            event.type = MATCH_EVENT_CHAT;
            event.chat.player_id = player_id;
            memcpy(event.chat.message, input.chat.message, input.chat.message_length);
            state.events.push_back(event);
            break;
        }
        default:
            break;
    }
}

// UPDATE

void match_update(match_state_t& state) {
    // Update particles
    {
        uint32_t particle_index = 0;
        while (particle_index < state.particles.size()) {
            animation_update(state.particles[particle_index].animation);

            // On particle finish
            if (!animation_is_playing(state.particles[particle_index].animation)) {
                if (state.particles[particle_index].animation.name == ANIMATION_PARTICLE_SMOKE_START) {
                    state.particles[particle_index].animation = animation_create(ANIMATION_PARTICLE_SMOKE);
                } else if (state.particles[particle_index].animation.name == ANIMATION_PARTICLE_SMOKE) {
                    state.particles[particle_index].animation = animation_create(ANIMATION_PARTICLE_SMOKE_END);
                }
            }

            // If particle finished and is not playing an animation, then remove it
            if (!animation_is_playing(state.particles[particle_index].animation)) {
                state.particles.erase(state.particles.begin() + particle_index);
            } else {
                particle_index++;
            }
        }
    }

    // Update projectiles
    {
        uint32_t projectile_index = 0;
        while (projectile_index < state.projectiles.size()) {
            projectile_t& projectile = state.projectiles[projectile_index];
            if (projectile.position.distance_to(projectile.target) <= PROJECTILE_SMOKE_SPEED) {
                // On projectile finish
                state.particles.push_back((particle_t) {
                    .sprite = SPRITE_PARTICLE_SMOKE,
                    .animation = animation_create(ANIMATION_PARTICLE_SMOKE_START),
                    .vframe = 0,
                    .position = projectile.target.to_xy()
                });
                match_event_play_sound(state, SOUND_SMOKE, projectile.position.to_xy());
                state.projectiles.erase(state.projectiles.begin() + projectile_index);
            } else {
                projectile.position += ((projectile.target - projectile.position) * PROJECTILE_SMOKE_SPEED) / projectile.position.distance_to(projectile.target);
                projectile_index++;
            }
        }
    }

    // Update map reveals
    {
        uint32_t reveal_index = 0;
        while (reveal_index < state.map_reveals.size()) {
            map_reveal_t& reveal = state.map_reveals[reveal_index];
            reveal.timer--;
            if (reveal.timer == 0) {
                map_fog_update(state, reveal.player_id, reveal.cell, reveal.cell_size, reveal.sight, false, false);
                state.map_reveals.erase(state.map_reveals.begin() + reveal_index);
            } else {
                reveal_index++;
            }
        }
    }

    // Update entities
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        entity_update(state, entity_index);
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
                    map_fog_update(state, state.entities[entity_index].player_id, state.entities[entity_index].cell, entity_cell_size(state.entities[entity_index].type), ENTITY_DATA.at(state.entities[entity_index].type).sight, false, ENTITY_DATA.at(state.entities[entity_index].type).has_detection);
                }
                state.entities.remove_at(entity_index);
            } else {
                entity_index++;
            }
        }
    }

    if (state.map_is_fog_dirty) {
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == PLAYER_NONE) {
                continue;
            }
            // Remove any remembered entities (but only if this player can see that they should be removed)
            auto it = state.remembered_entities[player_id].begin();
            while (it != state.remembered_entities[player_id].end()) {
                if ((state.entities.get_index_of(it->first) == INDEX_INVALID || state.entities.get_by_id(it->first).health == 0) && 
                        (map_is_cell_rect_revealed(state, player_id, it->second.cell, it->second.cell_size) || it->second.sprite_params.recolor_id == network_get_player(player_id).recolor_id)) {
                    it = state.remembered_entities[player_id].erase(it);
                } else {
                    it++;
                }
            }
        }
        state.map_is_fog_dirty = false;
    }
}

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const entity_t& entity : state.entities) {
        if (entity.player_id == player_id && entity_is_unit(entity.type) && entity.health != 0) {
            population += ENTITY_DATA.at(entity.type).unit_data.population_cost;
        }
    }
    
    return population;
}

uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id) {
    uint32_t max_population = 0;
    for (const entity_t& building : state.entities) {
        if (building.player_id == player_id && building.mode == MODE_BUILDING_FINISHED && (building.type == ENTITY_HOUSE || building.type == ENTITY_HALL)) {
            max_population += 10;
            if (max_population == 200) {
                return 200;
            }
        }
    }

    return max_population;
}

bool match_player_has_upgrade(const match_state_t& state, uint8_t player_id, uint32_t upgrade) {
    return (state.player_upgrades[player_id] & upgrade) == upgrade;
}

bool match_player_upgrade_is_available(const match_state_t& state, uint8_t player_id, uint32_t upgrade) {
    return ((state.player_upgrades[player_id] | state.player_upgrades_in_progress[player_id]) & upgrade) == 0;
}

void match_grant_player_upgrade(match_state_t& state, uint8_t player_id, uint32_t upgrade) {
    state.player_upgrades[player_id] |= upgrade;
}

entity_id match_get_nearest_builder(const match_state_t& state, const std::vector<entity_id>& builders, xy cell) {
    entity_id nearest_unit_id; 
    int nearest_unit_dist = -1;
    for (entity_id id : builders) {
        int selection_dist = xy::manhattan_distance(cell, state.entities.get_by_id(id).cell);
        if (nearest_unit_dist == -1 || selection_dist < nearest_unit_dist) {
            nearest_unit_id = id;
            nearest_unit_dist = selection_dist;
        }
    }

    return nearest_unit_id;
}

uint32_t match_get_miners_on_gold_mine(const match_state_t& state, uint8_t player_id, entity_id gold_mine_id) {
    uint32_t miner_count = 0;
    for (const entity_t& entity : state.entities) {
        if (entity.player_id == player_id && entity.type == ENTITY_MINER && entity.gold_mine_id == gold_mine_id) {
            miner_count++;
        }
    }
    return miner_count;
}

// EVENTS

void match_event_play_sound(match_state_t& state, Sound sound, xy position) {
    state.events.push_back((match_event_t) {
        .type = MATCH_EVENT_SOUND,
        .sound = (match_event_sound_t) {
            .position = position,
            .sound = sound
        }
    });
}

void match_event_alert(match_state_t& state, MatchAlertType type, uint8_t player_id, xy cell, int cell_size) {
    state.events.push_back((match_event_t) {
        .type = MATCH_EVENT_ALERT,
        .alert = (match_event_alert_t) {
            .type = type,
            .player_id = player_id,
            .cell = cell,
            .cell_size = cell_size
        }
    });
}

void match_event_show_status(match_state_t& state, uint8_t player_id, const char* message) {
    state.events.push_back((match_event_t) {
        .type = MATCH_EVENT_STATUS,
        .status = (match_event_status_t) {
            .player_id = player_id,
            .message = message
        }
    });
}
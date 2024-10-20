#include "match.h"

#include "logger.h"
#include "network.h"
#include "lcg.h"

const uint32_t UNIT_PATH_PAUSE_DURATION = 30;
const uint32_t UNIT_BUILD_TICK_DURATION = 6;
const uint32_t UNIT_MINE_TICK_DURATION = 40;
const uint32_t UNIT_MAX_GOLD_HELD = 10;
const uint32_t UNIT_CANT_BE_FERRIED = 0;
const uint32_t UNIT_IN_DURATION = 40;

const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA = {
    { UNIT_MINER, (unit_data_t) {
        .name = "Miner",
        .sprite = SPRITE_UNIT_MINER,
        .cell_size = 1,
        .max_health = 30,
        .damage = 3,
        .armor = 0,
        .range_squared = 1,
        .attack_cooldown = 16,
        .speed = fixed::from_int_and_raw_decimal(0, 200),
        .sight = 7,
        .cost = 50,
        .population_cost = 1,
        .train_duration = 20,
        .ferry_capacity = 0,
        .ferry_size = 1
    }},
    { UNIT_COWBOY, (unit_data_t) {
        .name = "Cowboy",
        .sprite = SPRITE_UNIT_COWBOY,
        .cell_size = 1,
        .max_health = 40,
        .damage = 4,
        .armor = 1,
        .range_squared = 25,
        .attack_cooldown = 30,
        .speed = fixed::from_int_and_raw_decimal(0, 200),
        .sight = 7,
        .cost = 75,
        .population_cost = 1,
        .train_duration = 25,
        .ferry_capacity = 0,
        .ferry_size = 1
    }},
    { UNIT_WAGON, (unit_data_t) {
        .name = "Wagon",
        .sprite = SPRITE_UNIT_WAGON,
        .cell_size = 2,
        .max_health = 80,
        .damage = 0,
        .armor = 1,
        .range_squared = 1,
        .attack_cooldown = 0,
        .speed = fixed::from_int_and_raw_decimal(1, 80),
        .sight = 10,
        .cost = 100,
        .population_cost = 2,
        .train_duration = 30,
        .ferry_capacity = 4,
        .ferry_size = UNIT_CANT_BE_FERRIED
    }}
};

// Unit

entity_id unit_create(match_state_t& state, uint8_t player_id, UnitType type, const xy& cell) {
    auto it = UNIT_DATA.find(type);
    GOLD_ASSERT(it != UNIT_DATA.end());

    unit_t unit;
    unit.mode = UNIT_MODE_IDLE;
    unit.type = type;
    unit.player_id = player_id;
    unit.animation = animation_create(ANIMATION_UNIT_IDLE);
    unit.health = it->second.max_health;

    unit.direction = DIRECTION_SOUTH;
    unit.cell = cell;
    unit.position = unit_get_target_position(unit.type, unit.cell);
    unit.target = (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
    unit.hold_position = false;

    unit.gold_held = 0;
    unit.timer = 0;
    unit.garrison_id = ID_NULL;

    unit.taking_damage_timer = 0;
    unit.taking_damage_flicker_timer = 0;
    unit.taking_damage_flicker = false;

    entity_id unit_id = state.units.push_back(unit);
    map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_UNIT, unit_id);
    return unit_id;
}

void unit_update(match_state_t& state, uint32_t unit_index) {
    entity_id unit_id = state.units.get_id_of(unit_index);
    unit_t& unit = state.units[unit_index];
    const unit_data_t& unit_data = UNIT_DATA.at(unit.type);

                                                                                                   // Don't play death animation for ferried units
    if (unit.health == 0 && !(unit.mode == UNIT_MODE_DEATH || unit.mode == UNIT_MODE_DEATH_FADE || unit.mode == UNIT_MODE_FERRY)) {
        unit.mode = UNIT_MODE_DEATH;
        unit.animation = animation_create(unit_get_expected_animation(unit));

        for (entity_id ferried_id : unit.ferried_units) {
            state.units.get_by_id(ferried_id).health = 0;
        }

        return;
    }

    bool unit_update_finished = false;
    fixed movement_left = unit_data.speed;
    while (!unit_update_finished) {
        switch (unit.mode) {
            case UNIT_MODE_IDLE: {
                // If unit is idle, try to find a nearby target
                if (unit.target.type == UNIT_TARGET_NONE && unit.type != UNIT_MINER && UNIT_DATA.at(unit.type).damage != 0) {
                    unit.target = unit_target_nearest_insight_enemy(state, unit);
                }
                if (unit.target.type == UNIT_TARGET_NONE) {
                    unit_update_finished = true;
                    break;
                } else if ((unit.target.type == UNIT_TARGET_UNIT || unit.target.type == UNIT_TARGET_BUILDING || unit.target.type == UNIT_TARGET_BUILD_ASSIST) && unit_is_target_dead_or_ferried(state, unit)) {
                    unit.target = (unit_target_t) {
                        .type = UNIT_TARGET_NONE
                    };
                    break;
                } else if (unit_has_reached_target(state, unit)) {
                    unit.mode = UNIT_MODE_MOVE_FINISHED;
                    break;
                } else if (!unit.hold_position) {
                    map_pathfind(state, unit.cell, unit_get_target_cell(state, unit), unit_cell_size(unit.type), &unit.path, unit.target.type == UNIT_TARGET_MINE || unit.target.type == UNIT_TARGET_CAMP);
                    if (!unit.path.empty()) {
                        unit.mode = UNIT_MODE_MOVE;
                        break;
                    } else {
                        if (unit.target.type == UNIT_TARGET_BUILD) {
                            ui_show_status(state, UI_STATUS_CANT_BUILD);
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit_update_finished = true;
                            break;
                        } 

                        unit.timer = UNIT_PATH_PAUSE_DURATION;
                        unit.mode = UNIT_MODE_MOVE_BLOCKED;
                        unit_update_finished = true;
                        break;
                    } 
                }
            } // End case UNIT_MODE_IDLE
            case UNIT_MODE_MOVE_BLOCKED: {
                unit.timer--;
                if (unit.timer == 0) {
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_MOVE: {
                bool is_path_blocked = false;
                while (movement_left.raw_value > 0) {
                    // If the unit is not moving between tiles, then pop the next cell off the path
                    if (unit.position == unit_get_target_position(unit.type, unit.cell) && !unit.path.empty()) {
                        unit.direction = get_enum_direction_from_xy_direction(unit.path[0] - unit.cell);
                        if (map_is_cell_rect_occupied(state, rect_t(unit.path[0], unit_cell_size(unit.type)), unit.cell, unit.target.type == UNIT_TARGET_MINE || unit.target.type == UNIT_TARGET_CAMP)) {
                            is_path_blocked = true;
                            // Since we are not moving, repopulate the cell grid with the unit's rect
                            map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_UNIT, unit_id);
                            // breaks out of while movement_left > 0
                            break;
                        }

                        map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_EMPTY);
                        unit.cell = unit.path[0];
                        map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_UNIT, unit_id);
                        unit.path.erase(unit.path.begin());
                    }

                    // Step unit along movement
                    if (unit.position.distance_to(unit_get_target_position(unit.type, unit.cell)) > movement_left) {
                        unit.position += DIRECTION_XY_FIXED[unit.direction] * movement_left;
                        movement_left = fixed::from_raw(0);
                    } else {
                        movement_left -= unit.position.distance_to(unit_get_target_position(unit.type, unit.cell));
                        unit.position = unit_get_target_position(unit.type, unit.cell);
                        // On step finished
                        if (unit.target.type == UNIT_TARGET_ATTACK) {
                            unit_target_t attack_target = unit_target_nearest_insight_enemy(state, unit);
                            if (attack_target.type != UNIT_TARGET_NONE) {
                                unit.target = attack_target;
                                unit.path.clear();
                                unit.mode = unit_has_reached_target(state, unit) ? UNIT_MODE_MOVE_FINISHED : UNIT_MODE_IDLE;
                                // breaks out of while movement_left > 0
                                break;
                            }
                        }
                        if (unit_has_reached_target(state, unit) || unit.garrison_id == unit.target.id) {
                            unit.mode = UNIT_MODE_MOVE_FINISHED;
                            unit.path.clear();
                            // breaks out of while movement_left > 0
                            break;
                        }
                        if (unit.path.empty()) {
                            unit.mode = UNIT_MODE_IDLE;
                            // breaks out of while movement_left > 0
                            break;
                        }
                    }
                } // End while movement_left > 0

                if (is_path_blocked) {
                    // Trigger repath
                    unit.mode = UNIT_MODE_MOVE_BLOCKED;
                    unit.timer = UNIT_PATH_PAUSE_DURATION;
                    unit_update_finished = true;
                    break;
                }

                unit_update_finished = unit.mode != UNIT_MODE_MOVE_FINISHED;
                break;
            } // End case UNIT_MODE_MOVE
            case UNIT_MODE_MOVE_FINISHED: {
                switch (unit.target.type) {
                    case UNIT_TARGET_NONE:
                        unit.mode = UNIT_MODE_IDLE;
                        break;
                    case UNIT_TARGET_ATTACK:
                    case UNIT_TARGET_CELL: {
                        unit.target = (unit_target_t) {
                            .type = UNIT_TARGET_NONE
                        };
                        unit.mode = UNIT_MODE_IDLE;
                        break;
                    }
                    case UNIT_TARGET_UNLOAD: {
                        // Unload ferried units
                        for (uint32_t ferried_id_index = 0; ferried_id_index < unit.ferried_units.size(); ferried_id_index++) {
                            entity_id ferried_id = unit.ferried_units[ferried_id_index];
                            log_trace("Attempt unload for unit ferried id index %u ferried id %u", ferried_id_index, ferried_id);
                            unit_t& ferried_unit = state.units.get_by_id(ferried_id);
                            xy dropoff_cell = unit_get_best_unload_cell(state, unit, unit_cell_size(ferried_unit.type));
                            log_trace("Dropoff cell %xi", &dropoff_cell);
                            // If this is true, then no free spaces are available to unload
                            if (dropoff_cell == xy(-1, -1)) {
                                log_trace("No free cells!");
                                break;
                            }

                            ferried_unit.cell = dropoff_cell;
                            ferried_unit.position = unit_get_target_position(ferried_unit.type, ferried_unit.cell);
                            map_set_cell_rect(state, rect_t(ferried_unit.cell, unit_cell_size(ferried_unit.type)), CELL_UNIT, ferried_id);
                            ferried_unit.mode = UNIT_MODE_IDLE;
                            ferried_unit.target = (unit_target_t) { .type = UNIT_TARGET_NONE };
                            unit.ferried_units.erase(unit.ferried_units.begin() + ferried_id_index);
                            ferried_id_index--;
                        }
                        // Set this units target to none
                        unit.target = (unit_target_t) {
                            .type = UNIT_TARGET_NONE
                        };
                        unit.mode = UNIT_MODE_IDLE;
                        // Check if this unit is selected, if it is, refresh the UI buttons
                        if (state.ui_buttonset == UI_BUTTONSET_WAGON) {
                            bool is_selecting_this_unit = false;
                            for (entity_id id : state.selection.ids) {
                                if (id == state.units.get_id_of(unit_index)) {
                                    is_selecting_this_unit = true;
                                    break;
                                }
                            }
                            if (is_selecting_this_unit) {
                                ui_set_selection(state, state.selection);
                            }
                        }
                        break;
                    }
                    case UNIT_TARGET_BUILD: {
                        // Temporarily set cell to empty so that we can check if the space is clear for building
                        map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_EMPTY);
                        bool can_build = unit.cell == unit.target.build.unit_cell && 
                                                        !map_is_cell_rect_occupied(state, rect_t(unit.target.build.building_cell, building_cell_size(unit.target.build.building_type)));
                        if (!can_build) {
                            // Set the cell back to filled
                            map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_UNIT, state.units.get_id_of(unit_index));
                            ui_show_status(state, UI_STATUS_CANT_BUILD);
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        } 

                        if (state.player_gold[unit.player_id] < BUILDING_DATA.at(unit.target.build.building_type).cost) {
                            ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }

                        // Place building
                        state.player_gold[unit.player_id] -= BUILDING_DATA.at(unit.target.build.building_type).cost;
                        unit.target.build.building_id = building_create(state, unit.player_id, unit.target.build.building_type, unit.target.build.building_cell);
                        unit.timer = UNIT_BUILD_TICK_DURATION;
                        unit.mode = UNIT_MODE_BUILD;

                        // De-select the unit if it is selected
                        auto it = std::find(state.selection.ids.begin(), state.selection.ids.end(), unit_id);
                        if (it != state.selection.ids.end()) {
                            // De-select the unit
                            state.selection.ids.erase(it);

                            // If the selection is now empty, select the building instead
                            if (state.selection.ids.empty()) {
                                state.selection.type = SELECTION_TYPE_BUILDINGS;
                                state.selection.ids.push_back(unit.target.build.building_id);
                            }

                            ui_set_selection(state, state.selection);
                        }
                        break;
                    } // End case UNIT_TARGET_BUILD
                    case UNIT_TARGET_BUILD_ASSIST: {
                        if (unit_is_target_dead_or_ferried(state, unit)) {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }
                        unit_t& builder = state.units.get_by_id(unit.target.id);
                        if (builder.mode == UNIT_MODE_BUILD) {
                            GOLD_ASSERT(builder.target.build.building_id != ID_NULL);
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_BUILDING,
                                .id = builder.target.build.building_id
                            };
                            unit.mode = UNIT_MODE_REPAIR;
                        }
                        break;
                    }
                    case UNIT_TARGET_MINE: {
                        // Check if target is valid
                        uint32_t mine_index = state.mines.get_index_of(unit.target.id);
                        if (mine_index == INDEX_INVALID || state.mines[mine_index].gold_left == 0) {
                            unit.mode = UNIT_MODE_IDLE;
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            break;
                        }

                        // Confirm that unit has reached target
                        if (!unit_has_reached_target(state, unit) && unit.garrison_id != unit.target.id) {
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        } 

                        // Make sure that the mine / camp is free
                        mine_t& mine = state.mines[mine_index];
                        // If this mine / camp is reserved by the current unit, then we can go inside
                        if (mine.occupancy == OCCUPANCY_RESERVED && unit.garrison_id == unit.target.id) {
                            unit.gold_held = std::min(UNIT_MAX_GOLD_HELD, mine.gold_left);
                            unit.mode = UNIT_MODE_IN_MINE;
                            mine.gold_left -= unit.gold_held;
                            mine.occupancy = OCCUPANCY_FULL;
                            unit.timer = UNIT_IN_DURATION;
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            state.is_fog_dirty = true;
                            break;
                        } 
                        
                        // If this mine / camp is occupied or reserved, then wait
                        if (mine.occupancy != OCCUPANCY_EMPTY) {
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }

                        // Otherwise, if the mine is available, reserve it and begin walking into it it
                        mine.occupancy = OCCUPANCY_RESERVED;
                        map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_EMPTY);
                        rect_t enter_rect = rect_t(mine.cell, xy(MINE_SIZE, MINE_SIZE));
                        unit.direction = get_enum_direction_to_rect(unit.cell, enter_rect);
                        unit.cell = unit.cell + DIRECTION_XY[unit.direction];
                        unit.path.clear();
                        unit.mode = UNIT_MODE_MOVE;
                        unit.garrison_id = unit.target.id;
                        ui_deselect_unit_if_selected(state, state.units.get_id_of(unit_index));
                        break;
                    }
                    case UNIT_TARGET_CAMP: {
                        // Check if target is valid
                        uint32_t camp_index = state.buildings.get_index_of(unit.target.id);
                        if (camp_index == INDEX_INVALID || state.buildings[camp_index].health == 0) {
                            unit.mode = UNIT_MODE_IDLE;
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            break;
                        }

                        // Confirm that unit has reached target
                        if (!unit_has_reached_target(state, unit)) {
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        } 

                        state.player_gold[unit.player_id] += unit.gold_held;
                        unit.gold_held = 0;
                        unit.mode = UNIT_MODE_IDLE;
                        unit.target = unit_target_nearest_mine(state, unit);
                        break;
                    }
                    case UNIT_TARGET_UNIT: 
                    case UNIT_TARGET_BUILDING: {
                        if (unit_is_target_dead_or_ferried(state, unit)) {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }

                        if (!unit_has_reached_target(state, unit)) {
                            // This will trigger a repath
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }

                        uint32_t target_index = unit.target.type == UNIT_TARGET_UNIT 
                                                    ? state.units.get_index_of(unit.target.id)
                                                    : state.buildings.get_index_of(unit.target.id);
                        GOLD_ASSERT(target_index != INDEX_INVALID);

                        uint8_t target_player_id = unit.target.type == UNIT_TARGET_UNIT
                                            ? state.units[target_index].player_id
                                            : state.buildings[target_index].player_id;
                        if (unit.player_id == target_player_id && unit.target.type == UNIT_TARGET_UNIT) {
                            uint32_t target_ferry_capacity = UNIT_DATA.at(state.units[target_index].type).ferry_capacity;
                            if (target_ferry_capacity != 0) {
                                for (entity_id ferried_unit_id : state.units[target_index].ferried_units) {
                                    target_ferry_capacity -= UNIT_DATA.at(state.units.get_by_id(ferried_unit_id).type).ferry_size;
                                }

                                if (target_ferry_capacity >= UNIT_DATA.at(unit.type).ferry_size) {
                                    state.units[target_index].ferried_units.push_back(state.units.get_id_of(unit_index));
                                    unit.mode = UNIT_MODE_FERRY;
                                    unit_update_finished = true;
                                    map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_EMPTY);
                                    // Force re-select so that unit is deselected if needed and so that ferry unload button pops up if needed
                                    ui_set_selection(state, state.selection);
                                    break;
                                }
                            }
                            unit.mode = UNIT_MODE_IDLE;
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit_update_finished = true;
                            break;
                        }

                        // Begin repairing building
                        if (unit.player_id == target_player_id && unit.type == UNIT_MINER && unit.target.type == UNIT_TARGET_BUILDING) {
                            building_t& building = state.buildings.get_by_id(unit.target.id);
                            if (building.health < BUILDING_DATA.at(building.type).max_health) {
                                unit.mode = UNIT_MODE_REPAIR;
                                if (unit.cell.y >= building.cell.y && unit.cell.y < building.cell.y + building_cell_size(building.type).y) {
                                    if (unit.cell.x < building.cell.x) {
                                        unit.direction = DIRECTION_EAST;
                                    } else {
                                        unit.direction = DIRECTION_WEST;
                                    }
                                } else {
                                    if (unit.cell.y < building.cell.y) {
                                        unit.direction = DIRECTION_SOUTH;
                                    } else {
                                        unit.direction = DIRECTION_NORTH;
                                    }
                                }
                                log_trace("index: %u, Beginning repair state. Direction: %u", unit_index, unit.direction);
                                unit.timer = UNIT_BUILD_TICK_DURATION;
                                unit_update_finished = true;
                                break;
                            }
                        }
                        
                        // Don't attack allied units
                        if (unit.player_id == target_player_id) {
                            unit.mode = UNIT_MODE_IDLE;
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit_update_finished = true;
                            break;
                        }

                        // Don't attack if this is a non-violent unit
                        if (UNIT_DATA.at(unit.type).damage == 0) {
                            unit.mode = UNIT_MODE_IDLE;
                            unit_update_finished = true;
                            break;
                        }

                        // If target is building a building, attack that building instead
                        if (unit.target.type == UNIT_TARGET_UNIT && state.units[target_index].mode == UNIT_MODE_BUILD) {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_BUILDING,
                                .id = state.units[target_index].target.build.building_id
                            };
                            target_index = state.buildings.get_index_of(unit.target.id);
                            GOLD_ASSERT(target_index != INDEX_INVALID);

                            if (!unit_has_reached_target(state, unit)) {
                                unit.mode = UNIT_MODE_IDLE;
                                break;
                            }
                        }

                        rect_t target_rect = unit.target.type == UNIT_TARGET_UNIT
                                            ? rect_t(state.units[target_index].cell, unit_cell_size(state.units[target_index].type))
                                            : rect_t(state.buildings[target_index].cell, building_cell_size(state.buildings[target_index].type));
                        unit.direction = get_enum_direction_to_rect(unit.cell, target_rect);
                        unit.mode = UNIT_MODE_ATTACK_WINDUP;
                        break;
                    }
                } // End switch unit target type
                unit_update_finished = !(unit.mode == UNIT_MODE_MOVE && movement_left.raw_value > 0);
                break;
            } // End case UNIT_MODE_MOVE_FINISHED
            case UNIT_MODE_BUILD: {
                // This code handles the case where 1. the building is destroyed while the unit is building it
                // and 2. the unit was unable to exit the building and is now stuck inside it
                uint32_t building_index = state.buildings.get_index_of(unit.target.build.building_id);
                if (building_index == INDEX_INVALID || state.buildings[building_index].mode != BUILDING_MODE_IN_PROGRESS) {
                    unit_stop_building(state, state.units.get_id_of(unit_index));
                    unit_update_finished = true;
                    break;
                }

                unit.timer--;
                if (unit.timer == 0) {
                    // Building tick
                    building_t& building = state.buildings[building_index];

                    int building_hframe = building_get_hframe(building.type, building.mode, building.health, building.occupancy);
#ifdef GOLD_DEBUG_FAST_BUILD
                    building.health = std::min(building.health + 20, BUILDING_DATA.at(building.type).max_health);
#else
                    building.health++;
#endif
                    if (building.health == BUILDING_DATA.at(building.type).max_health) {
                        building_on_finish(state, unit.target.build.building_id);
                    } else {
                        unit.timer = UNIT_BUILD_TICK_DURATION;
                    }

                    if (building_get_hframe(building.type, building.mode, building.health, building.occupancy) != building_hframe) {
                        state.is_fog_dirty = true;
                    }
                }
                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_REPAIR: {
                // Stop repairing if the building is destroyed
                if (unit_is_target_dead_or_ferried(state, unit)) {
                    unit.target = (unit_target_t) {
                        .type = UNIT_TARGET_NONE
                    };
                    unit.mode = UNIT_MODE_IDLE;
                    unit_update_finished = true;
                    break;
                }

                building_t& building = state.buildings.get_by_id(unit.target.id);

                if (building.health == BUILDING_DATA.at(building.type).max_health) {
                    unit.target = (unit_target_t) {
                        .type = UNIT_TARGET_NONE
                    };
                    unit.mode = UNIT_MODE_IDLE;
                }

                unit.timer--;
                if (unit.timer == 0) {
                    int building_hframe = building_get_hframe(building.type, building.mode, building.health, building.occupancy);
                    building.health++;
                    if (building.health == BUILDING_DATA.at(building.type).max_health) {
                        if (building.mode == BUILDING_MODE_IN_PROGRESS) {
                            building_on_finish(state, unit.target.id);
                        } else {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                        }
                    } else {
                        unit.timer = UNIT_BUILD_TICK_DURATION;
                    }
                    if (building_get_hframe(building.type, building.mode, building.queue_timer, building.occupancy) != building_hframe) {
                        state.is_fog_dirty = true;
                    }
                }
                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_IN_MINE: {
                bool timer_just_ended = false;
                if (unit.timer != 0) {
                    unit.timer--;
                    if (unit.timer == 0) {
                        timer_just_ended = true;
                    }
                }
                if (unit.timer == 0) {
                    mine_t& mine = state.mines.get_by_id(unit.garrison_id);
                    rect_t mine_rect = rect_t(mine.cell, xy(MINE_SIZE, MINE_SIZE));
                    unit_target_t camp_target = unit_target_nearest_camp(state, unit.cell, unit.player_id);
                    rect_t camp_rect;

                    xy preferred_exit_cell = xy(-1, -1);
                    if (camp_target.type != UNIT_TARGET_NONE) {
                        camp_rect = rect_t(state.buildings.get_by_id(camp_target.id).cell, building_cell_size(BUILDING_CAMP));
                        bool y_overlaps = !(camp_rect.position.y < mine_rect.position.y || 
                                            camp_rect.position.y + camp_rect.size.y - 1 > mine_rect.position.y + mine_rect.size.y - 1);
                        bool x_overlaps = !(camp_rect.position.x < mine_rect.position.x || 
                                            camp_rect.position.x + camp_rect.size.x - 1 > mine_rect.position.x + mine_rect.size.x - 1);
                        if (x_overlaps) {
                            if (camp_rect.position.y > mine_rect.position.y) {
                                preferred_exit_cell = mine_rect.position + xy(1, mine_rect.size.y);
                            } else {
                                preferred_exit_cell = mine_rect.position + xy(1, -1);
                            }
                        } else if (y_overlaps) {
                            if (camp_rect.position.x > mine_rect.position.x) {
                                preferred_exit_cell = mine_rect.position + xy(mine_rect.size.x, 1);
                            } else {
                                preferred_exit_cell = mine_rect.position + xy(-1, 1);
                            }
                        } else {
                            preferred_exit_cell.y = camp_rect.position.y > mine_rect.position.y
                                                        ? mine_rect.position.y + mine_rect.size.y
                                                        : mine_rect.position.y - 1;
                            preferred_exit_cell.x = camp_rect.position.x > mine_rect.position.x
                                                        ? mine_rect.position.x + mine_rect.size.x - 1
                                                        : mine_rect.position.x;
                        }
                    }
                    // Check to make sure that the unit can exit
                    xy exit_cell = !map_is_cell_rect_occupied(state, rect_t(preferred_exit_cell, unit_cell_size(unit.type)), xy(-1, -1), true)
                                        ? preferred_exit_cell
                                        : get_exit_cell(state, mine_rect, unit_cell_size(unit.type), preferred_exit_cell);
                    if (exit_cell.x != -1) {
                        mine_t& mine = state.mines.get_by_id(unit.garrison_id);
                        mine.occupancy = OCCUPANCY_EMPTY;

                        if (unit.player_id == network_get_player_id() && mine.gold_left == 0) {
                            ui_show_status(state, UI_STATUS_MINE_COLLAPSED);
                            rect_t mine_screen_rect = rect_t(mine_rect.position * TILE_SIZE, mine_rect.size * TILE_SIZE);
                            rect_t screen_rect = rect_t(state.camera_offset, xy(SCREEN_WIDTH, SCREEN_HEIGHT));
                            if (!screen_rect.intersects(mine_screen_rect)) {
                                state.alerts.push_back((alert_t) {
                                    .type = ALERT_GOLD,
                                    .status = ALERT_STATUS_SHOW,
                                    .cell = mine.cell,
                                    .cell_size = xy(MINE_SIZE, MINE_SIZE),
                                    .timer = MATCH_ALERT_DURATION
                                });
                            }
                        }

                        unit.target = camp_target;
                        unit.cell = exit_cell;
                        unit.position = cell_center(unit.cell);
                        map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_UNIT, state.units.get_id_of(unit_index));
                        unit.mode = UNIT_MODE_IDLE;
                        unit.direction = get_enum_direction_to_rect(unit.cell, camp_rect);
                        unit.garrison_id = ID_NULL;
                    } else {
                        if (timer_just_ended && unit.player_id == network_get_player_id()) {
                            ui_show_status(state, UI_STATUS_MINE_EXIT_BLOCKED);
                        }
                    }
                }
                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_ATTACK_WINDUP: {
                if (unit_is_target_dead_or_ferried(state, unit)) {
                    unit.target = unit_target_nearest_insight_enemy(state, unit);
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                if (!animation_is_playing(unit.animation)) {
                    bool attack_missed = false;

                    rect_t enemy_rect;
                    if (unit.target.type == UNIT_TARGET_UNIT) {
                        uint32_t enemy_index = state.units.get_index_of(unit.target.id);
                        GOLD_ASSERT(enemy_index != INDEX_INVALID);
                        unit_t& enemy = state.units[enemy_index];
                        enemy_rect = unit_get_rect(enemy);

                        int damage = std::max(1, unit_get_damage(state, unit) - unit_get_armor(state, enemy));
                        // Elevation accuracy disadvantage - If unit is on lower elevation than target, unit has 50% chance to miss
                        if (unit_get_elevation(state, unit) < unit_get_elevation(state, enemy)) {
                            if (lcg_rand() % 2 == 0) {
                                damage = 0;
                                attack_missed = true;
                            }
                        }
                        enemy.health = std::max(0, enemy.health - damage);
                        if (enemy.taking_damage_timer == 0) {
                            // Only show the alert if the enemy being attacked belongs to the current player
                            if (enemy.player_id == network_get_player_id()) {
                                // Only show the alert if the enemy being attacked is not on screen
                                rect_t screen_rect = rect_t(state.camera_offset, xy(SCREEN_WIDTH, SCREEN_HEIGHT - UI_HEIGHT));
                                rect_t enemy_rect = unit_get_rect(enemy);
                                if (!screen_rect.intersects(enemy_rect)) {
                                    bool is_existing_attack_alert_close_by = false;
                                    for (const alert_t& alert : state.alerts) {
                                        if (xy::manhattan_distance(alert.cell, enemy.cell) < MATCH_ATTACK_ALERT_DISTANCE) {
                                            is_existing_attack_alert_close_by = true;
                                            break;
                                        }
                                    }

                                    // Only show the alert if this is the first enemy being attacked
                                    if (!is_existing_attack_alert_close_by) {
                                        state.alerts.push_back((alert_t) {
                                            .type = ALERT_RED,
                                            .status = ALERT_STATUS_SHOW,
                                            .cell = enemy.cell,
                                            .cell_size = unit_cell_size(enemy.type),
                                            .timer = MATCH_ALERT_DURATION
                                        });
                                        ui_show_status(state, UI_STATUS_UNDER_ATTACK);
                                    }
                                }
                            }

                            enemy.taking_damage_flicker = true;
                            enemy.taking_damage_flicker_timer = MATCH_TAKING_DAMAGE_FLICKER_TIMER_DURATION;
                        }
                        enemy.taking_damage_timer = MATCH_TAKING_DAMAGE_TIMER_DURATION;

                        // Make the enemy attack back
                        if (enemy.mode == UNIT_MODE_IDLE && enemy.target.type == UNIT_TARGET_NONE && UNIT_DATA.at(enemy.type).damage != 0 && unit_can_see_rect(enemy, rect_t(unit.cell, unit_cell_size(unit.type)))) {
                            enemy.target = (unit_target_t) {
                                .type = UNIT_TARGET_UNIT,
                                .id = unit_id
                            };
                        }
                    } else if (unit.target.type == UNIT_TARGET_BUILDING) {
                        uint32_t enemy_index = state.buildings.get_index_of(unit.target.id);
                        GOLD_ASSERT(enemy_index != INDEX_INVALID);
                        building_t& enemy = state.buildings[enemy_index];
                        enemy_rect = building_get_rect(enemy);

                        // NOTE: Buildings have no armor 
                        int damage = std::min(1, unit_get_damage(state, unit));
                        enemy.health = std::max(0, enemy.health - damage);
                        if (enemy.taking_damage_timer == 0) {
                            // Only show the alert if the enemy being attacked belongs to the current player
                            if (enemy.player_id == network_get_player_id()) {
                                // Only show the alert if the enemy being attacked is not on screen
                                rect_t screen_rect = rect_t(state.camera_offset, xy(SCREEN_WIDTH, SCREEN_HEIGHT - UI_HEIGHT));
                                rect_t enemy_rect = building_get_rect(enemy);
                                if (!screen_rect.intersects(enemy_rect)) {
                                    bool is_existing_attack_alert_close_by = false;
                                    for (const alert_t& alert : state.alerts) {
                                        if (alert.type == ALERT_RED && xy::manhattan_distance(alert.cell, enemy.cell) < MATCH_ATTACK_ALERT_DISTANCE) {
                                            is_existing_attack_alert_close_by = true;
                                            break;
                                        }
                                    }

                                    // Only show the alert if this is the first enemy being attacked
                                    if (!is_existing_attack_alert_close_by) {
                                        state.alerts.push_back((alert_t) {
                                            .type = ALERT_RED,
                                            .status = ALERT_STATUS_SHOW,
                                            .cell = enemy.cell,
                                            .cell_size = building_cell_size(enemy.type),
                                            .timer = MATCH_ALERT_DURATION
                                        });
                                        ui_show_status(state, UI_STATUS_UNDER_ATTACK);
                                    }
                                }
                            }

                            enemy.taking_damage_flicker = true;
                            enemy.taking_damage_flicker_timer = MATCH_TAKING_DAMAGE_FLICKER_TIMER_DURATION;
                        }
                        enemy.taking_damage_timer = MATCH_TAKING_DAMAGE_TIMER_DURATION;
                    }

                    // Create particle effects
                    if (!attack_missed) {
                        if (unit.type == UNIT_COWBOY) {
                            xy particle_position;
                            particle_position.x = enemy_rect.position.x + (enemy_rect.size.x / 4) + (lcg_rand() % (enemy_rect.size.x / 2));
                            particle_position.y = enemy_rect.position.y + (enemy_rect.size.y / 4) + (lcg_rand() % (enemy_rect.size.y / 2));
                            state.particles.push_back((particle_t) {
                                .sprite = SPRITE_PARTICLE_SPARKS,
                                .animation = animation_create(ANIMATION_PARTICLE_SPARKS),
                                .vframe = lcg_rand() % 3,
                                .position = particle_position
                            });
                        }
                    }

                    unit.timer = unit_data.attack_cooldown;
                    unit.mode = UNIT_MODE_ATTACK_COOLDOWN;
                } // End if animation not playing

                unit_update_finished = true;
                break;
            } // End case UNIT_MODE_ATTACK_WINDUP
            case UNIT_MODE_ATTACK_COOLDOWN: {
                if (unit_is_target_dead_or_ferried(state, unit)) {
                    // TODO target nearest enemy
                    unit.timer = 0;
                    unit.target = unit_target_nearest_insight_enemy(state, unit);
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                if (!unit_has_reached_target(state, unit)) {
                    // Unit needs to repath to the target
                    unit.timer = 0;
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                unit.timer--;
                if (unit.timer == 0) {
                    unit.mode = UNIT_MODE_ATTACK_WINDUP;
                }

                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_FERRY:
                unit_update_finished = true;
                break;
            case UNIT_MODE_DEATH: {
                if (!animation_is_playing(unit.animation)) {
                    map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_EMPTY);
                    unit.mode = UNIT_MODE_DEATH_FADE;
                }
                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_DEATH_FADE: {
                // Removing the unit is handled in match_update()
                unit_update_finished = true;
                break;
            }
        }
    } // End while !unit_update_finished

    if (unit.taking_damage_timer > 0) {
        unit.taking_damage_timer--;
        if (unit.taking_damage_timer == 0) {
            unit.taking_damage_flicker = false;
        } else {
            unit.taking_damage_flicker_timer--;
            if (unit.taking_damage_flicker_timer == 0) {
                unit.taking_damage_flicker_timer = MATCH_TAKING_DAMAGE_FLICKER_TIMER_DURATION;
                unit.taking_damage_flicker = !unit.taking_damage_flicker;
            }
        }
    }
}

xy unit_cell_size(UnitType type) {
    return xy(UNIT_DATA.at(type).cell_size, UNIT_DATA.at(type).cell_size);
}

rect_t unit_get_rect(const unit_t& unit) {
    int unit_size = UNIT_DATA.at(unit.type).cell_size * TILE_SIZE;
    return rect_t(unit.position.to_xy() - xy(unit_size / 2, unit_size / 2), xy(unit_size, unit_size));
}

int8_t unit_get_elevation(const match_state_t& state, const unit_t& unit) {
    int8_t elevation = map_get_elevation(state, unit.cell);
    for (int x = unit.cell.x; x < unit.cell.x + unit_cell_size(unit.type).x; x++) {
        for (int y = unit.cell.y; y < unit.cell.y + unit_cell_size(unit.type).y; y++) {
            elevation = std::max(elevation, map_get_elevation(state, xy(x, y)));
        }
    }
    if (unit.mode == UNIT_MODE_MOVE) {
        xy unit_prev_cell = unit.cell - DIRECTION_XY[unit.direction];
        for (int x = unit_prev_cell.x; x < unit_prev_cell.x + unit_cell_size(unit.type).x; x++) {
            for (int y = unit_prev_cell.y; y < unit_prev_cell.y + unit_cell_size(unit.type).y; y++) {
                elevation = std::max(elevation, map_get_elevation(state, xy(x, y)));
            }
        }
    }

    return elevation;
}

void unit_set_target(const match_state_t& state, unit_t& unit, unit_target_t target) {
    GOLD_ASSERT(unit.mode != UNIT_MODE_BUILD);
    unit.target = target;
    unit.path.clear();
    unit.hold_position = false;

    if (unit.mode != UNIT_MODE_MOVE && unit.mode != UNIT_MODE_IN_MINE) {
        // Abandon current behavior in favor of new order
        unit.timer = 0;
        unit.mode = UNIT_MODE_IDLE;
    }
}

xy unit_get_target_cell(const match_state_t& state, const unit_t& unit) {
    switch (unit.target.type) {
        case UNIT_TARGET_NONE:
            return unit.cell;
        case UNIT_TARGET_BUILD:
            return unit.target.build.unit_cell;
        case UNIT_TARGET_BUILD_ASSIST: {
            const unit_t& builder = state.units.get_by_id(unit.target.id);
            rect_t building_rect = rect_t(builder.target.build.building_cell, building_cell_size(builder.target.build.building_type));
            rect_t unit_rect = rect_t(unit.cell, unit_cell_size(unit.type));
            return get_nearest_cell_around_rect(state, unit_rect, building_rect);
        }
        case UNIT_TARGET_ATTACK:
        case UNIT_TARGET_UNLOAD:
        case UNIT_TARGET_CELL:
            return unit.target.cell;
        case UNIT_TARGET_MINE:
            return get_nearest_cell_around_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), rect_t(state.mines.get_by_id(unit.target.id).cell, xy(3, 3)), true);
        case UNIT_TARGET_UNIT: {
            uint32_t target_unit_index = state.units.get_index_of(unit.target.id);
            if (target_unit_index == INDEX_INVALID || state.units[target_unit_index].health == 0) {
                return unit.cell;
            }
            return get_nearest_cell_around_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), rect_t(state.units[target_unit_index].cell, unit_cell_size(state.units[target_unit_index].type)));
        }
        case UNIT_TARGET_BUILDING:
        case UNIT_TARGET_CAMP: {
            rect_t camp_rect = rect_t(state.buildings.get_by_id(unit.target.id).cell, building_cell_size(BUILDING_CAMP));
            return get_nearest_cell_around_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), camp_rect, unit.target.type == UNIT_TARGET_CAMP);
        }
    }
}

xy_fixed unit_get_target_position(UnitType type, xy cell) {
    int unit_size = UNIT_DATA.at(type).cell_size * TILE_SIZE;
    return xy_fixed((cell * TILE_SIZE) + xy(unit_size / 2, unit_size / 2));
}

bool unit_has_reached_target(const match_state_t& state, const unit_t& unit) {
    switch (unit.target.type) {
        case UNIT_TARGET_NONE:
            return true;
        case UNIT_TARGET_BUILD:
            return unit.cell == unit.target.build.unit_cell;
        case UNIT_TARGET_BUILD_ASSIST: {
            const unit_t& builder = state.units.get_by_id(unit.target.id);
            rect_t building_rect = rect_t(builder.target.build.building_cell, building_cell_size(builder.target.build.building_type));
            rect_t unit_rect = rect_t(unit.cell, unit_cell_size(unit.type));
            return unit_rect.is_adjacent_to(building_rect); 
        }
        case UNIT_TARGET_ATTACK:
        case UNIT_TARGET_CELL:
            return unit.cell == unit.target.cell;
        case UNIT_TARGET_UNLOAD:
            return unit.path.empty() && xy::manhattan_distance(unit.cell, unit.target.cell) < 3;
        case UNIT_TARGET_MINE: {
            rect_t unit_rect = rect_t(unit.cell, unit_cell_size(unit.type));
            rect_t mine_rect = rect_t(state.mines.get_by_id(unit.target.id).cell, xy(3, 3));
            return unit_rect.is_adjacent_to(mine_rect); 
        }
        case UNIT_TARGET_UNIT: {
            uint32_t target_unit_index = state.units.get_index_of(unit.target.id);
            GOLD_ASSERT(target_unit_index != INDEX_INVALID);
            rect_t unit_rect = rect_t(unit.cell, unit_cell_size(unit.type));
            rect_t target_unit_rect = rect_t(state.units[target_unit_index].cell, unit_cell_size(state.units[target_unit_index].type));
            return state.units[target_unit_index].player_id == unit.player_id || UNIT_DATA.at(unit.type).range_squared == 1
                        ? unit_rect.is_adjacent_to(target_unit_rect) 
                        : unit_rect.euclidean_distance_squared_to(target_unit_rect) <= UNIT_DATA.at(unit.type).range_squared;
        }
        case UNIT_TARGET_BUILDING: {
            uint32_t target_building_index = state.buildings.get_index_of(unit.target.id);
            GOLD_ASSERT(target_building_index != INDEX_INVALID);
            const building_t& target_building = state.buildings[target_building_index];
            rect_t unit_rect = rect_t(unit.cell, unit_cell_size(unit.type));
            rect_t building_rect = rect_t(target_building.cell, building_cell_size(target_building.type));
            return unit.player_id == target_building.player_id || UNIT_DATA.at(unit.type).range_squared == 1
                        ? unit_rect.is_adjacent_to(building_rect) 
                        : unit_rect.euclidean_distance_squared_to(building_rect) <= UNIT_DATA.at(unit.type).range_squared;
        }
        case UNIT_TARGET_CAMP: {
            uint32_t camp_index = state.buildings.get_index_of(unit.target.id);
            GOLD_ASSERT(camp_index != INDEX_INVALID);
            rect_t unit_rect = rect_t(unit.cell, unit_cell_size(unit.type));
            rect_t building_rect = rect_t(state.buildings[camp_index].cell, building_cell_size(state.buildings[camp_index].type));
            return unit_rect.is_adjacent_to(building_rect); 
        }
    }
}

bool unit_is_target_dead_or_ferried(const match_state_t& state, const unit_t& unit) {
    GOLD_ASSERT(unit.target.type == UNIT_TARGET_UNIT || unit.target.type == UNIT_TARGET_BUILDING || unit.target.type == UNIT_TARGET_BUILD_ASSIST);

    uint32_t target_index = (unit.target.type == UNIT_TARGET_UNIT || unit.target.type == UNIT_TARGET_BUILD_ASSIST) ? state.units.get_index_of(unit.target.id) : state.buildings.get_index_of(unit.target.id);
    if (target_index == INDEX_INVALID) {
        return true;
    }
    int target_health = (unit.target.type == UNIT_TARGET_UNIT || unit.target.type == UNIT_TARGET_BUILD_ASSIST) ? state.units[target_index].health : state.buildings[target_index].health;
    if (target_health == 0) {
        return true;
    }
    if (unit.target.type == UNIT_TARGET_BUILD_ASSIST && state.units[target_index].target.type != UNIT_TARGET_BUILD) {
        return true;
    }
    if (unit.target.type == UNIT_TARGET_UNIT && (state.units[target_index].mode == UNIT_MODE_FERRY || state.units[target_index].mode == UNIT_MODE_IN_MINE)) {
        return true;
    }
    return false;
}

bool unit_can_see_rect(const unit_t& unit, rect_t rect) {
    int unit_sight = UNIT_DATA.at(unit.type).sight;
    rect_t unit_sight_rect = rect_t(unit.cell - xy(unit_sight, unit_sight), xy((2 * unit_sight) + 1, (2 * unit_sight) + 1));
    return unit_sight_rect.intersects(rect);
}

int unit_get_damage(const match_state_t& state, const unit_t& unit) {
    return UNIT_DATA.at(unit.type).damage;
}

int unit_get_armor(const match_state_t& state, const unit_t& unit) {
    return UNIT_DATA.at(unit.type).armor;
}

AnimationName unit_get_expected_animation(const unit_t& unit) {
    switch (unit.mode) {
        case UNIT_MODE_MOVE:
            return ANIMATION_UNIT_MOVE; 
        case UNIT_MODE_BUILD:
        case UNIT_MODE_REPAIR:
            return ANIMATION_UNIT_BUILD;
        case UNIT_MODE_ATTACK_WINDUP:
            return ANIMATION_UNIT_ATTACK;
        case UNIT_MODE_DEATH:
            return ANIMATION_UNIT_DEATH;
        case UNIT_MODE_DEATH_FADE:
            return ANIMATION_UNIT_DEATH_FADE;
        case UNIT_MODE_ATTACK_COOLDOWN:
        case UNIT_MODE_IDLE:
        default:
            return ANIMATION_UNIT_IDLE;
    }
}

int unit_get_animation_vframe(const unit_t& unit) {
    if (unit.mode == UNIT_MODE_BUILD) {
        return 2;
    }
    
    if (unit.animation.name == ANIMATION_UNIT_DEATH || unit.animation.name == ANIMATION_UNIT_DEATH_FADE) {
        return unit.direction == DIRECTION_NORTH ? 1 : 0;
    }

    int vframe;
    if (unit.direction == DIRECTION_NORTH) {
        vframe = 1;
    } else if (unit.direction == DIRECTION_SOUTH) {
        vframe = 0;
    } else {
        vframe = 2;
    } 
    if (unit.gold_held > 0 && (unit.animation.name == ANIMATION_UNIT_MOVE || unit.animation.name == ANIMATION_UNIT_IDLE)) {
        vframe += 3;
    }
    return vframe;
}

bool unit_sprite_should_flip_h(const unit_t& unit) {
    return unit.direction > DIRECTION_SOUTH; 
}

Sprite unit_get_select_ring(UnitType type, bool is_enemy) {
    if (UNIT_DATA.at(type).cell_size == 2) {
        return is_enemy ? SPRITE_SELECT_RING_WAGON_ATTACK : SPRITE_SELECT_RING_WAGON;
    } else {
        return is_enemy ? SPRITE_SELECT_RING_ATTACK : SPRITE_SELECT_RING;
    }
}

bool unit_can_be_selected(const unit_t& unit) {
    return !(unit.mode == UNIT_MODE_BUILD || unit.mode == UNIT_MODE_FERRY || 
             unit.mode == UNIT_MODE_IN_MINE ||
             (unit.mode == UNIT_MODE_MOVE && unit.garrison_id == unit.target.id));
}

void unit_stop_building(match_state_t& state, entity_id unit_id) {
    uint32_t unit_index = state.units.get_index_of(unit_id);
    GOLD_ASSERT(unit_index != INDEX_INVALID);
    unit_t& unit = state.units[unit_index];

    rect_t building_rect = rect_t(unit.target.build.building_cell, building_cell_size(unit.target.build.building_type));
    xy exit_cell = get_nearest_cell_around_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), building_rect);
    if (exit_cell == unit.cell) {
        // Unable to exit the building
        exit_cell = xy(-1, -1);
        for (int x = building_rect.position.x; x < building_rect.position.x + building_rect.size.x; x++) {
            for (int y = building_rect.position.y; y < building_rect.position.y + building_rect.size.y; y++) {
                if (!map_is_cell_rect_occupied(state, rect_t(xy(x, y), unit_cell_size(unit.type)))) {
                    exit_cell = xy(x, y);
                    break;
                }
            }
            if (exit_cell.x != -1) {
                break;
            }
        }
        if (exit_cell.x == -1) {
            return;
        }
    }

    unit.cell = exit_cell;
    unit.position = cell_center(unit.cell);
    unit.target = (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
    unit.mode = UNIT_MODE_IDLE;
    map_set_cell_rect(state, rect_t(unit.cell, unit_cell_size(unit.type)), CELL_UNIT, unit_id);
}

unit_target_t unit_target_nearest_camp(const match_state_t& state, xy unit_cell, uint8_t unit_player_id) {
    int nearest_camp_dist = -1;
    entity_id nearest_camp_id = ID_NULL;

    // Find the nearest mining camp
    for (uint32_t building_index = 0; building_index < state.buildings.size(); building_index++) {
        if (building_is_finished(state.buildings[building_index]) && state.buildings[building_index].type == BUILDING_CAMP && state.buildings[building_index].player_id == unit_player_id) {
            if (nearest_camp_dist == -1 || xy::manhattan_distance(unit_cell, state.buildings[building_index].cell) < nearest_camp_dist) {
                nearest_camp_id = state.buildings.get_id_of(building_index);
                nearest_camp_dist = xy::manhattan_distance(unit_cell, state.buildings[building_index].cell);
            }
        }
    }

    if (nearest_camp_id != ID_NULL && nearest_camp_dist < 20) {
        return (unit_target_t) {
            .type = UNIT_TARGET_CAMP,
            .id = nearest_camp_id
        };
    } 

    return (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
}

unit_target_t unit_target_nearest_mine(const match_state_t& state, const unit_t& unit) {
    int nearest_mine_dist = -1;
    entity_id nearest_mine_id = ID_NULL;

    // Find the nearest mining camp
    for (uint32_t mine_index = 0; mine_index < state.mines.size(); mine_index++) {
        // Don't target empty mines                    Don't target mines the player can't see
        if (state.mines[mine_index].gold_left == 0 || !map_is_cell_rect_revealed(state, unit.player_id, rect_t(state.mines[mine_index].cell, xy(MINE_SIZE, MINE_SIZE)))) {
            continue;
        }

        if (nearest_mine_dist == -1 || xy::manhattan_distance(unit.cell, state.mines[mine_index].cell) < nearest_mine_dist) {
            nearest_mine_id = state.mines.get_id_of(mine_index);
            nearest_mine_dist = xy::manhattan_distance(unit.cell, state.mines[mine_index].cell);
        }
    }

    if (nearest_mine_id != ID_NULL && nearest_mine_dist < 20) {
        return (unit_target_t) {
            .type = UNIT_TARGET_MINE,
            .id = nearest_mine_id
        };
    } 

    return (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
}

unit_target_t unit_target_nearest_insight_enemy(const match_state_t& state, const unit_t& unit) {
    entity_id nearest_enemy_index = INDEX_INVALID;

    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        const unit_t& other_unit = state.units[unit_index];
        if (unit.player_id == other_unit.player_id) {
            continue;
        }
        if (other_unit.mode == UNIT_MODE_FERRY || other_unit.mode == UNIT_MODE_IN_MINE || other_unit.mode == UNIT_MODE_BUILD) {
            continue;
        }
        if (other_unit.health == 0) {
            continue;
        }
        if (!unit_can_see_rect(unit, rect_t(other_unit.cell, unit_cell_size(other_unit.type)))) {
            continue;
        }
        if (nearest_enemy_index == INDEX_INVALID || xy::manhattan_distance(unit.cell, other_unit.cell) < xy::manhattan_distance(unit.cell, state.units[nearest_enemy_index].cell)) {
            nearest_enemy_index = unit_index;
        }
    }

    if (nearest_enemy_index != INDEX_INVALID) {
        return (unit_target_t) {
            .type = UNIT_TARGET_UNIT,
            .id = state.units.get_id_of(nearest_enemy_index)
        };
    }

    for (uint32_t building_index = 0; building_index < state.buildings.size(); building_index++) {
        const building_t& building = state.buildings[building_index];
        if (building.player_id == unit.player_id) {
            continue;
        }
        if (building.health == 0) {
            continue;
        }
        if (!unit_can_see_rect(unit, rect_t(building.cell, building_cell_size(building.type)))) {
            continue;
        }
        if (nearest_enemy_index == INDEX_INVALID || xy::manhattan_distance(unit.cell, building.cell) < xy::manhattan_distance(unit.cell, state.buildings[nearest_enemy_index].cell)) {
            nearest_enemy_index = building_index;
        }
    }

    if (nearest_enemy_index != INDEX_INVALID) {
        return (unit_target_t) {
            .type = UNIT_TARGET_BUILDING,
            .id = state.buildings.get_id_of(nearest_enemy_index)
        };
    }

    return (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
}

xy unit_get_best_unload_cell(const match_state_t& state, const unit_t& unit, xy cell_size) {
    xy target_position = unit_get_target_position(unit.type, unit.cell).to_xy();
    xy dropoff_origin = unit.cell;
    xy unit_size = unit_cell_size(unit.type);
    std::vector<Direction> dropoff_directions;

    if (unit.position != target_position) {
        xy previous_target_position = unit_get_target_position(unit.type, unit.cell + DIRECTION_XY[(unit.direction + 4) % DIRECTION_COUNT]).to_xy();
        if (xy::manhattan_distance(unit.position.to_xy(), previous_target_position) < xy::manhattan_distance(unit.position.to_xy(), target_position)) {
            dropoff_origin = unit.cell + DIRECTION_XY[(unit.direction + 4) % DIRECTION_COUNT];
        }
    }

    Direction facing_direction = unit.direction;
    if (unit.position == target_position) {
        if (facing_direction > DIRECTION_SOUTH) {
            facing_direction = DIRECTION_WEST;
        } else if (facing_direction > DIRECTION_NORTH && facing_direction < DIRECTION_SOUTH) {
            facing_direction = DIRECTION_EAST;
        }
    }

    if (facing_direction % 2 == 1) {
        // Diagonal directions
        int opposite_direction = (facing_direction + 4) % DIRECTION_COUNT;
        dropoff_directions.push_back((Direction)((opposite_direction + 1) % DIRECTION_COUNT)); // next direction
        dropoff_directions.push_back((Direction)(opposite_direction == 0 ? DIRECTION_COUNT - 1 : opposite_direction - 1)); // previous direction
    } else {
        // Adjacent directions
        int dropoff_direction = (facing_direction + 2) % DIRECTION_COUNT;
        while (dropoff_direction != facing_direction) {
            dropoff_directions.push_back((Direction)dropoff_direction);
            dropoff_direction = (dropoff_direction + 2) % DIRECTION_COUNT;
        }
    }

    for (Direction direction : dropoff_directions) {
        if (direction == DIRECTION_EAST || direction == DIRECTION_WEST) {
            int dropoff_x = direction == DIRECTION_EAST
                                ? dropoff_origin.x - cell_size.x
                                : dropoff_origin.x + unit_size.x;
            for (int dropoff_y = dropoff_origin.y; dropoff_y < dropoff_origin.y + unit_size.y; dropoff_y++) {
                if (map_is_cell_rect_in_bounds(state, rect_t(xy(dropoff_x, dropoff_y), cell_size)) && !map_is_cell_rect_occupied(state, rect_t(xy(dropoff_x, dropoff_y), cell_size))) {
                    return xy(dropoff_x, dropoff_y);
                }
            }
        } else {
            GOLD_ASSERT(direction == DIRECTION_SOUTH || direction == DIRECTION_NORTH);
            int dropoff_y = direction == DIRECTION_NORTH
                                ? dropoff_origin.y - cell_size.y
                                : dropoff_origin.y + unit_size.y;
            for (int dropoff_x = dropoff_origin.x; dropoff_x < dropoff_origin.x + unit_size.x; dropoff_x++) {
                if (map_is_cell_rect_in_bounds(state, rect_t(xy(dropoff_x, dropoff_y), cell_size)) && !map_is_cell_rect_occupied(state, rect_t(xy(dropoff_x, dropoff_y), cell_size))) {
                    return xy(dropoff_x, dropoff_y);
                }
            }
        }
    }

    return xy(-1, -1);
}
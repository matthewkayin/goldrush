#include "match.h"

#include "logger.h"

const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA = {
    { UNIT_MINER, (unit_data_t) {
        .sprite = SPRITE_UNIT_MINER,
        .max_health = 20,
        .damage = 5,
        .armor = 0,
        .range = 1,
        .attack_cooldown = 16,
        .speed = fixed::from_int(1),
        .sight = 7,
        .cost = 50,
        .population_cost = 1,
        .train_duration = 300
    }},
    { UNIT_COWBOY, (unit_data_t) {
        .sprite = SPRITE_UNIT_COWBOY,
        .max_health = 40,
        .damage = 8,
        .armor = 1,
        .range = 5,
        .attack_cooldown = 30,
        .speed = fixed::from_int(1),
        .sight = 7,
        .cost = 50,
        .population_cost = 1,
        .train_duration = 300
    }}
};

// Unit

void unit_create(match_state_t& state, uint8_t player_id, UnitType type, const xy& cell) {
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
    unit.position = cell_center(unit.cell);
    unit.target = (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };

    unit.gold_held = 0;
    unit.timer = 0;

    entity_id unit_id = state.units.push_back(unit);
    map_set_cell(state, unit.cell, CELL_UNIT, unit_id);
}

void unit_destroy(match_state_t& state, uint32_t unit_index) {
    map_set_cell(state, state.units[unit_index].cell, CELL_EMPTY);
    state.units.remove_at(unit_index);
}

void unit_update(match_state_t& state, uint32_t unit_index) {
    entity_id unit_id = state.units.get_id_of(unit_index);
    unit_t& unit = state.units[unit_index];
    const unit_data_t& unit_data = UNIT_DATA.at(unit.type);

    bool unit_update_finished = false;
    fixed movement_left = unit_data.speed;
    while (!unit_update_finished) {
        log_info("unit update, index %u mode %u target type %u", unit_index, unit.mode, unit.target.type);
        switch (unit.mode) {
            case UNIT_MODE_IDLE: {
                if (unit.target.type == UNIT_TARGET_NONE) {
                    unit_update_finished = true;
                    break;
                } else if ((unit.target.type == UNIT_TARGET_UNIT || unit.target.type == UNIT_TARGET_BUILDING) && unit_is_target_dead(state, unit)) {
                    unit.target = (unit_target_t) {
                        .type = UNIT_TARGET_NONE
                    };
                    break;
                } else if (unit_has_reached_target(state, unit)) {
                    unit.mode = UNIT_MODE_MOVE_FINISHED;
                    break;
                } else {
                    map_pathfind(state, unit.cell, unit_get_target_cell(state, unit), &unit.path);
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
                    if (unit.position == cell_center(unit.cell) && !unit.path.empty()) {
                        unit.direction = get_enum_direction_from_xy_direction(unit.path[0] - unit.cell);
                        if (map_is_cell_blocked(state, unit.path[0])) {
                            is_path_blocked = true;
                            // breaks out of while movement_left > 0
                            break;
                        }

                        map_set_cell(state, unit.cell, CELL_EMPTY);
                        unit.cell = unit.path[0];
                        map_set_cell(state, unit.cell, CELL_UNIT, unit_id);
                        unit.path.erase(unit.path.begin());
                    }

                    // Step unit along movement
                    if (unit.position.distance_to(cell_center(unit.cell)) > movement_left) {
                        unit.position += DIRECTION_XY_FIXED[unit.direction] * movement_left;
                        movement_left = fixed::from_raw(0);
                    } else {
                        movement_left -= unit.position.distance_to(cell_center(unit.cell));
                        unit.position = cell_center(unit.cell);
                        // On step finished
                        if (unit.target.type == UNIT_TARGET_ATTACK) {
                            unit_target_t attack_target = unit_target_nearest_insight_enemy(state, unit);
                            if (attack_target.type != UNIT_TARGET_NONE) {
                                // breaks out of while movement_left > 0
                                unit_set_target(state, unit, attack_target);
                                break;
                            }
                        }
                        if (unit.path.empty()) {
                            // breaks out of while movement_left > 0
                            break;
                        }
                    }
                } // End while movement_left > 0

                if (is_path_blocked) {
                    // Trigger repath
                    if (unit.target.type == UNIT_TARGET_GOLD) {
                        unit.target = unit_target_nearest_gold(state, unit);
                    }
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                // On cell movement finished
                if (unit.position == cell_center(unit.cell)) {
                    if (unit_has_reached_target(state, unit)) {
                        unit.mode = UNIT_MODE_MOVE_FINISHED;
                        break;
                    } else if (unit.path.empty()) {
                        unit.mode = UNIT_MODE_IDLE;
                        break;
                    }
                } 

                unit_update_finished = true;
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
                    case UNIT_TARGET_BUILD: {
                        // Temporarily set cell to empty so that we can check if the space is clear for building
                        map_set_cell(state, unit.cell, CELL_EMPTY);
                        bool can_build = unit.cell == unit.target.build.unit_cell && 
                                                        !map_is_cell_rect_blocked(state, rect_t(unit.target.build.building_cell, building_cell_size(unit.target.build.building_type)));
                        if (!can_build) {
                            // Set the cell back to filled
                            map_set_cell(state, unit.cell, CELL_EMPTY);
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
                    case UNIT_TARGET_GOLD: {
                        if (xy::manhattan_distance(unit.cell, unit.target.cell) != 1 || !map_is_cell_gold(state, unit.target.cell)) {
                            unit.target = unit_target_nearest_gold(state, unit);
                            unit.mode = UNIT_MODE_IDLE;
                        } else if (unit.gold_held < UNIT_MAX_GOLD_HELD) {
                            unit.timer = UNIT_MINE_TICK_DURATION;
                            unit.direction = get_enum_direction_from_xy_direction(unit.target.cell - unit.cell);
                            unit.mode = UNIT_MODE_MINE;
                        } else {
                            unit.target = unit_target_nearest_camp(state, unit);
                            unit.mode = UNIT_MODE_IDLE;
                        }
                        break;
                    }
                    case UNIT_TARGET_CAMP: {
                        uint32_t building_index = state.buildings.get_index_of(unit.target.id);
                        if (building_index == INDEX_INVALID) {
                            unit.target = (unit_target_t) { 
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        } 

                        // Return gold
                        building_t& building = state.buildings[building_index];
                        bool is_unit_around_building = rect_t(building.cell - xy(1, 1), building_cell_size(building.type) + xy(2, 2)).has_point(unit.cell);
                        if (is_unit_around_building) {
                            state.player_gold[unit.player_id] += unit.gold_held;
                            unit.gold_held = 0;
                            unit.target = unit_target_nearest_gold(state, unit);
                            unit.mode = UNIT_MODE_IDLE;
                        } else {
                            // This will trigger a repath
                            unit.mode = UNIT_MODE_IDLE;
                        }
                        break;
                    }
                    case UNIT_TARGET_UNIT: 
                    case UNIT_TARGET_BUILDING: {
                        if (unit_is_target_dead(state, unit)) {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            };
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }

                        if (!unit_has_reached_target(state, unit)) {
                            // This will trigger a repath
                            log_info("has not reached target");
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
                        if (unit.player_id == target_player_id) {
                            unit.mode = UNIT_MODE_IDLE;
                            break;
                        }

                        // If target is building a building, attack that building instead
                        if (unit.target.type == UNIT_TARGET_UNIT && state.units[target_index].mode == UNIT_MODE_BUILD) {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_BUILDING,
                                .id = state.units[target_index].target.build.building_id
                            };

                            if (!unit_has_reached_target(state, unit)) {
                                unit.mode = UNIT_MODE_IDLE;
                                break;
                            }
                        }

                        rect_t target_rect = unit.target.type == UNIT_TARGET_UNIT
                                            ? rect_t(state.units[target_index].cell, xy(1, 1))
                                            : rect_t(state.buildings[target_index].cell, building_cell_size(state.buildings[target_index].type));
                        unit.direction = get_enum_direction_to_rect(unit.cell, target_rect);
                        unit.mode = UNIT_MODE_ATTACK_WINDUP;
                        break;
                    }
                } // End switch unit target type
                unit_update_finished = true;
                break;
            } // End case UNIT_MODE_MOVE_FINISHED
            case UNIT_MODE_BUILD: {
                unit.timer--;
                if (unit.timer == 0) {
                    // Building tick
                    building_t& building = state.buildings[state.buildings.get_index_of(unit.target.build.building_id)];

#ifdef DEBUG_FAST_BUILD
                    building.health = std::min(building.health + 20, BUILDING_DATA.at(building.type).max_health);
#else
                    building.health++;
#endif
                    if (building.health == BUILDING_DATA.at(building.type).max_health) {
                        // On building finished
                        building.is_finished = true;

                        // If selecting the building
                        if (state.selection.type == SELECTION_TYPE_BUILDINGS && state.selection.ids[0] == unit.target.build.building_id) {
                            // Trigger a re-select so that UI buttons are updated correctly
                            ui_set_selection(state, state.selection);
                        }

                        unit_stop_building(state, unit_id, building);
                    } else {
                        unit.timer = UNIT_BUILD_TICK_DURATION;
                    }
                }
                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_MINE: {
                // Handles case where this unit was mining has ran out
                if (!map_is_cell_gold(state, unit.target.cell)) {
                    log_info("gold ran out on me!");
                    unit.timer = 0;
                    unit.target = unit_target_nearest_gold(state, unit);
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                unit.timer--;
                if (unit.timer == 0) {
                    // Collect a gold
                    unit.gold_held++;
                    map_decrement_gold(state, unit.target.cell);

                    if (unit.gold_held == UNIT_MAX_GOLD_HELD) {
                        // Return to mining camp
                        unit.target = unit_target_nearest_camp(state, unit);
                        unit.mode = UNIT_MODE_IDLE;
                    } else if (!map_is_cell_gold(state, unit.target.cell)) {
                        // Gold ran out, find a new gold cell to mine
                        unit.target = unit_target_nearest_gold(state, unit);
                        log_info("unit gold %u unit target %u", unit.gold_held, unit.target.type);
                        unit.mode = UNIT_MODE_IDLE;
                    } else {
                        // Continue mining
                        unit.timer = UNIT_MINE_TICK_DURATION;
                    }
                }
                unit_update_finished = true;
                break;
            }
            case UNIT_MODE_ATTACK_WINDUP: {
                if (unit_is_target_dead(state, unit)) {
                    unit.target = unit_target_nearest_insight_enemy(state, unit);
                    unit.mode = UNIT_MODE_IDLE;
                    break;
                }

                if (!animation_is_playing(unit.animation)) {
                    if (unit.target.type == UNIT_TARGET_UNIT) {
                        uint32_t enemy_index = state.units.get_index_of(unit.target.id);
                        GOLD_ASSERT(enemy_index != INDEX_INVALID);
                        unit_t& enemy = state.units[enemy_index];

                        int damage = std::max(1, unit_get_damage(state, unit) - unit_get_armor(state, enemy));
                        enemy.health = std::max(0, enemy.health - damage);

                        // Make the enemy attack back
                        if (enemy.mode == UNIT_MODE_IDLE && enemy.target.type == UNIT_TARGET_NONE && unit_can_see_rect(enemy, rect_t(unit.cell, xy(1, 1)))) {
                            enemy.target = (unit_target_t) {
                                .type = UNIT_TARGET_UNIT,
                                .id = unit_id
                            };
                        }
                    } else if (unit.target.type == UNIT_TARGET_BUILDING) {
                        uint32_t enemy_index = state.buildings.get_index_of(unit.target.id);
                        GOLD_ASSERT(enemy_index != INDEX_INVALID);
                        building_t& enemy = state.buildings[enemy_index];

                        // NOTE: Building armor is hard-coded to 1
                        int damage = std::min(1, unit_get_damage(state, unit) - 1);
                        enemy.health = std::max(0, enemy.health - damage);
                    }

                    unit.timer = unit_data.attack_cooldown;
                    unit.mode = UNIT_MODE_ATTACK_COOLDOWN;
                } // End if animation not playing

                unit_update_finished = true;
                break;
            } // End case UNIT_MODE_ATTACK_WINDUP
            case UNIT_MODE_ATTACK_COOLDOWN: {
                if (unit_is_target_dead(state, unit)) {
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
        }
    }
}

rect_t unit_get_rect(const unit_t& unit) {
    return rect_t(unit.position.to_xy() - xy(TILE_SIZE / 2, TILE_SIZE / 2), xy(TILE_SIZE, TILE_SIZE));
}

void unit_set_target(const match_state_t& state, unit_t& unit, unit_target_t target) {
    GOLD_ASSERT(unit.mode != UNIT_MODE_BUILD);
    unit.target = target;
    unit.path.clear();

    if (unit.mode != UNIT_MODE_MOVE) {
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
        case UNIT_TARGET_ATTACK:
        case UNIT_TARGET_CELL:
        case UNIT_TARGET_GOLD:
            return unit.target.cell;
        case UNIT_TARGET_UNIT: {
            uint32_t target_unit_index = state.units.get_index_of(unit.target.id);
            return target_unit_index != INDEX_INVALID 
                                    ? state.units[target_unit_index].cell 
                                    : unit.cell;
        }
        case UNIT_TARGET_BUILDING: {
            uint32_t target_building_index = state.buildings.get_index_of(unit.target.id);
            return target_building_index != INDEX_INVALID
                                    ? get_nearest_free_cell_around_building(state, unit.cell, state.buildings[target_building_index])
                                    : unit.cell;
        }
        case UNIT_TARGET_CAMP: {
            uint32_t building_index = state.buildings.get_index_of(unit.target.id);
            return building_index != INDEX_INVALID 
                                ? get_nearest_free_cell_around_building(state, unit.cell, state.buildings[building_index])
                                : unit.cell;
        }
    }
}

bool unit_has_reached_target(const match_state_t& state, const unit_t& unit) {
    switch (unit.target.type) {
        case UNIT_TARGET_NONE:
            return true;
        case UNIT_TARGET_BUILD:
            return unit.cell == unit.target.build.unit_cell;
        case UNIT_TARGET_ATTACK:
        case UNIT_TARGET_CELL:
            return unit.cell == unit.target.cell;
        case UNIT_TARGET_GOLD:
            return xy::manhattan_distance(unit.cell, unit.target.cell) == 1;
        case UNIT_TARGET_UNIT: {
            uint32_t target_unit_index = state.units.get_index_of(unit.target.id);
            GOLD_ASSERT(target_unit_index != INDEX_INVALID);
            int range = state.units[target_unit_index].player_id == unit.player_id ? 1 : UNIT_DATA.at(unit.type).range;
            return xy::manhattan_distance(unit.cell, state.units[target_unit_index].cell) <= range; 
        }
        case UNIT_TARGET_BUILDING: {
            uint32_t target_building_index = state.buildings.get_index_of(unit.target.id);
            GOLD_ASSERT(target_building_index != INDEX_INVALID);
            return is_unit_adjacent_to_building(unit, state.buildings[target_building_index]);
        }
        case UNIT_TARGET_CAMP: {
            uint32_t camp_index = state.buildings.get_index_of(unit.target.id);
            GOLD_ASSERT(camp_index != INDEX_INVALID);
            return is_unit_adjacent_to_building(unit, state.buildings[camp_index]);
        }
    }
}

bool unit_is_target_dead(const match_state_t& state, const unit_t& unit) {
    GOLD_ASSERT(unit.target.type == UNIT_TARGET_UNIT || unit.target.type == UNIT_TARGET_BUILDING);
    uint32_t target_index = unit.target.type == UNIT_TARGET_UNIT ? state.units.get_index_of(unit.target.id) : state.buildings.get_index_of(unit.target.id);
    return target_index == INDEX_INVALID;
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
            return ANIMATION_UNIT_BUILD;
        case UNIT_MODE_MINE:
        case UNIT_MODE_ATTACK_WINDUP:
            return ANIMATION_UNIT_ATTACK;
        case UNIT_MODE_ATTACK_COOLDOWN:
        case UNIT_MODE_IDLE:
        default:
            return ANIMATION_UNIT_IDLE;
    }
}

int unit_get_animation_vframe(const unit_t& unit) {
    if (unit.animation.name == ANIMATION_UNIT_BUILD) {
        return 0;
    }
    int vframe;
    if (unit.direction == DIRECTION_NORTH) {
        vframe = 1;
    } else if (unit.direction == DIRECTION_SOUTH) {
        vframe = 0;
    } else if (unit.direction > DIRECTION_SOUTH) {
        vframe = 3;
    } else {
        vframe = 2;
    }
    if (unit.gold_held > 0 && (unit.animation.name == ANIMATION_UNIT_MOVE || unit.animation.name == ANIMATION_UNIT_IDLE)) {
        vframe += 4;
    }
    return vframe;
}

void unit_stop_building(match_state_t& state, entity_id unit_id, const building_t& building) {
    uint32_t unit_index = state.units.get_index_of(unit_id);
    GOLD_ASSERT(unit_index != INDEX_INVALID);
    unit_t& unit = state.units[unit_index];

    unit.cell = get_first_empty_cell_around_rect(state, rect_t(building.cell, building_cell_size(building.type)));
    unit.position = cell_center(unit.cell);
    unit.target = (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
    unit.mode = UNIT_MODE_IDLE;
    map_set_cell(state, unit.cell, CELL_UNIT, unit_id);
}

entity_id unit_find_nearest_camp(const match_state_t& state, const unit_t& unit) {
    int nearest_camp_dist = -1;
    entity_id nearest_camp_id = ID_NULL;

    for (uint32_t building_index = 0; building_index < state.buildings.size(); building_index++) {
        if (state.buildings[building_index].is_finished && state.buildings[building_index].type == BUILDING_CAMP && state.buildings[building_index].player_id == unit.player_id) {
            if (nearest_camp_dist == -1 || xy::manhattan_distance(unit.cell, state.buildings[building_index].cell) < nearest_camp_dist) {
                nearest_camp_id = state.buildings.get_id_of(building_index);
                nearest_camp_dist = xy::manhattan_distance(unit.cell, state.buildings[building_index].cell);
            }
        }
    }

    return nearest_camp_id;
}

unit_target_t unit_target_nearest_camp(const match_state_t& state, const unit_t& unit) {
    // Find the nearest mining camp
    entity_id nearest_camp_id = unit_find_nearest_camp(state, unit);
    if (nearest_camp_id != ID_NULL) {
        return (unit_target_t) {
            .type = UNIT_TARGET_CAMP,
            .id = nearest_camp_id
        };
    } 

    return (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
}

unit_target_t unit_target_nearest_gold(const match_state_t& state, const unit_t& unit) {
    xy start_cell = unit.cell;

    xy nearest_gold_cell = unit.cell;
    int nearest_gold_dist = -1;
    for (int x = 0; x < state.map_width; x++) {
        for (int y = 0; y < state.map_height; y++) {
            xy gold_cell = xy(x, y);
            // Check if the cell is gold
            if (!map_is_cell_gold(state, gold_cell)) {
                continue;
            }

            // Check if the gold can be mined around
            bool is_gold_cell_free = false;
            for (int direction = DIRECTION_NORTH; direction < DIRECTION_COUNT; direction += 2) {
                xy adjacent_cell = gold_cell + DIRECTION_XY[direction];
                if (map_is_cell_in_bounds(state, adjacent_cell) && (adjacent_cell == start_cell || !map_is_cell_blocked(state, adjacent_cell))) {
                    is_gold_cell_free = true;
                    break;
                }
            }

            // Check if the cell is closer to the nearest cell
            if (is_gold_cell_free && (nearest_gold_dist == -1 || xy::manhattan_distance(start_cell, gold_cell) < nearest_gold_dist)) {
                nearest_gold_cell = gold_cell;
                nearest_gold_dist = xy::manhattan_distance(start_cell, nearest_gold_cell);
            }
        }
    }

    if (nearest_gold_dist != -1) {
        return (unit_target_t) {
            .type = UNIT_TARGET_GOLD,
            .cell = nearest_gold_cell
        };
    }

    log_info("no nearest gold cell found");
    return (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
}

unit_target_t unit_target_nearest_insight_enemy(const match_state_t state, const unit_t& unit) {
    entity_id nearest_enemy_index = INDEX_INVALID;

    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        const unit_t& other_unit = state.units[unit_index];
        if (unit.player_id == other_unit.player_id) {
            continue;
        }
        if (!unit_can_see_rect(unit, rect_t(other_unit.cell, xy(1, 1)))) {
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
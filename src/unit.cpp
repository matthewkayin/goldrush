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
        .speed = fixed::from_int(1)
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
    unit_set_target(state, unit, (unit_target_t) {
        .type = UNIT_TARGET_NONE
    });

    unit.gold_held = 0;
    unit.timer = 0;

    entity_id unit_id = state.units.push_back(unit);
    map_set_cell_value(state, unit.cell, CELL_UNIT, unit_id);
}

void unit_update(match_state_t& state, uint32_t unit_index) {
    entity_id unit_id = state.units.get_id_of(unit_index);
    unit_t& unit = state.units[unit_index];
    const unit_data_t& unit_data = UNIT_DATA.at(unit.type);

    switch (unit.mode) {
        case UNIT_MODE_IDLE: {
            if (unit.cell != unit_get_target_cell(state, unit)) {
                map_pathfind(state, unit.cell, unit_get_target_cell(state, unit), &unit.path);
                if (unit.path.empty()) {
                    if (unit.target.type == UNIT_TARGET_BUILD) {
                        ui_show_status(state, UI_STATUS_CANT_BUILD);
                        unit_set_target(state, unit, (unit_target_t) {
                            .type = UNIT_TARGET_NONE
                        });
                        return;
                    }

                    unit.timer = UNIT_PATH_PAUSE_DURATION;
                    unit.mode = UNIT_MODE_MOVE_BLOCKED;
                } else {
                    unit.mode = UNIT_MODE_MOVE;
                }
            }
            break;
        }
        case UNIT_MODE_MOVE_BLOCKED: {
            unit.timer--;
            if (unit.timer == 0) {
                unit.mode = UNIT_MODE_IDLE;
            }
            break;
        }
        case UNIT_MODE_MOVE: {
            fixed movement_left = unit_data.speed;
            while (movement_left.raw_value > 0) {
                // If the unit is not moving between tiles, then pop the next cell off the path
                if (unit.position == cell_center(unit.cell)) {
                    unit.direction = get_enum_direction_from_xy_direction(unit.path[0] - unit.cell);
                    if (map_is_cell_blocked(state, unit.path[0])) {
                        // Path is blocked
                        map_pathfind(state, unit.cell, unit_get_target_cell(state, unit), &unit.path);
                        if (unit.path.empty()) {
                            unit.timer = UNIT_PATH_PAUSE_DURATION;
                            unit.mode = UNIT_MODE_MOVE_BLOCKED;
                            return;
                        }
                        continue;
                    }

                    map_set_cell_value(state, unit.cell, CELL_EMPTY);
                    unit.cell = unit.path[0];
                    map_set_cell_value(state, unit.cell, CELL_UNIT, unit_id);
                    unit.path.erase(unit.path.begin());
                }

                // Step unit along movement
                if (unit.position.distance_to(cell_center(unit.cell)) > movement_left) {
                    unit.position += DIRECTION_XY_FIXED[unit.direction] * movement_left;
                    movement_left = fixed::from_raw(0);
                } else {
                    movement_left -= unit.position.distance_to(cell_center(unit.cell));
                    unit.position = cell_center(unit.cell);
                    if (unit.path.empty()) {
                        movement_left = fixed::from_raw(0);
                    }
                }
            } // End while movement_left > 0

            // On movement finished
            if (unit.position == cell_center(unit.cell) && unit.path.empty()) {
                switch (unit.target.type) {
                    case UNIT_TARGET_CELL: {
                        unit_set_target(state, unit, (unit_target_t) {
                            .type = UNIT_TARGET_NONE
                        });
                        unit.mode = UNIT_MODE_IDLE;
                        break;
                    }
                    case UNIT_TARGET_BUILD: {
                        // Temporarily set cell to empty so that we can check if the space is clear for building
                        map_set_cell_value(state, unit.cell, CELL_EMPTY);
                        bool can_build = unit.cell == unit.target.build.unit_cell && 
                                                        !map_is_cell_rect_blocked(state, rect_t(unit.target.build.building_cell, building_cell_size(unit.target.build.building_type)));
                        if (!can_build) {
                            // Set the cell back to filled
                            map_set_cell_value(state, unit.cell, CELL_EMPTY);
                            ui_show_status(state, UI_STATUS_CANT_BUILD);
                            unit_set_target(state, unit, (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            });
                            unit.mode = UNIT_MODE_IDLE;
                            return;
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
                            unit_set_target(state, unit, (unit_target_t) { 
                                .type = UNIT_TARGET_NONE
                            });
                            return;
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
                    case UNIT_TARGET_ENEMY: {
                        uint32_t enemy_index = state.units.get_index_of(unit.target.id);
                        if (enemy_index == INDEX_INVALID) {
                            unit_set_target(state, unit, (unit_target_t) {
                                .type = UNIT_TARGET_NONE
                            });
                            unit.mode = UNIT_MODE_IDLE;
                            return;
                        }

                        if (unit_is_enemy_in_range(state, unit)) {
                            unit.direction = get_enum_direction_to(unit.cell, state.units[enemy_index].cell);
                            unit.mode = UNIT_MODE_ATTACK_WINDUP;
                        } else {
                            // This will trigger a repath
                            unit.mode = UNIT_MODE_IDLE;
                        }
                        break;
                    }
                    default: {
                        log_info("On movement finished: unhandled target type %u", unit.target.type);
                        break;
                    }
                } // End switch unit target type
            } // End on movement finished
            break;
        } // End case UNIT_MODE_MOVE
        case UNIT_MODE_BUILD: {
            unit.timer--;
            if (unit.timer == 0) {
                // Building tick
                building_t& building = state.buildings[state.buildings.get_index_of(unit.target.build.building_id)];

                building.health++;
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
            break;
        }
        case UNIT_MODE_MINE: {
            if (!map_is_cell_gold(state, unit.target.cell)) {
                unit.timer = 0;
                unit.target = unit_target_nearest_gold(state, unit);
                unit.mode = UNIT_MODE_IDLE;
                return;
            }

            unit.timer--;
            if (unit.timer == 0) {
                if (unit.gold_held < UNIT_MAX_GOLD_HELD) {
                    unit.gold_held++;
                    map_decrement_gold(state, unit.target.cell);
                    if (map_is_cell_gold(state, unit.target.cell)) {
                        unit.timer = UNIT_MINE_TICK_DURATION;
                    } else {
                        unit.target = unit.gold_held < UNIT_MAX_GOLD_HELD 
                                        ? unit_target_nearest_gold(state, unit)
                                        : unit_target_nearest_camp(state, unit);
                        log_info("unit gold %u unit target %u", unit.gold_held, unit.target.type);
                        unit.mode = UNIT_MODE_IDLE;
                    }
                } else {
                    unit_set_target(state, unit, unit_target_nearest_camp(state, unit));
                    unit.mode = UNIT_MODE_IDLE;
                }
            }
            break;
        }
        case UNIT_MODE_ATTACK_WINDUP: {
            if (!animation_is_playing(unit.animation)) {
                if (unit.target.type == UNIT_TARGET_ENEMY) {
                    uint32_t enemy_index = state.units.get_index_of(unit.target.id);
                    if (enemy_index == INDEX_INVALID) {
                        unit_set_target(state, unit, (unit_target_t) {
                            .type = UNIT_TARGET_NONE
                        });
                        unit.mode = UNIT_MODE_IDLE;
                        return;
                    }

                    if (!unit_is_enemy_in_range(state, unit)) {
                        // Triggers a repath
                        unit.mode = UNIT_MODE_IDLE;
                        return;
                    }

                    unit_t& enemy = state.units[enemy_index];
                    int damage = std::min(1, unit_get_damage(state, unit) - unit_get_armor(state, enemy));
                    enemy.health = std::max(0, enemy.health - damage);

                    unit.timer = unit_data.attack_cooldown;
                    unit.mode = UNIT_MODE_ATTACK_COOLDOWN;

                } // End if target type == ENEMY
            } // End if animation not playing

            break;
        } // End case UNIT_MODE_ATTACK_WINDUP
        case UNIT_MODE_ATTACK_COOLDOWN: {
            unit.timer--;
            if (unit.timer == 0) {
                if (unit.target.type == UNIT_TARGET_ENEMY) {
                    uint32_t enemy_index = state.units.get_index_of(unit.target.id);
                    if (enemy_index == INDEX_INVALID) {
                        // TOOD: try to target a different nearby enemy
                        unit_set_target(state, unit, (unit_target_t) {
                            .type = UNIT_TARGET_NONE
                        });
                        unit.mode = UNIT_MODE_IDLE;
                        return;
                    }

                    if (!unit_is_enemy_in_range(state, unit)) {
                        // Triggers a repath
                        unit.mode = UNIT_MODE_IDLE;
                        return;
                    }

                    unit.mode = UNIT_MODE_ATTACK_WINDUP;
                }
            }
        }
    }
}

rect_t unit_get_rect(const unit_t& unit) {
    return rect_t(unit.position.to_xy() - xy(TILE_SIZE / 2, TILE_SIZE / 2), xy(TILE_SIZE, TILE_SIZE));
}

bool unit_is_enemy_in_range(const match_state_t& state, const unit_t& unit) {
    GOLD_ASSERT(unit.target.type == UNIT_TARGET_ENEMY);
    uint32_t target_unit_index = state.units.get_index_of(unit.target.id);
    if (target_unit_index == INDEX_INVALID) {
        return false;
    }

    return xy::manhattan_distance(unit.cell, state.units[target_unit_index].cell) <= UNIT_DATA.at(unit.type).range;
}

void unit_set_target(const match_state_t& state, unit_t& unit, unit_target_t target) {
    unit.target = target;
    unit.path.clear();
    map_pathfind(state, unit.cell, unit_get_target_cell(state, unit), &unit.path);
}

xy unit_get_target_cell(const match_state_t& state, const unit_t& unit) {
    switch (unit.target.type) {
        case UNIT_TARGET_NONE:
            return unit.cell;
        case UNIT_TARGET_BUILD:
        case UNIT_TARGET_CELL:
        case UNIT_TARGET_GOLD:
            return unit.target.cell;
        case UNIT_TARGET_ENEMY: {
            uint32_t enemy_index = state.units.get_index_of(unit.target.id);
            return enemy_index != INDEX_INVALID 
                                    ? state.units[enemy_index].cell 
                                    : unit.cell;
        }
        case UNIT_TARGET_ENEMY_BUILDING:
            return unit.cell;
        case UNIT_TARGET_CAMP: {
            uint32_t building_index = state.buildings.get_index_of(unit.target.id);
            return building_index != INDEX_INVALID 
                                ? get_nearest_free_cell_around_building(state, unit.cell, state.buildings[building_index])
                                : unit.cell;
        }
    }
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
    map_set_cell_value(state, unit.cell, CELL_UNIT, unit_id);
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
    entity_id nearest_camp_id = unit_find_nearest_camp(state, unit);
    xy start_cell = nearest_camp_id != ID_NULL ? state.buildings[state.buildings.get_index_of(nearest_camp_id)].cell : unit.cell;

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
                if (map_is_cell_in_bounds(state, adjacent_cell) && !map_is_cell_blocked(state, adjacent_cell)) {
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

    return (unit_target_t) {
        .type = UNIT_TARGET_NONE
    };
}
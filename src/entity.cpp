#include "match.h"

#include "network.h"
#include "logger.h"
#include <unordered_map>

const uint32_t UNIT_MOVE_BLOCKED_DURATION = 30;

const std::unordered_map<EntityType, entity_data_t> ENTITY_DATA = {
    { UNIT_MINER, (entity_data_t) {
        .name = "Miner",
        .sprite = SPRITE_UNIT_MINER,
        .ui_button = UI_BUTTON_UNIT_MINER,
        .cell_size = 1,

        .gold_cost = 50,
        .train_duration = 15,
        .max_health = 30,
        .sight = 7,
        .armor = 0,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),

            .damage = 3,
            .attack_cooldown = 16,
            .range_squared = 1
        }
    }},
    { BUILDING_CAMP, (entity_data_t) {
        .name = "Mining Camp",
        .sprite = SPRITE_BUILDING_CAMP,
        .ui_button = UI_BUTTON_BUILD_CAMP,
        .cell_size = 2,

        .gold_cost = 200,
        .train_duration = 60,
        .max_health = 200,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = 0,

        .building_data = (building_data_t) {
            .builder_positions_x = { 1, 15, 14 },
            .builder_positions_y = { 13, 13, 2 },
            .builder_flip_h = { false, true, false },
            .can_rally = true
        }
    }}
};

entity_id entity_create(match_state_t& state, EntityType type, uint8_t player_id, xy cell) {
    auto entity_data_it = ENTITY_DATA.find(type);
    GOLD_ASSERT_MESSAGE(entity_data_it != ENTITY_DATA.end(), "Entity data not defined for this entity");
    const entity_data_t& entity_data = entity_data_it->second;

    entity_t entity;
    entity.type = type;
    entity.mode = entity_is_unit(type) ? MODE_UNIT_IDLE : MODE_BUILDING_IN_PROGRESS;
    entity.player_id = player_id;
    entity.flags = 0;

    entity.cell = cell;
    entity.position = entity_is_unit(type) 
                        ? cell_center(entity.cell) 
                        : xy_fixed(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = entity_data.max_health;
    entity.target = (target_t) {
        .type = TARGET_NONE
    };
    entity.timer = 0;

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity_id id = state.entities.push_back(entity);
    map_set_cell_rect(state, entity.cell, entity_cell_size(type), id);

    return id;
}

bool entity_is_unit(EntityType type) {
    return type < BUILDING_CAMP;
}

bool entity_is_building(EntityType type) {
    return type >= BUILDING_CAMP;
}

int entity_cell_size(EntityType type) {
    return ENTITY_DATA.at(type).cell_size;
}

SDL_Rect entity_get_rect(const entity_t& entity) {
    SDL_Rect rect = (SDL_Rect) {
        .x = entity.position.x.integer_part(),
        .y = entity.position.y.integer_part(),
        .w = entity_cell_size(entity.type) * TILE_SIZE,
        .h = entity_cell_size(entity.type) * TILE_SIZE
    };
    if (entity_is_unit(entity.type)) {
        rect.x -= rect.w / 2;
        rect.y -= rect.h / 2;
    }
    return rect;
}

xy entity_get_center_position(const entity_t& entity) {
    if (entity_is_unit(entity.type)) {
        return entity.position.to_xy();
    }

    SDL_Rect rect = entity_get_rect(entity);
    return xy(rect.x + (rect.w / 2), rect.y + (rect.h / 2));
}

Sprite entity_get_sprite(const entity_t entity) {
    return ENTITY_DATA.at(entity.type).sprite;
}

Sprite entity_get_select_ring(const entity_t entity, bool is_ally) {
    Sprite select_ring; 
    if (entity_is_unit(entity.type)) {
        select_ring = (Sprite)(SPRITE_SELECT_RING_UNIT_1 + ((entity_cell_size(entity.type) - 1) * 2));
    } else {
        select_ring = (Sprite)(SPRITE_SELECT_RING_BUILDING_2 + ((entity_cell_size(entity.type) - 2) * 2));
    }
    if (!is_ally) {
        select_ring = (Sprite)(select_ring + 1);
    }
    return select_ring;
}

uint16_t entity_get_elevation(const match_state_t& state, const entity_t& entity) {
    uint16_t elevation = state.map_tiles[entity.cell.x + (entity.cell.y * state.map_width)].elevation;
    // TODO when unit is in bunker, elevation is based on bunker elevation
    for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size(entity.type); x++) {
        for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size(entity.type); y++) {
            elevation = std::max(elevation, state.map_tiles[x + (y * state.map_width)].elevation);
        }
    }

    /*
    if (unit.mode == UNIT_MODE_MOVE) {
        xy unit_prev_cell = unit.cell - DIRECTION_XY[unit.direction];
        for (int x = unit_prev_cell.x; x < unit_prev_cell.x + unit_cell_size(unit.type).x; x++) {
            for (int y = unit_prev_cell.y; y < unit_prev_cell.y + unit_cell_size(unit.type).y; y++) {
                elevation = std::max(elevation, map_get_elevation(state, xy(x, y)));
            }
        }
    }
    */

    return elevation;
}

bool entity_is_selectable(const entity_t& entity) {
    return !(
        entity.health == 0 || 
        entity.mode == MODE_UNIT_BUILD || 
        entity_check_flag(entity, ENTITY_FLAG_IS_GARRISONED)
    );
}

bool entity_check_flag(const entity_t& entity, uint32_t flag) {
    return (entity.flags & flag) == flag;
}

void entity_set_flag(entity_t& entity, uint32_t flag, bool value) {
    if (value) {
        entity.flags |= flag;
    } else {
        entity.flags &= ~flag;
    }
}

bool entity_is_target_invalid(const match_state_t& state, const entity_t& entity) {
    if (!(entity.target.type == TARGET_ENTITY || entity.target.type == TARGET_ATTACK_ENTITY || entity.target.type == TARGET_REPAIR || entity.target.type == TARGET_BUILD_ASSIST)) {
        return false;
    }

    uint32_t entity_index = state.entities.get_index_of(entity.target.id);
    return entity_index == INDEX_INVALID || 
           !entity_is_selectable(state.entities[entity_index]) || 
           (entity.target.type == TARGET_BUILD_ASSIST && state.entities[entity_index].target.type != TARGET_BUILD);
}

bool entity_has_reached_target(const match_state_t& state, const entity_t& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return true;
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
            return entity.cell == entity.target.cell;
        case TARGET_BUILD:
            return entity.cell == entity.target.build.unit_cell;
        case TARGET_BUILD_ASSIST: {
            const entity_t& builder = state.entities.get_by_id(entity.target.id);
            SDL_Rect building_rect = (SDL_Rect) {
                .x = builder.target.build.building_cell.x, .y = builder.target.build.building_cell.y,
                .w = entity_cell_size(builder.target.build.building_type), .h = entity_cell_size(builder.target.build.building_type)
            };
            SDL_Rect unit_rect = (SDL_Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size(entity.type), .h = entity_cell_size(entity.type)
            };
            return sdl_rects_are_adjacent(building_rect, unit_rect);
        }
        case TARGET_UNLOAD:
            return entity.path.empty() && xy::manhattan_distance(entity.cell, entity.target.cell) < 3;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            SDL_Rect entity_rect = (SDL_Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size(entity.type), .h = entity_cell_size(entity.type)
            };
            const entity_t& target = state.entities.get_by_id(entity.target.id);
            SDL_Rect target_rect = (SDL_Rect) {
                .x = target.cell.x, .y = target.cell.y,
                .w = entity_cell_size(target.type), .h = entity_cell_size(target.type)
            };

            int entity_range_squared = ENTITY_DATA.at(entity.type).unit_data.range_squared;
            // TODO mines
            return entity.target.type != TARGET_ATTACK_ENTITY || entity_range_squared == 1
                        ? sdl_rects_are_adjacent(entity_rect, target_rect)
                        : euclidean_distance_squared_between(entity_rect, target_rect) <= entity_range_squared;
        }
    }
}

xy entity_get_target_cell(const match_state_t& state, const entity_t& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return entity.cell;
        case TARGET_BUILD:
            return entity.target.build.unit_cell;
        case TARGET_BUILD_ASSIST:
            // TODO
            return entity.cell;
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
        case TARGET_UNLOAD:
            return entity.target.cell;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR:
            return state.entities.get_by_id(entity.target.id).cell;
    }
}

xy_fixed entity_get_target_position(const entity_t& entity) {
    int unit_size = ENTITY_DATA.at(entity.type).cell_size * TILE_SIZE;
    return xy_fixed((entity.cell * TILE_SIZE) + xy(unit_size / 2, unit_size / 2));
}

AnimationName entity_get_expected_animation(const entity_t& entity) {
    switch (entity.mode) {
        case MODE_UNIT_MOVE:
            return ANIMATION_UNIT_MOVE;
        case MODE_UNIT_BUILD:
        case MODE_UNIT_REPAIR:
            return ANIMATION_UNIT_BUILD;
        case MODE_UNIT_ATTACK_WINDUP:
            return ANIMATION_UNIT_ATTACK;
        case MODE_UNIT_DEATH:
            return ANIMATION_UNIT_DEATH;
        case MODE_UNIT_DEATH_FADE:
            return ANIMATION_UNIT_DEATH_FADE;
        default:
            return ANIMATION_UNIT_IDLE;
    }
}

xy entity_get_animation_frame(const entity_t& entity) {
    if (entity_is_unit(entity.type)) {
        xy frame = entity.animation.frame;

        if (entity.mode == MODE_UNIT_BUILD) {
            frame.y = 2;
        } else if (entity.animation.name == ANIMATION_UNIT_DEATH || entity.animation.name == ANIMATION_UNIT_DEATH_FADE) {
            frame.y = entity.direction == DIRECTION_NORTH ? 1 : 0;
        } else if (entity.direction == DIRECTION_NORTH) {
            frame.y = 1;
        } else if (entity.direction == DIRECTION_SOUTH) {
            frame.y = 0;
        } else {
            frame.y = 2;
        }

        if (entity_check_flag(entity, ENTITY_FLAG_GOLD_HELD) && (entity.animation.name == ANIMATION_UNIT_MOVE || entity.animation.name == ANIMATION_UNIT_IDLE)) {
            frame.y += 3;
        }

        return frame;
    } else if (entity_is_building(entity.type)) {
        int hframe = entity.mode == MODE_BUILDING_IN_PROGRESS
                    ? ((3 * entity.health) / ENTITY_DATA.at(entity.type).max_health)
                    : 3;
        return xy(hframe, 0);
    } else {
        // TODO mines
        return xy(0, 0);
    }
}

bool entity_should_flip_h(const entity_t& entity) {
    return entity_is_unit(entity.type) && entity.direction > DIRECTION_SOUTH;
}

bool entity_should_die(const entity_t& entity) {
    if (entity.health != 0) {
        return false;
    }

    if (entity_is_unit(entity.type)) {
        if (entity.mode == MODE_UNIT_DEATH || entity.mode == MODE_UNIT_DEATH_FADE) {
            return false;
        }
        if(entity_check_flag(entity, ENTITY_FLAG_IS_GARRISONED)) {
            return false;
        }

        return true;
    } else if (entity_is_building(entity.type)) {
        return entity.mode != MODE_BUILDING_DESTROYED;
    }

    return false;
}

void entity_set_target(entity_t& entity, target_t target) {
    GOLD_ASSERT(entity.mode != MODE_UNIT_BUILD);
    entity.target = target;
    entity.path.clear();
    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, false);

    if (entity.mode != MODE_UNIT_MOVE && entity.mode != MODE_UNIT_IN_MINE) {
        // Abandon current behavior in favor of new order
        entity.timer = 0;
        entity.mode = MODE_UNIT_IDLE;
    }
}

void entity_update(match_state_t& state, uint32_t entity_index) {
    // TODO if this entity is a mine, don't update
    entity_id id = state.entities.get_id_of(entity_index);
    entity_t& entity = state.entities[entity_index];
    const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

    if (entity_should_die(entity)) {
        if (entity_is_unit(entity.type)) {
            entity.mode = MODE_UNIT_DEATH;
            entity.animation = animation_create(entity_get_expected_animation(entity));
        } else {
            entity.mode = MODE_BUILDING_DESTROYED;
            entity.timer = BUILDING_FADE_DURATION;
            entity.queue.clear();

            // Set building cells to empty, done a special way to avoid overriding the miner cell
            // in the case of a building cancel where the builder is placed on top of the building's old location
            for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size(entity.type); x++) {
                for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size(entity.type); y++) {
                    if (map_get_cell(state, xy(x, y)) == id) {
                        state.map_cells[x + (y * state.map_width)] = CELL_EMPTY;
                    }
                }
            }
        }

        ui_deselect_entity_if_selected(state, id);

        // TODO
        /*
        for (entity_id garrisoned_unit_id : entity.garrisoned_units) {

        }
        */
        return;
    } // End if entity_should_die

    bool update_finished = false;
    fixed movement_left = entity_is_unit(entity.type) ? entity_data.unit_data.speed : fixed::from_raw(0);
    while (!update_finished) {
        switch (entity.mode) {
            case MODE_UNIT_IDLE: {
                if (entity.target.type == TARGET_NONE) {
                    update_finished = true;
                    break;
                }

                if (entity_is_target_invalid(state, entity)) {
                    entity.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    update_finished = true;
                    break;
                }

                if (entity_has_reached_target(state, entity)) {
                    entity.mode = MODE_UNIT_MOVE_FINISHED;
                    break;
                }

                if (entity_check_flag(entity, ENTITY_FLAG_HOLD_POSITION)) {
                    update_finished = true;
                    break;
                }

                map_pathfind(state, entity.cell, entity_get_target_cell(state, entity), entity_cell_size(entity.type), &entity.path, false);
                if (!entity.path.empty()) {
                    entity.mode = MODE_UNIT_MOVE;
                    break;
                } else {
                    if (entity.target.type == TARGET_BUILD) {
                        // show can't build message
                        update_finished = true;
                        break;
                    } else {
                        entity.timer = UNIT_MOVE_BLOCKED_DURATION;
                        entity.mode = MODE_UNIT_MOVE_BLOCKED;
                        update_finished = true;
                        break;
                    }
                }

                break;
            }
            case MODE_UNIT_MOVE_BLOCKED: {
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
                        entity.direction = enum_from_xy_direction(entity.path[0] - entity.cell);
                        if (map_is_cell_rect_occupied(state, entity.path[0], entity_cell_size(entity.type), entity.cell, false)) {
                            path_is_blocked = true;
                            // breaks out of while movement left
                            break;
                        }

                        map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                        entity.cell = entity.path[0];
                        map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);
                        entity.path.erase(entity.path.begin());
                    }

                    // Step unit along movement
                    if (entity.position.distance_to(entity_get_target_position(entity)) > movement_left) {
                        entity.position += DIRECTION_XY_FIXED[entity.direction] * movement_left;
                        movement_left = fixed::from_raw(0);
                    } else {
                        movement_left -= entity.position.distance_to(entity_get_target_position(entity));
                        entity.position = entity_get_target_position(entity);
                        // On step finished
                        if (entity.target.type == TARGET_ATTACK_CELL) {
                            // check for nearby target
                        }
                        if (entity_has_reached_target(state, entity)) {
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
                    entity.mode = MODE_UNIT_MOVE_BLOCKED;
                    entity.timer = UNIT_MOVE_BLOCKED_DURATION;
                }

                update_finished = entity.mode != MODE_UNIT_MOVE_FINISHED;
                break;
            }
            case MODE_UNIT_MOVE_FINISHED: {
                switch (entity.target.type) {
                    case TARGET_NONE:
                    case TARGET_ATTACK_CELL:
                    case TARGET_CELL: {
                        entity.target = (target_t) {
                            .type = TARGET_NONE
                        };
                        entity.mode = MODE_UNIT_IDLE;
                        break;
                    }
                    case TARGET_UNLOAD: {
                        break;
                    }
                    case TARGET_BUILD: {
                        break;
                    }
                    case TARGET_BUILD_ASSIST: {
                        break;
                    }
                    case TARGET_REPAIR:
                    case TARGET_ENTITY: 
                    case TARGET_ATTACK_ENTITY: {
                        if (entity_is_target_invalid(state, entity)) {
                            entity.target = (target_t) {
                                .type = TARGET_NONE
                            };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        if (!entity_has_reached_target(state, entity)) {
                            // This will trigger a repath
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        entity_t& target = state.entities.get_by_id(entity.target.id);

                        if (entity.target.type == TARGET_ATTACK_ENTITY && ENTITY_DATA.at(entity.type).unit_data.damage != 0) {
                            SDL_Rect target_rect = (SDL_Rect) {
                                .x = target.cell.x, .y = target.cell.y,
                                .w = entity_cell_size(target.type), .h = entity_cell_size(target.type)
                            };
                            entity.direction = enum_direction_to_rect(entity.cell, target_rect);
                            entity.mode = MODE_UNIT_ATTACK_WINDUP;
                            update_finished = true;
                            break;
                        }

                        // TODO garrison
                        // TODO repair

                        // Don't attack allied units
                        if (entity.player_id == target.player_id || ENTITY_DATA.at(entity.type).unit_data.damage == 0) {
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (target_t) {
                                .type = TARGET_NONE
                            };
                            update_finished = true;
                            break;
                        }

                        break;
                    }
                }
                update_finished = !(entity.mode == MODE_UNIT_MOVE && movement_left.raw_value > 0);
                break;
            } // End mode move finished
            case MODE_UNIT_ATTACK_WINDUP: {
                if (entity_is_target_invalid(state, entity)) {
                    // TOOD target = nearest insight enemy
                    entity.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                if (!animation_is_playing(entity.animation)) {
                    entity_attack_target(state, id, state.entities.get_by_id(entity.target.id));
                    entity.timer = ENTITY_DATA.at(entity.type).unit_data.attack_cooldown;
                    entity.mode = MODE_UNIT_ATTACK_COOLDOWN;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_ATTACK_COOLDOWN: {
                if (entity_is_target_invalid(state, entity)) {
                    // TODO target = nearest insight enemy
                    entity.timer = 0;
                    entity.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                if (!entity_has_reached_target(state, entity)) {
                    // Repath to target
                    entity.timer = 0;
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    entity.mode = MODE_UNIT_ATTACK_WINDUP;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_DEATH: {
                if (!animation_is_playing(entity.animation)) {
                    map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                    entity.mode = MODE_UNIT_DEATH_FADE;
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
                        #ifdef GOLD_DEBUG_FAST_TRAIN
                            entity.timer = std::max((int)entity.timer - 10, 0);
                        #else
                            entity.timer--;
                        #endif
                    }

                    if ((entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) || entity.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                        // TODO rally points
                        xy rally_cell = entity.cell + xy(0, entity_cell_size(entity.type));
                        xy exit_cell = entity_get_exit_cell(state, entity.cell, entity_cell_size(entity.type), entity_cell_size(entity.queue[0].unit_type), rally_cell);
                        if (exit_cell.x == -1) {
                            if (entity.timer == 0 && entity.player_id == network_get_player_id()) {
                                ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
                            }
                            entity.timer = BUILDING_QUEUE_EXIT_BLOCKED;
                            update_finished = true;
                            break;
                        } 

                        entity.timer = 0;
                        entity_id unit_id = entity_create(state, entity.queue[0].unit_type, entity.player_id, exit_cell);

                        // TODO on create alert
                        // TODO rally point sets unit target
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
    } // End while !update_finished

    // Update animation
    if (entity_is_unit(entity.type)) {
        AnimationName expected_animation = entity_get_expected_animation(entity);
        if (entity.animation.name != expected_animation || !animation_is_playing(entity.animation)) {
            entity.animation = animation_create(expected_animation);
        }
        animation_update(entity.animation);
    }
}

void entity_attack_target(match_state_t& state, entity_id attacker_id, entity_t& defender) {
    entity_t& attacker = state.entities.get_by_id(attacker_id);
    bool attack_missed = false; // TODO

    int attacker_damage = ENTITY_DATA.at(attacker.type).unit_data.damage;
    int defender_armor = ENTITY_DATA.at(defender.type).armor;
    int damage = std::max(1, attacker_damage - defender_armor);

    defender.health = std::max(0, defender.health - damage);

    // Make the enemy attack back
    // TODO && defender can see attacker
    if (entity_is_unit(defender.type) && defender.mode == MODE_UNIT_IDLE && 
        defender.target.type == TARGET_NONE && ENTITY_DATA.at(defender.type).unit_data.damage != 0 && 
        defender.player_id != attacker.player_id) {
            defender.target = (target_t) {
                .type = TARGET_ATTACK_ENTITY,
                .id = attacker_id
            };
    }

    // TODO attack alerts
    // TOOD create particle effects for cowboys
}

xy entity_get_exit_cell(const match_state_t& state, xy building_cell, int building_size, int unit_size, xy rally_cell) {
    xy exit_cell = xy(-1, -1);
    int exit_cell_dist = -1;
    for (int x = building_cell.x - unit_size; x < building_cell.x + building_size + unit_size; x++) {
        xy cell = xy(x, building_cell.y - unit_size);
        int cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = xy(x, building_cell.y + building_size + (unit_size - 1));
        cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }
    for (int y = building_cell.y - unit_size; y < building_cell.y + building_size + unit_size; y++) {
        xy cell = xy(building_cell.x - unit_size, y);
        int cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = xy(building_cell.x + building_size + (unit_size - 1), y);
        cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }

    return exit_cell;
}

void entity_building_enqueue(match_state_t& state, entity_t& building, building_queue_item_t item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    building.queue.push_back(item);
    if (building.queue.size() == 1) {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.player_id == network_get_player_id() && building.timer != BUILDING_QUEUE_BLOCKED) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(item);
        }
    }
}

void entity_building_dequeue(match_state_t& state, entity_t& building) {
    GOLD_ASSERT(!building.queue.empty());
    building.queue.erase(building.queue.begin());
    if (building.queue.empty()) {
        building.timer = 0;
    } else {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.player_id == network_get_player_id() && building.timer != BUILDING_QUEUE_BLOCKED) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(building.queue[0]);
        }
    }
}

bool entity_building_is_supply_blocked(const match_state_t& state, const entity_t& building) {
    const building_queue_item_t& item = building.queue[0];
    if (item.type == BUILDING_QUEUE_ITEM_UNIT) {
        uint32_t required_population = match_get_player_population(state, building.player_id) + ENTITY_DATA.at(item.unit_type).unit_data.population_cost;
        if (match_get_player_max_population(state, building.player_id) < required_population) {
            return true;
        }
    }
    return false;
}

UiButton building_queue_item_icon(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).ui_button;
        }
    }
}

uint32_t building_queue_item_duration(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).train_duration * 60;
        }
    }
}

uint32_t building_queue_item_cost(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).gold_cost;
        }
    }
}

uint32_t building_queue_population_cost(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).unit_data.population_cost;
        }
        default:
            return 0;
    }
}
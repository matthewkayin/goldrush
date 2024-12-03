#include "match.h"

#include "network.h"
#include "logger.h"
#include <unordered_map>

const uint32_t UNIT_MOVE_BLOCKED_DURATION = 30;

std::unordered_map<EntityType, entity_data_t> ENTITY_DATA = {
    { UNIT_MINER, (entity_data_t) {
        .name = "Miner",
        .sprite = SPRITE_UNIT_MINER,
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
    }}
};

entity_id entity_create_unit(match_state_t& state, EntityType type, uint8_t player_id, xy cell) {
    entity_t unit;
    unit.type = type;
    unit.mode = MODE_UNIT_IDLE;
    unit.player_id = player_id;
    unit.flags = 0;

    unit.cell = cell;
    unit.position = cell_center(unit.cell);
    unit.direction = DIRECTION_SOUTH;

    unit.health = ENTITY_DATA.at(unit.type).max_health;
    unit.target = (target_t) {
        .type = TARGET_NONE
    };
    unit.timer = 0;

    entity_id id = state.entities.push_back(unit);
    map_set_cell_rect(state, unit.cell, entity_cell_size(unit.type), id);

    return id;
}

bool entity_is_unit(EntityType entity) {
    return true;
}

int entity_cell_size(EntityType entity) {
    return 1;
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
    if (entity_is_unit(entity.type)) {
        return (Sprite)(SPRITE_UNIT_MINER + entity.type);
    }
    log_warn("Unhandled condition in entity_get_sprite()");
    return SPRITE_UNIT_MINER;
}

Sprite entity_get_select_ring(const entity_t entity) {
    Sprite select_ring; 
    if (entity_is_unit(entity.type)) {
        select_ring = (Sprite)(SPRITE_SELECT_RING_UNIT_1 + ((entity_cell_size(entity.type) - 1) * 2));
    } else {
        select_ring = (Sprite)(SPRITE_SELECT_RING_BUILDING_2 + ((entity_cell_size(entity.type) - 2) * 2));
    }
    if (entity.player_id != PLAYER_NONE && entity.player_id != network_get_player_id()) {
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
    if (!(entity.target.type == TARGET_ENTITY || entity.target.type == TARGET_REPAIR || entity.target.type == TARGET_BUILD_ASSIST)) {
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
        case TARGET_ATTACK_MOVE:
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
            return entity.player_id == target.player_id || entity_range_squared == 1
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
        case TARGET_ATTACK_MOVE:
        case TARGET_UNLOAD:
            return entity.target.cell;
        case TARGET_ENTITY:
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
    } else {
        // TODO
        return xy(0, 0);
    }
}

bool entity_should_flip_h(const entity_t& entity) {
    return entity_is_unit(entity.type) && entity.direction > DIRECTION_SOUTH;
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
                        if (entity.target.type == TARGET_ATTACK_MOVE) {
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
                    case TARGET_ATTACK_MOVE:
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
                    case TARGET_ENTITY: {
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
                        
                        // TODO attack

                        break;
                    }
                }
                update_finished = !(entity.mode == MODE_UNIT_MOVE && movement_left.raw_value > 0);
                break;
            }
            default:
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
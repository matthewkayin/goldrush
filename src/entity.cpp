#include "match.h"

#include "logger.h"

xy_fixed cell_center(xy cell) {
    return xy_fixed(
        fixed::from_int((cell.x * TILE_SIZE) + (TILE_SIZE / 2)),
        fixed::from_int((cell.y * TILE_SIZE) + (TILE_SIZE / 2))
    );
}

entity_id entity_create_unit(match_state_t& state, EntityType type, uint8_t player_id, xy cell) {
    entity_t unit;
    unit.type = type;
    unit.player_id = player_id;
    unit.cell = cell;
    unit.position = cell_center(unit.cell);

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

Sprite entity_get_sprite(const entity_t entity) {
    if (entity_is_unit(entity.type)) {
        return (Sprite)(SPRITE_UNIT_MINER + entity.type);
    }
    log_warn("Unhandled condition in entity_get_sprite()");
    return SPRITE_UNIT_MINER;
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
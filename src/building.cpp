#include "match.h"

const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA = {
    { BUILDING_HOUSE, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 100,
        .max_health = 100,
        .builder_positions_x = { 3, 16, -4 },
        .builder_positions_y = { 15, 15, 3 },
        .builder_flip_h = { false, true, false }
    }},
    { BUILDING_CAMP, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 100,
        .max_health = 12,
        .builder_positions_x = { 1, 15, 14 },
        .builder_positions_y = { 13, 13, 2 },
        .builder_flip_h = { false, true, true }
    }},
    { BUILDING_SALOON, (building_data_t) {
        .cell_width = 3,
        .cell_height = 3,
        .cost = 200,
        .max_health = 200,
        .builder_positions_x = { 6, 27, 9 },
        .builder_positions_y = { 32, 27, 9 },
        .builder_flip_h = { false, true, false }
    }}
};


entity_id building_create(match_state_t& state, uint8_t player_id, BuildingType type, xy cell) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;

    building_t building;
    building.player_id = player_id;
    building.type = type;
    building.health = building_data.max_health / 10;

    building.cell = cell;

    building.is_finished = false;

    entity_id building_id = state.buildings.push_back(building);
    map_set_cell_rect(state, rect_t(cell, xy(building_data.cell_width, building_data.cell_height)), CELL_BUILDING, building_id);
    return building_id;
}

void building_destroy(match_state_t& state, entity_id building_id) {
    uint32_t building_index = state.buildings.get_index_of(building_id);
    GOLD_ASSERT(building_index != INDEX_INVALID);
    building_t& building = state.buildings[building_index];
    const building_data_t& building_data = BUILDING_DATA.find(building.type)->second;

    map_set_cell_rect(state, rect_t(building.cell, xy(building_data.cell_width, building_data.cell_height)), CELL_EMPTY);
    state.buildings.remove_at(building_index);
}

xy building_cell_size(BuildingType type) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;
    return xy(building_data.cell_width, building_data.cell_height);
}

rect_t building_get_rect(const building_t& building) {
    return rect_t(building.cell * TILE_SIZE, building_cell_size(building.type) * TILE_SIZE);
}

bool building_can_be_placed(const match_state_t& state, BuildingType type, xy cell) {
    rect_t building_rect = rect_t(cell, building_cell_size(type));
    if (!map_is_cell_in_bounds(state, building_rect.position + building_rect.size)) {
        return false;
    }

    // Check that all cells in the rect are explored
    for (int x = building_rect.position.x; x < building_rect.position.x + building_rect.size.x; x++) {
        for (int y = building_rect.position.y; y < building_rect.position.y + building_rect.size.y; y++) {
            if (state.map_fog[x + (y * state.map_width)] == FOG_HIDDEN) {
                return false;
            }
        }
    }

    return !map_is_cell_rect_blocked(state, building_rect);
}

Sprite building_get_select_ring(BuildingType type) {
    if (building_cell_size(type) == xy(2, 2)) {
        return SPRITE_SELECT_RING_BUILDING_2X2;
    } else {
        return SPRITE_SELECT_RING_BUILDING_3X3;
    }
}
#include "match.h"

// Building

entity_id building_create(match_state_t& state, uint8_t player_id, BuildingType type, xy cell) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;

    building_t building;
    building.player_id = player_id;
    building.type = type;
    building.health = 3;

    building.cell = cell;

    building.is_finished = false;

    entity_id building_id = state.buildings.push_back(building);
    map_set_cell_rect_value(state, rect_t(cell, xy(building_data.cell_width, building_data.cell_height)), CELL_BUILDING, building_id);
    return building_id;
}

void building_destroy(match_state_t& state, entity_id building_id) {
    uint32_t building_index = state.buildings.get_index_of(building_id);
    GOLD_ASSERT(building_index != INDEX_INVALID);
    building_t& building = state.buildings[building_index];
    const building_data_t& building_data = BUILDING_DATA.find(building.type)->second;

    map_set_cell_rect_value(state, rect_t(building.cell, xy(building_data.cell_width, building_data.cell_height)), CELL_EMPTY);
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
    return map_is_cell_in_bounds(state, building_rect.position + building_rect.size) && !map_is_cell_rect_blocked(state, building_rect);
}
#include "match.h"

#include <algorithm>

const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA = {
    { BUILDING_HOUSE, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 100,
        .max_health = 150,
        .builder_positions_x = { 3, 16, -4 },
        .builder_positions_y = { 15, 15, 3 },
        .builder_flip_h = { false, true, false }
    }},
    { BUILDING_CAMP, (building_data_t) {
        .cell_width = 2,
        .cell_height = 2,
        .cost = 75,
        .max_health = 100,
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
    building.queue_timer = 0;

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

void building_update(match_state_t& state, building_t& building) {
    if (building.queue_timer != 0) {
        if (building.queue_timer != BUILDING_QUEUE_BLOCKED) {
#ifdef GOLD_DEBUG_FAST_TRAIN
            building.queue_timer = std::max((int)building.queue_timer - 50, 0);
#else
            building.queue_timer--;
#endif
        }
        
        // On queue item finish
        if (building.queue_timer == 0 || building.queue_timer == BUILDING_QUEUE_BLOCKED) {
            building_queue_item_t& item = building.queue[0];
            bool item_should_dequeue = false;
            switch (item.type) {
                case BUILDING_QUEUE_ITEM_UNIT: {
                    uint32_t required_population = match_get_player_population(state, building.player_id) + UNIT_DATA.at(item.unit_type).population_cost;
                    if (match_get_player_max_population(state, building.player_id) < required_population) {
                        if (building.queue_timer != BUILDING_QUEUE_BLOCKED) {
                            ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
                        }
                        building.queue_timer = BUILDING_QUEUE_BLOCKED;
                        break;
                    }

                    xy unit_spawn_cell = get_first_empty_cell_around_rect(state, rect_t(building.cell, building_cell_size(building.type)));
                    unit_create(state, building.player_id, item.unit_type, unit_spawn_cell);
                    item_should_dequeue = true;

                    break;
                }
            }

            if (item_should_dequeue) {
                building_dequeue(building);
            }
        }
    }
}

void building_enqueue(building_t& building, building_queue_item_t item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    if (building.queue.empty()) {
        building.queue_timer = building_queue_item_duration(item);
    }
    building.queue.push_back(item);
}

void building_dequeue(building_t& building) {
    GOLD_ASSERT(!building.queue.empty());
    building.queue.erase(building.queue.begin());
    if (building.queue.empty()) {
        building.queue_timer = 0;
    } else {
        building.queue_timer = building_queue_item_duration(building.queue[0]);
    }
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
            if (map_get_fog(state, xy(x, y)).type == FOG_HIDDEN) {
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

uint32_t building_queue_item_duration(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return UNIT_DATA.at(item.unit_type).train_duration;
        }
    }
}

UiButton building_queue_item_icon(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return (UiButton)(UI_BUTTON_UNIT_MINER + (item.unit_type - UNIT_MINER));
        }
    }
}

uint32_t building_queue_item_cost(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return UNIT_DATA.at(item.unit_type).cost;
        }
    }
}
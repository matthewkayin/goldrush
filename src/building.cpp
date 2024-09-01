#include "match.h"

#include "network.h"
#include "logger.h"
#include <algorithm>

const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA = {
    { BUILDING_HOUSE, (building_data_t) {
        .name = "House",
        .cell_size = 2,
        .cost = 100,
        .max_health = 300,
        .builder_positions_x = { 3, 16, -4 },
        .builder_positions_y = { 15, 15, 3 },
        .builder_flip_h = { false, true, false },
        .can_rally = false
    }},
    { BUILDING_CAMP, (building_data_t) {
        .name = "Mining Camp",
        .cell_size = 2,
        .cost = 150,
        .max_health = 300,
        .builder_positions_x = { 1, 15, 14 },
        .builder_positions_y = { 13, 13, 2 },
        .builder_flip_h = { false, true, true },
        .can_rally = true
    }},
    { BUILDING_SALOON, (building_data_t) {
        .name = "Saloon",
        .cell_size = 3,
        .cost = 200,
        .max_health = 800,
        .builder_positions_x = { 6, 27, 9 },
        .builder_positions_y = { 32, 27, 9 },
        .builder_flip_h = { false, true, false },
        .can_rally = true
    }}
};

entity_id building_create(match_state_t& state, uint8_t player_id, BuildingType type, xy cell) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;

    building_t building;
    building.player_id = player_id;
    building.type = type;
    building.health = building_data.max_health / 10;

    building.cell = cell;

    building.mode = BUILDING_MODE_IN_PROGRESS;
    building.queue_timer = 0;
    building.rally_point = xy(-1, -1);

    entity_id building_id = state.buildings.push_back(building);
    map_set_cell_rect(state, rect_t(cell, xy(building_data.cell_size, building_data.cell_size)), CELL_BUILDING, building_id);
    return building_id;
}

void building_destroy(match_state_t& state, uint32_t building_index) {
    state.buildings.remove_at(building_index);
}

void building_update(match_state_t& state, building_t& building) {
    if (building.health == 0 && building.mode != BUILDING_MODE_DESTROYED) {
        const building_data_t& building_data = BUILDING_DATA.find(building.type)->second;
        map_set_cell_rect(state, rect_t(building.cell, xy(building_data.cell_size, building_data.cell_size)), CELL_EMPTY);
        building.mode = BUILDING_MODE_DESTROYED;
        building.queue.clear();
        building.queue_timer = BUILDING_FADE_DURATION;
    }

    // This code uses the queue_timer in order to handle building decay
    if (building.health == 0) {
        if (building.queue_timer != 0) {
            building.queue_timer--;
        }
        return;
    }

    if (building.queue_timer != 0) {
        if (building.queue_timer == BUILDING_QUEUE_BLOCKED && !building_is_supply_blocked(state, building)) {
            building.queue_timer = building_queue_item_duration(building.queue[0]);
        } else if (building.queue_timer != BUILDING_QUEUE_BLOCKED && building_is_supply_blocked(state, building)) {
            building.queue_timer = BUILDING_QUEUE_BLOCKED;
        }

        if (building.queue_timer != BUILDING_QUEUE_BLOCKED) {
#ifdef GOLD_DEBUG_FAST_TRAIN
            building.queue_timer = std::max((int)building.queue_timer - 10, 0);
#else
            building.queue_timer--;
#endif
        }
        
        // On queue item finish
        if (building.queue_timer == 0) {
            building_queue_item_t& item = building.queue[0];
            switch (item.type) {
                case BUILDING_QUEUE_ITEM_UNIT: {
                    xy unit_spawn_cell = get_first_empty_cell_around_rect(state, unit_cell_size(item.unit_type), rect_t(building.cell, building_cell_size(building.type)));
                    entity_id unit_id = unit_create(state, building.player_id, item.unit_type, unit_spawn_cell);
                    if (building.rally_point.x != -1) {
                        xy rally_cell = building.rally_point / TILE_SIZE;
                        unit_t& unit = state.units.get_by_id(unit_id);
                        unit.target = (unit_target_t) {
                            .type = map_get_cell(state, rally_cell).type == CELL_MINE && unit.type == UNIT_MINER
                                        ? UNIT_TARGET_MINE
                                        : UNIT_TARGET_CELL,
                            .id = map_get_cell(state, rally_cell).value
                        };
                    }
                    break;
                }
            }

            building_dequeue(state, building);
        }
    }
}

void building_enqueue(match_state_t& state, building_t& building, building_queue_item_t item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    building.queue.push_back(item);
    if (building.queue.size() == 1) {
        if (building_is_supply_blocked(state, building)) {
            if (building.player_id == network_get_player_id() && building.queue_timer != BUILDING_QUEUE_BLOCKED) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.queue_timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.queue_timer = building_queue_item_duration(item);
        }
    }
}

void building_dequeue(match_state_t& state, building_t& building) {
    GOLD_ASSERT(!building.queue.empty());
    building.queue.erase(building.queue.begin());
    if (building.queue.empty()) {
        building.queue_timer = 0;
    } else {
        if (building_is_supply_blocked(state, building)) {
            if (building.player_id == network_get_player_id() && building.queue_timer != BUILDING_QUEUE_BLOCKED) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
            }
            building.queue_timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.queue_timer = building_queue_item_duration(building.queue[0]);
        }
    }
}

bool building_is_supply_blocked(const match_state_t& state, const building_t& building) {
    const building_queue_item_t& item = building.queue[0];
    if (item.type == BUILDING_QUEUE_ITEM_UNIT) {
        uint32_t required_population = match_get_player_population(state, building.player_id) + UNIT_DATA.at(item.unit_type).population_cost;
        if (match_get_player_max_population(state, building.player_id) < required_population) {
            return true;
        }
    }
    return false;
}

xy building_cell_size(BuildingType type) {
    const building_data_t& building_data = BUILDING_DATA.find(type)->second;
    return xy(building_data.cell_size, building_data.cell_size);
}

rect_t building_get_rect(const building_t& building) {
    return rect_t(building.cell * TILE_SIZE, building_cell_size(building.type) * TILE_SIZE);
}

bool building_can_be_placed(const match_state_t& state, BuildingType type, xy cell) {
    rect_t building_rect = rect_t(cell, building_cell_size(type));
    if (!map_is_cell_in_bounds(state, building_rect.position + building_rect.size - xy(1, 1))) {
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

Sprite building_get_select_ring(BuildingType type, bool is_enemy) {
    if (building_cell_size(type) == xy(2, 2)) {
        return is_enemy 
                    ? SPRITE_SELECT_RING_BUILDING_2_ATTACK
                    : SPRITE_SELECT_RING_BUILDING_2;
    } else {
        return is_enemy
                    ? SPRITE_SELECT_RING_BUILDING_3_ATTACK
                    : SPRITE_SELECT_RING_BUILDING_3;
    }
}

uint32_t building_queue_item_duration(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return UNIT_DATA.at(item.unit_type).train_duration * 60;
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

uint32_t building_queue_population_cost(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return UNIT_DATA.at(item.unit_type).population_cost;
        }
        default:
            return 0;
    }
}
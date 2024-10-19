#include "match.h"

#include "network.h"
#include "logger.h"
#include <algorithm>

const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA = {
    { BUILDING_HOUSE, (building_data_t) {
        .name = "House",
        .cell_size = 2,
        .cost = 100,
        .max_health = 200,
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
        .max_health = 500,
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

    building.taking_damage_timer = 0;
    building.taking_damage_flicker_timer = 0;
    building.taking_damage_flicker = false;

    building.occupancy = OCCUPANCY_EMPTY;

    entity_id building_id = state.buildings.push_back(building);
    map_set_cell_rect(state, rect_t(cell, xy(building_data.cell_size, building_data.cell_size)), CELL_BUILDING, building_id);
    return building_id;
}

void building_on_finish(match_state_t& state, entity_id building_id) {
    building_t& building = state.buildings.get_by_id(building_id);

    building.mode = BUILDING_MODE_FINISHED;

    // Show alert
    if (building.player_id == network_get_player_id()) {
        rect_t screen_rect = rect_t(state.camera_offset, xy(SCREEN_WIDTH, SCREEN_HEIGHT));
        rect_t building_rect = building_get_rect(building);
        if (!screen_rect.intersects(building_rect)) {
            state.alerts.push_back((alert_t) {
                .type = ALERT_GREEN,
                .status = ALERT_STATUS_SHOW,
                .cell = building.cell,
                .cell_size = building_cell_size(building.type),
                .timer = MATCH_ALERT_DURATION
            });
        }
    }

    // If selecting the building
    if (state.selection.type == SELECTION_TYPE_BUILDINGS && state.selection.ids[0] == building_id) {
        // Trigger a re-select so that UI buttons are updated correctly
        ui_set_selection(state, state.selection);
    }

    for (uint32_t unit_index = 0; unit_index < state.units.size(); unit_index++) {
        unit_t& unit = state.units[unit_index];
        if (unit.target.type == UNIT_TARGET_BUILD && unit.target.build.building_id == building_id) {
            unit_stop_building(state, state.units.get_id_of(unit_index));
            // If the unit was unable to stop building, notify the user that the exit is blocked
            if (unit.mode != UNIT_MODE_IDLE && unit.player_id == network_get_player_id()) {
                ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
            }
            if (unit.mode == UNIT_MODE_IDLE && building.type == BUILDING_CAMP) {
                unit.target = unit_target_nearest_mine(state, unit);
            }
        } else if (unit.mode == UNIT_MODE_REPAIR && building.type == BUILDING_CAMP && unit.target.id == building_id) {
            unit.target = unit_target_nearest_mine(state, unit);
            unit.mode = UNIT_MODE_IDLE;
        }
    }
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

    if (building.queue_timer != 0 && building.mode == BUILDING_MODE_FINISHED) {
        if (building.queue_timer == BUILDING_QUEUE_BLOCKED && !building_is_supply_blocked(state, building)) {
            building.queue_timer = building_queue_item_duration(building.queue[0]);
        } else if (building.queue_timer != BUILDING_QUEUE_BLOCKED && building_is_supply_blocked(state, building)) {
            building.queue_timer = BUILDING_QUEUE_BLOCKED;
        } 

        if (building.queue_timer != BUILDING_QUEUE_BLOCKED && building.queue_timer != BUILDING_QUEUE_EXIT_BLOCKED) {
#ifdef GOLD_DEBUG_FAST_TRAIN
            building.queue_timer = std::max((int)building.queue_timer - 10, 0);
#else
            building.queue_timer--;
#endif
        }

        if ((building.queue_timer == 0 && building.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) ||
             building.queue_timer == BUILDING_QUEUE_EXIT_BLOCKED) {
            xy rally_cell = building.rally_point.x != -1 ? (building.rally_point / TILE_SIZE) : building.cell + xy(0, building_cell_size(building.type).y);
            if (get_exit_cell(state, rect_t(building.cell, building_cell_size(building.type)), unit_cell_size(building.queue[0].unit_type), rally_cell).x == -1) {
                if (building.queue_timer == 0 && building.player_id == network_get_player_id()) {
                    ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
                }
                building.queue_timer = BUILDING_QUEUE_EXIT_BLOCKED;
            } else {
                building.queue_timer = 0;
            }
        }
        
        // On queue item finish
        if (building.queue_timer == 0) {
            building_queue_item_t& item = building.queue[0];
            switch (item.type) {
                case BUILDING_QUEUE_ITEM_UNIT: {
                    // Spawn unit
                    xy rally_cell = building.rally_point.x != -1 ? (building.rally_point / TILE_SIZE) : building.cell + xy(0, building_cell_size(building.type).y);
                    xy exit_cell = get_exit_cell(state, rect_t(building.cell, building_cell_size(building.type)), unit_cell_size(building.queue[0].unit_type), rally_cell);
                    GOLD_ASSERT(exit_cell.x != -1);
                    entity_id unit_id = unit_create(state, building.player_id, item.unit_type, exit_cell);

                    if (building.player_id == network_get_player_id()) {
                        rect_t screen_rect = rect_t(state.camera_offset, xy(SCREEN_WIDTH, SCREEN_HEIGHT));
                        rect_t building_rect = building_get_rect(building);
                        if (!screen_rect.intersects(building_rect)) {
                            state.alerts.push_back((alert_t) {
                                .type = ALERT_GREEN,
                                .status = ALERT_STATUS_SHOW,
                                .cell = exit_cell,
                                .cell_size = unit_cell_size(item.unit_type),
                                .timer = MATCH_ALERT_DURATION
                            });
                        }
                    }

                    // Set unit target if there is a rally point
                    if (building.rally_point.x != -1) {
                        xy rally_cell = building.rally_point / TILE_SIZE;
                        unit_t& unit = state.units.get_by_id(unit_id);
                        if (map_get_cell(state, rally_cell).type == CELL_MINE && unit.type == UNIT_MINER) {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_MINE,
                                .id = map_get_cell(state, rally_cell).value
                            };
                        } else {
                            unit.target = (unit_target_t) {
                                .type = UNIT_TARGET_CELL,
                                .cell = rally_cell
                            };
                        }
                    }
                    break;
                }
            }

            building_dequeue(state, building);
        }
    }

    if (building.taking_damage_timer > 0) {
        building.taking_damage_timer--;
        if (building.taking_damage_timer == 0) {
            building.taking_damage_flicker = false;
        } else {
            building.taking_damage_flicker_timer--;
            if (building.taking_damage_flicker_timer == 0) {
                building.taking_damage_flicker_timer = MATCH_TAKING_DAMAGE_FLICKER_TIMER_DURATION;
                building.taking_damage_flicker = !building.taking_damage_flicker;
            }
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

bool building_is_finished(const building_t& building) {
    return building.mode == BUILDING_MODE_FINISHED;
}

int building_get_hframe(BuildingType type, BuildingMode mode, int health, Occupancy occupancy) {
    if (mode == BUILDING_MODE_IN_PROGRESS) {
        return ((3 * health) / BUILDING_DATA.at(type).max_health);
    } else if (occupancy == OCCUPANCY_FULL) {
        return 4;
    } else {
        return 3;
    }
}

bool building_can_be_placed(const match_state_t& state, BuildingType type, xy cell, xy miner_cell) {
    rect_t building_rect = rect_t(cell, building_cell_size(type));
    if (!map_is_cell_in_bounds(state, building_rect.position + building_rect.size - xy(1, 1))) {
        return false;
    }

    // Check that all cells in the rect are explored
    for (int x = building_rect.position.x; x < building_rect.position.x + building_rect.size.x; x++) {
        for (int y = building_rect.position.y; y < building_rect.position.y + building_rect.size.y; y++) {
            if (map_get_fog(state, network_get_player_id(), xy(x, y)) == FOG_HIDDEN) {
                return false;
            }
        }
    }

    if (type == BUILDING_CAMP) {
        for (const mine_t& mine : state.mines) {
            if (building_rect.intersects(mine_get_block_building_rect(mine.cell))) {
                return false;
            }
        }
    }

    for (int x = building_rect.position.x; x < building_rect.position.x + building_rect.size.x; x++) {
        for (int y = building_rect.position.y; y < building_rect.position.y + building_rect.size.y; y++) {
            if (xy(x, y) == miner_cell) {
                continue;
            }
            if (map_is_cell_blocked(state, xy(x, y)) || state.map_tiles[x + (y * state.map_width)].is_ramp == 1) {
                return false;
            }
        }
    }

    return true;
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
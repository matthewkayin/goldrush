#include "match.h"

#include "network.h"
#include "input.h"
#include "logger.h"

static const uint32_t UI_STATUS_DURATION = 60;
const std::unordered_map<UiButtonset, std::array<UiButton, 6>> UI_BUTTONS = {
    { UI_BUTTONSET_NONE, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_UNIT, { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_MINER, { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_BUILD,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_BUILD, { UI_BUTTON_BUILD_CAMP, UI_BUTTON_BUILD_HOUSE, UI_BUTTON_BUILD_SALOON,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_CANCEL, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_CAMP, { UI_BUTTON_UNIT_MINER, UI_BUTTON_NONE, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_SALOON, { UI_BUTTON_UNIT_COWBOY, UI_BUTTON_NONE, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_WAGON, { UI_BUTTON_STOP, UI_BUTTON_UNLOAD, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
};
static const xy UI_BUTTON_SIZE = xy(32, 32);
static const xy UI_BUTTON_PADDING = xy(4, 6);
static const xy UI_BUTTON_TOP_LEFT = xy(SCREEN_WIDTH - 132 + 14, SCREEN_HEIGHT - UI_HEIGHT + 10);
static const xy UI_BUTTON_POSITIONS[6] = { UI_BUTTON_TOP_LEFT, UI_BUTTON_TOP_LEFT + xy(UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x, 0), UI_BUTTON_TOP_LEFT + xy(2 * (UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x), 0),
                                              UI_BUTTON_TOP_LEFT + xy(0, UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y), UI_BUTTON_TOP_LEFT + xy(UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x, UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y), UI_BUTTON_TOP_LEFT + xy(2 * (UI_BUTTON_SIZE.x + UI_BUTTON_PADDING.x), UI_BUTTON_SIZE.y + UI_BUTTON_PADDING.y) };
static const rect_t UI_BUTTON_RECT[6] = {
    rect_t(UI_BUTTON_POSITIONS[0], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[1], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[2], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[3], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[4], UI_BUTTON_SIZE),
    rect_t(UI_BUTTON_POSITIONS[5], UI_BUTTON_SIZE),
};

const xy UI_FRAME_BOTTOM_POSITION = xy(136, SCREEN_HEIGHT - UI_HEIGHT);
const xy BUILDING_QUEUE_TOP_LEFT = xy(164, 12);
const xy UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX] = {
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT,
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(0, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36 * 2, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36 * 3, 35)
};

const std::unordered_map<UiButton, ui_button_requirements_t> UI_BUTTON_REQUIREMENTS = {
    { UI_BUTTON_BUILD_HOUSE, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = BUILDING_CAMP
    }},
    { UI_BUTTON_BUILD_SALOON, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = BUILDING_CAMP
    }}
};

void ui_show_status(match_state_t& state, const char* message) {
    state.ui_status_message = std::string(message);
    state.ui_status_timer = UI_STATUS_DURATION;
}

UiButton ui_get_ui_button(const match_state_t& state, int index) {
    if (index == 5 && state.selection.type == SELECTION_TYPE_BUILDINGS && !state.buildings[state.buildings.get_index_of(state.selection.ids[0])].queue.empty()) {
        return UI_BUTTON_CANCEL;
    }

    return UI_BUTTONS.at(state.ui_buttonset)[index];
}

int ui_get_ui_button_hovered(const match_state_t& state) {
    xy mouse_pos = input_get_mouse_position();
    for (int i = 0; i < 6; i++) {
        if (ui_get_ui_button(state, i) == UI_BUTTON_NONE) {
            continue;
        }

        if (UI_BUTTON_RECT[i].has_point(mouse_pos)) {
            return i;
        }
    }

    return -1;
}

const rect_t& ui_get_ui_button_rect(int index) {
    return UI_BUTTON_RECT[index];
}

bool ui_button_requirements_met(const match_state_t& state, UiButton button) {
    auto ui_button_requirements_it = UI_BUTTON_REQUIREMENTS.find(button);

    // Buttons with no defined requirements are enabled by default
    if (ui_button_requirements_it == UI_BUTTON_REQUIREMENTS.end()) {
        return true;
    }

    switch (ui_button_requirements_it->second.type) {
        case UI_BUTTON_REQUIRES_BUILDING: {
            for (const building_t& building : state.buildings) {
                if (building.player_id == network_get_player_id() && building_is_finished(building) && building.type == ui_button_requirements_it->second.building_type) {
                    return true;
                }
            }
            return false;
        }
    }
}

void ui_handle_button_pressed(match_state_t& state, UiButton button) {
    if (!ui_button_requirements_met(state, button)) {
        return;
    }

    switch (button) {
        case UI_BUTTON_STOP: {
            if (state.selection.type == SELECTION_TYPE_UNITS) {
                input_t input;
                input.type = INPUT_STOP;
                memcpy(input.stop.unit_ids, &state.selection.ids[0], state.selection.ids.size() * sizeof(entity_id));
                input.stop.unit_count = state.selection.ids.size();
                state.input_queue.push_back(input);
            }
            break;
        }
        case UI_BUTTON_BUILD: {
            state.ui_buttonset = UI_BUTTONSET_BUILD;
            break;
        }
        case UI_BUTTON_CANCEL: {
            if (state.ui_buttonset == UI_BUTTONSET_BUILD) {
                state.ui_buttonset = UI_BUTTONSET_MINER;
            } else if (state.ui_mode == UI_MODE_BUILDING_PLACE) {
                state.ui_mode = UI_MODE_NONE;
                state.ui_buttonset = UI_BUTTONSET_BUILD;
            } else if (state.selection.type == SELECTION_TYPE_BUILDINGS && !state.buildings.get_by_id(state.selection.ids[0]).queue.empty()) {
                input_t input = (input_t) {
                    .type = INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (input_building_dequeue_t) {
                        .building_id = state.selection.ids[0],
                        .index = BUILDING_QUEUE_MAX
                    }
                };
                state.input_queue.push_back(input);
            } else if (state.ui_mode == UI_MODE_ATTACK_MOVE) {
                state.ui_mode = UI_MODE_NONE;
                ui_set_selection(state, state.selection);
            } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
                GOLD_ASSERT(state.buildings[state.buildings.get_index_of(state.selection.ids[0])].mode == BUILDING_MODE_IN_PROGRESS);
                input_t input;
                input.type = INPUT_BUILD_CANCEL;
                input.build_cancel.building_id = state.selection.ids[0];
                state.input_queue.push_back(input);

                // Clear the player's selection
                selection_t empty_selection;
                empty_selection.type = SELECTION_TYPE_NONE;
                ui_set_selection(state, empty_selection);
            }
            break;
        }
        case UI_BUTTON_ATTACK: {
            state.ui_mode = UI_MODE_ATTACK_MOVE;
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
            break;
        }
        case UI_BUTTON_UNLOAD: {
            input_t input;
            input.type = INPUT_UNLOAD_ALL;
            input.unload_all.unit_count = 0;
            for (entity_id id : state.selection.ids) {
                if (state.units.get_by_id(id).ferried_units.empty()) {
                    continue;
                }
                input.unload_all.unit_ids[input.unload_all.unit_count] = id;
                input.unload_all.unit_count++;
            }
            state.input_queue.push_back(input);
            break;
        }
        case UI_BUTTON_BUILD_HOUSE:
        case UI_BUTTON_BUILD_CAMP: 
        case UI_BUTTON_BUILD_SALOON: {
            // Begin building placement
            BuildingType building_type = (BuildingType)(BUILDING_HOUSE + (button - UI_BUTTON_BUILD_HOUSE));

            if (state.player_gold[network_get_player_id()] < BUILDING_DATA.at(building_type).cost) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
            } else {
                state.ui_mode = UI_MODE_BUILDING_PLACE;
                state.ui_buttonset = UI_BUTTONSET_CANCEL;
                state.ui_building_type = building_type;
            }

            break;
        }
        case UI_BUTTON_UNIT_MINER: 
        case UI_BUTTON_UNIT_COWBOY: {
            // Begin unit training
            UnitType unit_type = (UnitType)(UNIT_MINER + (button - UI_BUTTON_UNIT_MINER));

            uint32_t selected_building_index = state.buildings.get_index_of(state.selection.ids[0]);
            GOLD_ASSERT(selected_building_index != INDEX_INVALID);
            building_t& building = state.buildings[selected_building_index];

            building_queue_item_t item = (building_queue_item_t) {
                .type = BUILDING_QUEUE_ITEM_UNIT,
                .unit_type = unit_type
            };

            if (building.queue.size() == BUILDING_QUEUE_MAX) {
                ui_show_status(state, UI_STATUS_BUILDING_QUEUE_FULL);
            } else if (state.player_gold[network_get_player_id()] < building_queue_item_cost(item)) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
            } else {
                input_t input = (input_t) {
                    .type = INPUT_BUILDING_ENQUEUE,
                    .building_enqueue = (input_building_enqueue_t) {
                        .building_id = state.selection.ids[0],
                        .item = item
                    }
                };
                state.input_queue.push_back(input);
            }

            break;
        }
        case UI_BUTTON_NONE:
            break;
        default:
            GOLD_ASSERT_MESSAGE(false, "Unhandled UI button press");
    }
}

bool ui_is_mouse_in_ui() {
    xy mouse_pos = input_get_mouse_position();
    return (mouse_pos.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
           (mouse_pos.x <= 136 && mouse_pos.y >= SCREEN_HEIGHT - 136) ||
           (mouse_pos.x >= SCREEN_WIDTH - 132 && mouse_pos.y >= SCREEN_HEIGHT - 106);
}

xy ui_get_building_cell(const match_state_t& state) {
    xy offset = building_cell_size(state.ui_building_type) - xy(2, 2);
    xy building_cell = ((input_get_mouse_position() + state.camera_offset) / TILE_SIZE) - offset;
    building_cell.x = std::max(0, building_cell.x);
    building_cell.y = std::max(0, building_cell.y);
    return building_cell;
}

selection_t ui_create_selection_from_rect(const match_state_t& state) {
    selection_t selection;
    selection.type = SELECTION_TYPE_NONE;

    // Check all the current player's units
    for (uint32_t index = 0; index < state.units.size(); index++) {
        const unit_t& unit = state.units[index];

        // Don't select other player's units
        if (unit.player_id != network_get_player_id()) {
            continue;
        }

        // Don't select units which are building
        if (unit.mode == UNIT_MODE_BUILD || unit.mode == UNIT_MODE_FERRY || unit.mode == UNIT_MODE_IN_MINE) {
            continue;
        }

        if (unit_get_rect(unit).intersects(state.select_rect)) {
            selection.ids.push_back(state.units.get_id_of(index));
        }
    }
    // If we're selecting units, then return the unit selection
    if (!selection.ids.empty()) {
        selection.type = SELECTION_TYPE_UNITS;
        return selection;
    }

    // Otherwise, check the player's buildings
    for (uint32_t index = 0; index < state.buildings.size(); index++) {
        const building_t& building = state.buildings[index];

        // Don't select other player's buildings
        // TODO: remove this to allow enemy building selection
        if (building.player_id != network_get_player_id()) {
            continue;
        }

        if (building_get_rect(building).intersects(state.select_rect)) {
            // Return here so that only one building can be selected at once
            selection.ids.push_back(state.buildings.get_id_of(index));
            selection.type = SELECTION_TYPE_BUILDINGS;
            return selection;
        }
    }

    // Otherwise, check for enemy units
    for (uint32_t index = 0; index < state.units.size(); index++) {
        const unit_t& unit = state.units[index];

        if (unit.player_id == network_get_player_id()) {
            continue;
        }
        // Don't select hidden units
        if (!map_is_cell_rect_revealed(state, network_get_player_id(), rect_t(unit.cell, unit_cell_size(unit.type)))) {
            continue;
        }

        // Don't select units which are building
        if (unit.mode == UNIT_MODE_BUILD || unit.mode == UNIT_MODE_FERRY || unit.mode == UNIT_MODE_IN_MINE) {
            continue;
        }

        if (unit_get_rect(unit).intersects(state.select_rect)) {
            selection.ids.push_back(state.units.get_id_of(index));
            selection.type = SELECTION_TYPE_ENEMY_UNIT;
            return selection;
        }
    }

    // Otherwise, check for enemy buildings
    for (uint32_t index = 0; index < state.buildings.size(); index++) {
        const building_t& building = state.buildings[index];

        if (building.player_id == network_get_player_id()) {
            continue;
        }
        // Don't select hidden buildings
        if (!map_is_cell_rect_revealed(state, network_get_player_id(), rect_t(building.cell, building_cell_size(building.type)))) {
            continue;
        }

        if (building_get_rect(building).intersects(state.select_rect)) {
            selection.ids.push_back(state.buildings.get_id_of(index));
            selection.type = SELECTION_TYPE_ENEMY_BUILDING;
            return selection;
        }
    }

    // Otherwise, check for mines
    for (uint32_t index = 0; index < state.mines.size(); index++) {
        const mine_t& mine = state.mines[index];
        // Don't select hidden gold
        if (!map_is_cell_rect_revealed(state, network_get_player_id(), rect_t(mine.cell, xy(MINE_SIZE, MINE_SIZE)))) {
            continue;
        }

        rect_t mine_rect = rect_t(mine.cell * TILE_SIZE, xy(MINE_SIZE * TILE_SIZE, MINE_SIZE * TILE_SIZE));
        if (mine_rect.intersects(state.select_rect)) {
            selection.ids.push_back(state.mines.get_id_of(index));
            selection.type = SELECTION_TYPE_MINE;
            return selection;
        }
    }

    return selection;
}

void ui_set_selection(match_state_t& state, selection_t& selection) {
    state.selection = selection;
    state.ui_selected_control_group = -1;

    if (state.selection.type == SELECTION_TYPE_UNITS) {
        for (uint32_t id_index = 0; id_index < state.selection.ids.size(); id_index++) {
            uint32_t unit_index = state.units.get_index_of(state.selection.ids[id_index]);
            if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0 || state.units[unit_index].mode == UNIT_MODE_BUILD || state.units[unit_index].mode == UNIT_MODE_FERRY) {
                state.selection.ids.erase(state.selection.ids.begin() + id_index);
                id_index--;
                continue;
            }
        }
    } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        for (uint32_t id_index = 0; id_index < state.selection.ids.size(); id_index++) {
            uint32_t building_index = state.buildings.get_index_of(state.selection.ids[id_index]);
            if (building_index == INDEX_INVALID || state.buildings[building_index].health == 0) {
                state.selection.ids.erase(state.selection.ids.begin() + id_index);
                id_index--;
                continue;
            }
        }
    }

    if (state.selection.ids.size() == 0) {
        state.selection.type = SELECTION_TYPE_NONE;
    }

    if (state.ui_mode == UI_MODE_BUILDING_PLACE) {
        if (state.selection.type != SELECTION_TYPE_UNITS) {
            state.ui_mode = UI_MODE_NONE;
            state.ui_buttonset = UI_BUTTONSET_NONE;
        } else {
            bool is_selection_miners_only = true;
            bool is_selecting_miners = false;
            for (entity_id id : state.selection.ids) {
                if (state.units.get_by_id(id).type == UNIT_MINER) {
                    is_selecting_miners = true;
                } else {
                    is_selection_miners_only = false;
                }
            }
            if (!is_selecting_miners || !is_selection_miners_only) {
                state.ui_mode = UI_MODE_NONE;
                state.ui_buttonset = UI_BUTTONSET_NONE;
            }
        }
    } else if (state.selection.type == SELECTION_TYPE_NONE || state.selection.type == SELECTION_TYPE_ENEMY_UNIT || state.selection.type == SELECTION_TYPE_ENEMY_BUILDING || state.selection.type == SELECTION_TYPE_MINE) {
        state.ui_buttonset = UI_BUTTONSET_NONE;
    } else if (state.selection.type == SELECTION_TYPE_UNITS) {
        bool all_unit_types_are_same = true;
        bool at_least_one_selected_unit_is_ferrying = false;
        UnitType selected_unit_type = state.units.get_by_id(state.selection.ids[0]).type;
        for (entity_id id : state.selection.ids) {
            const unit_t& id_unit = state.units.get_by_id(id);
            if (id_unit.type != selected_unit_type) {
                all_unit_types_are_same = false;
            }
            if (!id_unit.ferried_units.empty()) {
                at_least_one_selected_unit_is_ferrying = true;
            }
        }

        if (all_unit_types_are_same && selected_unit_type == UNIT_MINER) {
            state.ui_buttonset = UI_BUTTONSET_MINER;
        } else if (all_unit_types_are_same && at_least_one_selected_unit_is_ferrying) {
            state.ui_buttonset = UI_BUTTONSET_WAGON;
        } else {
            state.ui_buttonset = UI_BUTTONSET_UNIT;
        }
    } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        building_t& building = state.buildings[state.buildings.get_index_of(state.selection.ids[0])];
        if (building.mode == BUILDING_MODE_IN_PROGRESS) {
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
        } else if (building.type == BUILDING_CAMP) {
            state.ui_buttonset = UI_BUTTONSET_CAMP;
        } else if (building.type == BUILDING_SALOON) {
            state.ui_buttonset = UI_BUTTONSET_SALOON;
        } else {
            state.ui_buttonset = UI_BUTTONSET_NONE;
        }
    }
}

selection_mode_t ui_get_mode_of_selection(const match_state_t& state, const selection_t& selection) {
    // This function is only meant to be used for player control groups
    GOLD_ASSERT(selection.type != SELECTION_TYPE_ENEMY_UNIT && selection.type != SELECTION_TYPE_ENEMY_BUILDING && selection.type != SELECTION_TYPE_MINE);

    selection_mode_t mode;
    mode.type = SELECTION_MODE_NONE;
    mode.count = 0;
    if (selection.ids.empty()) {
        return mode;
    }

    if (selection.type == SELECTION_TYPE_UNITS) {
        std::unordered_map<UnitType, uint32_t> unit_type_count;
        UnitType mode_unit_type = UNIT_MINER;
        uint32_t mode_unit_type_count = 0;

        for (entity_id id : selection.ids) {
            uint32_t unit_index = state.units.get_index_of(id);
            if (unit_index == INDEX_INVALID || state.units[unit_index].health == 0) {
                continue;
            }
            UnitType unit_type = state.units[unit_index].type;
            auto it = unit_type_count.find(unit_type);
            if (it == unit_type_count.end()) {
                unit_type_count[mode_unit_type] = 1;
                if (mode_unit_type_count == 0) {
                    mode_unit_type = unit_type;
                    mode_unit_type_count = 1;
                }
            } else {
                it->second++;
                if (mode_unit_type_count < it->second) {
                    mode_unit_type = unit_type;
                    mode_unit_type_count = it->second;
                }
            }
        }

        if (mode_unit_type_count == 0) {
            return mode;
        }
        mode.type = SELECTION_MODE_UNIT;
        mode.unit_type = mode_unit_type;
        mode.count = selection.ids.size();
        return mode;
    } else if (selection.type == SELECTION_TYPE_BUILDINGS) {
        uint32_t building_index = state.buildings.get_index_of(selection.ids[0]);
        if (building_index == INDEX_INVALID || state.buildings[building_index].health == 0) {
            return mode;
        }
        mode.type = SELECTION_MODE_BUILDING;
        mode.building_type = state.buildings[building_index].type;
        mode.count = selection.ids.size();
        return mode;
    }

    return mode;
}


xy ui_alert_get_cell(const match_state_t& state, const alert_t& alert) {
    switch (alert.type) {
        case ALERT_UNIT_ATTACKED:
        case ALERT_UNIT_FINISHED:
            return state.units.get_by_id(alert.id).cell;
        case ALERT_BUILDING_ATTACKED:
        case ALERT_BUILDING_FINISHED:
            return state.buildings.get_by_id(alert.id).cell;
        case ALERT_MINE_COLLAPSED:
            return state.mines.get_by_id(alert.id).cell;
    }
}

entity_id ui_get_nearest_builder(const match_state_t& state, xy cell) {
    entity_id nearest_unit_id; 
    int nearest_unit_dist = -1;
    for (entity_id id : state.selection.ids) {
        int selection_dist = xy::manhattan_distance(cell, state.units.get_by_id(id).cell);
        if (nearest_unit_dist == -1 || selection_dist < nearest_unit_dist) {
            nearest_unit_id = id;
            nearest_unit_dist = selection_dist;
        }
    }

    return nearest_unit_id;
}

int ui_get_building_queue_index_hovered(const match_state_t& state) {
    if (state.selection.type != SELECTION_TYPE_BUILDINGS || state.selection.ids.size() != 1) {
        return -1;
    }

    const building_t& building = state.buildings.get_by_id(state.selection.ids[0]);
    for (int building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
        if (rect_t(UI_BUILDING_QUEUE_POSITIONS[building_queue_index], xy(32, 32)).has_point(input_get_mouse_position())) {
            return building_queue_index;
        }
    }
    
    return -1;
}
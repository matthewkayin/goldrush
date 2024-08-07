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
    { UI_BUTTONSET_BUILD, { UI_BUTTON_BUILD_HOUSE, UI_BUTTON_BUILD_CAMP, UI_BUTTON_BUILD_SALOON,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_CANCEL, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_SALOON, { UI_BUTTON_UNIT_MINER, UI_BUTTON_NONE, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }}
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

void ui_show_status(match_state_t& state, const char* message) {
    state.ui_status_message = std::string(message);
    state.ui_status_timer = UI_STATUS_DURATION;
}

UiButton ui_get_ui_button(const match_state_t& state, int index) {
    if (state.ui_buttonset == UI_BUTTONSET_SALOON && index == 5 && !state.buildings[state.buildings.get_index_of(state.selection.ids[0])].queue.empty()) {
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

void ui_handle_button_pressed(match_state_t& state, UiButton button) {
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
                log_info("cancel ui mode %u", state.ui_mode);
            } else if (state.ui_buttonset == UI_BUTTONSET_SALOON) {
                uint32_t selected_building_index = state.buildings.get_index_of(state.selection.ids[0]);
                GOLD_ASSERT(selected_building_index != INDEX_INVALID);
                building_t& building = state.buildings[selected_building_index];
                state.player_gold[network_get_player_id()] += building_queue_item_cost(building.queue[0]);
                building_dequeue(building);
            } else if (state.ui_mode == UI_MODE_ATTACK_MOVE) {
                state.ui_mode = UI_MODE_NONE;
                ui_set_selection(state, state.selection);
            } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
                GOLD_ASSERT(!state.buildings[state.buildings.get_index_of(state.selection.ids[0])].is_finished);
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
        case UI_BUTTON_UNIT_MINER: {
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
                building_enqueue(building, item);
                state.player_gold[network_get_player_id()] -= building_queue_item_cost(item);
            }

            break;
        }
        case UI_BUTTON_NONE:
        default:
            break;
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
        if (unit.mode == UNIT_MODE_BUILD) {
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

    return selection;
}

void ui_set_selection(match_state_t& state, selection_t& selection) {
    state.selection = selection;

    if (state.selection.type == SELECTION_TYPE_UNITS) {
        for (uint32_t id_index = 0; id_index < state.selection.ids.size(); id_index++) {
            uint32_t unit_index = state.units.get_index_of(state.selection.ids[id_index]);
            if (unit_index == INDEX_INVALID) {
                state.selection.ids.erase(state.selection.ids.begin() + id_index);
                id_index--;
                continue;
            }
            if (state.units[unit_index].mode == UNIT_MODE_BUILD) {
                state.selection.ids.erase(state.selection.ids.begin() + id_index);
                id_index--;
                continue;
            }
        }
    } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        for (uint32_t id_index = 0; id_index < state.selection.ids.size(); id_index++) {
            uint32_t building_index = state.units.get_index_of(state.selection.ids[id_index]);
            if (building_index == INDEX_INVALID) {
                state.selection.ids.erase(state.selection.ids.begin() + id_index);
                id_index--;
                continue;
            }
        }
    }

    if (state.selection.ids.size() == 0) {
        state.selection.type = SELECTION_TYPE_NONE;
    }

    if (state.selection.type == SELECTION_TYPE_NONE) {
        state.ui_buttonset = UI_BUTTONSET_NONE;
    } else if (state.selection.type == SELECTION_TYPE_UNITS) {
        if (state.selection.ids.size() == 1 && state.units[state.units.get_index_of(state.selection.ids[0])].type == UNIT_MINER) {
            state.ui_buttonset = UI_BUTTONSET_MINER;
        } else {
            state.ui_buttonset = UI_BUTTONSET_UNIT;
        }
    } else if (state.selection.type == SELECTION_TYPE_BUILDINGS) {
        building_t& building = state.buildings[state.buildings.get_index_of(state.selection.ids[0])];
        if (!building.is_finished) {
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
        } else if (building.type == BUILDING_SALOON) {
            state.ui_buttonset = UI_BUTTONSET_SALOON;
        } else {
            state.ui_buttonset = UI_BUTTONSET_NONE;
        }
    }
}

xy ui_camera_clamp(xy camera_offset, int map_width, int map_height) {
    return xy(std::clamp(camera_offset.x, 0, (map_width * TILE_SIZE) - SCREEN_WIDTH),
                 std::clamp(camera_offset.y, 0, (map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT));
}

xy ui_camera_centered_on_cell(xy cell) {
    return xy((cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2), (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2));
}
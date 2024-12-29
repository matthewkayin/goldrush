#include "match.h"

#include "network.h"
#include "logger.h"

static const uint32_t UI_CHAT_MESSAGE_DURATION = 180;
static const uint32_t UI_STATUS_DURATION = 60;

static const int UI_BUTTON_SIZE = 32;
static const xy UI_BUTTON_PADDING = xy(4, 6);
static const int UI_BUTTON_LEFT_PAD = 4;
static const int UI_BUTTON_TOP_PAD = 6;
static const int UI_BUTTON_LEFT = SCREEN_WIDTH - 132 + 14;
static const int UI_BUTTON_TOP = SCREEN_HEIGHT - UI_HEIGHT + 10;
const SDL_Rect UI_BUTTON_RECT[UI_BUTTONSET_SIZE] = {
    (SDL_Rect) { .x = UI_BUTTON_LEFT, .y = UI_BUTTON_TOP, 
                 .w = UI_BUTTON_SIZE, .h = UI_BUTTON_SIZE },
    (SDL_Rect) { .x = UI_BUTTON_LEFT + UI_BUTTON_SIZE + UI_BUTTON_LEFT_PAD, .y = UI_BUTTON_TOP, 
                 .w = UI_BUTTON_SIZE, .h = UI_BUTTON_SIZE },
    (SDL_Rect) { .x = UI_BUTTON_LEFT + (2 * (UI_BUTTON_LEFT_PAD + UI_BUTTON_SIZE)), .y = UI_BUTTON_TOP, 
                 .w = UI_BUTTON_SIZE, .h = UI_BUTTON_SIZE },
    (SDL_Rect) { .x = UI_BUTTON_LEFT, .y = UI_BUTTON_TOP + UI_BUTTON_SIZE + UI_BUTTON_TOP_PAD, 
                 .w = UI_BUTTON_SIZE, .h = UI_BUTTON_SIZE },
    (SDL_Rect) { .x = UI_BUTTON_LEFT + UI_BUTTON_SIZE + UI_BUTTON_LEFT_PAD, .y = UI_BUTTON_TOP + UI_BUTTON_SIZE + UI_BUTTON_TOP_PAD, 
                 .w = UI_BUTTON_SIZE, .h = UI_BUTTON_SIZE },
    (SDL_Rect) { .x = UI_BUTTON_LEFT + (2 * (UI_BUTTON_LEFT_PAD + UI_BUTTON_SIZE)), .y = UI_BUTTON_TOP + UI_BUTTON_SIZE + UI_BUTTON_TOP_PAD, 
                 .w = UI_BUTTON_SIZE, .h = UI_BUTTON_SIZE },
};

const std::unordered_map<UiButton, ui_button_requirements_t> UI_BUTTON_REQUIREMENTS = {
    { UI_BUTTON_BUILD_HOUSE, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_HALL
    }},
    { UI_BUTTON_BUILD_CAMP, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_HALL
    }},
    { UI_BUTTON_BUILD_SALOON, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_HALL
    }},
    { UI_BUTTON_BUILD_BUNKER, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_SALOON
    }}
};

bool ui_is_mouse_in_ui() {
    return (engine.mouse_position.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
           (engine.mouse_position.x <= 136 && engine.mouse_position.y >= SCREEN_HEIGHT - 136) ||
           (engine.mouse_position.x >= SCREEN_WIDTH - 132 && engine.mouse_position.y >= SCREEN_HEIGHT - 106);
}

bool ui_is_selecting(const match_state_t& state) {
    return state.select_rect_origin.x != -1;
}

std::vector<entity_id> ui_create_selection_from_rect(const match_state_t& state) {
    std::vector<entity_id> selection;

    // Select player units
    for (uint32_t index = 0; index < state.entities.size(); index++) {
        const entity_t& entity = state.entities[index];
        if (entity.player_id != network_get_player_id() || 
            !entity_is_unit(entity.type) || 
            !entity_is_selectable(entity)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select player buildings
    for (uint32_t index = 0; index < state.entities.size(); index++) {
        const entity_t& entity = state.entities[index];
        if (entity.player_id != network_get_player_id() || 
            !entity_is_building(entity.type) || 
            !entity_is_selectable(entity)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select enemy unit
    for (uint32_t index = 0; index < state.entities.size(); index++) {
        const entity_t& entity = state.entities[index];
        if (entity.player_id == network_get_player_id() || 
            !entity_is_unit(entity.type) || 
            !entity_is_selectable(entity) || 
            !map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.entities.get_id_of(index));
            return selection;
        }
    }

    // Select enemy buildings
    for (uint32_t index = 0; index < state.entities.size(); index++) {
        const entity_t& entity = state.entities[index];
        if (entity.player_id == network_get_player_id() || 
            !entity_is_building(entity.type) || 
            !entity_is_selectable(entity) || 
            !map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.entities.get_id_of(index));
            return selection;
        }
    }

    for (uint32_t index = 0; index < state.entities.size(); index++) {
        const entity_t& entity = state.entities[index];
        if (entity.type != ENTITY_GOLD || !map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.entities.get_id_of(index));
            return selection;
        }
    }

    return selection;
}

void ui_set_selection(match_state_t& state, const std::vector<entity_id>& selection) {
    state.selection = selection;

    for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
        uint32_t entity_index = state.entities.get_index_of(state.selection[selection_index]);
        if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
            state.selection.erase(state.selection.begin() + selection_index);
            selection_index--;
            continue;
        }
    }

    while (state.selection.size() > SELECTION_LIMIT) {
        state.selection.pop_back();
    }

    state.ui_mode = UI_MODE_NONE;
}

void ui_update_buttons(match_state_t& state) {
    for (int i = 0; i < 6; i++) {
        state.ui_buttons[i] = UI_BUTTON_NONE;
    }

    if (ui_is_targeting(state) || state.ui_mode == UI_MODE_BUILDING_PLACE) {
        state.ui_buttons[5] = UI_BUTTON_CANCEL;
        return;
    }
    if (state.ui_mode == UI_MODE_BUILD) {
        state.ui_buttons[0] = UI_BUTTON_BUILD_HALL;
        state.ui_buttons[1] = UI_BUTTON_BUILD_HOUSE;
        state.ui_buttons[2] = UI_BUTTON_BUILD_CAMP;
        state.ui_buttons[3] = UI_BUTTON_BUILD_SALOON;
        state.ui_buttons[4] = UI_BUTTON_BUILD_BUNKER;
        state.ui_buttons[5] = UI_BUTTON_CANCEL;
        return;
    }

    if (state.selection.empty()) {
        return;
    }

    const entity_t entity = state.entities.get_by_id(state.selection[0]);
    // This covers enemy selection and mine selection
    if (entity.player_id != network_get_player_id()) {
        return;
    }

    if (state.selection.size() == 1 && entity.mode == MODE_BUILDING_IN_PROGRESS) {
        state.ui_buttons[5] = UI_BUTTON_CANCEL;
        return;
    }

    if (entity_is_unit(entity.type)) {
        state.ui_buttons[0] = UI_BUTTON_ATTACK;
        state.ui_buttons[1] = UI_BUTTON_STOP;
        state.ui_buttons[2] = UI_BUTTON_DEFEND;
    }

    // This block returns if selected entities are not all the same type
    uint32_t garrison_count = entity.garrisoned_units.size();
    for (uint32_t id_index = 1; id_index < state.selection.size(); id_index++) {
        const entity_t& other = state.entities.get_by_id(state.selection[id_index]);
        garrison_count += other.garrisoned_units.size();
        if (other.type != entity.type || (!entity_is_unit(entity.type) && other.mode != entity.mode)) {
            return;
        }
    }

    if (garrison_count != 0) {
        state.ui_buttons[3] = UI_BUTTON_UNLOAD;
    }

    if (!entity.queue.empty()) {
        state.ui_buttons[5] = UI_BUTTON_CANCEL;
    }

    switch (entity.type) {
        case ENTITY_MINER: {
            state.ui_buttons[3] = UI_BUTTON_REPAIR;
            state.ui_buttons[4] = UI_BUTTON_BUILD;
            break;
        }
        case ENTITY_HALL: {
            state.ui_buttons[0] = UI_BUTTON_UNIT_MINER;
            break;
        }
        case ENTITY_SALOON: {
            state.ui_buttons[0] = UI_BUTTON_UNIT_COWBOY;
            state.ui_buttons[1] = UI_BUTTON_UNIT_BANDIT;
            break;
        }
        default:
            break;
    }
}

SelectionType ui_get_selection_type(const match_state_t& state, const std::vector<entity_id>& selection) {
    if (selection.empty()) {
        return SELECTION_TYPE_NONE;
    }

    const entity_t& entity = state.entities.get_by_id(state.selection[0]);
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return SELECTION_TYPE_UNITS;
        } else {
            return SELECTION_TYPE_ENEMY_UNIT;
        }
    } else if (entity_is_building(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return SELECTION_TYPE_BUILDINGS;
        } else {
            return SELECTION_TYPE_ENEMY_BUILDING;
        }
    } else {
        return SELECTION_TYPE_GOLD;
    }
}

bool ui_is_targeting(const match_state_t& state) {
    return state.ui_mode >= UI_MODE_TARGET_ATTACK && state.ui_mode < UI_MODE_CHAT;
}

void ui_add_chat_message(match_state_t& state, std::string message) {
    state.ui_chat.push_back((chat_message_t) {
        .message = message,
        .timer = UI_CHAT_MESSAGE_DURATION
    });
}

int ui_get_ui_button_hovered(const match_state_t& state) {
    if (state.ui_button_pressed != -1) {
        return state.ui_button_pressed;
    }
    if (ui_is_selecting(state) || state.ui_is_minimap_dragging || state.ui_mode >= UI_MODE_CHAT) {
        return -1;
    } 

    for (int i = 0; i < 6; i++) {
        if (state.ui_buttons[i] == UI_BUTTON_NONE) {
            continue;
        }

        if (sdl_rect_has_point(UI_BUTTON_RECT[i], engine.mouse_position)) {
            return i;
        }
    }

    return -1;
}

bool ui_button_requirements_met(const match_state_t& state, UiButton button) {
    auto ui_button_requirements_it = UI_BUTTON_REQUIREMENTS.find(button);

    // Buttons with no defined requirements are enabled by default
    if (ui_button_requirements_it == UI_BUTTON_REQUIREMENTS.end()) {
        return true;
    }

    switch (ui_button_requirements_it->second.type) {
        case UI_BUTTON_REQUIRES_BUILDING: {
            for (const entity_t& building : state.entities) {
                if (building.player_id == network_get_player_id() && building.mode == MODE_BUILDING_FINISHED && building.type == ui_button_requirements_it->second.building_type) {
                    return true;
                }
            }
            return false;
        }
    }
}

void ui_handle_ui_button_press(match_state_t& state, UiButton button) {
    if (!ui_button_requirements_met(state, button)) {
        return;
    }

    switch (button) {
        case UI_BUTTON_ATTACK: {
            state.ui_mode = UI_MODE_TARGET_ATTACK;
            break;
        }
        case UI_BUTTON_REPAIR: {
            state.ui_mode = UI_MODE_TARGET_REPAIR;
            break;
        }
        case UI_BUTTON_UNLOAD: {
            if (ui_get_selection_type(state, state.selection) == SELECTION_TYPE_UNITS) {
                state.ui_mode = UI_MODE_TARGET_UNLOAD;
            } else {
                input_t input;
                input.type = INPUT_UNLOAD;
                input.unload.entity_count = state.selection.size();
                for (int selection_index = 0; selection_index < state.selection.size(); selection_index++) {
                    input.unload.entity_ids[selection_index] = state.selection[selection_index];
                }
                state.input_queue.push_back(input);
            }
            break;
        }
        case UI_BUTTON_STOP:
        case UI_BUTTON_DEFEND: {
            input_t input;
            input.type = button == UI_BUTTON_STOP ? INPUT_STOP : INPUT_DEFEND;
            input.stop.entity_count = (uint8_t)state.selection.size();
            memcpy(&input.stop.entity_ids, &state.selection[0], input.stop.entity_count * sizeof(entity_id));
            state.input_queue.push_back(input);
            break;
        }
        case UI_BUTTON_BUILD: {
            state.ui_mode = UI_MODE_BUILD;
            break;
        }
        case UI_BUTTON_CANCEL: {
            if (state.ui_mode == UI_MODE_BUILDING_PLACE) {
                state.ui_mode = UI_MODE_BUILD;
            } else if (state.selection.size() == 1 && state.entities.get_by_id(state.selection[0]).mode == MODE_BUILDING_IN_PROGRESS) {
                state.input_queue.push_back((input_t) {
                    .type = INPUT_BUILD_CANCEL,
                    .build_cancel = (input_build_cancel_t) {
                        .building_id = state.selection[0]
                    }
                });
            } else if (state.selection.size() == 1 && !state.entities.get_by_id(state.selection[0]).queue.empty()) {
                state.input_queue.push_back((input_t) {
                    .type = INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (input_building_dequeue_t) {
                        .building_id = state.selection[0],
                        .index = BUILDING_DEQUEUE_POP_FRONT
                    }
                });
            } else if (state.ui_mode == UI_MODE_BUILD) {
                state.ui_mode = UI_MODE_NONE;
            } else if (state.ui_mode == UI_MODE_TARGET_REPAIR || state.ui_mode == UI_MODE_TARGET_ATTACK || state.ui_mode == UI_MODE_TARGET_UNLOAD) {
                state.ui_mode = UI_MODE_NONE;
            } 
        }
        default:
            break;
    }

    for (auto it : ENTITY_DATA) {
        if (it.second.ui_button == button) {
            EntityType entity_type = it.first;
            // Enqueue unit into building
            if (entity_is_unit(entity_type)) {
                 // Decide which building to enqueue
                uint32_t selected_building_index = state.entities.get_index_of(state.selection[0]);
                for (int id_index = 1; id_index < state.selection.size(); id_index++) {
                    if (state.entities.get_by_id(state.selection[id_index]).queue.size() < state.entities[selected_building_index].queue.size()) {
                        selected_building_index = state.entities.get_index_of(state.selection[id_index]);
                    }
                }
                entity_t& building = state.entities[selected_building_index];

                building_queue_item_t item = (building_queue_item_t) {
                    .type = BUILDING_QUEUE_ITEM_UNIT,
                    .unit_type = entity_type
                };

                if (building.queue.size() == BUILDING_QUEUE_MAX) {
                    ui_show_status(state, UI_STATUS_BUILDING_QUEUE_FULL);
                } else if (state.player_gold[network_get_player_id()] < building_queue_item_cost(item)) {
                    ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                } else {
                    input_t input = (input_t) {
                        .type = INPUT_BUILDING_ENQUEUE,
                        .building_enqueue = (input_building_enqueue_t) {
                            .building_id = state.entities.get_id_of(selected_building_index),
                            .item = item
                        }
                    };
                    state.input_queue.push_back(input);
                }
            } else {
                if (state.player_gold[network_get_player_id()] < it.second.gold_cost) {
                    ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                    return;
                }

                state.ui_mode = UI_MODE_BUILDING_PLACE;
                state.ui_building_type = entity_type;
            }

            return;
        }
    }
}

void ui_deselect_entity_if_selected(match_state_t& state, entity_id id) {
    for (uint32_t id_index = 0; id_index < state.selection.size(); id_index++) {
        if (state.selection[id_index] == id) {
            state.selection.erase(state.selection.begin() + id_index);
            return;
        }
    }
}

void ui_show_status(match_state_t& state, const char* message) {
    state.ui_status_message = std::string(message);
    state.ui_status_timer = UI_STATUS_DURATION;
}

xy ui_get_building_cell(const match_state_t& state) {
    int building_cell_size = entity_cell_size(state.ui_building_type);
    xy offset = xy(building_cell_size, building_cell_size) - xy(2, 2);
    xy building_cell = ((engine.mouse_position + state.camera_offset) / TILE_SIZE) - offset;
    building_cell.x = std::max(0, building_cell.x);
    building_cell.y = std::max(0, building_cell.y);
    return building_cell;
}

entity_id ui_get_nearest_builder(const match_state_t& state, const std::vector<entity_id>& builders, xy cell) {
    entity_id nearest_unit_id; 
    int nearest_unit_dist = -1;
    for (entity_id id : builders) {
        int selection_dist = xy::manhattan_distance(cell, state.entities.get_by_id(id).cell);
        if (nearest_unit_dist == -1 || selection_dist < nearest_unit_dist) {
            nearest_unit_id = id;
            nearest_unit_dist = selection_dist;
        }
    }

    return nearest_unit_id;
}

bool ui_building_can_be_placed(const match_state_t& state) {
    xy building_cell = ui_get_building_cell(state);
    int building_cell_size = ENTITY_DATA.at(state.ui_building_type).cell_size;
    xy miner_cell = state.entities.get_by_id(ui_get_nearest_builder(state, state.selection, building_cell)).cell;

    if (!map_is_cell_rect_in_bounds(state, building_cell, building_cell_size)) {
        return false;
    }

    SDL_Rect building_rect = (SDL_Rect) { .x = building_cell.x, .y = building_cell.y, .w = building_cell_size, .h = building_cell_size };
    for (const entity_t& gold : state.entities) {
        if (gold.type != ENTITY_GOLD || gold.gold_held == 0) {
            continue;
        }
        SDL_Rect gold_block_rect = entity_gold_get_block_building_rect(gold.cell);
        if ((state.ui_building_type == ENTITY_CAMP || state.ui_building_type == ENTITY_HALL) && SDL_HasIntersection(&building_rect, &gold_block_rect) == SDL_TRUE) {
            return false;
        }
    }

    for (int x = building_cell.x; x < building_cell.x + building_cell_size; x++) {
        for (int y = building_cell.y; y < building_cell.y + building_cell_size; y++) {
            if ((xy(x, y) != miner_cell && state.map_cells[x + (y * state.map_width)] != CELL_EMPTY) || 
                    state.map_fog[network_get_player_id()][x + (y * state.map_width)] == FOG_HIDDEN || map_is_tile_ramp(state, xy(x, y))) {
                return false;
            }
        }
    }

    return true;
}

ui_tooltip_info_t ui_get_hovered_tooltip_info(const match_state_t& state) {
    ui_tooltip_info_t info;
    memset(&info, 0, sizeof(info));
    char* info_text_ptr = info.text;

    UiButton button = state.ui_buttons[ui_get_ui_button_hovered(state)];
    if (!ui_button_requirements_met(state, button)) {
        auto button_requirements_it = UI_BUTTON_REQUIREMENTS.find(button);
        info_text_ptr += sprintf(info_text_ptr, "Requires ");
        switch (button_requirements_it->second.type) {
            case UI_BUTTON_REQUIRES_BUILDING: {
                info_text_ptr += sprintf(info_text_ptr, "%s", ENTITY_DATA.at(button_requirements_it->second.building_type).name);
                break;
            }
        }

        return info;
    }

    switch (button) {
        case UI_BUTTON_ATTACK:
            info_text_ptr += sprintf(info_text_ptr, "Attack");
            break;
        case UI_BUTTON_STOP:
            info_text_ptr += sprintf(info_text_ptr, "Stop");
            break;
        case UI_BUTTON_BUILD:
            info_text_ptr += sprintf(info_text_ptr, "Build");
            break;
        case UI_BUTTON_REPAIR:
            info_text_ptr += sprintf(info_text_ptr, "Repair");
            break;
        case UI_BUTTON_DEFEND:
            info_text_ptr += sprintf(info_text_ptr, "Defend");
            break;
        case UI_BUTTON_UNLOAD:
            info_text_ptr += sprintf(info_text_ptr, "Unload");
            break;
        case UI_BUTTON_CANCEL:
            info_text_ptr += sprintf(info_text_ptr, "Cancel");
            break;
        default: {
            for (auto entity_data_it : ENTITY_DATA) {
                if (entity_data_it.second.ui_button == button) {
                    info_text_ptr += sprintf(info_text_ptr, "%s %s", entity_is_unit(entity_data_it.first) ? "Hire" : "Build", entity_data_it.second.name);
                    info.gold_cost = entity_data_it.second.gold_cost;
                    if (entity_is_unit(entity_data_it.first)) {
                        info.population_cost = entity_data_it.second.unit_data.population_cost;
                    }
                    break; // breaks for entity_data_it
                }
            }
            break;
        }
    }

    if (ui_button_requirements_met(state, button)) {
        info_text_ptr += sprintf(info_text_ptr, " (");
        SDL_Keycode hotkey = hotkeys.at(button);
        if (hotkey == SDLK_ESCAPE) {
            info_text_ptr += sprintf(info_text_ptr, "ESC");
        } else if (hotkey >= SDLK_a && hotkey <= SDLK_z) {
            info_text_ptr += sprintf(info_text_ptr, "%c", (char)(hotkey - 32));
        } else {
            log_error("Unhandled hotkey %u in ui_get_hovered_tooltip_info()", hotkey);
        }
        info_text_ptr += sprintf(info_text_ptr, ")");
    }

    return info;
}

xy ui_garrisoned_icon_position(int index) {
    return UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(index * 34, 0);
}

int ui_get_garrisoned_index_hovered(const match_state_t& state) {
    SelectionType selection_type = ui_get_selection_type(state, state.selection);
    if (!(selection_type == SELECTION_TYPE_UNITS || selection_type == SELECTION_TYPE_BUILDINGS) ||
            state.selection.size() != 1 || ui_is_selecting(state) || state.ui_is_minimap_dragging || 
            !(state.ui_mode == UI_MODE_NONE || state.ui_mode == UI_MODE_BUILD)) {
        return -1;
    } 

    const entity_t& entity = state.entities.get_by_id(state.selection[0]);
    for (int index = 0; index < entity.garrisoned_units.size(); index++) {
        SDL_Rect icon_rect = (SDL_Rect) { .x = ui_garrisoned_icon_position(index).x, .y = ui_garrisoned_icon_position(index).y, .w = 32, .h = 32 };
        if (sdl_rect_has_point(icon_rect, engine.mouse_position)) {
            return index;
        }
    }

    return -1;
}

xy ui_get_selected_unit_icon_position(uint32_t selection_index) {
    return SELECTION_LIST_TOP_LEFT + xy(((selection_index % 10) * 34) - 12, (selection_index / 10) * 34);
}

int ui_get_selected_unit_hovered(const match_state_t& state) {
    if (state.selection.size() < 2 || ui_is_selecting(state) || state.ui_is_minimap_dragging || !(state.ui_mode == UI_MODE_NONE || state.ui_mode == UI_MODE_BUILD)) {
        return -1;
    } 

    for (int index = 0; index < state.selection.size(); index++) {
        xy icon_position = ui_get_selected_unit_icon_position(index);
        SDL_Rect icon_rect = (SDL_Rect) { .x = icon_position.x, .y = icon_position.y, .w = 32, .h = 32 };
        if (sdl_rect_has_point(icon_rect, engine.mouse_position)) {
            return index;
        }
    }

    return -1;
}

int ui_get_building_queue_item_hovered(const match_state_t& state) {
    if (state.selection.size() != 1 || ui_get_selection_type(state, state.selection) != SELECTION_TYPE_BUILDINGS || 
                ui_is_selecting(state) || state.ui_is_minimap_dragging || !(state.ui_mode == UI_MODE_NONE)) {
        return -1;
    } 

    for (int index = 0; index < state.entities.get_by_id(state.selection[0]).queue.size(); index++) {
        SDL_Rect icon_rect = (SDL_Rect) {
            .x = UI_BUILDING_QUEUE_POSITIONS[index].x,
            .y = UI_BUILDING_QUEUE_POSITIONS[index].y,
            .w = 32,
            .h = 32
        };

        if (sdl_rect_has_point(icon_rect, engine.mouse_position)) {
            return index;
        }
    }

    return -1;
}

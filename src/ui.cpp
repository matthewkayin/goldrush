#include "match.h"

#include "network.h"
#include "logger.h"

static const uint32_t UI_CHAT_MESSAGE_DURATION = 180;
static const uint32_t UI_STATUS_DURATION = 60;

static const std::unordered_map<UiButtonset, std::array<UiButton, UI_BUTTONSET_SIZE>> UI_BUTTONS = {
    { UI_BUTTONSET_NONE, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_UNIT, { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_MINER, { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                      UI_BUTTON_REPAIR, UI_BUTTON_BUILD, UI_BUTTON_NONE }},
    { UI_BUTTONSET_BUILD, { UI_BUTTON_BUILD_CAMP, UI_BUTTON_BUILD_HOUSE, UI_BUTTON_BUILD_SALOON,
                      UI_BUTTON_BUILD_BUNKER, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_CANCEL, { UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE,
                      UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_CANCEL }},
    { UI_BUTTONSET_CAMP, { UI_BUTTON_UNIT_MINER, UI_BUTTON_NONE, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_SALOON, { UI_BUTTON_UNIT_COWBOY, UI_BUTTON_UNIT_BANDIT, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_WAGON, { UI_BUTTON_ATTACK, UI_BUTTON_STOP, UI_BUTTON_DEFEND,
                             UI_BUTTON_UNLOAD, UI_BUTTON_NONE, UI_BUTTON_NONE }},
    { UI_BUTTONSET_BUNKER, { UI_BUTTON_UNLOAD, UI_BUTTON_NONE, UI_BUTTON_NONE,
                             UI_BUTTON_NONE, UI_BUTTON_NONE, UI_BUTTON_NONE }}
};

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

bool ui_is_mouse_in_ui() {
    return (engine.mouse_position.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
           (engine.mouse_position.x <= 136 && engine.mouse_position.y >= SCREEN_HEIGHT - 136) ||
           (engine.mouse_position.x >= SCREEN_WIDTH - 132 && engine.mouse_position.y >= SCREEN_HEIGHT - 106);
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
            !entity_is_selectable(entity)) {
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
            !entity_is_selectable(entity)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.entities.get_id_of(index));
            return selection;
        }
    }


    // TODO
    // Select mines
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

    if (state.selection.empty()) {
        state.ui_buttonset = UI_BUTTONSET_NONE;
        return;
    }

    const entity_t entity = state.entities.get_by_id(state.selection[0]);
    // This covers enemy selection and mine selection
    if (entity.player_id != network_get_player_id()) {
        state.ui_buttonset = UI_BUTTONSET_NONE;
        return;
    }

    if (state.selection.size() == 1 && entity.mode == MODE_BUILDING_IN_PROGRESS) {
        state.ui_buttonset = UI_BUTTONSET_CANCEL;
    }

    EntityType selected_entity_type = entity.type;
    bool selected_entities_are_all_the_same_type = true;
    for (uint32_t id_index = 1; id_index < state.selection.size(); id_index++) {
        if (state.entities.get_by_id(state.selection[id_index]).type != selected_entity_type) {
            selected_entities_are_all_the_same_type = false;
        }
    }

    if (selected_entities_are_all_the_same_type) {
        switch (selected_entity_type) {
            case UNIT_MINER:
                state.ui_buttonset = UI_BUTTONSET_MINER;
                return;
            case BUILDING_CAMP:
                state.ui_buttonset = UI_BUTTONSET_CAMP;
                return;
            default:
                break;
        }
    } 

    if (entity_is_unit(entity.type)) {
        state.ui_buttonset = UI_BUTTONSET_UNIT;
    } else {
        state.ui_buttonset = UI_BUTTONSET_NONE;
    }
}

SelectionType ui_get_selection_type(const match_state_t& state) {
    if (state.selection.empty()) {
        return SELECTION_TYPE_NONE;
    }

    const entity_t& entity = state.entities.get_by_id(state.selection[0]);
    // TODO check gold mine type
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return SELECTION_TYPE_UNITS;
        } else {
            return SELECTION_TYPE_ENEMY_UNIT;
        }
    } else {
        if (entity.player_id == network_get_player_id()) {
            return SELECTION_TYPE_BUILDINGS;
        } else {
            return SELECTION_TYPE_ENEMY_BUILDING;
        }
    }

    return SELECTION_TYPE_NONE;
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

    for (int i = 0; i < 6; i++) {
        if (ui_get_ui_button(state, i) == UI_BUTTON_NONE) {
            continue;
        }

        if (sdl_rect_has_point(UI_BUTTON_RECT[i], engine.mouse_position)) {
            return i;
        }
    }

    return -1;
}

UiButton ui_get_ui_button(const match_state_t& state, int index) {
    if (index == 5 && state.selection.size() == 1 && state.entities.get_by_id(state.selection[0]).queue.size() > 0) {
        return UI_BUTTON_CANCEL;
    }

    return UI_BUTTONS.at(state.ui_buttonset)[index];
}

bool ui_button_requirements_met(const match_state_t& state, UiButton button) {
    return true;
    /*
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
    */
}

void ui_handle_ui_button_press(match_state_t& state, UiButton button) {
    switch (button) {
        case UI_BUTTON_ATTACK: {
            state.ui_mode = UI_MODE_TARGET_ATTACK;
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
            break;
        }
        case UI_BUTTON_REPAIR: {
            state.ui_mode = UI_MODE_TARGET_REPAIR;
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
            break;
        }
        case UI_BUTTON_UNLOAD: {
            state.ui_mode = UI_MODE_TARGET_UNLOAD;
            state.ui_buttonset = UI_BUTTONSET_CANCEL;
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
            state.ui_buttonset = UI_BUTTONSET_BUILD;
            break;
        }
        case UI_BUTTON_CANCEL: {
            if (state.selection.size() == 1 && !state.entities.get_by_id(state.selection[0]).queue.empty()) {
                state.input_queue.push_back((input_t) {
                    .type = INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (input_building_dequeue_t) {
                        .building_id = state.selection[0],
                        .index = BUILDING_DEQUEUE_POP_FRONT
                    }
                });
            } else if (state.ui_buttonset == UI_BUTTONSET_BUILD) {
                state.ui_buttonset = UI_BUTTONSET_MINER;
            } else if (state.ui_mode == UI_MODE_TARGET_REPAIR || state.ui_mode == UI_MODE_TARGET_ATTACK || state.ui_mode == UI_MODE_TARGET_UNLOAD) {
                state.ui_mode = UI_MODE_NONE;
                ui_set_selection(state, state.selection);
            } else if (state.ui_mode == UI_MODE_BUILDING_PLACE) {
                state.ui_mode = UI_MODE_NONE;
                state.ui_buttonset = UI_BUTTONSET_BUILD;
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
                // } else if (state.player_gold[network_get_player_id()] < building_queue_item_cost(item)) {
                    // ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
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
            } // End if entity is unit

            return;
        }
    }
}

void ui_deselect_entity_if_selected(match_state_t& state, entity_id id) {
    for (uint32_t id_index = 0; id_index < state.selection.size(); id_index++) {
        if (state.selection[id_index] == id) {
            state.selection.erase(state.selection.begin() + id_index);
            if (state.ui_mode == UI_MODE_NONE) {
                ui_set_selection(state, state.selection);
            }
            return;
        }
    }
}

void ui_show_status(match_state_t& state, const char* message) {
    state.ui_status_message = std::string(message);
    state.ui_status_timer = UI_STATUS_DURATION;
}
#include "match.h"

#include "network.h"

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

    // TODO 
    // Select player buildings

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

    // TODO 
    // Select enemy building

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

    // TODO set buttonset based on selection
}

SelectionType ui_get_selection_type(const match_state_t& state) {
    if (state.selection.empty()) {
        return SELECTION_TYPE_NONE;
    }

    if (entity_is_unit(state.entities.get_by_id(state.selection[0]).type)) {
        return SELECTION_TYPE_UNITS;
    }

    // TODO
    return SELECTION_TYPE_NONE;
}

bool ui_is_targeting(const match_state_t& state) {
    return state.ui_mode >= UI_MODE_TARGET_ATTACK && state.ui_mode < UI_MODE_CHAT;
}
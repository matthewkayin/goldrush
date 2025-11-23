#include "replay.h"

ReplayShell::ReplayShell() {

}

bool ReplayShell::is_entity_visible(const Entity& entity) const {
    if (replay_fox_index == REPLAY_FOG_NONE) {
        return true;
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if ((replay_fog_player_ids[replay_fog_index] == PLAYER_NONE ||
                replay_fog_player_ids[replay_fog_index] == player_id) &&
                entity_is_visible_to_player(match_state, entity, player_id)) {
            return true;
        }
    }

    return false;
}

void ReplayShell::handle_match_event(const MatchEvent& event) {
    return;
}

std::vector<EntityId> ReplayShell::create_selection(Rect select_rect) const {
    std::vector<EntityId> selection;

    // Select units
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (!is_entity_visible(entity) ||
                !entity_is_unit(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select buildings
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (!is_entity_visible(entity) ||
                !entity_is_building(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select gold mines
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (entity.type != ENTITY_GOLDMINE ||
                !is_entity_visible(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(match_state.entities.get_id_of(index));
            return selection;
        }
    }

    // Returns an empty selection
    return selection;
}

void ReplayShell::on_selection() {}
#include "match.h"

#include "match/upgrade.h"
#include "network/network.h"

// Alert
static const uint32_t ALERT_DURATION = 90;
static const uint32_t ALERT_LINGER_DURATION = 60 * 20;
static const uint32_t ALERT_TOTAL_DURATION = ALERT_DURATION + ALERT_LINGER_DURATION;
static const int ATTACK_ALERT_DISTANCE = 20;

MatchShell::MatchShell() {
    #ifdef GOLD_DEBUG
        debug_fog = DEBUG_FOG_ENABLED;
    #endif
}

bool MatchShell::is_entity_visible(const Entity& entity) const {
    #ifdef GOLD_DEBUG
        if (debug_fog == DEBUG_FOG_BOT_VISION) {
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                        entity_is_visible_to_player(match_state, entity, player_id)) {
                    return true;
                }
            }
        } else if (debug_fog == DEBUG_FOG_DISABLED) {
            return entity.garrison_id == ID_NULL;
        }
    #endif
    return entity_is_visible_to_player(match_state, entity, network_get_player_id());
}

bool MatchShell::is_cell_rect_revealed(ivec2 cell, int cell_size) const {
    #ifdef GOLD_DEBUG
        if (debug_fog == DEBUG_FOG_BOT_VISION) {
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                        match_is_cell_rect_revealed(match_state, match_state.players[player_id].team, cell, cell_size)) {
                    return true;
                }
            }
        } else if (debug_fog == DEBUG_FOG_DISABLED) {
            return true;
        }
    #endif
    return match_is_cell_rect_revealed(match_state, match_state.players[network_get_player_id()].team, cell, cell_size);
}

void MatchShell::handle_match_event(const MatchEvent& event) {
    if (!match_state.players[network_get_player_id()].active) {
        return;
    }

    switch (event.type) {
         case MATCH_EVENT_ALERT: {
            if ((event.alert.type == MATCH_ALERT_TYPE_ATTACK && match_state.players[event.alert.player_id].team != match_state.players[network_get_player_id()].team) ||
                (event.alert.type != MATCH_ALERT_TYPE_ATTACK && event.alert.player_id != network_get_player_id())) {
                return;
            }

            // Check if an existing attack alert already exists nearby
            if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                bool is_existing_attack_alert_nearby = false;
                for (const Alert& existing_alert : alerts) {
                    if (existing_alert.pixel == MINIMAP_PIXEL_WHITE && ivec2::manhattan_distance(existing_alert.cell, event.alert.cell) < ATTACK_ALERT_DISTANCE) {
                        return;
                    }
                }
            }

            // Play the sound even if we don't show the alert
            switch (event.alert.type) {
                case MATCH_ALERT_TYPE_BUILDING:
                    sound_play(SOUND_ALERT_BUILDING);
                    break;
                case MATCH_ALERT_TYPE_UNIT:
                    sound_play(SOUND_ALERT_UNIT);
                    break;
                case MATCH_ALERT_TYPE_RESEARCH:
                    sound_play(SOUND_ALERT_RESEARCH);
                    break;
                case MATCH_ALERT_TYPE_MINE_COLLAPSE:
                    sound_play(SOUND_GOLD_MINE_COLLAPSE);
                    break;
                default:
                    break;
            }
            latest_alert_cell = event.alert.cell;

            Rect camera_rect = (Rect) { 
                .x = camera_offset.x, 
                .y = camera_offset.y, 
                .w = SCREEN_WIDTH, 
                .h = SCREEN_HEIGHT 
            };
            Rect alert_rect = (Rect) { 
                .x = event.alert.cell.x * TILE_SIZE, 
                .y = event.alert.cell.y * TILE_SIZE, 
                .w = event.alert.cell_size * TILE_SIZE, 
                .h = event.alert.cell_size * TILE_SIZE 
            };
            if (camera_rect.intersects(alert_rect)) {
                break;
            }

            MinimapPixel pixel;
            if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                pixel = MINIMAP_PIXEL_WHITE;
            } else if (event.alert.type == MATCH_ALERT_TYPE_MINE_COLLAPSE) {
                pixel = MINIMAP_PIXEL_GOLD;
            } else {
                pixel = (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + match_state.players[network_get_player_id()].recolor_id);
            }

            alerts.push_back((Alert) {
                .pixel = pixel,
                .cell = event.alert.cell,
                .cell_size = event.alert.cell_size,
                .timer = ALERT_TOTAL_DURATION
            });

            if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                show_status(event.alert.player_id == network_get_player_id() 
                                                ? MATCH_UI_STATUS_UNDER_ATTACK 
                                                : MATCH_UI_STATUS_ALLY_UNDER_ATTACK);
                sound_play(SOUND_ALERT_BELL);
            } 
            break;
        }
        case MATCH_EVENT_SELECTION_HANDOFF: {
            if (event.selection_handoff.player_id != network_get_player_id()) {
                break;
            }

            if (selection.size() == 1 && selection[0] == event.selection_handoff.to_deselect) {
                if (is_in_menu()) {
                    selection.clear();
                } else {
                    std::vector<EntityId> new_selection;
                    new_selection.push_back(event.selection_handoff.to_select);
                    set_selection(new_selection);
                }
            }
            break;
        }
        case MATCH_EVENT_STATUS: {
            if (network_get_player_id() == event.status.player_id) {
                show_status(event.status.message);
            }
            break;
        }
        case MATCH_EVENT_RESEARCH_COMPLETE: {
            if (event.research_complete.player_id != network_get_player_id()) {
                break;
            }

            char message[128];
            sprintf(message, "%s research complete.", upgrade_get_data(event.research_complete.upgrade).name);
            show_status(message);
            break;
        }
    }
}

std::vector<EntityId> MatchShell::create_selection(Rect select_rect) const {
    std::vector<EntityId> selection;

    // Select player units
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (entity.player_id != network_get_player_id() ||
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

    // Select player buildings
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (entity.player_id != network_get_player_id() ||
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

    // Select enemy units
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (entity.player_id == network_get_player_id() ||
                !entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                !is_entity_visible(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(match_state.entities.get_id_of(index));
            return selection;
        }
    }

    // Select enemy buildings
    for (uint32_t index = 0; index < match_state.entities.size(); index++) {
        const Entity& entity = match_state.entities[index];
        if (entity.player_id == network_get_player_id() ||
                entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                !is_entity_visible(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(match_state.entities.get_id_of(index));
            return selection;
        }
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

void MatchShell::leave_match(bool exit_program) {
    network_disconnect();
    replay_file_close(replay_file);
    mode = exit_program ? SHELL_MODE_EXIT_PROGRAM : SHELL_MODE_LEAVE_MATCH;
}
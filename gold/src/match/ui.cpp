#include "match/ui.h"

#include "core/network.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/cursor.h"
#include "menu/ui.h"
#include "menu/match_setting.h"
#include "render/sprite.h"
#include "render/render.h"
#include "hotkey.h"
#include <algorithm>

static const uint32_t TURN_OFFSET = 2;
static const uint32_t TURN_DURATION = 4;

static const uint32_t UI_STATUS_DURATION = 60;

static const int MATCH_CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_SPEED = 16; // TODO: move to options
static const Rect SCREEN_RECT = (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
static const Rect MINIMAP_RECT = (Rect) { .x = 4, .y = SCREEN_HEIGHT - 132, .w = 128, .h = 128 };
static const int SOUND_LISTEN_MARGIN = 128;
static const uint32_t SOUND_COOLDOWN_DURATION = 5;
static const uint32_t UI_ALERT_DURATION = 90;
static const uint32_t UI_ALERT_LINGER_DURATION = 60 * 20;
static const uint32_t UI_ALERT_TOTAL_DURATION = UI_ALERT_DURATION + UI_ALERT_LINGER_DURATION;
static const uint32_t UI_ATTACK_ALERT_DISTANCE = 20;
static const uint32_t MATCH_UI_DOUBLE_CLICK_DURATION = 16;

static const int MATCH_UI_BUTTON_X = SCREEN_WIDTH - 132 + 14; 
static const int MATCH_UI_BUTTON_Y = SCREEN_HEIGHT - MATCH_UI_HEIGHT + 10;
static const int MATCH_UI_BUTTON_PADDING_X = 32 + 4;
static const int MATCH_UI_BUTTON_PADDING_Y = 32 + 6;
static const ivec2 MATCH_UI_BUTTON_POSITIONS[HOTKEY_GROUP_SIZE] = {
    ivec2(MATCH_UI_BUTTON_X, MATCH_UI_BUTTON_Y),
    ivec2(MATCH_UI_BUTTON_X + MATCH_UI_BUTTON_PADDING_X, MATCH_UI_BUTTON_Y),
    ivec2(MATCH_UI_BUTTON_X + (2 * MATCH_UI_BUTTON_PADDING_X), MATCH_UI_BUTTON_Y),
    ivec2(MATCH_UI_BUTTON_X, MATCH_UI_BUTTON_Y + MATCH_UI_BUTTON_PADDING_Y),
    ivec2(MATCH_UI_BUTTON_X + MATCH_UI_BUTTON_PADDING_X, MATCH_UI_BUTTON_Y + MATCH_UI_BUTTON_PADDING_Y),
    ivec2(MATCH_UI_BUTTON_X + (2 * MATCH_UI_BUTTON_PADDING_X), MATCH_UI_BUTTON_Y + MATCH_UI_BUTTON_PADDING_Y)
};
static const ivec2 SELECTION_LIST_TOP_LEFT = ivec2(164, 284);
static const ivec2 BUILDING_QUEUE_TOP_LEFT = ivec2(320, 284);
static const ivec2 UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX] = {
    BUILDING_QUEUE_TOP_LEFT,
    BUILDING_QUEUE_TOP_LEFT + ivec2(0, 35),
    BUILDING_QUEUE_TOP_LEFT + ivec2(36, 35),
    BUILDING_QUEUE_TOP_LEFT + ivec2(36 * 2, 35),
    BUILDING_QUEUE_TOP_LEFT + ivec2(36 * 3, 35)
};
static const Rect UI_BUILDING_QUEUE_PROGRESS_BAR_RECT = (Rect) {
    .x = 320 + 36,
    .y = 284 + 24,
    .w = 104, .h = 6
};

static const int HEALTHBAR_HEIGHT = 4;
static const int HEALTHBAR_PADDING = 3;

// INIT

MatchUiState match_ui_init(int32_t lcg_seed, Noise& noise) {
    MatchUiState state;

    state.mode = MATCH_UI_MODE_NOT_STARTED;
    state.camera_offset = ivec2(0, 0);
    state.select_origin = ivec2(-1, -1);
    state.is_minimap_dragging = false;
    state.turn_timer = 0;
    state.turn_counter = 0;
    state.disconnect_timer = 0;
    state.status_timer = 0;
    state.control_group_selected = MATCH_UI_CONTROL_GROUP_NONE;
    state.double_click_timer = 0;
    state.control_group_double_tap_timer = 0;
    memset(state.sound_cooldown_timers, 0, sizeof(state.sound_cooldown_timers));
    state.rally_flag_animation = animation_create(ANIMATION_RALLY_FLAG);

    // Populate match player info using network player info
    MatchPlayer players[MAX_PLAYERS];
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const NetworkPlayer& network_player = network_get_player(player_id);
        if (network_player.status == NETWORK_PLAYER_STATUS_NONE) {
            memset(&players[player_id], 0, sizeof(MatchPlayer));
            continue;
        }

        players[player_id].active = true;
        strcpy(players[player_id].name, network_player.name);
        // Use the player_id as the "team" in a FFA game to ensure everyone is on a separate team
        players[player_id].team = network_get_match_setting(MATCH_SETTING_TEAMS) == MATCH_SETTING_TEAMS_ENABLED 
                                        ? network_player.team
                                        : player_id;
        players[player_id].recolor_id = network_player.recolor_id;
    }
    state.match = match_init(lcg_seed, noise, players);

    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
            continue;
        }

        for (uint8_t i = 0; i < TURN_OFFSET - 1; i++) {
            state.inputs[player_id].push_back({ (MatchInput) { .type = MATCH_INPUT_NONE } });
        }
    }

    // Init camera
    for (const Entity& entity : state.match.entities) {
        // TODO choose the first Town Hall instead
        if (entity.type == ENTITY_MINER && entity.player_id == network_get_player_id()) {
            match_ui_center_camera_on_cell(state, entity.cell);
            break;
        }
    }

    network_set_player_ready(true);

    return state;
}

// HANDLE EVENT

void match_ui_handle_network_event(MatchUiState& state, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_INPUT: {
            // Deserialize input
            std::vector<MatchInput> inputs;

            const uint8_t* in_buffer = event.input.in_buffer;
            size_t in_buffer_head = 1; // Advance the head by once since the first byte will contain the network message type

            while (in_buffer_head < event.input.in_buffer_length) {
                inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
            }

            state.inputs[event.input.player_id].push_back(inputs);
            break;
        }
        default:
            break;
    }
}

// UPDATE

// This function returns after it handles a single input to avoid double input happening
void match_ui_handle_input(MatchUiState& state) {
    // Hotkey click
    for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
        InputAction hotkey = input_get_hotkey(hotkey_index);
        if (hotkey == INPUT_HOTKEY_NONE) {
            continue;
        }
        if (state.is_minimap_dragging || match_ui_is_selecting(state)) {
            continue;
        }
        if (!match_does_player_meet_hotkey_requirements(state.match, network_get_player_id(), hotkey)) {
            continue;
        }

        const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
        ivec2 hotkey_position = MATCH_UI_BUTTON_POSITIONS[hotkey_index];
        Rect hotkey_rect = (Rect) {
            .x = hotkey_position.x, .y = hotkey_position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };

        if (input_is_action_just_pressed(hotkey) || (
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && hotkey_rect.has_point(input_get_mouse_position())
        )) {
            const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);

            if (hotkey_info.type == HOTKEY_BUTTON_ACTION) {
                switch (hotkey) {
                    case INPUT_HOTKEY_ATTACK: {
                        state.mode = MATCH_UI_MODE_TARGET_ATTACK;
                        break;
                    }
                    case INPUT_HOTKEY_BUILD: {
                        state.mode = MATCH_UI_MODE_BUILD;
                        break;
                    }
                    case INPUT_HOTKEY_BUILD2: {
                        state.mode = MATCH_UI_MODE_BUILD2;
                        break;
                    }
                    case INPUT_HOTKEY_CANCEL: {
                        if (state.mode == MATCH_UI_MODE_BUILD || state.mode == MATCH_UI_MODE_BUILD2 || match_ui_is_targeting(state)) {
                            state.mode = MATCH_UI_MODE_NONE;
                        } else if (state.mode == MATCH_UI_MODE_BUILDING_PLACE) {
                            switch (state.building_type) {
                                case ENTITY_HALL:
                                case ENTITY_HOUSE:
                                case ENTITY_SALOON:
                                case ENTITY_SMITH:
                                case ENTITY_BUNKER:
                                    state.mode = MATCH_UI_MODE_BUILD;
                                    break;
                                case ENTITY_BARRACKS:
                                case ENTITY_COOP:
                                case ENTITY_SHERIFFS:
                                    state.mode = MATCH_UI_MODE_BUILD2;
                                    break;
                                case ENTITY_LANDMINE:
                                    state.mode = MATCH_UI_MODE_NONE;
                                    break;
                                default:
                                    log_warn("Tried to cancel building place with unhandled type of %u", state.building_type);
                                    state.mode = MATCH_UI_MODE_NONE;
                                    break;
                            }
                        } else if (state.selection.size() == 1) { 
                            const Entity& entity = state.match.entities.get_by_id(state.selection[0]);
                            if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
                                state.input_queue.push_back((MatchInput) {
                                    .type = MATCH_INPUT_BUILD_CANCEL,
                                    .build_cancel = (MatchInputBuildCancel) {
                                        .building_id = state.selection[0]
                                    }
                                });
                            } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty()) {
                                state.input_queue.push_back((MatchInput) {
                                    .type = MATCH_INPUT_BUILDING_DEQUEUE,
                                    .building_dequeue = (MatchInputBuildingDequeue) {
                                        .building_id = state.selection[0],
                                        .index = BUILDING_DEQUEUE_POP_FRONT
                                    }
                                });
                            }
                        } 
                        break;
                    }
                    case INPUT_HOTKEY_STOP:
                    case INPUT_HOTKEY_DEFEND: {
                        MatchInput input;
                        input.type = hotkey == INPUT_HOTKEY_STOP ? MATCH_INPUT_STOP : MATCH_INPUT_DEFEND;
                        input.stop.entity_count = (uint8_t)state.selection.size();
                        memcpy(&input.stop.entity_ids, &state.selection[0], input.stop.entity_count * sizeof(EntityId));
                        state.input_queue.push_back(input);
                        break;
                    }
                    default:
                        break;
                }
            } else if (hotkey_info.type == HOTKEY_BUTTON_BUILD) {
                if (state.match.players[network_get_player_id()].gold < entity_get_data(hotkey_info.entity_type).gold_cost) {
                    match_ui_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                } else {
                    state.mode = MATCH_UI_MODE_BUILDING_PLACE;
                    state.building_type = hotkey_info.entity_type;
                }
            } else if (hotkey_info.type == HOTKEY_BUTTON_TRAIN) {
                // Decide which building to enqueue
                uint32_t selected_building_index = state.match.entities.get_index_of(state.selection[0]);
                for (uint32_t selection_index = 1; selection_index < state.selection.size(); selection_index++) {
                    if (state.match.entities.get_by_id(state.selection[selection_index]).queue.size() < state.match.entities[selected_building_index].queue.size()) {
                        selected_building_index = state.match.entities.get_index_of(state.selection[selection_index]);
                    }
                }
                const Entity& building = state.match.entities[selected_building_index];

                BuildingQueueItem item = (BuildingQueueItem) {
                    .type = BUILDING_QUEUE_ITEM_UNIT,
                    .unit_type = hotkey_info.entity_type
                };

                if (building.queue.size() == BUILDING_QUEUE_MAX) {
                    match_ui_show_status(state, MATCH_UI_STATUS_BUILDING_QUEUE_FULL);
                } else if (state.match.players[network_get_player_id()].gold < building_queue_item_cost(item)) {
                    match_ui_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                } else {
                    state.input_queue.push_back((MatchInput) {
                        .type = MATCH_INPUT_BUILDING_ENQUEUE,
                        .building_enqueue = (MatchInputBuildingEnqueue) {
                            .building_id = state.match.entities.get_id_of(selected_building_index),
                            .item = item
                        }
                    });
                }
            }

            sound_play(SOUND_UI_CLICK);
            return;
        }
    }

    // Selection list click
    if (state.selection.size() > 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
        !(state.is_minimap_dragging || match_ui_is_selecting(state))) {
        for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
            Rect icon_rect = match_ui_get_selection_list_item_rect(selection_index);
            if (icon_rect.has_point(input_get_mouse_position())) {
                if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                    state.selection.erase(state.selection.begin() + selection_index);
                } else {
                    std::vector<EntityId> selection;
                    selection.push_back(state.selection[selection_index]);
                    match_ui_set_selection(state, selection);
                }
                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Building queue click
    if (state.selection.size() == 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
        !(state.is_minimap_dragging || match_ui_is_selecting(state))) {
        const Entity& building = state.match.entities.get_by_id(state.selection[0]);
        const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
        for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
            Rect icon_rect = (Rect) {
                .x = UI_BUILDING_QUEUE_POSITIONS[building_queue_index].x,
                .y = UI_BUILDING_QUEUE_POSITIONS[building_queue_index].y,
                .w = icon_sprite_info.frame_width, .h = icon_sprite_info.frame_height
            };
            if (icon_rect.has_point(input_get_mouse_position())) {
                state.input_queue.push_back((MatchInput) {
                    .type = MATCH_INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (MatchInputBuildingDequeue) {
                        .building_id = state.selection[0],
                        .index = (uint8_t)building_queue_index
                    }
                });
                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Order movement
    {
        // Check that the left or right mouse button is being pressed
        InputAction action_required = match_ui_is_targeting(state) ? INPUT_ACTION_LEFT_CLICK : INPUT_ACTION_RIGHT_CLICK;
        bool is_action_pressed = input_is_action_just_pressed(action_required);
        // Check that a move command can be executed in the current UI state
        bool is_movement_allowed = state.mode != MATCH_UI_MODE_BUILDING_PLACE && 
                                  !state.is_minimap_dragging && 
                                  match_ui_get_selection_type(state, state.selection) 
                                        == MATCH_UI_SELECTION_UNITS;
        // Check that the mouse is either in the world or in the minimap
        bool is_mouse_in_position = !match_ui_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position());

        if (is_action_pressed && is_movement_allowed && is_mouse_in_position) {
            match_ui_order_move(state);
            return;
        }
    }

    // Set building rally
    if (input_is_action_just_pressed(INPUT_ACTION_RIGHT_CLICK) && 
            match_ui_get_selection_type(state, state.selection) == MATCH_UI_SELECTION_BUILDINGS &&
            !(state.is_minimap_dragging || match_ui_is_selecting(state) || match_ui_is_targeting(state)) &&
            (!match_ui_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position()))) {
        // Check to make sure that all buildings can rally
        for (EntityId id : state.selection) {
            const Entity& entity = state.match.entities.get_by_id(id);
            if (entity.mode == MODE_BUILDING_IN_PROGRESS || !entity_get_data(entity.type).building_data.can_rally) {
                return;
            }
        }

        // Create rally input
        MatchInput input;
        input.type = MATCH_INPUT_RALLY;

        // Determine rally point
        if (match_ui_is_mouse_in_ui()) {
            ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
            input.rally.rally_point = ivec2((state.match.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                                            (state.match.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        } else {
            input.rally.rally_point = input_get_mouse_position() + state.camera_offset;
        }

        // Check if rallied onto gold
        Cell cell = map_get_cell(state.match.map, CELL_LAYER_GROUND, input.rally.rally_point / TILE_SIZE);
        if (cell.type == CELL_GOLDMINE) {
            // If they rallied onto their gold, adjust their rally point and play an animation to clearly indicate that units will rally out to the mine
            // But only do this if the goldmine has been explored, otherwise we would reveal to players that there is a goldmine where they clicked
            const Entity& goldmine = state.match.entities.get_by_id(cell.id);
            if (match_is_cell_rect_explored(state.match, state.match.players[network_get_player_id()].team, goldmine.cell, entity_get_data(goldmine.type).cell_size)) {
                input.rally.rally_point = (goldmine.cell * TILE_SIZE) + ivec2(23, 16);
                state.move_animation = animation_create(ANIMATION_UI_MOVE_ENTITY);
                state.move_animation_entity_id = cell.id;
                state.move_animation_position = goldmine.cell * TILE_SIZE;
            }
        }

        // Add buildings to rally point input
        input.rally.building_count = (uint8_t)state.selection.size();
        memcpy(&input.rally.building_ids, &state.selection[0], state.selection.size() * sizeof(EntityId));

        state.input_queue.push_back(input);
        sound_play(SOUND_FLAG_THUMP);

        return;
    }

    // Begin minimap drag
    if (MINIMAP_RECT.has_point(input_get_mouse_position()) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state.is_minimap_dragging = true;
        return;
    } 
    // End minimap drag
    if (state.is_minimap_dragging && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state.is_minimap_dragging = false;
        return;
    }

    // Building placement
    if (state.mode == MATCH_UI_MODE_BUILDING_PLACE && 
            !state.is_minimap_dragging && 
            !match_ui_is_mouse_in_ui() && 
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        if (!match_ui_building_can_be_placed(state)) {
            match_ui_show_status(state, MATCH_UI_STATUS_CANT_BUILD);
            return;
        }

        MatchInput input;
        input.type = MATCH_INPUT_BUILD;
        input.build.shift_command = input_is_action_pressed(INPUT_ACTION_SHIFT);
        input.build.building_type = state.building_type;
        input.build.entity_count = state.selection.size();
        memcpy(&input.build.entity_ids, &state.selection[0], state.selection.size() * sizeof(EntityId));
        input.build.target_cell = match_ui_get_building_cell(entity_get_data(state.building_type).cell_size, state.camera_offset);
        state.input_queue.push_back(input);

        if (!input.build.shift_command) {
            state.mode = MATCH_UI_MODE_NONE;
        }

        sound_play(SOUND_BUILDING_PLACE);
        return;
    }

    // Begin selecting
    if (!match_ui_is_mouse_in_ui() && 
            (state.mode == MATCH_UI_MODE_NONE || state.mode == MATCH_UI_MODE_BUILD || state.mode == MATCH_UI_MODE_BUILD2) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state.select_origin = input_get_mouse_position() + state.camera_offset;
        return;
    }
    // End selecting
    if (match_ui_is_selecting(state) && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        ivec2 world_pos = input_get_mouse_position() + state.camera_offset;
        Rect select_rect = (Rect) {
            .x = std::min(world_pos.x, state.select_origin.x),
            .y = std::min(world_pos.y, state.select_origin.y),
            .w = std::max(1, std::abs(state.select_origin.x - world_pos.x)),
            .h = std::max(1, std::abs(state.select_origin.y - world_pos.y)),
        };

        std::vector<EntityId> selection = match_ui_create_selection(state, select_rect);
        state.select_origin = ivec2(-1, -1);
        state.control_group_selected = MATCH_UI_CONTROL_GROUP_NONE;

        // Append selection
        if (input_is_action_pressed(INPUT_ACTION_CTRL)) {
            // Don't append selection if units are not same type
            if (match_ui_get_selection_type(state, selection) != match_ui_get_selection_type(state, state.selection)) {
                return;
            }
            for (EntityId incoming_id : selection) {
                if (std::find(state.selection.begin(), state.selection.end(), incoming_id) == state.selection.end()) {
                    state.selection.push_back(incoming_id);
                }
            }

            match_ui_set_selection(state, state.selection);
            return;
        }

        if (selection.size() == 1) {
            if (state.double_click_timer == 0) {
                state.double_click_timer = MATCH_UI_DOUBLE_CLICK_DURATION;
            } else if (state.selection.size() == 1 && state.selection[0] == selection[0] &&
                        state.match.entities.get_by_id(state.selection[0]).player_id == network_get_player_id()) {
                EntityType selected_type = state.match.entities.get_by_id(state.selection[0]).type;
                selection.clear();

                for (uint32_t entity_index = 0; entity_index < state.match.entities.size(); entity_index++) {
                    if (state.match.entities[entity_index].type != selected_type || state.match.entities[entity_index].player_id != network_get_player_id()) {
                        continue;
                    }

                    Rect entity_rect = entity_get_rect(state.match.entities[entity_index]);
                    entity_rect.x -= state.camera_offset.x;
                    entity_rect.y -= state.camera_offset.y;
                    if (SCREEN_RECT.intersects(entity_rect)) {
                        selection.push_back(state.match.entities.get_id_of(entity_index));
                    }
                }
            }
        }

        match_ui_set_selection(state, selection);
        return;
    }

    // Control group key press
    {
        uint32_t number_key_pressed;
        for (number_key_pressed = 0; number_key_pressed < 10; number_key_pressed++) {
            if (input_is_action_just_pressed((InputAction)(INPUT_ACTION_NUM0 + number_key_pressed))) {
                break;
            }
        }
        if (number_key_pressed != 10) {
            uint32_t control_group_index = number_key_pressed == 0 ? 9 : (number_key_pressed - 1);
            // Set control group
            if (input_is_action_pressed(INPUT_ACTION_CTRL)) {
                if (state.selection.empty() || state.match.entities.get_by_id(state.selection[0]).player_id != network_get_player_id()) {
                    return;
                }

                state.control_groups[control_group_index] = state.selection;
                state.control_group_selected = control_group_index;
                return;
            }

            // Append control group
            if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                if (state.selection.empty() || state.match.entities.get_by_id(state.selection[0]).player_id != network_get_player_id()) {
                    return;
                }
                
                for (EntityId entity_id : state.selection) {
                    if (std::find(state.control_groups[control_group_index].begin(), state.control_groups[control_group_index].end(), entity_id) 
                            == state.control_groups[control_group_index].end()) {
                        state.control_groups[control_group_index].push_back(entity_id);
                    }
                }

                return;
            }

            // Snap to control group
            if (state.control_group_double_tap_timer != 0 && state.control_group_selected == control_group_index) {
                ivec2 group_min;
                ivec2 group_max;
                for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
                    const Entity& entity = state.match.entities.get_by_id(state.selection[selection_index]);
                    if (selection_index == 0) {
                        group_min = entity.cell;
                        group_max = entity.cell;
                        continue;
                    }

                    group_min.x = std::min(group_min.x, entity.cell.x);
                    group_min.y = std::min(group_min.y, entity.cell.y);
                    group_max.x = std::max(group_max.x, entity.cell.x);
                    group_max.y = std::max(group_max.y, entity.cell.y);
                }

                Rect group_rect = (Rect) {
                    .x = group_min.x, .y = group_min.y,
                    .w = group_max.x - group_min.x, .h = group_max.y - group_min.y
                };
                ivec2 group_center = ivec2(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));
                match_ui_center_camera_on_cell(state, group_center);
                return;
            }

            // Select control group
            if (!state.control_groups[control_group_index].empty()) {
                match_ui_set_selection(state, state.control_groups[control_group_index]);
                if (!state.selection.empty()) {
                    state.control_group_double_tap_timer = MATCH_UI_DOUBLE_CLICK_DURATION;
                    state.control_group_selected = control_group_index;
                }
            }
        }
    }

    // Jump to latest alert
    if (input_is_action_just_pressed(INPUT_ACTION_SPACE) && !state.alerts.empty()) {
        match_ui_center_camera_on_cell(state, state.alerts.back().cell);
    }
}

void match_ui_update(MatchUiState& state) {
    if (state.mode == MATCH_UI_MODE_NOT_STARTED) {
        // Check that all players are ready
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                return;
            }
        }
        // If we reached here, then all players are ready
        state.mode = MATCH_UI_MODE_NONE;
        log_info("Match started.");
    }

    // Turn loop
    if (state.turn_timer == 0) {
        bool all_inputs_received = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
                continue;
            }

            if (state.inputs[player_id].empty() || state.inputs[player_id][0].empty()) {
                all_inputs_received = false;
                continue;
            }
        }

        if (!all_inputs_received) {
            state.disconnect_timer++;
            return;
        }

        // Reset the disconnect timer if we recevied inputs
        state.disconnect_timer = 0;

        // All inputs received. Begin next turn
        state.turn_timer = TURN_DURATION;
        state.turn_counter++;

        // Handle input
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
                continue;
            }

            for (const MatchInput& input : state.inputs[player_id][0]) {
                match_handle_input(state.match, input);
            }
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        }

        // Flush input
        // Always send at least one input per turn
        if (state.input_queue.empty()) {
            state.input_queue.push_back((MatchInput) { .type = MATCH_INPUT_NONE });
        }

        // Serialize the inputs
        uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
        size_t out_buffer_length = 1;
        for (const MatchInput& input : state.input_queue) {
            match_input_serialize(out_buffer, out_buffer_length, input);
        }
        state.inputs[network_get_player_id()].push_back(state.input_queue);
        state.input_queue.clear();

        // Send inputs to other players
        network_send_input(out_buffer, out_buffer_length);
    } 
    // End if tick timer is 0

    state.turn_timer--;

    ui_begin();
    match_ui_handle_input(state);

    // Match update
    match_update(state.match);

    // Match events
    while (!state.match.events.empty()) {
        MatchEvent event = state.match.events[0];
        state.match.events.erase(state.match.events.begin());
        switch (event.type) {
            case MATCH_EVENT_SOUND: {
                if (state.sound_cooldown_timers[event.sound.sound] != 0) {
                    break;
                }
                Rect listen_rect = (Rect) {
                    .x = state.camera_offset.x - SOUND_LISTEN_MARGIN,
                    .y = state.camera_offset.y - SOUND_LISTEN_MARGIN,
                    .w = SCREEN_WIDTH + (SOUND_LISTEN_MARGIN * 2),
                    .h = SCREEN_HEIGHT + (SOUND_LISTEN_MARGIN * 2),
                };
                if (listen_rect.has_point(event.sound.position)) {
                    sound_play(event.sound.sound);
                    state.sound_cooldown_timers[event.sound.sound] = SOUND_COOLDOWN_DURATION;
                }
                
                break;
            }
            case MATCH_EVENT_ALERT: {
                if ((event.alert.type == MATCH_ALERT_TYPE_ATTACK && network_get_player(event.alert.player_id).team != network_get_player(network_get_player_id()).team) ||
                    (event.alert.type != MATCH_ALERT_TYPE_ATTACK && event.alert.player_id != network_get_player_id())) {
                    break;
                }

                // Check if an existing attack alert already exists nearby
                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    bool is_existing_attack_alert_nearby = false;
                    for (const Alert& existing_alert : state.alerts) {
                        if (existing_alert.pixel == MINIMAP_PIXEL_WHITE && ivec2::manhattan_distance(existing_alert.cell, event.alert.cell) < UI_ATTACK_ALERT_DISTANCE) {
                            is_existing_attack_alert_nearby = true;
                            break;
                        }
                    }
                    if (is_existing_attack_alert_nearby) {
                        break;
                    }
                }

                // Play the sound even if we don't show the alert
                switch (event.alert.type) {
                    case MATCH_ALERT_TYPE_BUILDING:
                        sound_play(SOUND_ALERT_BUILDING);
                        break;
                    case MATCH_ALERT_TYPE_UNIT:
                        // sound_play(SOUND_ALERT_UNIT);
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

                Rect camera_rect = (Rect) { 
                    .x = state.camera_offset.x, 
                    .y = state.camera_offset.y, 
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
                    pixel = (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + state.match.players[network_get_player_id()].recolor_id);
                }

                state.alerts.push_back((Alert) {
                    .pixel = pixel,
                    .cell = event.alert.cell,
                    .cell_size = event.alert.cell_size,
                    .timer = UI_ALERT_TOTAL_DURATION
                });

                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    match_ui_show_status(state, event.alert.player_id == network_get_player_id() 
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

                if (state.selection.size() == 1 && state.selection[0] == event.selection_handoff.to_deselect) {
                    std::vector<EntityId> new_selection;
                    new_selection.push_back(event.selection_handoff.to_select);
                    match_ui_set_selection(state, new_selection);
                    state.mode = MATCH_UI_MODE_NONE;
                }
                break;
            }
            case MATCH_EVENT_STATUS: {
                if (network_get_player_id() == event.status.player_id) {
                    match_ui_show_status(state, event.status.message);
                }
                break;
            }
            case MATCH_EVENT_RESEARCH_COMPLETE: {
                if (event.research_complete.player_id != network_get_player_id()) {
                    break;
                }

                // char message[128];
                // TODO
                // sprintf(message, "%s research complete.", UPGRADE_DATA.at(event.research_complete.upgrade).name);
                // ui_show_status(state, message);
                break;
            }
        }
    }

    // Camera drag
    if (!match_ui_is_selecting(state)) {
        ivec2 camera_drag_direction = ivec2(0, 0);
        if (input_get_mouse_position().x < MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (input_get_mouse_position().x > SCREEN_WIDTH - MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (input_get_mouse_position().y < MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (input_get_mouse_position().y > SCREEN_HEIGHT - MATCH_CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        state.camera_offset += camera_drag_direction * CAMERA_SPEED;
        match_ui_clamp_camera(state);
    }

    // Minimap drag
    if (state.is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (state.match.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state.match.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        match_ui_center_camera_on_cell(state, map_pos / TILE_SIZE);
    }

    // Update timers and animations
    if (animation_is_playing(state.move_animation)) {
        animation_update(state.move_animation);
    }
    animation_update(state.rally_flag_animation);
    if (state.status_timer != 0) {
        state.status_timer--;
    }
    if (state.double_click_timer != 0) {
        state.double_click_timer--;
    }
    if (state.control_group_double_tap_timer != 0) {
        state.control_group_double_tap_timer--;
    }
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        if (state.sound_cooldown_timers[sound] != 0) {
            state.sound_cooldown_timers[sound]--;
        }
    }

    // Update alerts
    {
        uint32_t alert_index = 0;
        while (alert_index < state.alerts.size()) {
            state.alerts[alert_index].timer--;
            if (state.alerts[alert_index].timer == 0) {
                state.alerts.erase(state.alerts.begin() + alert_index);
            } else {
                alert_index++;
            }
        }
    }

    // Clear hidden units from selection
    {
        int selection_index = 0;
        while (selection_index < state.selection.size()) {
            Entity& selected_entity = state.match.entities.get_by_id(state.selection[selection_index]);
            if (!entity_is_selectable(selected_entity) || !match_is_entity_visible_to_player(state.match, selected_entity, network_get_player_id())) {
                state.selection.erase(state.selection.begin() + selection_index);
            } else {
                selection_index++;
            }
        }
    }

    // Update cursor
    cursor_set(match_ui_is_targeting(state) && !match_ui_is_mouse_in_ui() ? CURSOR_TARGET : CURSOR_DEFAULT);
    if ((match_ui_is_targeting(state) || state.mode == MATCH_UI_MODE_BUILDING_PLACE || state.mode == MATCH_UI_MODE_BUILD || state.mode == MATCH_UI_MODE_BUILD2) && state.selection.empty()) {
        state.mode = MATCH_UI_MODE_NONE;
    }

    // Update UI buttons
    {
        uint32_t hotkey_group = 0;
        if (state.mode == MATCH_UI_MODE_BUILD) {
            hotkey_group = INPUT_HOTKEY_GROUP_BUILD;
        } else if (state.mode == MATCH_UI_MODE_BUILD2) {
            hotkey_group = INPUT_HOTKEY_GROUP_BUILD2;
        } else if (state.mode == MATCH_UI_MODE_BUILDING_PLACE || match_ui_is_targeting(state)) {
            hotkey_group = INPUT_HOTKEY_GROUP_CANCEL;
        } else if (!state.selection.empty()) {
            Entity& first_entity = state.match.entities.get_by_id(state.selection[0]);
            if (first_entity.player_id == network_get_player_id() && first_entity.mode == MODE_BUILDING_IN_PROGRESS) {
                hotkey_group = INPUT_HOTKEY_GROUP_CANCEL;
            } else if (first_entity.player_id == network_get_player_id()) {
                bool is_selection_uniform = true;
                for (uint32_t selection_index = 1; selection_index < state.selection.size(); selection_index++) {
                    if (state.match.entities.get_by_id(state.selection[selection_index]).type != first_entity.type) {
                        is_selection_uniform = false;
                        break;
                    }
                }

                if (is_selection_uniform) {
                    if (entity_is_unit(first_entity.type)) {
                        hotkey_group |= INPUT_HOTKEY_GROUP_UNIT;
                    } else if (entity_is_building(first_entity.type) && !first_entity.queue.empty() && state.selection.size() == 1) {
                        hotkey_group |= INPUT_HOTKEY_CANCEL;
                    }
                    switch (first_entity.type) {
                        case ENTITY_MINER: {
                            hotkey_group |= INPUT_HOTKEY_GROUP_MINER;
                            break;
                        }
                        case ENTITY_HALL: {
                            hotkey_group |= INPUT_HOTKEY_GROUP_HALL;
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }
        input_set_hotkey_group(hotkey_group);
    }
}

// UPDATE FUNCTIONS

void match_ui_clamp_camera(MatchUiState& state) {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, ((int)state.match.map.width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, ((int)state.match.map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_UI_HEIGHT);
}

void match_ui_center_camera_on_cell(MatchUiState& state, ivec2 cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    match_ui_clamp_camera(state);
}

bool match_ui_is_mouse_in_ui() {
    ivec2 mouse_position = input_get_mouse_position();
    return (mouse_position.y >= SCREEN_HEIGHT - MATCH_UI_HEIGHT) ||
           (mouse_position.x <= 136 && mouse_position.y >= SCREEN_HEIGHT - 136) ||
           (mouse_position.x >= SCREEN_WIDTH - 132 && mouse_position.y >= SCREEN_HEIGHT - 106);
}

bool match_ui_is_targeting(const MatchUiState& state) {
    return state.mode >= MATCH_UI_MODE_TARGET_ATTACK && state.mode < MATCH_UI_MODE_CHAT;
}

bool match_ui_is_selecting(const MatchUiState& state) {
    return state.select_origin.x != -1;
}

std::vector<EntityId> match_ui_create_selection(const MatchUiState& state, Rect select_rect) {
    std::vector<EntityId> selection;

    // Select player units
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id != network_get_player_id() ||
                !entity_is_unit(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select player buildings
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id != network_get_player_id() ||
                entity_is_unit(entity.type) ||
                !entity_is_selectable(entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select enemy units
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id == network_get_player_id() ||
                !entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select enemy buildings
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.player_id == network_get_player_id() ||
                entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select gold mines
    for (uint32_t index = 0; index < state.match.entities.size(); index++) {
        const Entity& entity = state.match.entities[index];
        if (entity.type != ENTITY_GOLDMINE ||
                !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state.match.entities.get_id_of(index));
        }
    }

    return selection;
}

void match_ui_set_selection(MatchUiState& state, std::vector<EntityId>& selection) {
    state.selection = selection;
}

MatchUiSelectionType match_ui_get_selection_type(const MatchUiState& state, const std::vector<EntityId>& selection) {
    if (selection.empty()) {
        return MATCH_UI_SELECTION_NONE;
    }

    const Entity& entity = state.match.entities.get_by_id(selection[0]);
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_UI_SELECTION_UNITS;
        } else {
            return MATCH_UI_SELECTION_ENEMY_UNIT;
        }
    } else if (entity_is_building(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_UI_SELECTION_BUILDINGS;
        } else {
            return MATCH_UI_SELECTION_ENEMY_BUILDING;
        }
    } else {
        return MATCH_UI_SELECTION_GOLD;
    }
}

void match_ui_order_move(MatchUiState& state) {
    // Determine move target
    ivec2 move_target;
    if (match_ui_is_mouse_in_ui()) {
        ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
        move_target = ivec2((state.match.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                            (state.match.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
    } else {
        move_target = input_get_mouse_position() + state.camera_offset;
    }

    // Create move input
    MatchInput input;
    input.move.shift_command = input_is_action_pressed(INPUT_ACTION_SHIFT);
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;

    // Checks if clicked on entity
    uint8_t player_team = state.match.players[network_get_player_id()].team;
    int fog_value = match_get_fog(state.match, state.match.players[network_get_player_id()].team, input.move.target_cell);
    // If clicked on hidden fog or the minimap, then don't check for entity click
    if (fog_value != FOG_HIDDEN && !match_ui_is_mouse_in_ui()) {
        for (uint32_t entity_index = 0; entity_index < state.match.entities.size(); entity_index++) {
            const Entity& entity = state.match.entities[entity_index];
            // Only gold mines can be moved to underneath explored fog
            if (fog_value == FOG_EXPLORED && entity.type != ENTITY_GOLDMINE) {
                continue;
            }
            // Don't target unselectable units
            if (!entity_is_selectable(entity)) {
                continue;
            }
            // Don't target invisible units (unless we have detection)
            if (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state.match.detection[player_team][input.move.target_cell.x + (input.move.target_cell.y * state.match.map.width)] == 0) {
                continue;
            }

            Rect entity_rect = entity_get_rect(state.match.entities[entity_index]);
            if (entity_rect.has_point(move_target)) {
                input.move.target_id = state.match.entities.get_id_of(entity_index);
                break;
            }
        }
    }

    if (state.mode == MATCH_UI_MODE_TARGET_UNLOAD) {
        input.type = MATCH_INPUT_MOVE_UNLOAD;
    } else if (state.mode == MATCH_UI_MODE_TARGET_REPAIR) {
        input.type = MATCH_INPUT_MOVE_REPAIR;
    } else if (state.mode == MATCH_UI_MODE_TARGET_SMOKE) {
        input.type = MATCH_INPUT_MOVE_SMOKE;
    } else if (input.move.target_id != ID_NULL && state.match.entities.get_by_id(input.move.target_id).type != ENTITY_GOLDMINE &&
               (state.mode == MATCH_UI_MODE_TARGET_ATTACK || 
                state.match.players[state.match.entities.get_by_id(input.move.target_id).player_id].team != state.match.players[network_get_player_id()].team)) {
        input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && state.mode == MATCH_UI_MODE_TARGET_ATTACK) {
        input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    } else if (input.move.target_id != ID_NULL) {
        input.type = MATCH_INPUT_MOVE_ENTITY;
    } else {
        input.type = MATCH_INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = (uint16_t)state.selection.size();
    memcpy(input.move.entity_ids, &state.selection[0], state.selection.size() * sizeof(EntityId));

    if (input.type == MATCH_INPUT_MOVE_REPAIR) {
        bool is_repair_target_valid = true;
        if (input.move.target_id == ID_NULL) {
            is_repair_target_valid = false;
        } else {
            const Entity& repair_target = state.match.entities.get_by_id(input.move.target_id);
            if (state.match.players[repair_target.player_id].team != state.match.players[network_get_player_id()].team || 
                    !entity_is_building(repair_target.type)) {
                is_repair_target_valid = false;
            }
        }

        if (!is_repair_target_valid) {
            match_ui_show_status(state, MATCH_UI_STATUS_REPAIR_TARGET_INVALID);
            state.mode = MATCH_UI_MODE_NONE;
            return;
        }
    }

    state.input_queue.push_back(input);

    // Check if clicked on remembered building
    EntityId remembered_entity_id = ID_NULL;
    if ((input.type == MATCH_INPUT_MOVE_CELL || input.type == MATCH_INPUT_MOVE_ATTACK_CELL) &&
            !match_ui_is_mouse_in_ui() &&
            match_get_fog(state.match, player_team, input.move.target_cell) == FOG_EXPLORED) {
        ivec2 move_target = input_get_mouse_position() + state.camera_offset;

        for (auto it : state.match.remembered_entities[player_team]) {
            Rect remembered_entity_rect = (Rect) {
                .x = it.second.cell.x * TILE_SIZE, .y = it.second.cell.y * TILE_SIZE,
                .w = it.second.cell_size * TILE_SIZE, .h = it.second.cell_size * TILE_SIZE
            };
            if (remembered_entity_rect.has_point(move_target)) {
                remembered_entity_id = it.first;
                break;
            }
        }
    }

    // Play animation
    if (remembered_entity_id != ID_NULL) {
        EntityType remembered_entity_type = state.match.remembered_entities[player_team][remembered_entity_id].type;
        state.move_animation = animation_create(remembered_entity_type == ENTITY_GOLDMINE
                                                        ? ANIMATION_UI_MOVE_ENTITY 
                                                        : ANIMATION_UI_MOVE_ATTACK_ENTITY);
        state.move_animation_position = cell_center(input.move.target_cell).to_ivec2();
        state.move_animation_entity_id = remembered_entity_id;
    } else if (input.type == MATCH_INPUT_MOVE_CELL || input.type == MATCH_INPUT_MOVE_ATTACK_CELL || input.type == MATCH_INPUT_MOVE_UNLOAD || input.type == MATCH_INPUT_MOVE_SMOKE) {
        state.move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
        state.move_animation_position = input_get_mouse_position() + state.camera_offset;
        state.move_animation_entity_id = ID_NULL;
    } else {
        state.move_animation = animation_create(input.type == MATCH_INPUT_MOVE_ATTACK_ENTITY ? ANIMATION_UI_MOVE_ATTACK_ENTITY : ANIMATION_UI_MOVE_ENTITY);
        state.move_animation_position = cell_center(input.move.target_cell).to_ivec2();
        state.move_animation_entity_id = input.move.target_id;
    }

    // Play ack sound
    // TODO: only play this if unit voices are on
    // sound_play(input.type == MATCH_INPUT_MOVE_ATTACK_CELL || input.type == MATCH_INPUT_MOVE_ATTACK_ENTITY ? SOUND_UNIT_HAW : SOUND_UNIT_OK);

    // Reset UI mode if targeting
    if (match_ui_is_targeting(state)) {
        state.mode = MATCH_UI_MODE_NONE;
    }
}

void match_ui_show_status(MatchUiState& state, const char* message) {
    state.status_message = std::string(message);
    state.status_timer = UI_STATUS_DURATION;
}

ivec2 match_ui_get_building_cell(int building_size, ivec2 camera_offset) {
    ivec2 offset = building_size == 1 ? ivec2(0, 0) : ivec2(building_size, building_size) - ivec2(2, 2);
    ivec2 building_cell = ((input_get_mouse_position() + camera_offset) / TILE_SIZE) - offset;
    building_cell.x = std::max(0, building_cell.x);
    building_cell.y = std::max(0, building_cell.y);
    return building_cell;
}

bool match_ui_building_can_be_placed(const MatchUiState& state) {
    const EntityData& entity_data = entity_get_data(state.building_type);
    ivec2 building_cell = match_ui_get_building_cell(entity_data.cell_size, state.camera_offset);
    ivec2 miner_cell = state.match.entities.get_by_id(match_get_nearest_builder(state.match, state.selection, building_cell)).cell;

    // Check that the building is in bounds
    if (!map_is_cell_rect_in_bounds(state.match.map, building_cell, entity_data.cell_size)) {
        return false;
    }

    // If building a hall, check that it is not too close to goldmines
    if (state.building_type == ENTITY_HALL) {
        Rect building_rect = (Rect) {
            .x = building_cell.x, .y = building_cell.y,
            .w = entity_data.cell_size, .h = entity_data.cell_size
        };

        for (const Entity& entity : state.match.entities) {
            if (entity.type == ENTITY_GOLDMINE) {
                Rect goldmine_block_rect = entity_goldmine_get_block_building_rect(entity.cell);
                if (building_rect.intersects(goldmine_block_rect)) {
                    return false;
                }
            }
        }
    }

    // Check individual squares in the building's rect to make sure they are valid
    for (int y = building_cell.y; y < building_cell.y + entity_data.cell_size; y++) {
        for (int x = building_cell.x; x < building_cell.x + entity_data.cell_size; x++) {
            ivec2 cell = ivec2(x, y);
            // Check that no entities are occupying the building rect
            if ((cell != miner_cell && map_get_cell(state.match.map, CELL_LAYER_GROUND, cell).type != CELL_EMPTY) ||
                    // Check that we're not building on a ramp
                    map_is_tile_ramp(state.match.map, cell) ||
                    // Check that we're not building on top of a landmine
                    map_get_cell(state.match.map, CELL_LAYER_UNDERGROUND, cell).type != CELL_EMPTY) {
                return false;
            }
        }
    }

    return true;
}

Rect match_ui_get_selection_list_item_rect(uint32_t selection_index) {
    ivec2 pos = SELECTION_LIST_TOP_LEFT + ivec2(((selection_index % 10) * 34) - 12, (selection_index / 10) * 34);
    return (Rect) { .x = pos.x, .y = pos.y, .w = 32, .h = 32 };
}

// RENDER

void match_ui_render(const MatchUiState& state) {
    static std::vector<RenderSpriteParams> render_sprite_params[RENDER_TOTAL_LAYER_COUNT];
    static std::vector<RenderHealthbarParams> render_healthbar_params[RENDER_TOTAL_LAYER_COUNT];
    static std::vector<RenderSpriteParams> above_fog_sprite_params;

    ivec2 base_pos = ivec2(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - MATCH_UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Render map
    for (int y = 0; y < max_visible_tiles.y; y++) {
        for (int x = 0; x < max_visible_tiles.x; x++) {
            int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.match.map.width);
            Tile tile = state.match.map.tiles[map_index];
            // Render sand below walls
            if (!(tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_WATER)) {
                render_sprite_params[match_ui_get_render_layer(tile.elevation == 0 ? 0 : tile.elevation - 1, RENDER_LAYER_TILE)].push_back((RenderSpriteParams) {
                    .sprite = SPRITE_TILE_SAND1,
                    .frame = ivec2(0, 0),
                    .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                });
            }

            // Render tile
            render_sprite_params[match_ui_get_render_layer(tile.elevation, RENDER_LAYER_TILE)].push_back((RenderSpriteParams) {
                .sprite = tile.sprite,
                .frame = tile.frame,
                .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                .options = RENDER_SPRITE_NO_CULL,
                .recolor_id = 0
            });

            // Decorations
            Cell cell = state.match.map.cells[CELL_LAYER_GROUND][map_index];
            if (cell.type >= CELL_DECORATION_1 && cell.type <= CELL_DECORATION_5) {
                render_sprite_params[match_ui_get_render_layer(tile.elevation, RENDER_LAYER_ENTITY)].push_back((RenderSpriteParams) {
                    .sprite = SPRITE_DECORATION,
                    .frame = ivec2(cell.type - CELL_DECORATION_1, 0),
                    .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                });
            }
        }  // End for each x
    } // End for each y

    // Select rings and healthbars
    for (EntityId id : state.selection) {
        const Entity& entity = state.match.entities.get_by_id(id);
        const EntityData& entity_data = entity_get_data(entity.type);
        Rect entity_rect = entity_get_rect(entity);
        uint16_t entity_elevation = entity_get_elevation(entity, state.match.map);
        uint16_t render_layer = match_ui_get_render_layer(entity_elevation, RENDER_LAYER_SELECT_RING);

        // Render select ring
        ivec2 entity_center_position = entity_is_unit(entity.type) 
                ? entity.position.to_ivec2()
                : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2)); 
        entity_center_position -= state.camera_offset;
        render_sprite_params[render_layer].push_back((RenderSpriteParams) {
            .sprite = match_ui_get_entity_select_ring(entity.type, entity.type == ENTITY_GOLDMINE ||
                            (state.match.players[entity.player_id].team != state.match.players[network_get_player_id()].team)),
            .frame = ivec2(0, 0),
            .position = entity_center_position,
            .options = RENDER_SPRITE_CENTERED,
            .recolor_id = 0
        });

        // Render healthbar
        ivec2 healthbar_position = ivec2(entity_rect.x, entity_rect.y + entity_rect.h + HEALTHBAR_PADDING) - state.camera_offset;
        if (entity_data.max_health != 0) {
            render_healthbar_params[render_layer].push_back((RenderHealthbarParams) {
                .type = RENDER_HEALTHBAR,
                .position = healthbar_position,
                .size = ivec2(entity_rect.w, HEALTHBAR_HEIGHT),
                .amount = entity.health,
                .max = entity_data.max_health
            });
            healthbar_position.y += HEALTHBAR_HEIGHT + 1;
        }

        // Render garrison bar
        if (entity_data.garrison_capacity != 0) {
            render_healthbar_params[render_layer].push_back((RenderHealthbarParams) {
                .type = RENDER_GARRISONBAR,
                .position = healthbar_position,
                .size = ivec2(entity_rect.w, HEALTHBAR_HEIGHT),
                .amount = (int)entity.garrisoned_units.size(),
                .max = (int)entity_data.garrison_capacity
            });
        }
    }

    // Entities
    for (const Entity& entity : state.match.entities) {
        if (!match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        RenderSpriteParams params = (RenderSpriteParams) {
            .sprite = entity_get_sprite(entity),
            .frame = entity_get_animation_frame(entity),
            .position = entity.position.to_ivec2() - state.camera_offset,
            .options = RENDER_SPRITE_NO_CULL,
            .recolor_id = entity.type == ENTITY_GOLDMINE ? 0 : state.match.players[entity.player_id].recolor_id
        };

        const SpriteInfo& sprite_info = render_get_sprite_info(params.sprite);
        if (entity_is_unit(entity.type)) {
            if (entity.mode == MODE_UNIT_BUILD) {
                const Entity& building = state.match.entities.get_by_id(entity.target.id);
                const EntityData& building_data = entity_get_data(building.type);
                int building_hframe = entity_get_animation_frame(building).x;
                params.position = building.position.to_ivec2() + 
                                    ivec2(building_data.building_data.builder_positions_x[building_hframe],
                                          building_data.building_data.builder_positions_y[building_hframe])
                                    - state.camera_offset;
                if (building_data.building_data.builder_flip_h[building_hframe]) {
                    params.options |= RENDER_SPRITE_FLIP_H;
                }
            } else {
                params.position.x -= sprite_info.frame_width / 2;
                params.position.y -= sprite_info.frame_height / 2;
                if (entity.direction > DIRECTION_SOUTH) {
                    params.options |= RENDER_SPRITE_FLIP_H;
                }
            }
        }

        Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };
        if (!render_rect.intersects(SCREEN_RECT)) {
            continue;
        }

        RenderLayer layer = entity.mode == MODE_UNIT_DEATH_FADE || 
                            entity.mode == MODE_BUILDING_DESTROYED || 
                            entity.type == ENTITY_LANDMINE
                                ? RENDER_LAYER_MINE : RENDER_LAYER_ENTITY;
        render_sprite_params[match_ui_get_render_layer(entity_get_elevation(entity, state.match.map), layer)].push_back(params);
    }

    // Remembered entities
    for (auto it : state.match.remembered_entities[state.match.players[network_get_player_id()].team]) {
        // Don't draw the remembered entity if we can see it, otherwise we will double draw them
        if (match_is_cell_rect_revealed(state.match, state.match.players[network_get_player_id()].team, it.second.cell, it.second.cell_size)) {
            continue;
        }

        const EntityData& entity_data = entity_get_data(it.second.type);
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_data.sprite);

        RenderSpriteParams params = (RenderSpriteParams) {
            .sprite = entity_data.sprite,
            .frame = it.second.frame,
            .position = (it.second.cell * TILE_SIZE) - state.camera_offset,
            .options = RENDER_SPRITE_NO_CULL,
            .recolor_id = it.second.recolor_id
        };

        Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };
        if (!render_rect.intersects(SCREEN_RECT)) {
            continue;
        }

        render_sprite_params[match_ui_get_render_layer(map_get_tile(state.match.map, it.second.cell).elevation, RENDER_LAYER_ENTITY)].push_back(params);
    }

    // Move animation
    if (animation_is_playing(state.move_animation)) {
        if (state.move_animation.name == ANIMATION_UI_MOVE_CELL) {
            RenderSpriteParams params = (RenderSpriteParams) {
                .sprite = SPRITE_UI_MOVE,
                .frame = state.move_animation.frame,
                .position = state.move_animation_position - state.camera_offset,
                .options = RENDER_SPRITE_CENTERED,
                .recolor_id = 0
            };
            ivec2 ui_move_cell = state.move_animation_position / TILE_SIZE;
            if (match_get_fog(state.match, state.match.players[network_get_player_id()].team, ui_move_cell) > 0) {
                render_sprite_params[match_ui_get_render_layer(
                    map_get_tile(state.match.map, ui_move_cell).elevation, 
                    RENDER_LAYER_SELECT_RING)].push_back(params);
            } else {
                above_fog_sprite_params.push_back(params);
            }
        } else if (state.move_animation.frame.x % 2 == 0) {
            // TODO: use mine select ring layer for landmines
            uint32_t entity_index = state.match.entities.get_index_of(state.move_animation_entity_id);
            if (entity_index != INDEX_INVALID) {
                const Entity& entity = state.match.entities[entity_index];

                Rect entity_rect = entity_get_rect(entity);
                ivec2 entity_center_position = entity_is_unit(entity.type) 
                        ? entity.position.to_ivec2()
                        : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2)); 
                entity_center_position -= state.camera_offset;

                RenderSpriteParams params = (RenderSpriteParams) {
                    .sprite = match_ui_get_entity_select_ring(entity.type, state.move_animation.name == ANIMATION_UI_MOVE_ATTACK_ENTITY),
                    .frame = ivec2(0, 0),
                    .position = entity_center_position,
                    .options = RENDER_SPRITE_CENTERED,
                    .recolor_id = 0
                };

                uint16_t render_layer = match_ui_get_render_layer(entity_get_elevation(entity, state.match.map), RENDER_LAYER_SELECT_RING);
                render_sprite_params[render_layer].push_back(params);
            } else {
                auto it = state.match.remembered_entities[state.match.players[network_get_player_id()].team].find(state.move_animation_entity_id);
                if (it != state.match.remembered_entities[state.match.players[network_get_player_id()].team].end()) {
                    ivec2 entity_center_position = (it->second.cell * TILE_SIZE) + ((ivec2(it->second.cell_size, it->second.cell_size) * TILE_SIZE) / 2);

                    RenderSpriteParams params = (RenderSpriteParams) {
                        .sprite = match_ui_get_entity_select_ring(it->second.type, state.move_animation.name == ANIMATION_UI_MOVE_ATTACK_ENTITY),
                        .frame = ivec2(0, 0),
                        .position = entity_center_position,
                        .options = RENDER_SPRITE_CENTERED,
                        .recolor_id = 0
                    };

                    uint16_t render_layer = match_ui_get_render_layer(map_get_tile(state.match.map, it->second.cell).elevation, RENDER_LAYER_SELECT_RING);
                    render_sprite_params[render_layer].push_back(params);
                }
            }
        }
    }

    // Rally points
    if (match_ui_get_selection_type(state, state.selection) == MATCH_UI_SELECTION_BUILDINGS) {
        for (EntityId id : state.selection) {
            const Entity& building = state.match.entities.get_by_id(id);
            if (building.mode != MODE_BUILDING_FINISHED || building.rally_point.x == -1) {
                continue;
            }

            RenderSpriteParams params = (RenderSpriteParams) {
                .sprite = SPRITE_RALLY_FLAG,
                .frame = state.rally_flag_animation.frame,
                .position = building.rally_point - ivec2(4, 15) - state.camera_offset,
                .options = 0,
                .recolor_id = state.match.players[network_get_player_id()].recolor_id
            };

            ivec2 rally_cell = building.rally_point / TILE_SIZE;
            if (match_get_fog(state.match, state.match.players[network_get_player_id()].team, rally_cell) > 0) {
                uint16_t layer = match_ui_get_render_layer(map_get_tile(state.match.map, rally_cell).elevation, RENDER_LAYER_ENTITY);
                render_sprite_params[layer].push_back(params);
            } else {
                above_fog_sprite_params.push_back(params);
            }
        }
    }

    // Render sprite params
    for (int render_layer = 0; render_layer < RENDER_TOTAL_LAYER_COUNT; render_layer++) {
        // If this layer is one of the entity layers, y sort it
        int elevation = render_layer / RENDER_LAYER_COUNT;
        int render_layer_without_elevation = render_layer - (elevation * RENDER_LAYER_COUNT);
        if (render_layer_without_elevation == RENDER_LAYER_ENTITY) {
            match_ui_ysort_render_params(render_sprite_params[render_layer], 0, render_sprite_params[render_layer].size() - 1);
        }

        for (const RenderSpriteParams& params : render_sprite_params[render_layer]) {
            render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
        render_sprite_params[render_layer].clear();

        for (const RenderHealthbarParams& params : render_healthbar_params[render_layer]) {
            match_ui_render_healthbar(params);
        }
        render_healthbar_params[render_layer].clear();
    }

    // Fog of War
    int player_team = state.match.players[network_get_player_id()].team;
    for (int fog_pass = 0; fog_pass < 2; fog_pass++) {
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                ivec2 fog_cell = base_coords + ivec2(x, y);
                int fog = match_get_fog(state.match, player_team, fog_cell);
                if (fog > 0) {
                    continue;
                }
                if (fog_pass == 1 && fog == FOG_EXPLORED) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(state.match.map, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if ((fog_pass == 0 && match_get_fog(state.match, player_team, neighbor_cell) < 1) ||
                        (fog_pass != 0 && match_get_fog(state.match, player_team, neighbor_cell) == FOG_HIDDEN)) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }

                for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                    int prev_direction = direction - 1;
                    int next_direction = (direction + 1) % DIRECTION_COUNT;
                    if ((neighbors & DIRECTION_MASK[prev_direction]) != DIRECTION_MASK[prev_direction] ||
                        (neighbors & DIRECTION_MASK[next_direction]) != DIRECTION_MASK[next_direction]) {
                        continue;
                    }
                    if (!map_is_cell_in_bounds(state.match.map, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if ((fog_pass == 0 && match_get_fog(state.match, player_team, neighbor_cell) < 1) ||
                        (fog_pass != 0 && match_get_fog(state.match, player_team, neighbor_cell) == FOG_HIDDEN)) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                int autotile_index = map_neighbors_to_autotile_index(neighbors);
                #ifndef GOLD_DEBUG_FOG_DISABLED
                    render_sprite_frame(
                            fog_pass == 0 ? SPRITE_FOG_EXPLORED : SPRITE_FOG_HIDDEN, 
                            ivec2(autotile_index % AUTOTILE_HFRAMES, autotile_index / AUTOTILE_HFRAMES), 
                            base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                #endif
            }
        }
    }

    for (const RenderSpriteParams& params : above_fog_sprite_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }
    above_fog_sprite_params.clear();

    // UI Building Placement
    if (state.mode == MATCH_UI_MODE_BUILDING_PLACE && !match_ui_is_mouse_in_ui()) {
        const EntityData& building_data = entity_get_data(state.building_type);

        // First draw the building
        ivec2 building_cell = match_ui_get_building_cell(building_data.cell_size, state.camera_offset);
        render_sprite_frame(building_data.sprite, ivec2(3, 0), (building_cell * TILE_SIZE) - state.camera_offset, RENDER_SPRITE_NO_CULL, state.match.players[network_get_player_id()].recolor_id);

        // Then draw the green / red squares
        bool is_placement_out_of_bounds = building_cell.x + building_data.cell_size > state.match.map.width ||
                                          building_cell.y + building_data.cell_size > state.match.map.height;
        ivec2 miner_cell = state.match.entities.get_by_id(match_get_nearest_builder(state.match, state.selection, building_cell)).cell;
        for (int y = building_cell.y; y < building_cell.y + building_data.cell_size; y++) {
            for (int x = building_cell.x; x < building_cell.x + building_data.cell_size; x++) {
                ivec2 cell = ivec2(x, y);
                // Don't allow buildings out of bounds
                bool is_cell_red = is_placement_out_of_bounds ||
                    // Don't allow buildings in hidden fog
                    match_get_fog(state.match, state.match.players[network_get_player_id()].team, cell) == FOG_HIDDEN ||
                    // Don't allow buildings on ramps
                    map_is_tile_ramp(state.match.map, cell) ||
                    // Don't allow buildings on top of units (unless it's the unit who will be building the building)
                    (cell != miner_cell && map_get_cell(state.match.map, CELL_LAYER_GROUND, cell).type != CELL_EMPTY) ||
                    // Don't allow buildigns on top of landmines
                    map_get_cell(state.match.map, CELL_LAYER_UNDERGROUND, cell).type != CELL_EMPTY;
                
                // Don't allow buildings too close to goldmines
                if (state.building_type == ENTITY_HALL) {
                    for (const Entity& goldmine : state.match.entities) {
                        if (goldmine.type == ENTITY_GOLDMINE && entity_goldmine_get_block_building_rect(goldmine.cell).has_point(cell)) {
                            is_cell_red = true;
                            break;
                        }
                    }
                }

                RenderColor cell_color = is_cell_red ? RENDER_COLOR_RED_TRANSPARENT : RENDER_COLOR_GREEN_TRANSPARENT;
                Rect cell_rect = (Rect) {
                    .x = (x * TILE_SIZE) - state.camera_offset.x,
                    .y = (y * TILE_SIZE) - state.camera_offset.y,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                render_fill_rect(cell_rect, cell_color);
            }
        }
    }
    // End UI building placement

    // Select rect
    if (match_ui_is_selecting(state)) {
        ivec2 mouse_world_pos = ivec2(input_get_mouse_position().x, std::min(input_get_mouse_position().y, SCREEN_HEIGHT - MATCH_UI_HEIGHT)) + state.camera_offset;
        Rect select_rect = (Rect) {
            .x = std::min(state.select_origin.x, mouse_world_pos.x) - state.camera_offset.x, 
            .y = std::min(state.select_origin.y, mouse_world_pos.y) - state.camera_offset.y,
            .w = std::abs(state.select_origin.x - mouse_world_pos.x),
            .h = std::abs(state.select_origin.y - mouse_world_pos.y)
        };
        render_draw_rect(select_rect, RENDER_COLOR_WHITE);
    }

    // UI frames
    const SpriteInfo& minimap_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
    const SpriteInfo& bottom_panel_sprite_info = render_get_sprite_info(SPRITE_UI_BOTTOM_PANEL);
    const SpriteInfo& button_panel_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_PANEL);
    ivec2 ui_frame_bottom_position = ivec2(minimap_sprite_info.frame_width, SCREEN_HEIGHT - bottom_panel_sprite_info.frame_height);
    render_sprite_frame(SPRITE_UI_MINIMAP, ivec2(0, 0), ivec2(0, SCREEN_HEIGHT - minimap_sprite_info.frame_height), 0, 0);
    render_sprite_frame(SPRITE_UI_BOTTOM_PANEL, ivec2(0, 0), ui_frame_bottom_position, 0, 0);
    render_sprite_frame(SPRITE_UI_BUTTON_PANEL, ivec2(0, 0), ivec2(ui_frame_bottom_position.x + bottom_panel_sprite_info.frame_width, SCREEN_HEIGHT - button_panel_sprite_info.frame_height), 0, 0);

    // UI Control groups
    for (uint32_t control_group_index = 0; control_group_index < MATCH_UI_CONTROL_GROUP_COUNT; control_group_index++) {
        // Count the entities in this control group and determine which is the most common
        uint32_t entity_count = 0;
        std::unordered_map<EntityType, uint32_t> entity_occurances;
        EntityType most_common_entity_type = ENTITY_MINER;
        entity_occurances[ENTITY_MINER] = 0;

        for (EntityId id : state.control_groups[control_group_index]) {
            uint32_t entity_index = state.match.entities.get_index_of(id);
            if (entity_index == INDEX_INVALID || !entity_is_selectable(state.match.entities[entity_index])) {
                continue;
            }

            EntityType entity_type = state.match.entities[entity_index].type;
            auto occurances_it = entity_occurances.find(entity_type);
            if (occurances_it == entity_occurances.end()) {
                entity_occurances[entity_type] = 1;
            } else {
                occurances_it->second++;
            }
            if (entity_occurances[entity_type] > entity_occurances[most_common_entity_type]) {
                most_common_entity_type = entity_type;
            }
            entity_count++;
        }

        if (entity_count == 0) {
            continue;
        }

        int button_frame = 0;
        FontName font = FONT_M3X6_OFFBLACK;
        if (state.control_group_selected != control_group_index) {
            font = FONT_M3X6_DARKBLACK;
            button_frame = 2;
        }

        SpriteName button_icon = entity_get_data(most_common_entity_type).icon;
        const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_CONTROL_GROUP);
        ivec2 render_pos = ui_frame_bottom_position + ivec2(2, 0) + ivec2((3 + sprite_info.frame_width) * control_group_index, -32);
        render_sprite_frame(SPRITE_UI_CONTROL_GROUP, ivec2(button_frame, 0), render_pos, RENDER_SPRITE_NO_CULL, 0);
        render_sprite_frame(button_icon, ivec2(button_frame, 0), render_pos + ivec2(1, 0), RENDER_SPRITE_NO_CULL, 0);
        char control_group_number_text[4];
        sprintf(control_group_number_text, "%u", control_group_index == 9 ? 0 : control_group_index + 1);
        render_text(font, control_group_number_text, render_pos + ivec2(3, -9));
        char control_group_count_text[4];
        sprintf(control_group_count_text, "%u", entity_count);
        ivec2 count_text_size = render_get_text_size(font, control_group_count_text);
        render_text(font, control_group_count_text, render_pos + ivec2(32 - count_text_size.x, 23 - count_text_size.y));
    }

    // UI Status message
    if (state.status_timer != 0) {
        int status_message_width = render_get_text_size(FONT_HACK_WHITE, state.status_message.c_str()).x;
        render_text(FONT_HACK_WHITE, state.status_message.c_str(), ivec2((SCREEN_WIDTH / 2) - (status_message_width / 2), SCREEN_HEIGHT - 148));
    }
    
    // Hotkeys
    for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
        InputAction hotkey = input_get_hotkey(hotkey_index);
        if (hotkey == INPUT_HOTKEY_NONE) {
            continue;
        }

        const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
        ivec2 hotkey_position = MATCH_UI_BUTTON_POSITIONS[hotkey_index];
        Rect hotkey_rect = (Rect) { 
            .x = hotkey_position.x, .y = hotkey_position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height 
        };
        int hframe = 0;
        if (!match_does_player_meet_hotkey_requirements(state.match, network_get_player_id(), hotkey)) {
            hframe = 2;
        } else if (!(state.is_minimap_dragging || match_ui_is_selecting(state)) && hotkey_rect.has_point(input_get_mouse_position())) {
            hframe = 1;
            hotkey_position.y--;
        }

        render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(hframe, 0), hotkey_position, RENDER_SPRITE_NO_CULL, 0);
        render_sprite_frame(hotkey_get_sprite(hotkey), ivec2(hframe, 0), hotkey_position, RENDER_SPRITE_NO_CULL, 0);
    }

    // UI Tooltip
    if (!(state.is_minimap_dragging || match_ui_is_selecting(state))) {
        uint32_t hotkey_hovered_index;
        for (hotkey_hovered_index = 0; hotkey_hovered_index < HOTKEY_GROUP_SIZE; hotkey_hovered_index++) {
            Rect hotkey_rect = (Rect) {
                .x = MATCH_UI_BUTTON_POSITIONS[hotkey_hovered_index].x,
                .y = MATCH_UI_BUTTON_POSITIONS[hotkey_hovered_index].y,
                .w = 32,
                .h = 32
            };
            if (hotkey_rect.has_point(input_get_mouse_position())) {
                break;
            }
        }

        // If we are actually hovering a hotkey
        if (hotkey_hovered_index != HOTKEY_GROUP_SIZE) {
            InputAction hotkey_hovered = input_get_hotkey(hotkey_hovered_index);
            if (hotkey_hovered != INPUT_HOTKEY_NONE) {
                const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey_hovered);
                const SpriteInfo& button_panel_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_PANEL);

                // Get tooltip info
                char tooltip_text[64];
                uint32_t tooltip_gold_cost;
                uint32_t tooltip_population_cost;
                if (match_does_player_meet_hotkey_requirements(state.match, network_get_player_id(), hotkey_hovered)) {
                    char* tooltip_text_ptr = tooltip_text;

                    switch (hotkey_info.type) {
                        case HOTKEY_BUTTON_ACTION: {
                            tooltip_text_ptr += sprintf(tooltip_text, "%s", hotkey_info.action.name);
                            tooltip_gold_cost = 0;
                            tooltip_population_cost = 0;
                            break;
                        }
                        case HOTKEY_BUTTON_TRAIN:
                        case HOTKEY_BUTTON_BUILD: {
                            const EntityData& entity_data = entity_get_data(hotkey_info.entity_type);
                            tooltip_text_ptr += sprintf(tooltip_text, "%s %s", hotkey_info.type == HOTKEY_BUTTON_TRAIN ? "Hire" : "Build", entity_data.name);
                            tooltip_gold_cost = entity_data.gold_cost;
                            tooltip_population_cost = hotkey_info.type == HOTKEY_BUTTON_TRAIN ? entity_data.unit_data.population_cost : 0;
                            break;
                        }
                        case HOTKEY_BUTTON_RESEARCH: {
                            tooltip_text_ptr += sprintf(tooltip_text, "Research fixme!");
                            tooltip_gold_cost = 0;
                            tooltip_population_cost = 0;
                            break;
                        }
                    }

                    tooltip_text_ptr += sprintf(tooltip_text_ptr, " (");
                    tooltip_text_ptr += input_sprintf_hotkey_str(tooltip_text_ptr, hotkey_hovered);
                    tooltip_text_ptr += sprintf(tooltip_text_ptr, ")");
                } else {
                    switch (hotkey_info.requirements.type) {
                        case HOTKEY_REQUIRES_NONE: {
                            GOLD_ASSERT(false);
                            break;
                        }
                        case HOTKEY_REQUIRES_BUILDING: {
                            sprintf(tooltip_text, "Requires %s", entity_get_data(hotkey_info.requirements.building).name);
                            break;
                        }
                        case HOTKEY_REQUIRES_UPGRADE: {
                            // TODO
                            sprintf(tooltip_text, "Requires Upgrade");
                            break;
                        }
                    }
                    tooltip_gold_cost = 0;
                    tooltip_population_cost = 0;
                }

                // Render the tooltip frame
                int tooltip_text_width = render_get_text_size(FONT_WESTERN8_OFFBLACK, tooltip_text).x;
                int tooltip_min_width = 10 + tooltip_text_width;
                int tooltip_cell_width = tooltip_min_width / 8;
                int tooltip_cell_height = tooltip_gold_cost != 0 ? 5 : 3;
                if (tooltip_min_width % 8 != 0) {
                    tooltip_cell_width++;
                }
                ivec2 tooltip_top_left = ivec2(
                    SCREEN_WIDTH - (tooltip_cell_width * 8) - 2,
                    SCREEN_HEIGHT - button_panel_sprite_info.frame_height - (tooltip_cell_height * 8) - 2
                );
                for (int y = 0; y < tooltip_cell_height; y++) {
                    for (int x = 0; x < tooltip_cell_width; x++) {
                        ivec2 frame;
                        if (x == 0) {
                            frame.x = 0;
                        } else if (x == tooltip_cell_width - 1) {
                            frame.x = 2;
                        } else {
                            frame.x = 1;
                        }
                        if (y == 0) {
                            frame.y = 0;
                        } else if (y == tooltip_cell_height - 1) {
                            frame.y = 2;
                        } else {
                            frame.y = 1;
                        }

                        render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, frame, tooltip_top_left + (ivec2(x, y) * 8), RENDER_SPRITE_NO_CULL, 0);
                    }
                }

                // Render tooltip text
                render_text(FONT_WESTERN8_OFFBLACK, tooltip_text, tooltip_top_left + ivec2(5, 5));

                // Render gold icon and text
                if (tooltip_gold_cost != 0) {
                    render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), tooltip_top_left + ivec2(5, 21), RENDER_SPRITE_NO_CULL, 0);
                    char gold_text[4];
                    sprintf(gold_text, "%u", tooltip_gold_cost);
                    render_text(FONT_WESTERN8_OFFBLACK, gold_text, tooltip_top_left + ivec2(23, 21));
                }

                // Render population icon and text
                if (tooltip_population_cost != 0) {
                    render_sprite_frame(SPRITE_UI_HOUSE_ICON, ivec2(0, 0), tooltip_top_left + ivec2(5 + 18 + 32, 19), RENDER_SPRITE_NO_CULL, 0);
                    char population_text[4];
                    sprintf(population_text, "%u", tooltip_population_cost);
                    render_text(FONT_WESTERN8_OFFBLACK, population_text, tooltip_top_left + ivec2(5 + 18 + 32 + 22, 23));
                }
            } 
        }
    }
    // End render tooltip

    // UI Selection list
    if (state.selection.size() == 1) {
        const Entity& entity = state.match.entities.get_by_id(state.selection[0]);
        const EntityData& entity_data = entity_get_data(entity.type);

        // Entity name
        const SpriteInfo& frame_sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, entity_data.name);
        int frame_count = (text_size.x / frame_sprite_info.frame_width) + 1;
        if (text_size.x % frame_sprite_info.frame_width != 0) {
            frame_count++;
        }
        for (int frame = 0; frame < frame_count; frame++) {
            int hframe = 1;
            if (frame == 0) {
                hframe = 0;
            } else if (frame == frame_count - 1) {
                hframe = 2;
            }
            render_sprite_frame(SPRITE_UI_TEXT_FRAME, ivec2(hframe, 0), SELECTION_LIST_TOP_LEFT + ivec2(frame * frame_sprite_info.frame_width, 0), RENDER_SPRITE_NO_CULL, 0);
        }
        ivec2 frame_size = ivec2(frame_count * frame_sprite_info.frame_width, frame_sprite_info.frame_height);
        render_text(FONT_WESTERN8_OFFBLACK, entity_data.name, SELECTION_LIST_TOP_LEFT + ivec2((frame_size.x / 2) - (text_size.x / 2), 0));

        // Entity icon
        render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(0, 18), RENDER_SPRITE_NO_CULL, 0);
        render_sprite_frame(entity_data.icon, ivec2(0, 0), SELECTION_LIST_TOP_LEFT+ ivec2(0, 18), RENDER_SPRITE_NO_CULL, 0);

        if (entity.type == ENTITY_GOLDMINE) {
            if (entity.mode == MODE_GOLDMINE_COLLAPSED) {
                render_text(FONT_WESTERN8_GOLD, "Collapsed!", SELECTION_LIST_TOP_LEFT + ivec2(36, 22));
            } else {
                char gold_left_str[17];
                sprintf(gold_left_str, "Gold Left: %u", entity.gold_held);
                render_text(FONT_WESTERN8_GOLD, gold_left_str, SELECTION_LIST_TOP_LEFT + ivec2(36, 22));
            }
        } else {
            RenderHealthbarParams params = (RenderHealthbarParams) {
                .type = RENDER_HEALTHBAR,
                .position = SELECTION_LIST_TOP_LEFT + ivec2(0, 18 + 35),
                .size = ivec2(64, 12),
                .amount = entity.health,
                .max = entity_data.max_health
            };
            match_ui_render_healthbar(params);

            char health_text[16];
            sprintf(health_text, "%i/%i", entity.health, entity_data.max_health);
            ivec2 health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
            ivec2 health_text_position = params.position + (params.size / 2) - (health_text_size / 2);
            render_text(FONT_HACK_WHITE, health_text, health_text_position);

            if (entity_data.has_detection) {
                render_text(FONT_WESTERN8_GOLD, "Detection", SELECTION_LIST_TOP_LEFT + ivec2(36, 22));
            }
            if (entity.type == ENTITY_TINKER && entity.cooldown_timer != 0) {
                char cooldown_text[32];
                sprintf(cooldown_text, "Smoke Bomb cooldown: %us", entity.cooldown_timer / 60);
                render_text(FONT_WESTERN8_GOLD, cooldown_text, SELECTION_LIST_TOP_LEFT + ivec2(36, 36));
            }
        }
    } else {
        for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
            const Entity& entity = state.match.entities.get_by_id(state.selection[selection_index]);
            const EntityData& entity_data = entity_get_data(entity.type);

            Rect icon_rect = match_ui_get_selection_list_item_rect(selection_index);
            bool icon_hovered = !(state.is_minimap_dragging || match_ui_is_selecting(state)) &&
                                    icon_rect.has_point(input_get_mouse_position());
            render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(icon_hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)icon_hovered), RENDER_SPRITE_NO_CULL, 0);
            render_sprite_frame(entity_data.icon, ivec2(icon_hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)icon_hovered), RENDER_SPRITE_NO_CULL, 0);
            match_ui_render_healthbar((RenderHealthbarParams) {
                .type = RENDER_HEALTHBAR,
                .position = ivec2(icon_rect.x + 1, icon_rect.y + 27 - (int)icon_hovered),
                .size = ivec2(30, 4),
                .amount = entity.health,
                .max = entity_data.max_health
            });
        }
    }
    // End UI Selection List

    // UI Building queues
    if (state.selection.size() == 1) {
        const Entity& building = state.match.entities.get_by_id(state.selection[0]);
        if (entity_is_building(building.type) && 
                building.player_id == network_get_player_id() &&
                !building.queue.empty()) {
            // Render building queue icon buttons
            const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
                Rect icon_rect = (Rect) {
                    .x = UI_BUILDING_QUEUE_POSITIONS[building_queue_index].x,
                    .y = UI_BUILDING_QUEUE_POSITIONS[building_queue_index].y,
                    .w = icon_sprite_info.frame_width,
                    .h = icon_sprite_info.frame_height
                };
                SpriteName item_sprite;
                switch (building.queue[building_queue_index].type) {
                    case BUILDING_QUEUE_ITEM_UNIT: {
                        item_sprite = entity_get_data(building.queue[building_queue_index].unit_type).icon;
                        break;
                    }
                    case BUILDING_QUEUE_ITEM_UPGRADE: {
                        // TODO
                        item_sprite = SPRITE_BUTTON_ICON_CANCEL;
                        break;
                    }
                }
                bool hovered = icon_rect.has_point(input_get_mouse_position());
                render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
                render_sprite_frame(item_sprite, ivec2(hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - (int)hovered), RENDER_SPRITE_NO_CULL, 0);
            }

            // Render building queue progress bar
            if (building.timer == BUILDING_QUEUE_BLOCKED) {
                render_text(FONT_WESTERN8_GOLD, "Build more houses.", ivec2(UI_BUILDING_QUEUE_PROGRESS_BAR_RECT.x + 2, UI_BUILDING_QUEUE_PROGRESS_BAR_RECT.y - 12));
            } else if (building.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                render_text(FONT_WESTERN8_GOLD, "Exit is blocked.", ivec2(UI_BUILDING_QUEUE_PROGRESS_BAR_RECT.x + 2, UI_BUILDING_QUEUE_PROGRESS_BAR_RECT.y - 12));
            } else {
                int item_duration = (int)building_queue_item_duration(building.queue[0]);
                Rect building_queue_progress_bar_subrect = UI_BUILDING_QUEUE_PROGRESS_BAR_RECT;
                building_queue_progress_bar_subrect.w = (UI_BUILDING_QUEUE_PROGRESS_BAR_RECT.w * (item_duration - (int)building.timer) / item_duration);
                render_fill_rect(building_queue_progress_bar_subrect, RENDER_COLOR_WHITE);
                render_draw_rect(UI_BUILDING_QUEUE_PROGRESS_BAR_RECT, RENDER_COLOR_OFFBLACK);
            }
        }
    }

    // Resource counters
    {
        char gold_text[8];
        sprintf(gold_text, "%u", state.match.players[network_get_player_id()].gold);
        render_text(FONT_WESTERN8_WHITE, gold_text, ivec2(SCREEN_WIDTH - 172 + 18, 2));
        render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), ivec2(SCREEN_WIDTH - 172, 2), RENDER_SPRITE_NO_CULL, 0);

        char population_text[8];
        sprintf(population_text, "%u/%u", match_get_player_population(state.match, network_get_player_id()), match_get_player_max_population(state.match, network_get_player_id()));
        render_text(FONT_WESTERN8_WHITE, population_text, ivec2(SCREEN_WIDTH - 88 + 22, 2));
        render_sprite_frame(SPRITE_UI_HOUSE_ICON, ivec2(0, 0), ivec2(SCREEN_WIDTH - 88, 0), RENDER_SPRITE_NO_CULL, 0);
    }

    // Debug unit info
    #ifdef GOLD_DEBUG_UNIT_INFO
        if (!match_ui_is_mouse_in_ui()) {
            ivec2 mouse_cell = (input_get_mouse_position() + state.camera_offset) / TILE_SIZE;
            Cell cell_value = map_get_cell(state.match.map, CELL_LAYER_GROUND, mouse_cell);
            char debug_text[128];
            char* debug_text_ptr = debug_text;
            debug_text_ptr += sprintf(debug_text, "%i,%i ", mouse_cell.x, mouse_cell.y);
            switch (cell_value.type) {
                case CELL_EMPTY:
                    debug_text_ptr += sprintf(debug_text_ptr, "EMPTY");
                    break;
                case CELL_BLOCKED:
                    debug_text_ptr += sprintf(debug_text_ptr, "BLOCKED");
                    break;
                case CELL_UNREACHABLE:
                    debug_text_ptr += sprintf(debug_text_ptr, "UNREACHABLE");
                    break;
                case CELL_DECORATION_1:
                case CELL_DECORATION_2:
                case CELL_DECORATION_3:
                case CELL_DECORATION_4:
                case CELL_DECORATION_5:
                    debug_text_ptr += sprintf(debug_text_ptr, "DECORATION");
                    break;
                case CELL_UNIT:
                    debug_text_ptr += sprintf(debug_text_ptr, "UNIT");
                    break;
                case CELL_BUILDING:
                    debug_text_ptr += sprintf(debug_text_ptr, "BUILDING");
                    break;
                case CELL_GOLDMINE:
                    debug_text_ptr += sprintf(debug_text_ptr, "GOLDMINE");
                    break;
                case CELL_MINER:
                    debug_text_ptr += sprintf(debug_text_ptr, "MINER");
                    break;
            }
            render_text(FONT_HACK_WHITE, debug_text, ivec2(0, 12));

            debug_text_ptr = debug_text;
            if (cell_value.type == CELL_MINER || cell_value.type == CELL_UNIT || cell_value.type == CELL_BUILDING) {
                const Entity& entity = state.match.entities.get_by_id(cell_value.id);
                debug_text_ptr += sprintf(debug_text_ptr, "%u %s | Mode: ", cell_value.id, entity_get_data(entity.type).name);
                switch (entity.mode) {
                    case MODE_UNIT_IDLE:
                        debug_text_ptr += sprintf(debug_text_ptr, "IDLE");
                        break;
                    case MODE_UNIT_MOVE:
                        debug_text_ptr += sprintf(debug_text_ptr, "MOVE");
                        break;
                    case MODE_UNIT_BLOCKED:
                        debug_text_ptr += sprintf(debug_text_ptr, "BLOCKED");
                        break;
                    case MODE_UNIT_MOVE_FINISHED:
                        debug_text_ptr += sprintf(debug_text_ptr, "MOVE FINISHED");
                        break;
                    case MODE_UNIT_IN_MINE:
                        debug_text_ptr += sprintf(debug_text_ptr, "IN_MINE");
                        break;
                    case MODE_UNIT_ATTACK_WINDUP:
                        debug_text_ptr += sprintf(debug_text_ptr, "ATTACKING");
                        break;
                    case MODE_UNIT_DEATH:
                        debug_text_ptr += sprintf(debug_text_ptr, "DEATH");
                        break;
                    case MODE_UNIT_DEATH_FADE:
                        debug_text_ptr += sprintf(debug_text_ptr, "DEATH FADE");
                        break;
                    case MODE_UNIT_REPAIR:
                        debug_text_ptr += sprintf(debug_text_ptr, "REPAIR");
                        break;
                    case MODE_UNIT_BUILD:
                        debug_text_ptr += sprintf(debug_text_ptr, "BUILD");
                        break;
                    case MODE_UNIT_SOLDIER_CHARGE:
                        debug_text_ptr += sprintf(debug_text_ptr, "CHARGE");
                        break;
                    case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP:
                        debug_text_ptr += sprintf(debug_text_ptr, "SOLDIER ATTACKING");
                        break;
                    case MODE_UNIT_TINKER_THROW:
                        debug_text_ptr += sprintf(debug_text_ptr, "TINKER THROW");
                        break;
                    case MODE_BUILDING_IN_PROGRESS:
                        debug_text_ptr += sprintf(debug_text_ptr, "IN PROGRESS");
                        break;
                    case MODE_BUILDING_FINISHED:
                        debug_text_ptr += sprintf(debug_text_ptr, "FINISHED");
                        break;
                    case MODE_BUILDING_DESTROYED:
                        debug_text_ptr += sprintf(debug_text_ptr, "DESTROYED");
                        break;
                    case MODE_MINE_ARM:
                        debug_text_ptr += sprintf(debug_text_ptr, "ARM");
                        break;
                    case MODE_MINE_PRIME:
                        debug_text_ptr += sprintf(debug_text_ptr, "PRIME");
                        break;
                    case MODE_GOLDMINE:
                        debug_text_ptr += sprintf(debug_text_ptr, "GOLDMINE");
                        break;
                    case MODE_GOLDMINE_COLLAPSED:
                        debug_text_ptr += sprintf(debug_text_ptr, "COLLAPSED");
                        break;
                }

                render_text(FONT_HACK_WHITE, debug_text, ivec2(0, 24));

                debug_text_ptr = debug_text;
                debug_text_ptr += sprintf(debug_text_ptr, "Target: %u Reached? %i", entity.target.type, (int)match_has_entity_reached_target(state.match, entity));
                render_text(FONT_HACK_WHITE, debug_text, ivec2(0, 36));
            }
        }
    #endif

    ui_render();
    render_sprite_batch();

    // MINIMAP
    // Minimap tiles
    for (int y = 0; y < state.match.map.height; y++) {
        for (int x = 0; x < state.match.map.width; x++) {
            MinimapPixel pixel; 
            Tile tile = map_get_tile(state.match.map, ivec2(x, y));
            if (tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_SAND3) {
                pixel = MINIMAP_PIXEL_SAND;
            } else if (tile.sprite == SPRITE_TILE_WATER) {
                pixel = MINIMAP_PIXEL_WATER;
            } else {
                pixel = MINIMAP_PIXEL_WALL;
            }
            render_minimap_putpixel(ivec2(x, y), pixel);
        }
    }
    // Minimap entities
    for (const Entity& entity : state.match.entities) {
        if (!entity_is_selectable(entity) || !match_is_entity_visible_to_player(state.match, entity, network_get_player_id())) {
            continue;
        }

        MinimapPixel pixel = entity.type == ENTITY_GOLDMINE ? MINIMAP_PIXEL_GOLD : (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + state.match.players[entity.player_id].recolor_id);
        int entity_cell_size = entity_get_data(entity.type).cell_size;
        Rect entity_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        render_minimap_fill_rect(entity_rect, pixel);
    }
    // Minimap remembered entities
    for (auto it : state.match.remembered_entities[state.match.players[network_get_player_id()].team]) {
        Rect entity_rect = (Rect) {
            .x = it.second.cell.x, .y = it.second.cell.y,
            .w = it.second.cell_size, .h = it.second.cell_size
        };
        MinimapPixel pixel = it.second.type == ENTITY_GOLDMINE ? MINIMAP_PIXEL_GOLD : (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + it.second.recolor_id);
        render_minimap_fill_rect(entity_rect, pixel);
    }
    // Minimap fog of war
    for (int y = 0; y < state.match.map.height; y++) {
        for (int x = 0; x < state.match.map.width; x++) {
            int fog_value = match_get_fog(state.match, player_team, ivec2(x, y));
            if (fog_value > 0) {
                continue;
            }

            render_minimap_putpixel(ivec2(x, y), fog_value == FOG_HIDDEN ? MINIMAP_PIXEL_OFFBLACK : MINIMAP_PIXEL_OFFBLACK_TRANSPARENT);
        }
    }
    // Minimap alerts
    for (const Alert& alert : state.alerts) {
        if (alert.timer <= UI_ALERT_LINGER_DURATION) {
            continue;
        }

        int alert_timer = alert.timer - UI_ALERT_LINGER_DURATION;
        int alert_rect_margin = 3 + (alert_timer <= 60 
                                        ? 0
                                        : ((alert_timer - 60) / 3));
        Rect alert_rect = (Rect) {
            .x = alert.cell.x - alert_rect_margin,
            .y = alert.cell.y - alert_rect_margin,
            .w = alert.cell_size + 1 + (alert_rect_margin * 2),
            .h = alert.cell_size + 1 + (alert_rect_margin * 2),
        };
        // We want this on the fog layer because the minimap rect might go into the fog
        render_minimap_draw_rect(alert_rect, alert.pixel);
    }
    // Minimap camera rect
    Rect camera_rect = (Rect) {
        .x = state.camera_offset.x / TILE_SIZE,
        .y = state.camera_offset.y / TILE_SIZE,
        .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
        .h = ((SCREEN_HEIGHT - MATCH_UI_HEIGHT) / TILE_SIZE) - 1
    };
    render_minimap_draw_rect(camera_rect, MINIMAP_PIXEL_WHITE);
    render_minimap(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state.match.map.width, state.match.map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));
}

// RENDER FUNCTIONS

uint16_t match_ui_get_render_layer(uint16_t elevation, RenderLayer layer) {
    return (elevation * RENDER_LAYER_COUNT) + layer;
}

SpriteName match_ui_get_entity_select_ring(EntityType type, bool attacking) {
    if (type == ENTITY_GOLDMINE) {
        return SPRITE_SELECT_RING_GOLDMINE;
    }
    if (type == ENTITY_LANDMINE) {
        return attacking ? SPRITE_SELECT_RING_LANDMINE_ATTACK : SPRITE_SELECT_RING_LANDMINE;
    }

    SpriteName select_ring;
    int entity_cell_size = entity_get_data(type).cell_size;
    if (entity_is_unit(type)) {
        select_ring = (SpriteName)(SPRITE_SELECT_RING_UNIT + ((entity_cell_size - 1) * 2));
    } else {
        select_ring = (SpriteName)(SPRITE_SELECT_RING_BUILDING_SIZE2 + ((entity_cell_size - 2) * 2));
    }
    if (attacking) {
        select_ring = (SpriteName)(select_ring + 1);
    }
    return select_ring;
}

int match_ui_ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high) {
    RenderSpriteParams pivot = params[high];
    int i = low - 1;

    for (int j = low; j <= high - 1; j++) {
        if (params[j].position.y < pivot.position.y) {
            i++;
            RenderSpriteParams temp = params[j];
            params[j] = params[i];
            params[i] = temp;
        }
    }

    RenderSpriteParams temp = params[high];
    params[high] = params[i + 1];
    params[i + 1] = temp;

    return i + 1;
}

void match_ui_ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high) {
    if (low < high) {
        int partition_index = match_ui_ysort_render_params_partition(params, low, high);
        match_ui_ysort_render_params(params, low, partition_index - 1);
        match_ui_ysort_render_params(params, partition_index + 1, high);
    }
}

void match_ui_render_healthbar(const RenderHealthbarParams& params) {
    Rect healthbar_rect = (Rect) { 
        .x = params.position.x, 
        .y = params.position.y, 
        .w = params.size.x, 
        .h = params.size.y 
    };

    // Cull the healthbar
    if (!healthbar_rect.intersects(SCREEN_RECT)) {
        return;
    }

    Rect healthbar_subrect = healthbar_rect;
    healthbar_subrect.w = (healthbar_rect.w * params.amount) / params.max;
    
    RenderColor healthbar_color;
    if (params.type == RENDER_GARRISONBAR) {
        healthbar_color = RENDER_COLOR_WHITE;
    } else if (healthbar_subrect.w <= healthbar_rect.w / 3) {
        healthbar_color = RENDER_COLOR_RED;
    } else {
        healthbar_color = RENDER_COLOR_GREEN;
    }

    render_fill_rect(healthbar_subrect, healthbar_color);
    render_draw_rect(healthbar_rect, RENDER_COLOR_OFFBLACK);

    if (params.type == RENDER_GARRISONBAR) {
        for (int line_index = 1; line_index < params.max; line_index++) {
            int line_x = healthbar_rect.x + ((healthbar_rect.w * line_index) / params.max);
            render_line(ivec2(line_x, healthbar_rect.y), ivec2(line_x, healthbar_rect.y + healthbar_rect.h - 1), RENDER_COLOR_OFFBLACK);
        }
    }
}
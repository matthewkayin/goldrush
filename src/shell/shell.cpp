#include "shell.h"

#include "core/logger.h"
#include "network/network.h"
#include <algorithm>

// Timing
static const uint32_t TURN_OFFSET = 4;
static const uint32_t TURN_DURATION = 4;

// Status
static const uint32_t STATUS_DURATION = 60;

// Menu
static const ivec2 MENU_BUTTON_POSITION = ivec2(2, 2);
static const int MENU_WIDTH = 300;
static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (MENU_WIDTH / 2), .y = 128,
    .w = MENU_WIDTH, .h = 288
};

// Chat
static const size_t CHAT_MAX_LENGTH = 64;
static const uint32_t CHAT_MESSAGE_DURATION = 180;
static const uint32_t CHAT_MAX_LINES = 8;
static const uint32_t CHAT_CURSOR_BLINK_DURATION = 30;

// UI panel rects
static const Rect BOTTOM_PANEL_RECT = (Rect) {
    .x = 272, .y = SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT,
    .w = 744, .h = MATCH_SHELL_UI_HEIGHT
};
static const Rect BUTTON_PANEL_RECT = (Rect) {
    .x = BOTTOM_PANEL_RECT.x + BOTTOM_PANEL_RECT.w,
    .y = SCREEN_HEIGHT - 212,
    .w = 264, .h = 212
};
static const Rect REPLAY_PANEL_RECT = (Rect) {
    .x = BOTTOM_PANEL_RECT.x + BOTTOM_PANEL_RECT.w,
    .y = SCREEN_HEIGHT - 232,
    .w = 264, .h = 232
};
static const ivec2 WANTED_SIGN_POSITION = ivec2(BUTTON_PANEL_RECT.x + 62, BUTTON_PANEL_RECT.y + 18);

// Replay
static const double UPDATE_DURATION = 1.0 / UPDATES_PER_SECOND;
static const uint32_t REPLAY_CHECKPOINT_FREQ = 32U;
static const uint32_t REPLAY_FOG_NONE = 0U;
static const uint32_t REPLAY_FOG_EVERYONE = 1U;

// Camera
static const int CAMERA_DRAG_MARGIN = 4;

// Selection
static const uint32_t MATCH_SHELL_DOUBLE_CLICK_DURATION = 30U;

// Rects
static const Rect SCREEN_RECT = (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
static const Rect MINIMAP_RECT = (Rect) { .x = 8, .y = SCREEN_HEIGHT - 264, .w = 256, .h = 256 };

// Sound
static const int SOUND_LISTEN_MARGIN = 128;
static const Rect SOUND_LISTEN_RECT = (Rect) {
    .x = -SOUND_LISTEN_MARGIN,
    .y = -SOUND_LISTEN_MARGIN,
    .w = SCREEN_WIDTH + (SOUND_LISTEN_MARGIN * 2),
    .h = SCREEN_HEIGHT + (SOUND_LISTEN_MARGIN * 2),
};
static const uint32_t SOUND_COOLDOWN_DURATION = 5;

MatchShellState* match_shell_base_init() {
    MatchShellState* state = new MatchShellState();

    state->mode = MATCH_SHELL_MODE_NONE;
    state->ui = ui_init();
    state->options_menu.mode = OPTIONS_MENU_CLOSED;

    // Simulation timers
    state->match_timer = 0;
    state->disconnect_timer = 0;
    state->is_paused = false;

    // Camera
    state->camera_offset = ivec2(0, 0);
    state->is_minimap_dragging = false;
    for (int index = 0; index < MATCH_SHELL_CAMERA_HOTKEY_COUNT; index++) {
        state->camera_hotkeys[index] = ivec2(-1, -1);
    }

    // Selection
    state->select_origin = ivec2(-1, -1);
    state->double_click_timer = 0;
    state->control_group_selected = MATCH_SHELL_CONTROL_GROUP_NONE; 
    state->control_group_double_tap_timer = 0;

    // Status
    state->status_timer = 0;

    // Chat
    state->chat_cursor_visible = false;
    state->chat_cursor_blink_timer = 0;

    // Alerts
    state->latest_alert_cell = ivec2(-1, -1);

    // Sound
    memset(state->sound_cooldown_timers, 0, sizeof(state->sound_cooldown_timers));

    // Animations
    state->rally_flag_animation = animation_create(ANIMATION_RALLY_FLAG);
    state->building_fire_animation = animation_create(ANIMATION_FIRE_BURN);

    // Gold amounts
    memset(state->displayed_gold_amounts, 0, sizeof(state->displayed_gold_amounts));

    // Replay file
    state->replay_file = NULL;

    return state;
}

MatchShellState* match_shell_init(int lcg_seed, Noise& noise) {
    MatchShellState* state = match_shell_base_init();

    state->mode = MATCH_SHELL_MODE_NOT_STARTED;

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
    state->replay_file = replay_file_open(lcg_seed, noise, players);
    state->replay_mode = false;
    state->match_state = match_init(lcg_seed, noise, players);

    // Init input queues
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE) {
            continue;
        }

        for (uint8_t i = 0; i < TURN_OFFSET - 1; i++) {
            state->inputs[player_id].push({ (MatchInput) { .type = MATCH_INPUT_NONE } });
        }
    }

    // Init camera
    for (const Entity& entity : state->match_state.entities) {
        if (entity.type == ENTITY_MINER && entity.player_id == network_get_player_id()) {
            match_shell_center_camera_on_cell(state, entity.cell);
            break;
        }
    }

    // TODO: init bots

    return state;
}

static int match_shell_load_replay_checkpoints(void* state_ptr) {
    MatchShellState* state = (MatchShellState*)state_ptr;

    while (state->replay_loading_match_timer < match_shell_replay_end_of_tape(state)) {
        // Match update
        match_shell_replay_begin_turn(state, state->replay_loading_match_state, state->replay_loading_match_timer);
        match_update(state->replay_loading_match_state);
        state->replay_loading_match_state.events.clear();

        // Increment timer and save replay checkpoint
        SDL_LockMutex(state->replay_loading_mutex);
        state->replay_loading_match_timer++;
        if (state->replay_loading_match_timer % REPLAY_CHECKPOINT_FREQ == 0) {
            state->replay_checkpoints.push_back(state->replay_loading_match_state);
        }
        SDL_UnlockMutex(state->replay_loading_mutex);

        // Check for early exit
        if (SDL_TryLockMutex(state->replay_loading_early_exit_mutex)) {
            bool should_break = state->replay_loading_early_exit;
            SDL_UnlockMutex(state->replay_loading_early_exit_mutex);
            if (should_break) {
                break;
            }
        } 
    }

    return 0;
}

MatchShellState* replay_shell_init(const char* replay_path) {
    MatchShellState* state = match_shell_base_init();

    state->replay_mode = true;
    state->replay_ui = ui_init();

    // Read replay file
    if (!replay_file_read(replay_path, &state->match_state, state->replay_inputs, &state->replay_chatlog)) {
        delete state;
        return NULL;
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        log_debug("Player %u input count %u", player_id, state->replay_inputs[player_id].size());
    }

    state->replay_checkpoints.reserve(((match_shell_replay_end_of_tape(state) + 1) / REPLAY_CHECKPOINT_FREQ) + 1);
    state->replay_checkpoints.push_back(state->match_state);

    state->replay_loading_mutex = SDL_CreateMutex();
    state->replay_loading_match_timer = 0;
    state->replay_loaded_match_timer = 0;

    state->replay_loading_early_exit_mutex = SDL_CreateMutex();
    state->replay_loading_early_exit = false;

    state->replay_loading_match_state = state->match_state;
    state->replay_loading_thread = SDL_CreateThread(match_shell_load_replay_checkpoints, "replay_loading_thread", state);
    if (!state->replay_loading_thread) {
        log_error("Error creating loading thread %s", SDL_GetError());
        delete state;
        return NULL;
    }

    match_shell_replay_scrub(state, 0);

    // Init replay fog picker
    state->replay_fog_index = REPLAY_FOG_EVERYONE;
    state->replay_fog_player_ids.push_back(PLAYER_NONE);
    state->replay_fog_texts.push_back("None");
    state->replay_fog_player_ids.push_back(PLAYER_NONE);
    state->replay_fog_texts.push_back("Everyone");
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state->match_state.players[player_id].active) {
            continue;
        }
        state->replay_fog_player_ids.push_back(player_id);
        state->replay_fog_texts.push_back(state->match_state.players[player_id].name);
    }

    // Init camera
    for (const Entity& entity : state->match_state.entities) {
        if (entity.type == ENTITY_MINER && entity.player_id == 0) {
            match_shell_center_camera_on_cell(state, entity.cell);
            break;
        }
    }

    return state;
}

// NETWORK EVENT

void match_shell_handle_network_event(MatchShellState* state, NetworkEvent event) {
    switch (event.type) {
        case NETWORK_EVENT_INPUT: {
            // Deserialize input
            std::vector<MatchInput> inputs;

            const uint8_t* in_buffer = event.input.in_buffer;
            size_t in_buffer_head = 1; // Advance the head by once since the first byte will contain the network message type

            while (in_buffer_head < event.input.in_buffer_length) {
                inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
            }

            state->inputs[event.input.player_id].push(inputs);
            break;
        }
        case NETWORK_EVENT_CHAT: {
            match_shell_add_chat_message(state, event.chat.player_id, event.chat.message);
            break;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            match_shell_handle_player_disconnect(state, event.player_disconnected.player_id);
            break;
        }
        default:
            break;
    }
}

// UPDATE

void match_shell_update(MatchShellState* state) {
    if (state->mode == MATCH_SHELL_MODE_LEAVE_MATCH || state->mode == MATCH_SHELL_MODE_EXIT_PROGRAM) {
        return;
    }

    // Await match start
    if (state->mode == MATCH_SHELL_MODE_NOT_STARTED) {
        if (network_get_player(network_get_player_id()).status == NETWORK_PLAYER_STATUS_NOT_READY) {
            network_set_player_ready(true);
        }

        // Check that all players are ready
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NOT_READY) {
                return;
            }
        }
        // If we reached here, then all players are ready
        state->mode = MATCH_SHELL_MODE_NONE;
        log_info("Match started.");
    }

    // Open menu
    const SpriteInfo& menu_button_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_BURGER);
    Rect menu_button_rect = (Rect) {
        .x = MENU_BUTTON_POSITION.x, .y = MENU_BUTTON_POSITION.y,
        .w = menu_button_sprite_info.frame_width * 2, .h = menu_button_sprite_info.frame_height * 2
    };
    if (!(state->is_minimap_dragging || match_shell_is_selecting(state)) && 
            ((menu_button_rect.has_point(input_get_mouse_position()) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) || input_is_action_just_pressed(INPUT_ACTION_MATCH_MENU))) {
        if (state->mode == MATCH_SHELL_MODE_MENU || state->mode == MATCH_SHELL_MODE_MENU_SURRENDER || state->mode == MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP) {
            state->options_menu.mode = OPTIONS_MENU_CLOSED;
            state->mode = MATCH_SHELL_MODE_NONE;
        } else if (state->mode != MATCH_SHELL_MODE_MATCH_OVER_DEFEAT && state->mode != MATCH_SHELL_MODE_MATCH_OVER_VICTORY) {
            state->mode = MATCH_SHELL_MODE_MENU;
            input_stop_text_input();
        }
        sound_play(SOUND_UI_CLICK);

        return;
    }

    // Menu
    ui_begin(state->ui);
    if (match_shell_is_in_menu(state)) {
        if (state->is_minimap_dragging) {
            state->is_minimap_dragging = false;
        }

        state->ui.input_enabled = state->options_menu.mode == OPTIONS_MENU_CLOSED;
        ui_frame_rect(state->ui, MENU_RECT);

        const char* header_text = match_shell_get_menu_header_text(state);
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_GOLD, header_text);
        ivec2 text_pos = ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (text_size.x / 2), MENU_RECT.y + 20);
        ui_element_position(state->ui, text_pos);
        ui_text(state->ui, FONT_WESTERN8_GOLD, header_text);

        ivec2 column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 64);
        if (state->mode != MATCH_SHELL_MODE_MENU) {
            column_position.y += 22;
        }
        ui_begin_column(state->ui, column_position, 10);
            ivec2 button_size = ui_button_size("Return to Menu");
            if (state->mode == MATCH_SHELL_MODE_MENU) {
                if (ui_button(state->ui, "Leave Match", button_size, true)) {
                    if (match_shell_is_surrender_required_to_leave(state)) {
                        state->mode = MATCH_SHELL_MODE_MENU_SURRENDER;
                    } else {
                        match_shell_leave_match(state, false);
                    }
                }
                if (ui_button(state->ui, "Exit Program", button_size, true)) {
                    if (match_shell_is_surrender_required_to_leave(state)) {
                        state->mode = MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP;
                    } else {
                        match_shell_leave_match(state, true);
                    }
                }
                if (ui_button(state->ui, "Options", button_size, true)) {
                    state->options_menu = options_menu_open();
                }
                if (ui_button(state->ui, "Back", button_size, true)) {
                    state->mode = MATCH_SHELL_MODE_NONE;
                }
            } else if (state->mode == MATCH_SHELL_MODE_MENU_SURRENDER || state->mode == MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP) {
                if (ui_button(state->ui, "Yes", button_size, true)) {
                    match_shell_leave_match(state, state->mode == MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP);
                }
                if (ui_button(state->ui, "Back", button_size, true)) {
                    state->mode = MATCH_SHELL_MODE_MENU;
                }
            } else if (state->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY || state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT) {
                if (ui_button(state->ui, "Keep Playing", button_size, true)) {
                    state->mode = MATCH_SHELL_MODE_NONE;
                }
                if (ui_button(state->ui, "Return to Menu", button_size, true)) {
                    match_shell_leave_match(state, false);
                }
                if (ui_button(state->ui, "Exit Program", button_size, true)) {
                    match_shell_leave_match(state, true);
                }
            }
        ui_end_container(state->ui);

        if (state->options_menu.mode != OPTIONS_MENU_CLOSED) {
            options_menu_update(state->options_menu, state->ui);
        }
    } 
    
    // Replay UI
    if (state->replay_mode) {
        ui_begin(state->replay_ui);
        state->replay_ui.input_enabled = !match_shell_is_in_menu(state);

        // Loading thread handling
        if (SDL_TryLockMutex(state->replay_loading_mutex)) {
            state->replay_loaded_match_timer = state->replay_loading_match_timer;
            SDL_UnlockMutex(state->replay_loading_mutex);
        }
        if (state->replay_loading_thread != NULL && SDL_GetThreadState(state->replay_loading_thread) == SDL_THREAD_COMPLETE) {
            // Clean up the thread
            SDL_WaitThread(state->replay_loading_thread, NULL);
            state->replay_loading_thread = NULL;
        }

        ui_begin_column(state->replay_ui, ivec2(BUTTON_PANEL_RECT.x + 16, BUTTON_PANEL_RECT.y + 8), 4);
            ui_element_size(state->replay_ui, ivec2(0, 32));
            ui_begin_row(state->replay_ui, ivec2(0, 0), 8);
                ui_element_position(state->replay_ui, ivec2(0, 4));
                ui_text(state->replay_ui, FONT_HACK_WHITE, "Fog:");

                ui_element_position(state->replay_ui, ivec2(render_get_text_size(FONT_HACK_WHITE, "Fog:").x, 0));
                ui_dropdown(state->replay_ui, UI_DROPDOWN_MINI, &state->replay_fog_index, state->replay_fog_texts, false);
            ui_end_container(state->replay_ui);

            uint32_t position = state->match_timer;
            if (ui_slider(state->replay_ui, &position, &state->replay_loaded_match_timer, 0, match_shell_replay_end_of_tape(state), UI_SLIDER_DISPLAY_NO_VALUE)) {
                state->selection.clear();
                state->chat.clear();
                match_shell_replay_scrub(state, position);
            }

            ui_begin_row(state->replay_ui, ivec2(0, 0), 12);
                if (ui_sprite_button(state->replay_ui, state->is_paused ? SPRITE_UI_REPLAY_PLAY : SPRITE_UI_REPLAY_PAUSE, false, false) ||
                        input_is_action_just_pressed(INPUT_ACTION_SPACE)) {
                    state->is_paused = !state->is_paused;
                }

                // Time elapsed text
                uint32_t seconds_elapsed = state->match_timer == 0 ? 0 : (uint32_t)((state->match_timer - 1) * UPDATE_DURATION);
                Time time_elapsed = Time::from_seconds(seconds_elapsed);
                uint32_t seconds_total = (uint32_t)((match_shell_replay_end_of_tape(state) - 1) * UPDATE_DURATION);
                Time time_total = Time::from_seconds(seconds_total);
                char time_text[16];
                ui_element_position(state->replay_ui, ivec2(0, 4));
                sprintf(time_text, "%i:%02i:%02i/%i:%02i:%02i", time_elapsed.hours, time_elapsed.minutes, time_elapsed.seconds, time_total.hours, time_total.minutes, time_total.seconds);
                ui_text(state->replay_ui, FONT_HACK_WHITE, time_text);
            ui_end_container(state->replay_ui);
        ui_end_container(state->replay_ui);
    }

    // Camera drag
    if (!match_shell_is_selecting(state) && !state->is_minimap_dragging) {
        ivec2 camera_drag_direction = ivec2(0, 0);
        if (input_get_mouse_position().x < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (input_get_mouse_position().x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (input_get_mouse_position().y < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (input_get_mouse_position().y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        state->camera_offset += camera_drag_direction * option_get_value(OPTION_CAMERA_SPEED);
        match_shell_clamp_camera(state);
    }

    // Begin minimap drag
    if (MINIMAP_RECT.has_point(input_get_mouse_position()) && 
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !match_shell_is_in_menu(state)) {
        state->is_minimap_dragging = true;
    } 

    // End minimap drag
    if (state->is_minimap_dragging && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        state->is_minimap_dragging = false;
    }

    // During minimap drag
    if (state->is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (state->match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state->match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        match_shell_center_camera_on_cell(state, map_pos / TILE_SIZE);
    }

    // Begin selecting
    if (!match_shell_is_mouse_in_ui() && 
            (state->mode == MATCH_SHELL_MODE_NONE || match_shell_is_in_hotkey_submenu(state)) &&
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        state->select_origin = input_get_mouse_position() + state->camera_offset;
    }
    // End selecting
    if (match_shell_is_selecting(state) && input_is_action_just_released(INPUT_ACTION_LEFT_CLICK)) {
        ivec2 world_pos = input_get_mouse_position() + state->camera_offset;
        Rect select_rect = (Rect) {
            .x = std::min(world_pos.x, state->select_origin.x),
            .y = std::min(world_pos.y, state->select_origin.y),
            .w = std::max(1, std::abs(state->select_origin.x - world_pos.x)),
            .h = std::max(1, std::abs(state->select_origin.y - world_pos.y)),
        };

        std::vector<EntityId> selection = match_shell_create_selection(state, select_rect);
        state->select_origin = ivec2(-1, -1);
        state->control_group_selected = MATCH_SHELL_CONTROL_GROUP_NONE;

        // Append selection
        if (input_is_action_pressed(INPUT_ACTION_CTRL)) {
            MatchShellSelectionType current_selection_type = match_shell_get_selection_type(state, state->selection);
            // Don't append selection if selection is not allied units or buildings
            if (!(current_selection_type == MATCH_SHELL_SELECTION_UNITS || current_selection_type == MATCH_SHELL_SELECTION_BUILDINGS)) {
                return;
            }
            // Don't append selection if units are not same type
            if (match_shell_get_selection_type(state, selection) != current_selection_type) {
                return;
            }
            for (EntityId incoming_id : selection) {
                if (std::find(state->selection.begin(), state->selection.end(), incoming_id) == state->selection.end()) {
                    state->selection.push_back(incoming_id);
                }
            }

            match_shell_set_selection(state, state->selection);
            return;
        }

        if (selection.size() == 1) {
            if (state->double_click_timer == 0) {
                state->double_click_timer = MATCH_SHELL_DOUBLE_CLICK_DURATION;
            } else if (state->selection.size() == 1 && state->selection[0] == selection[0] &&
                        state->match_state.entities.get_by_id(state->selection[0]).player_id == network_get_player_id()) {
                EntityType selected_type = state->match_state.entities.get_by_id(state->selection[0]).type;
                selection.clear();

                for (uint32_t entity_index = 0; entity_index < state->match_state.entities.size(); entity_index++) {
                    if (state->match_state.entities[entity_index].type != selected_type || state->match_state.entities[entity_index].player_id != network_get_player_id()) {
                        continue;
                    }

                    Rect entity_rect = entity_get_rect(state->match_state.entities[entity_index]);
                    entity_rect.x -= state->camera_offset.x;
                    entity_rect.y -= state->camera_offset.y;
                    if (SCREEN_RECT.intersects(entity_rect)) {
                        selection.push_back(state->match_state.entities.get_id_of(entity_index));
                    }
                }
            }
        }

        match_shell_set_selection(state, selection);
        return;
    }

    // Update displayed gold amounts
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state->match_state.players[player_id].active) {
            continue;
        } 
        int difference = std::abs((int)state->displayed_gold_amounts[player_id] - (int)state->match_state.players[player_id].gold);
        int step = 1;
        if (difference > 100) {
            step = 10;
        } else if (difference > 10) {
            step = 5;
        } else if (difference > 1) {
            step = 2;
        }
        if (state->displayed_gold_amounts[player_id] < state->match_state.players[player_id].gold) {
            state->displayed_gold_amounts[player_id] += step;
        } else if (state->displayed_gold_amounts[player_id] > state->match_state.players[player_id].gold) {
            state->displayed_gold_amounts[player_id] -= step;
        }
    }

    // Play fire sound effects
    bool is_fire_on_screen = match_shell_is_fire_on_screen(state);
    if (is_fire_on_screen && !sound_is_fire_loop_playing()) {
        sound_begin_fire_loop();
    } else if (!is_fire_on_screen && sound_is_fire_loop_playing()) {
        sound_end_fire_loop();
    }

    // Paused
    if (state->replay_mode && state->match_timer == match_shell_replay_end_of_tape(state)) {
        state->is_paused = true;
    }
    if (state->is_paused) {
        return;
    }

    // Non-replay begin turn
    if (state->match_timer % TURN_DURATION == 0 && !state->replay_mode) {
        bool all_inputs_received = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (!state->match_state.players[player_id].active) {
                continue;
            }

            if (state->inputs[player_id].empty() || state->inputs[player_id].front().empty()) {
                all_inputs_received = false;
                continue;
            }
        }

        if (!all_inputs_received) {
            state->disconnect_timer++;
            return;
        }

        // Reset the disconnect timer if we recevied inputs
        state->disconnect_timer = 0;

        // All inputs received. Begin next turn
        // Handle input
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (!state->match_state.players[player_id].active) {
                replay_file_write_inputs(state->replay_file, player_id, NULL);
                continue;
            }

            replay_file_write_inputs(state->replay_file, player_id, &state->inputs[player_id].front());

            for (const MatchInput& input : state->inputs[player_id].front()) {
                match_handle_input(state->match_state, input);
                if (input.type != MATCH_INPUT_NONE) {
                    char debug_buffer[512];
                    char* out_ptr = debug_buffer;
                    out_ptr += sprintf(out_ptr, "TURN %u PLAYER %u ", state->match_timer / TURN_DURATION, player_id);
                    match_input_print(out_ptr, input);
                    log_info(debug_buffer);
                }
            }
            state->inputs[player_id].pop();
        }

        // Flush input
        if (state->match_state.players[network_get_player_id()].active) {
            // Always send at least one input per turn
            if (state->input_queue.empty()) {
                state->input_queue.push_back((MatchInput) { .type = MATCH_INPUT_NONE });
            } 

            // Serialize the inputs
            uint8_t out_buffer[NETWORK_INPUT_BUFFER_SIZE];
            size_t out_buffer_length = 1;
            for (const MatchInput& input : state->input_queue) {
                match_input_serialize(out_buffer, out_buffer_length, input);
                GOLD_ASSERT(out_buffer_length <= NETWORK_INPUT_BUFFER_SIZE);
            }
            state->inputs[network_get_player_id()].push(state->input_queue);
            state->input_queue.clear();

            // Send inputs to other players
            network_send_input(out_buffer, out_buffer_length);
        }
    }

    // Replay begin turn
    if (state->match_timer % TURN_DURATION == 0 && state->replay_mode) {
        match_shell_replay_begin_turn(state, state->match_state, state->match_timer);

        // Chatlog
        if (state->match_timer % TURN_DURATION == 0) {
            uint32_t turn_index = state->match_timer / TURN_DURATION;
            for (const ReplayChatMessage& message : state->replay_chatlog) {
                if (message.turn == turn_index) {
                    match_shell_add_chat_message(state, message.player_id, message.message);
                }
            }
        }
    }

    // Increment match timer
    state->match_timer++;

    // Handle input
}

// STATE QUERIES

bool match_shell_is_mouse_in_ui() {
    ivec2 mouse_position = input_get_mouse_position();
    return (mouse_position.y >= SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) ||
           (mouse_position.x <= 272 && mouse_position.y >= SCREEN_HEIGHT - 272) ||
           (mouse_position.x >= SCREEN_WIDTH - 264 && mouse_position.y >= SCREEN_HEIGHT - 212);
}

bool match_shell_is_selecting(const MatchShellState* state) {
    return state->select_origin.x != -1;
}

bool match_shell_is_in_menu(const MatchShellState* state) {
    return state->mode == MATCH_SHELL_MODE_MENU ||
            state->mode == MATCH_SHELL_MODE_MENU_SURRENDER ||
            state->mode == MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP ||
            state->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY ||
            state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT;
}

bool match_shell_is_in_hotkey_submenu(const MatchShellState* state) {
    return state->mode == MATCH_SHELL_MODE_BUILD || state->mode == MATCH_SHELL_MODE_BUILD2;
}

bool match_shell_is_targeting(const MatchShellState* state) {
    return state->mode >= MATCH_SHELL_MODE_TARGET_ATTACK && state->mode < MATCH_SHELL_MODE_CHAT;
}

// CAMERA

void match_shell_clamp_camera(MatchShellState* state) {
    state->camera_offset.x = std::clamp(state->camera_offset.x, 0, (state->match_state.map.width * TILE_SIZE) - SCREEN_WIDTH);
    state->camera_offset.y = std::clamp(state->camera_offset.y, 0, (state->match_state.map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);
}

void match_shell_center_camera_on_cell(MatchShellState* state, ivec2 cell) {
    state->camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state->camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2);
    match_shell_clamp_camera(state);
}

// SELECTION

std::vector<EntityId> match_shell_create_selection(const MatchShellState* state, Rect select_rect) {
    std::vector<EntityId> selection;

    // Select player units
    for (uint32_t index = 0; index < state->match_state.entities.size(); index++) {
        const Entity& entity = state->match_state.entities[index];
        // Select only selectable units
        if (!entity_is_unit(entity.type) || !entity_is_selectable(entity)) {
            continue;
        }
        // In non-replay mode, select only player units
        if (!state->replay_mode && entity.player_id != network_get_player_id()) {
            continue;
        }
        // In replay mode, select only visible units
        if (state->replay_mode && !match_shell_is_entity_visible(state, entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state->match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select player buildings
    for (uint32_t index = 0; index < state->match_state.entities.size(); index++) {
        const Entity& entity = state->match_state.entities[index];
        // Select only selectable buildings
        if (!entity_is_building(entity.type) || !entity_is_selectable(entity)) {
            continue;
        }
        // In non-replay mode, select only player buildings
        if (!state->replay_mode && entity.player_id != network_get_player_id()) {
            continue;
        }
        // In replay mode, select only visible units
        if (state->replay_mode && !match_shell_is_entity_visible(state, entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state->match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    if (!state->replay_mode) {
        // Select enemy units
        for (uint32_t index = 0; index < state->match_state.entities.size(); index++) {
            const Entity& entity = state->match_state.entities[index];
            if (entity.player_id == network_get_player_id() ||
                    !entity_is_unit(entity.type) ||
                    !entity_is_selectable(entity) ||
                    !match_shell_is_entity_visible(state, entity)) {
                continue;
            }

            Rect entity_rect = entity_get_rect(entity);
            if (entity_rect.intersects(select_rect)) {
                selection.push_back(state->match_state.entities.get_id_of(index));
                return selection;
            }
        }

        // Select enemy buildings
        for (uint32_t index = 0; index < state->match_state.entities.size(); index++) {
            const Entity& entity = state->match_state.entities[index];
            if (entity.player_id == network_get_player_id() ||
                    entity_is_unit(entity.type) ||
                    !entity_is_selectable(entity) ||
                    !match_shell_is_entity_visible(state, entity)) {
                continue;
            }

            Rect entity_rect = entity_get_rect(entity);
            if (entity_rect.intersects(select_rect)) {
                selection.push_back(state->match_state.entities.get_id_of(index));
                return selection;
            }
        }
    }

    // Select gold mines
    for (uint32_t index = 0; index < state->match_state.entities.size(); index++) {
        const Entity& entity = state->match_state.entities[index];
        if (entity.type != ENTITY_GOLDMINE ||
                !match_shell_is_entity_visible(state, entity)) {
            continue;
        }

        Rect entity_rect = entity_get_rect(entity);
        if (entity_rect.intersects(select_rect)) {
            selection.push_back(state->match_state.entities.get_id_of(index));
            return selection;
        }
    }

    // Returns an empty selection
    return selection;
}

void match_shell_set_selection(MatchShellState* state, std::vector<EntityId>& selection) {
    state->selection = selection;

    for (uint32_t selection_index = 0; selection_index < state->selection.size(); selection_index++) {
        uint32_t entity_index = state->match_state.entities.get_index_of(state->selection[selection_index]);
        if (entity_index == INDEX_INVALID || !entity_is_selectable(state->match_state.entities[entity_index])) {
            state->selection.erase(state->selection.begin() + selection_index);
            selection_index--;
            continue;
        }
    }

    while (state->selection.size() > SELECTION_LIMIT) {
        state->selection.pop_back();
    }

    state->mode = MATCH_SHELL_MODE_NONE;
}

MatchShellSelectionType match_shell_get_selection_type(const MatchShellState* state, const std::vector<EntityId>& selection) {
    if (selection.empty()) {
        return MATCH_SHELL_SELECTION_NONE;
    }

    const Entity& entity = state->match_state.entities.get_by_id(selection[0]);
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_SHELL_SELECTION_UNITS;
        } else {
            return MATCH_SHELL_SELECTION_ENEMY_UNIT;
        }
    } else if (entity_is_building(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return MATCH_SHELL_SELECTION_BUILDINGS;
        } else {
            return MATCH_SHELL_SELECTION_ENEMY_BUILDING;
        }
    } else {
        return MATCH_SHELL_SELECTION_GOLD;
    }
}

// VISION

bool match_shell_is_entity_visible(const MatchShellState* state, const Entity& entity) {
    if (state->replay_mode) {
        if (state->replay_fog_index == REPLAY_FOG_NONE) {
            return 1;
        }
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if ((state->replay_fog_player_ids[state->replay_fog_index] == PLAYER_NONE ||
                    state->replay_fog_player_ids[state->replay_fog_index] == player_id) &&
                    entity_is_visible_to_player(state->match_state, entity, player_id)) {
                return true;
            }
        }

        return false;
    } else {
        #ifdef GOLD_DEBUG
            if (state->debug_fog == DEBUG_FOG_BOT_VISION) {
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                            entity_is_visible_to_player(state->match_state, entity, player_id)) {
                        return true;
                    }
                }
            } else if (state->debug_fog == DEBUG_FOG_DISABLED) {
                return entity.garrison_id == ID_NULL;
            }
        #endif
        return entity_is_visible_to_player(state->match_state, entity, network_get_player_id());
    }
}

bool match_shell_is_cell_rect_revealed(const MatchShellState* state, ivec2 cell, int cell_size) {
    if (state->replay_mode) {
        if (state->replay_fog_index == REPLAY_FOG_NONE) {
            return true;
        }
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if ((state->replay_fog_player_ids[state->replay_fog_index] == PLAYER_NONE ||
                    state->replay_fog_player_ids[state->replay_fog_index] == player_id) &&
                    match_is_cell_rect_revealed(state->match_state, state->match_state.players[player_id].team, cell, cell_size)) {
                return true;
            }
        }

        return false;
    } else {
        #ifdef GOLD_DEBUG
            if (state->debug_fog == DEBUG_FOG_BOT_VISION) {
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT &&
                            match_is_cell_rect_revealed(state->match_state, state->match_state.players[player_id].team, cell, cell_size)) {
                        return true;
                    }
                }
            } else if (state->debug_fog == DEBUG_FOG_DISABLED) {
                return true;
            }
        #endif
        return match_is_cell_rect_revealed(state->match_state, state->match_state.players[network_get_player_id()].team, cell, cell_size);
    }
}

int match_shell_get_fog(const MatchShellState* state, ivec2 cell) {
    if (state->replay_mode) {
        if (state->replay_fog_index == REPLAY_FOG_NONE) {
            return 1;
        }
        int fog_value = FOG_HIDDEN;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (state->replay_fog_player_ids[state->replay_fog_index] == PLAYER_NONE ||
                    state->replay_fog_player_ids[state->replay_fog_index] == player_id) {
                int player_fog_value = match_get_fog(state->match_state, state->match_state.players[player_id].team, cell);
                // If at least one player has revealed fog, then return a revealed fog value
                if (player_fog_value > 0) {
                    return 1;
                }

                // If set the returned value to explored, but keep iterating in case we get a revealed value
                if (player_fog_value == FOG_EXPLORED) {
                    fog_value = FOG_EXPLORED;
                }
            }
        }

        return fog_value;
    } else {
        #ifdef GOLD_DEBUG
            if (state->debug_fog == DEBUG_FOG_BOT_VISION) {
                int fog_value = FOG_HIDDEN;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if ((network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT || network_get_player_id() == player_id)) {
                        int player_fog_value = match_get_fog(state->match_state, state->match_state.players[player_id].team, cell);

                        // If at least one player has revealed fog, then return a revealed fog value
                        if (player_fog_value > 0) {
                            return 1;
                        }
                        // If set the returned value to explored, but keep iterating in case we get a revealed value
                        if (player_fog_value == FOG_EXPLORED) {
                            fog_value = FOG_EXPLORED;
                        }
                    }
                }
                return fog_value;
            } else if (state->debug_fog == DEBUG_FOG_DISABLED) {
                return 1;
            }
        #endif
        return match_get_fog(state->match_state, state->match_state.players[network_get_player_id()].team, cell);
    }
}

// CHAT

void match_shell_add_chat_message(MatchShellState* state, uint8_t player_id, const char* message) {
    ChatMessage chat_message = (ChatMessage) {
        .message = std::string(message),
        .timer = CHAT_MESSAGE_DURATION,
        .player_id = player_id
    };
    if (state->chat.size() == CHAT_MAX_LINES) {
        state->chat.erase(state->chat.begin());
    }
    state->chat.push_back(chat_message);

    if (!state->replay_mode) {
        replay_file_write_chat(state->replay_file, player_id, state->match_timer / TURN_DURATION, message);
    }
}

void match_shell_handle_player_disconnect(MatchShellState* state, uint8_t player_id) {
    char message[128];
    sprintf(message, "%s left the game.", network_get_player(player_id).name);
    match_shell_add_chat_message(state, PLAYER_NONE, message);

    // Show the victory banner to other players when this player leaves,
    // but only if the player was still active. If they are not active, it means they've 
    // been defeated already, so don't show the victory banner for that player
    if (state->match_state.players[player_id].active) {
        state->match_state.players[player_id].active = false;
        if (state->match_state.players[network_get_player_id()].active && !match_shell_is_at_least_one_opponent_in_match(state)) {
            state->mode = MATCH_SHELL_MODE_MATCH_OVER_VICTORY;
        }
    }

}

// MENU

const char* match_shell_get_menu_header_text(const MatchShellState* state) {
    switch (state->mode) {
        case MATCH_SHELL_MODE_MENU:
            return "Game Menu";
        case MATCH_SHELL_MODE_MENU_SURRENDER:
        case MATCH_SHELL_MODE_MENU_SURRENDER_TO_DESKTOP:
            return "Surrender?";
        case MATCH_SHELL_MODE_MATCH_OVER_VICTORY:
            return "Victory!";
        case MATCH_SHELL_MODE_MATCH_OVER_DEFEAT:
            return "Defeat!";
        default:
            return "";
    }
}

// FIRE

bool match_shell_is_fire_on_screen(const MatchShellState* state) {
    // Return true if any fire cells intersect the screen
    for (const Fire& fire : state->match_state.fires) {
        if (!match_shell_is_cell_rect_revealed(state, fire.cell, 1)) {
            continue;
        }
        if (map_get_cell(state->match_state.map, CELL_LAYER_GROUND, fire.cell).type != CELL_EMPTY) {
            continue;
        }
        Rect fire_rect = (Rect) {
            .x = (fire.cell.x * TILE_SIZE) - state->camera_offset.x,
            .y = (fire.cell.y * TILE_SIZE) - state->camera_offset.y,
            .w = TILE_SIZE,
            .h = TILE_SIZE
        };
        if (SOUND_LISTEN_RECT.intersects(fire_rect)) {
            return true;
        }
    }

    // Return true if any burning buildings intersect the screen
    for (const Entity& entity : state->match_state.entities) {
        if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
            continue;
        }
        if (!entity_is_building(entity.type) || !entity_check_flag(entity, ENTITY_FLAG_ON_FIRE)) {
            continue;
        }
        if (!match_shell_is_entity_visible(state, entity)) {
            continue;
        }

        RenderSpriteParams params = match_shell_create_entity_render_params(state, entity);
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(state->match_state, entity));
        Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width, .h = sprite_info.frame_height
        };

        // Entity is on fire and on screen
        if (SOUND_LISTEN_RECT.intersects(render_rect)) {
            return true;
        }
    }

    return false;
}

// REPLAY

void match_shell_replay_begin_turn(MatchShellState* state, MatchState& match_state, uint32_t match_timer) {
    if (match_timer % TURN_DURATION == 0) {
        uint32_t turn_index = match_timer / TURN_DURATION;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            for (const MatchInput& input : state->replay_inputs[player_id][turn_index]) {
                match_handle_input(match_state, input);
            }
        }
    }
}

void match_shell_replay_scrub(MatchShellState* state, uint32_t position) {
    if (state->match_timer == position || position > state->replay_loaded_match_timer) {
        return;
    }

    if (position < state->match_timer || (position > state->match_timer && position - state->match_timer > REPLAY_CHECKPOINT_FREQ)) {
        uint32_t nearest_checkpoint = position / REPLAY_CHECKPOINT_FREQ;
        SDL_LockMutex(state->replay_loading_mutex);
        state->match_state = state->replay_checkpoints[nearest_checkpoint];
        SDL_UnlockMutex(state->replay_loading_mutex);
        state->match_timer = nearest_checkpoint * REPLAY_CHECKPOINT_FREQ;
    }

    while (state->match_timer < position) {
        match_shell_replay_begin_turn(state, state->match_state, state->match_timer);
        match_update(state->match_state);
        state->match_state.events.clear();
        state->match_timer++;
    }
}

size_t match_shell_replay_end_of_tape(const MatchShellState* state) {
    return (state->replay_inputs[0].size() * 4) - 1;
}

// LEAVE MATCH

bool match_shell_is_at_least_one_opponent_in_match(const MatchShellState* state) {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->match_state.players[network_get_player_id()].team == state->match_state.players[player_id].team) {
            continue;
        }
        if (state->match_state.players[player_id].active) {
            return true;
        }
    }

    return false;
}

bool match_shell_is_surrender_required_to_leave(const MatchShellState* state) {
    if (state->replay_mode) {
        return false;
    }
    return state->match_state.players[network_get_player_id()].active && match_shell_is_at_least_one_opponent_in_match(state);
}

void match_shell_leave_match(MatchShellState* state, bool exit_program) {
    if (state->replay_mode) {
        SDL_LockMutex(state->replay_loading_early_exit_mutex);
        state->replay_loading_early_exit = true;
        SDL_UnlockMutex(state->replay_loading_early_exit_mutex);

        SDL_WaitThread(state->replay_loading_thread, NULL);

        SDL_DestroyMutex(state->replay_loading_mutex);
        SDL_DestroyMutex(state->replay_loading_early_exit_mutex);
    } else {
        network_disconnect();
        replay_file_close(state->replay_file);
    }
    state->mode = exit_program ? MATCH_SHELL_MODE_EXIT_PROGRAM : MATCH_SHELL_MODE_LEAVE_MATCH;
}

// RENDER

void match_shell_render(const MatchShellState* state) {

}

RenderSpriteParams match_shell_create_entity_render_params(const MatchShellState* state, const Entity& entity) {
    RenderSpriteParams params = (RenderSpriteParams) {
        .sprite = entity_get_sprite(state->match_state, entity),
        .frame = entity_get_animation_frame(entity),
        .position = entity.position.to_ivec2() - state->camera_offset,
        .options = 0,
        .recolor_id = entity.type == ENTITY_GOLDMINE || entity.mode == MODE_BUILDING_DESTROYED ? 0 : state->match_state.players[entity.player_id].recolor_id
    };
    const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(state->match_state, entity));
    if (entity_is_unit(entity.type)) {
        if (entity.mode == MODE_UNIT_BUILD) {
            const Entity& building = state->match_state.entities.get_by_id(entity.target.id);
            const EntityData& building_data = entity_get_data(building.type);
            int building_hframe = entity_get_animation_frame(building).x;
            params.position = building.position.to_ivec2() + 
                                ivec2(building_data.building_data.builder_positions_x[building_hframe],
                                    building_data.building_data.builder_positions_y[building_hframe])
                                - state->camera_offset;
            if (building_data.building_data.builder_flip_h[building_hframe]) {
                params.options |= RENDER_SPRITE_FLIP_H;
            }
        } else {
            // This is actually (frame_width * 2) / 2
            // It's times 2 because we doubled the resolution, divided by 2 because we're centering the sprite manually for some reason
            params.position.x -= sprite_info.frame_width;
            params.position.y -= sprite_info.frame_height;
            if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
                if (entity.mode == MODE_UNIT_BALLOON_DEATH) {
                    params.position.y += ((int)entity.timer * ENTITY_SKY_POSITION_Y_OFFSET) / (int)ENTITY_BALLOON_DEATH_DURATION;
                } else {
                    params.position.y += ENTITY_SKY_POSITION_Y_OFFSET;
                }
            }
            if (entity.direction > DIRECTION_SOUTH) {
                params.options |= RENDER_SPRITE_FLIP_H;
            }
        }
    }

    return params;
}
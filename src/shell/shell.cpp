#include "shell.h"

#include "core/logger.h"
#include "core/cursor.h"
#include "network/network.h"
#include "match/hotkey.h"
#include "match/upgrade.h"
#include "shell/desync.h"
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
static const int DESYNC_MENU_WIDTH = 332;
static const Rect DESYNC_MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (DESYNC_MENU_WIDTH / 2), .y = 128,
    .w = DESYNC_MENU_WIDTH, .h = 288
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
static const int SHELL_BUTTON_X = SCREEN_WIDTH - 264 + 28; 
static const int SHELL_BUTTON_Y = SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT + 20;
static const int SHELL_BUTTON_PADDING_X = 64 + 8;
static const int SHELL_BUTTON_PADDING_Y = 64 + 12;
static const ivec2 SHELL_BUTTON_POSITIONS[HOTKEY_GROUP_SIZE] = {
    ivec2(SHELL_BUTTON_X, SHELL_BUTTON_Y),
    ivec2(SHELL_BUTTON_X + SHELL_BUTTON_PADDING_X, SHELL_BUTTON_Y),
    ivec2(SHELL_BUTTON_X + (2 * SHELL_BUTTON_PADDING_X), SHELL_BUTTON_Y),
    ivec2(SHELL_BUTTON_X, SHELL_BUTTON_Y + SHELL_BUTTON_PADDING_Y),
    ivec2(SHELL_BUTTON_X + SHELL_BUTTON_PADDING_X, SHELL_BUTTON_Y + SHELL_BUTTON_PADDING_Y),
    ivec2(SHELL_BUTTON_X + (2 * SHELL_BUTTON_PADDING_X), SHELL_BUTTON_Y + SHELL_BUTTON_PADDING_Y)
};
static const ivec2 SELECTION_LIST_TOP_LEFT = ivec2(328, 568);
static const ivec2 BUILDING_QUEUE_TOP_LEFT = ivec2(640, 568);
static const ivec2 BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX] = {
    BUILDING_QUEUE_TOP_LEFT,
    BUILDING_QUEUE_TOP_LEFT + ivec2(0, 70),
    BUILDING_QUEUE_TOP_LEFT + ivec2(72, 70),
    BUILDING_QUEUE_TOP_LEFT + ivec2(72 * 2, 70),
    BUILDING_QUEUE_TOP_LEFT + ivec2(72 * 3, 70)
};
static const Rect BUILDING_QUEUE_PROGRESS_BAR_RECT = (Rect) {
    .x = 640 + 72,
    .y = 568 + 48,
    .w = 208, .h = 12
};
static const ivec2 GARRISON_ICON_TOP_LEFT = ivec2(640, 568 + 36);
static const ivec2 GARRISON_ICON_POSITIONS[4] = {
    GARRISON_ICON_TOP_LEFT,
    GARRISON_ICON_TOP_LEFT + ivec2(72, 0),
    GARRISON_ICON_TOP_LEFT + ivec2(72 * 2, 0),
    GARRISON_ICON_TOP_LEFT + ivec2(72 * 3, 0)
};

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

// Alerts
static const uint32_t ALERT_DURATION = 90;
static const uint32_t ALERT_LINGER_DURATION = 60 * 20;
static const uint32_t ALERT_TOTAL_DURATION = ALERT_DURATION + ALERT_LINGER_DURATION;
static const int ATTACK_ALERT_DISTANCE = 20;

// Disconnect
static const uint32_t DISCONNECT_GRACE = 10;
static const int DISCONNECT_FRAME_WIDTH = 400;
static const Rect DISCONNECT_FRAME_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (DISCONNECT_FRAME_WIDTH / 2), .y = 64,
    .w = DISCONNECT_FRAME_WIDTH, .h = 300
};

// Healthbar
static const int HEALTHBAR_HEIGHT = 8;
static const int HEALTHBAR_PADDING = 7;

// Desync
#ifdef GOLD_DEBUG_DESYNC
    static const uint32_t DESYNC_FREQUENCY = 1U;
#else
    static const uint32_t DESYNC_FREQUENCY = 60U;
#endif

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

    // Hotkey group
    for (int index = 0; index < HOTKEY_GROUP_SIZE; index++) {
        state->hotkey_group[index] = INPUT_HOTKEY_NONE;
    }

    // Replay file
    state->replay_file = NULL;

    #ifdef GOLD_DEBUG
        state->debug_fog = DEBUG_FOG_ENABLED;
        state->debug_show_region_lines = false;
    #endif

    return state;
}

MatchShellState* match_shell_init(int lcg_seed, Noise& noise) {
    MatchShellState* state = match_shell_base_init();

    state->mode = MATCH_SHELL_MODE_NOT_STARTED;

    // Populate match player info using network player info
    MatchPlayer players[MAX_PLAYERS];
    memset(players, 0, sizeof(players));
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const NetworkPlayer& network_player = network_get_player(player_id);
        if (network_player.status == NETWORK_PLAYER_STATUS_NONE) {
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

    // Init bots
    // memset(state->bots, 0, sizeof(state->bots));
    MatchSettingDifficultyValue difficulty = (MatchSettingDifficultyValue)network_get_match_setting(MATCH_SETTING_DIFFICULTY);
    int bot_lcg_seed = lcg_seed;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (network_get_player(player_id).status != NETWORK_PLAYER_STATUS_BOT) {
            continue;
        }

        BotOpener opener = bot_roll_opener(&bot_lcg_seed, difficulty);
        BotUnitComp preferred_unit_comp = bot_roll_preferred_unit_comp(&bot_lcg_seed);
        state->bots[player_id] = bot_init(player_id, difficulty, opener, preferred_unit_comp);
    }

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
        return nullptr;
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
        return nullptr;
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
        case NETWORK_EVENT_CHECKSUM: {
            state->checksums[event.checksum.player_id].push_back(event.checksum.checksum);
            if (state->mode == MATCH_SHELL_MODE_DESYNC) {
                return;
            }
            match_shell_compare_checksums(state, state->checksums[event.checksum.player_id].size() - 1);
            break;
        }
    #ifdef GOLD_DEBUG_DESYNC
        case NETWORK_EVENT_SERIALIZED_FRAME: {
            match_shell_handle_serialized_frame(event.serialized_frame.state_buffer, event.serialized_frame.state_buffer_length);
            free(event.serialized_frame.state_buffer);
        }
    #endif
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
            // Menu button doesn't work for defeat / victory / desync screens
            !(state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT || state->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY || state->mode == MATCH_SHELL_MODE_DESYNC) &&
            ((menu_button_rect.has_point(input_get_mouse_position()) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) || input_is_action_just_pressed(INPUT_ACTION_MATCH_MENU))) {
        if (match_shell_is_in_menu(state)) {
            state->options_menu.mode = OPTIONS_MENU_CLOSED;
            state->mode = MATCH_SHELL_MODE_NONE;
        } else {
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
        ui_frame_rect(state->ui, state->mode == MATCH_SHELL_MODE_DESYNC ? DESYNC_MENU_RECT : MENU_RECT);

        const char* header_text = match_shell_get_menu_header_text(state);
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_GOLD, header_text);
        ivec2 text_pos = ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (text_size.x / 2), MENU_RECT.y + 20);
        ui_element_position(state->ui, text_pos);
        ui_text(state->ui, FONT_WESTERN8_GOLD, header_text);

        if (state->mode == MATCH_SHELL_MODE_DESYNC) {
            ivec2 text_column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 64);
            ui_begin_column(state->ui, text_column_position, 10);
                ui_text(state->ui, FONT_HACK_WHITE, "Your game is out of", true);
                ui_text(state->ui, FONT_HACK_WHITE, "sync with your opponents.", true);
            ui_end_container(state->ui);
        }

        ivec2 column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 64);
        if (state->mode == MATCH_SHELL_MODE_DESYNC) {
            column_position.y += 94;
        } else if (state->mode != MATCH_SHELL_MODE_MENU) {
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
            } else if (state->mode == MATCH_SHELL_MODE_DESYNC) {
                if (ui_button(state->ui, "Leave Match", button_size, true)) {
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
            !match_shell_is_targeting(state) &&
            !match_shell_is_selecting(state) &&
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
    if (state->is_paused || 
            (match_shell_is_in_menu(state) && match_shell_is_in_single_player_game()) ||
            state->mode == MATCH_SHELL_MODE_LEAVE_MATCH || 
            state->mode == MATCH_SHELL_MODE_EXIT_PROGRAM || 
            state->mode == MATCH_SHELL_MODE_DESYNC) {
        return;
    }

    // Non-replay begin turn
    if (state->match_timer % TURN_DURATION == 0 && !state->replay_mode) {
        // Bot inputs
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            // Filter down to active, bot players
            if (!(state->match_state.players[player_id].active && 
                    network_get_player(player_id).status == NETWORK_PLAYER_STATUS_BOT)) {
                continue;
            }

            if (state->inputs[player_id].empty()) {
                MatchInput bot_input = bot_get_turn_input(state->match_state, state->bots[player_id], state->match_timer);
                state->inputs[player_id].push({ bot_input });

                // Buffer empty inputs. This way the bot can always assume that all its inputs have been applied when deciding on the next one
                for (uint32_t index = 0; index < TURN_OFFSET - 1; index++) {
                    state->inputs[player_id].push({ (MatchInput) { .type = MATCH_INPUT_NONE } });
                }

                // Check for bot surrender
                if (bot_should_surrender(state->match_state, state->bots[player_id], state->match_timer)) {
                    match_shell_add_chat_message(state, player_id, "gg");
                    match_shell_handle_player_disconnect(state, player_id);
                    // handle_player_disconnect should have set the bot to inactive for us
                    GOLD_ASSERT(!state->match_state.players[player_id].active);
                }
            }
        }

        // Check that all inputs have been received
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

    // Checksum
    if (!state->replay_mode && state->match_timer % DESYNC_FREQUENCY == 0) {
        uint32_t checksum = desync_compute_match_checksum(state->match_state);
        network_send_checksum(checksum);
        state->checksums[network_get_player_id()].push_back(checksum);
        match_shell_compare_checksums(state, state->checksums[network_get_player_id()].size() - 1);
    }

    // Match update
    match_update(state->match_state);

    // Increment match timer
    state->match_timer++;

    // Handle input
    if (!match_shell_is_in_menu(state)) {
        match_shell_handle_input(state);
    }

    // Match events
    for (const MatchEvent& event : state->match_state.events) {
        switch (event.type) {
            case MATCH_EVENT_SOUND: {
                if (state->sound_cooldown_timers[event.sound.sound] != 0) {
                    break;
                }
                if (!match_shell_is_cell_rect_revealed(state, event.sound.position / TILE_SIZE, 1)) {
                    break;
                }
                if (SOUND_LISTEN_RECT.has_point(event.sound.position - state->camera_offset)) {
                    sound_play(event.sound.sound);
                    state->sound_cooldown_timers[event.sound.sound] = SOUND_COOLDOWN_DURATION;
                }
                
                break;
            }
            case MATCH_EVENT_ALERT: {
                if (state->replay_mode || !state->match_state.players[network_get_player_id()].active) {
                    break;
                }
                if ((event.alert.type == MATCH_ALERT_TYPE_ATTACK && state->match_state.players[event.alert.player_id].team != state->match_state.players[network_get_player_id()].team) ||
                    (event.alert.type != MATCH_ALERT_TYPE_ATTACK && event.alert.player_id != network_get_player_id())) {
                    break;
                }

                // Check if an existing attack alert already exists nearby
                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    bool is_existing_attack_alert_nearby = false;
                    for (const Alert& existing_alert : state->alerts) {
                        if (existing_alert.pixel == MINIMAP_PIXEL_WHITE && ivec2::manhattan_distance(existing_alert.cell, event.alert.cell) < ATTACK_ALERT_DISTANCE) {
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
                state->latest_alert_cell = event.alert.cell;

                Rect camera_rect = (Rect) { 
                    .x = state->camera_offset.x, 
                    .y = state->camera_offset.y, 
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
                    pixel = (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + state->match_state.players[network_get_player_id()].recolor_id);
                }

                state->alerts.push_back((Alert) {
                    .pixel = pixel,
                    .cell = event.alert.cell,
                    .cell_size = event.alert.cell_size,
                    .timer = ALERT_TOTAL_DURATION
                });

                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    match_shell_show_status(state, event.alert.player_id == network_get_player_id() 
                                                    ? MATCH_UI_STATUS_UNDER_ATTACK 
                                                    : MATCH_UI_STATUS_ALLY_UNDER_ATTACK);
                    sound_play(SOUND_ALERT_BELL);
                } 
                break;
            }
            case MATCH_EVENT_SELECTION_HANDOFF: {
                if (state->replay_mode || !state->match_state.players[network_get_player_id()].active) {
                    break;
                }
                if (event.selection_handoff.player_id != network_get_player_id()) {
                    break;
                }

                if (state->selection.size() == 1 && state->selection[0] == event.selection_handoff.to_deselect) {
                    if (match_shell_is_in_menu(state)) {
                        state->selection.clear();
                    } else {
                        std::vector<EntityId> new_selection;
                        new_selection.push_back(event.selection_handoff.to_select);
                        match_shell_set_selection(state, new_selection);
                    }
                }
                break;
            }
            case MATCH_EVENT_STATUS: {
                if (state->replay_mode || !state->match_state.players[network_get_player_id()].active) {
                    break;
                }
                if (network_get_player_id() == event.status.player_id) {
                    match_shell_show_status(state, event.status.message);
                }
                break;
            }
            case MATCH_EVENT_RESEARCH_COMPLETE: {
                if (state->replay_mode || !state->match_state.players[network_get_player_id()].active) {
                    break;
                }
                if (event.research_complete.player_id != network_get_player_id()) {
                    break;
                }

                char message[128];
                sprintf(message, "%s research complete.", upgrade_get_data(event.research_complete.upgrade).name);
                match_shell_show_status(state, message);
                break;
            }
        }
    }
    state->match_state.events.clear();

    // Update timers and animations
    if (animation_is_playing(state->move_animation)) {
        animation_update(state->move_animation);
    }
    animation_update(state->rally_flag_animation);
    animation_update(state->building_fire_animation);
    if (state->status_timer != 0) {
        state->status_timer--;
    }
    if (state->double_click_timer != 0) {
        state->double_click_timer--;
    }
    if (state->control_group_double_tap_timer != 0) {
        state->control_group_double_tap_timer--;
    }
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        if (state->sound_cooldown_timers[sound] != 0) {
            state->sound_cooldown_timers[sound]--;
        }
    }
    if (input_is_text_input_active()) {
        state->chat_cursor_blink_timer--;
        if (state->chat_cursor_blink_timer == 0) {
            state->chat_cursor_visible = !state->chat_cursor_visible;
            state->chat_cursor_blink_timer = CHAT_CURSOR_BLINK_DURATION;
        }
    }

    // Update alerts
    {
        uint32_t alert_index = 0;
        while (alert_index < state->alerts.size()) {
            state->alerts[alert_index].timer--;
            if (state->alerts[alert_index].timer == 0) {
                state->alerts.erase(state->alerts.begin() + alert_index);
            } else {
                alert_index++;
            }
        }
    }

    // Update chat
    for (uint32_t chat_index = 0; chat_index < state->chat.size(); chat_index++) {
        state->chat[chat_index].timer--;
        if (state->chat[chat_index].timer == 0) {
            state->chat.erase(state->chat.begin() + chat_index);
            chat_index--;
        }
    }

    // Clear hidden units from selection
    {
        uint32_t selection_index = 0;
        while (selection_index < state->selection.size()) {
            Entity& selected_entity = state->match_state.entities.get_by_id(state->selection[selection_index]);
            if (!entity_is_selectable(selected_entity) || !match_shell_is_entity_visible(state, selected_entity)) {
                state->selection.erase(state->selection.begin() + selection_index);
            } else {
                selection_index++;
            }
        }
    }

    // Update cursor
    cursor_set(match_shell_is_targeting(state) && (!match_shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position())) ? CURSOR_TARGET : CURSOR_DEFAULT);
    if ((match_shell_is_targeting(state) || state->mode == MATCH_SHELL_MODE_BUILDING_PLACE || match_shell_is_in_hotkey_submenu(state)) && state->selection.empty()) {
        state->mode = MATCH_SHELL_MODE_NONE;
    }

    // Update UI buttons
    if (!state->replay_mode) {
        for (uint32_t index = 0; index < HOTKEY_GROUP_SIZE; index++) {
            state->hotkey_group[index] = INPUT_HOTKEY_NONE;
        }
        if (state->mode == MATCH_SHELL_MODE_BUILD) {
            state->hotkey_group[0] = INPUT_HOTKEY_HALL;
            state->hotkey_group[1] = INPUT_HOTKEY_HOUSE;
            state->hotkey_group[2] = INPUT_HOTKEY_SALOON;
            state->hotkey_group[3] = INPUT_HOTKEY_BUNKER;
            state->hotkey_group[4] = INPUT_HOTKEY_WORKSHOP;
            state->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
        } else if (state->mode == MATCH_SHELL_MODE_BUILD2) {
            state->hotkey_group[0] = INPUT_HOTKEY_SMITH;
            state->hotkey_group[1] = INPUT_HOTKEY_COOP;
            state->hotkey_group[2] = INPUT_HOTKEY_BARRACKS;
            state->hotkey_group[3] = INPUT_HOTKEY_SHERIFFS;
            state->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
        } else if (state->mode == MATCH_SHELL_MODE_BUILDING_PLACE || match_shell_is_targeting(state)) {
            state->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
        } else if (!state->selection.empty()) {
            Entity& first_entity = state->match_state.entities.get_by_id(state->selection[0]);
            if (first_entity.player_id == network_get_player_id() && first_entity.mode == MODE_BUILDING_IN_PROGRESS && state->selection.size() == 1) {
                state->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
            } else if (first_entity.player_id == network_get_player_id()) {
                bool is_selection_uniform = true;
                bool has_garrisoned_units = !first_entity.garrisoned_units.empty();
                if (first_entity.mode == MODE_BUILDING_IN_PROGRESS) {
                    is_selection_uniform = false;
                }
                for (uint32_t selection_index = 1; selection_index < state->selection.size(); selection_index++) {
                    const Entity& entity = state->match_state.entities.get_by_id(state->selection[selection_index]);
                    if (!entity.garrisoned_units.empty()) {
                        has_garrisoned_units = true;
                    }
                    // If units are not the same type, selection is not uniform
                    if (entity.type != first_entity.type) {
                        is_selection_uniform = false;
                        continue;
                    }
                    // Check invisibility values, ensures that camo / decamo command is the same
                    if (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) != entity_check_flag(first_entity, ENTITY_FLAG_INVISIBLE)) {
                        is_selection_uniform = false;
                        continue;
                    }
                    // Selection is not uniform for in-progress buildings, prevents showing unit train icons when one of the selected buildings is in-progress
                    if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
                        is_selection_uniform = false;
                        continue;
                    }
                }

                if (entity_is_unit(first_entity.type)) {
                    state->hotkey_group[0] = INPUT_HOTKEY_ATTACK;
                    state->hotkey_group[1] = INPUT_HOTKEY_STOP;
                    state->hotkey_group[2] = INPUT_HOTKEY_DEFEND;
                }
                if (entity_is_building(first_entity.type) && !first_entity.queue.empty() && state->selection.size() == 1) {
                    state->hotkey_group[5] = INPUT_HOTKEY_CANCEL;
                }

                if (is_selection_uniform) {
                    if (has_garrisoned_units) {
                        state->hotkey_group[3] = INPUT_HOTKEY_UNLOAD;
                    }
                    switch (first_entity.type) {
                        case ENTITY_MINER: {
                            state->hotkey_group[3] = INPUT_HOTKEY_REPAIR;
                            state->hotkey_group[4] = INPUT_HOTKEY_BUILD;
                            state->hotkey_group[5] = INPUT_HOTKEY_BUILD2;
                            break;
                        }
                        case ENTITY_HALL: {
                            state->hotkey_group[0] = INPUT_HOTKEY_MINER;
                            break;
                        }
                        case ENTITY_SALOON: {
                            state->hotkey_group[0] = INPUT_HOTKEY_COWBOY;
                            state->hotkey_group[1] = INPUT_HOTKEY_BANDIT;
                            state->hotkey_group[2] = INPUT_HOTKEY_DETECTIVE;
                            break;
                        }
                        case ENTITY_WORKSHOP: {
                            state->hotkey_group[0] = INPUT_HOTKEY_SAPPER;
                            state->hotkey_group[1] = INPUT_HOTKEY_PYRO;
                            state->hotkey_group[2] = INPUT_HOTKEY_BALLOON;
                            state->hotkey_group[3] = INPUT_HOTKEY_RESEARCH_LANDMINES;
                            state->hotkey_group[4] = INPUT_HOTKEY_RESEARCH_TAILWIND;
                            break;
                        }
                        case ENTITY_SMITH: {
                            state->hotkey_group[0] = INPUT_HOTKEY_RESEARCH_SERRATED_KNIVES;
                            state->hotkey_group[1] = INPUT_HOTKEY_RESEARCH_BAYONETS;
                            state->hotkey_group[2] = INPUT_HOTKEY_RESEARCH_WAGON_ARMOR;
                            break;
                        }
                        case ENTITY_COOP: {
                            state->hotkey_group[0] = INPUT_HOTKEY_JOCKEY;
                            state->hotkey_group[1] = INPUT_HOTKEY_WAGON;
                            break;
                        }
                        case ENTITY_BARRACKS: {
                            state->hotkey_group[0] = INPUT_HOTKEY_SOLDIER;
                            state->hotkey_group[1] = INPUT_HOTKEY_CANNON;
                            break;
                        }
                        case ENTITY_SHERIFFS: {
                            state->hotkey_group[0] = INPUT_HOTKEY_RESEARCH_PRIVATE_EYE;
                            state->hotkey_group[1] = INPUT_HOTKEY_RESEARCH_STAKEOUT;
                            break;
                        }
                        case ENTITY_PYRO: {
                            state->hotkey_group[3] = INPUT_HOTKEY_MOLOTOV;
                            state->hotkey_group[4] = INPUT_HOTKEY_LANDMINE;
                            break;
                        }
                        case ENTITY_DETECTIVE: {
                            state->hotkey_group[3] = INPUT_HOTKEY_CAMO;
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }
        for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
            if (state->hotkey_group[hotkey_index] == INPUT_HOTKEY_NONE) {
                continue;
            }
            const HotkeyButtonInfo& info = hotkey_get_button_info(state->hotkey_group[hotkey_index]);
            if (info.type == HOTKEY_BUTTON_RESEARCH && !match_player_upgrade_is_available(state->match_state, network_get_player_id(), info.upgrade)) {
                state->hotkey_group[hotkey_index] = INPUT_HOTKEY_NONE;
            }
        }
    }

    // Check win conditions
    if (!state->replay_mode) {
        bool player_has_buildings[MAX_PLAYERS];
        bool opposing_player_just_lost = false;
        memset(player_has_buildings, 0, sizeof(player_has_buildings));
        for (const Entity& entity : state->match_state.entities) {
            if (entity_is_building(entity.type) && entity.type != ENTITY_LANDMINE && entity_is_selectable(entity)) {
                player_has_buildings[entity.player_id] = true;
            }
        }
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (state->match_state.players[player_id].active && !player_has_buildings[player_id]) {
                state->match_state.players[player_id].active = false;

                char defeat_message[128];
                sprintf(defeat_message, "%s has been defeated.", state->match_state.players[player_id].name);
                match_shell_add_chat_message(state, PLAYER_NONE, defeat_message);

                if (player_id == network_get_player_id()) {
                    state->mode = MATCH_SHELL_MODE_MATCH_OVER_DEFEAT;
                } else {
                    opposing_player_just_lost = true;
                }
            }
        }
        if (opposing_player_just_lost && !match_shell_is_at_least_one_opponent_in_match(state)) {
            state->mode = MATCH_SHELL_MODE_MATCH_OVER_VICTORY;
        }
    }
}

void match_shell_handle_input(MatchShellState* state) {
    bool spectator_mode = state->replay_mode || !state->match_state.players[network_get_player_id()].active;

    // Begin chat
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && !input_is_text_input_active() && !state->replay_mode) {
        state->chat_message = "";
        state->chat_cursor_blink_timer = CHAT_CURSOR_BLINK_DURATION;
        state->chat_cursor_visible = false;
        input_start_text_input(&state->chat_message, CHAT_MAX_LENGTH);
        sound_play(SOUND_UI_CLICK);
        return;
    }

    // End chat
    if (input_is_action_just_pressed(INPUT_ACTION_ENTER) && input_is_text_input_active() && !state->replay_mode) {
        if (!state->chat_message.empty()) {
            #ifdef GOLD_DEBUG
                if (state->chat_message == "/fog off") {
                    state->debug_fog = DEBUG_FOG_DISABLED;
                } else if (state->chat_message == "/fog on") {
                    state->debug_fog = DEBUG_FOG_ENABLED;
                } else if (state->chat_message == "/fog bot") {
                    state->debug_fog = DEBUG_FOG_BOT_VISION;
                } else if (state->chat_message == "/gold") {
                    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                        state->match_state.players[player_id].gold += 5000;
                    }
                }  else if (state->chat_message == "/regions") {
                    state->debug_show_region_lines = !state->debug_show_region_lines;
                }
            #endif
            network_send_chat(state->chat_message.c_str());
            match_shell_add_chat_message(state, network_get_player_id(), state->chat_message.c_str());
        }
        sound_play(SOUND_UI_CLICK);
        input_stop_text_input();
    }

    // Hotkey click
    if (!spectator_mode) {
        for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
            InputAction hotkey = state->hotkey_group[hotkey_index];
            if (hotkey == INPUT_HOTKEY_NONE) {
                continue;
            }
            if (state->is_minimap_dragging || match_shell_is_selecting(state)) {
                continue;
            }
            if (!match_does_player_meet_hotkey_requirements(state->match_state, network_get_player_id(), hotkey)) {
                continue;
            }

            const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            ivec2 hotkey_position = SHELL_BUTTON_POSITIONS[hotkey_index];
            Rect hotkey_rect = (Rect) {
                .x = hotkey_position.x, .y = hotkey_position.y,
                .w = sprite_info.frame_width * 2, .h = sprite_info.frame_height * 2
            };

            if ((input_is_action_just_pressed(hotkey) && !input_is_text_input_active()) || (
                input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) && hotkey_rect.has_point(input_get_mouse_position())
            )) {
                const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);

                if (hotkey_info.type == HOTKEY_BUTTON_ACTION || hotkey_info.type == HOTKEY_BUTTON_TOGGLED_ACTION) {
                    switch (hotkey) {
                        case INPUT_HOTKEY_ATTACK: {
                            state->mode = MATCH_SHELL_MODE_TARGET_ATTACK;
                            break;
                        }
                        case INPUT_HOTKEY_BUILD: {
                            state->mode = MATCH_SHELL_MODE_BUILD;
                            break;
                        }
                        case INPUT_HOTKEY_BUILD2: {
                            state->mode = MATCH_SHELL_MODE_BUILD2;
                            break;
                        }
                        case INPUT_HOTKEY_CANCEL: {
                            if (match_shell_is_in_hotkey_submenu(state) || match_shell_is_targeting(state)) {
                                state->mode = MATCH_SHELL_MODE_NONE;
                            } else if (state->mode == MATCH_SHELL_MODE_BUILDING_PLACE) {
                                state->mode = MATCH_SHELL_MODE_NONE;
                            } else if (state->selection.size() == 1) { 
                                const Entity& entity = state->match_state.entities.get_by_id(state->selection[0]);
                                if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
                                    state->input_queue.push_back((MatchInput) {
                                        .type = MATCH_INPUT_BUILD_CANCEL,
                                        .build_cancel = (MatchInputBuildCancel) {
                                            .building_id = state->selection[0]
                                        }
                                    });
                                } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty()) {
                                    state->input_queue.push_back((MatchInput) {
                                        .type = MATCH_INPUT_BUILDING_DEQUEUE,
                                        .building_dequeue = (MatchInputBuildingDequeue) {
                                            .building_id = state->selection[0],
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
                            input.stop.entity_count = (uint8_t)state->selection.size();
                            memcpy(&input.stop.entity_ids, &state->selection[0], input.stop.entity_count * sizeof(EntityId));
                            state->input_queue.push_back(input);
                            break;
                        }
                        case INPUT_HOTKEY_REPAIR: {
                            state->mode = MATCH_SHELL_MODE_TARGET_REPAIR;
                            break;
                        }
                        case INPUT_HOTKEY_UNLOAD: {
                            if (entity_is_unit(state->match_state.entities.get_by_id(state->selection[0]).type)) {
                                state->mode = MATCH_SHELL_MODE_TARGET_UNLOAD;
                            } else {
                                MatchInput input;
                                input.type = MATCH_INPUT_UNLOAD;
                                input.unload.carrier_count = (uint8_t)state->selection.size();
                                memcpy(&input.unload.carrier_ids, &state->selection[0], input.unload.carrier_count * sizeof(EntityId));
                                state->input_queue.push_back(input);
                            }
                            break;
                        }
                        case INPUT_HOTKEY_MOLOTOV: {
                            if (!match_shell_selection_has_enough_energy(state, state->selection, MOLOTOV_ENERGY_COST)) {
                                match_shell_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                                break;
                            }
                            state->mode = MATCH_SHELL_MODE_TARGET_MOLOTOV;
                            break;
                        }
                        case INPUT_HOTKEY_CAMO: {
                            bool activate = !entity_check_flag(state->match_state.entities.get_by_id(state->selection[0]), ENTITY_FLAG_INVISIBLE);
                            if (activate && !match_shell_selection_has_enough_energy(state, state->selection, CAMO_ENERGY_COST)) {
                                match_shell_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                                break;
                            }
                            MatchInput input;
                            input.type = activate ? MATCH_INPUT_CAMO : MATCH_INPUT_DECAMO;
                            input.camo.unit_count = (uint8_t)state->selection.size();
                            memcpy(&input.camo.unit_ids, &state->selection[0], input.camo.unit_count * sizeof(EntityId));
                            state->input_queue.push_back(input);
                            break;
                        }
                        default:
                            break;
                    }
                } else if (hotkey_info.type == HOTKEY_BUTTON_BUILD) {
                    const EntityData& building_data = entity_get_data(hotkey_info.entity_type);
                    bool costs_energy = (building_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY;
                    if (costs_energy && !match_shell_selection_has_enough_energy(state, state->selection, building_data.gold_cost)) {
                        match_shell_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_ENERGY);
                    } else if (!costs_energy && state->match_state.players[network_get_player_id()].gold < entity_get_data(hotkey_info.entity_type).gold_cost) {
                        match_shell_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                    } else {
                        state->mode = MATCH_SHELL_MODE_BUILDING_PLACE;
                        state->building_type = hotkey_info.entity_type;
                    }
                } else if (hotkey_info.type == HOTKEY_BUTTON_TRAIN || hotkey_info.type == HOTKEY_BUTTON_RESEARCH) {
                    // Build the queue item
                    BuildingQueueItem item = hotkey_info.type == HOTKEY_BUTTON_TRAIN 
                                                ? (BuildingQueueItem) {
                                                    .type = BUILDING_QUEUE_ITEM_UNIT,
                                                    .unit_type = hotkey_info.entity_type
                                                }
                                                : (BuildingQueueItem) {
                                                    .type = BUILDING_QUEUE_ITEM_UPGRADE,
                                                    .upgrade = hotkey_info.upgrade
                                                };

                    // Check to make sure at least one building queue has room
                    bool are_building_queues_full = true;
                    for (EntityId building_id : state->selection) {
                        const Entity& building = state->match_state.entities.get_by_id(building_id);
                        if (building.queue.size() < BUILDING_QUEUE_MAX) {
                            are_building_queues_full = false;
                        }
                    }

                    if (are_building_queues_full) {
                        match_shell_show_status(state, MATCH_UI_STATUS_BUILDING_QUEUE_FULL);
                    } else if (state->match_state.players[network_get_player_id()].gold < building_queue_item_cost(item)) {
                        match_shell_show_status(state, MATCH_UI_STATUS_NOT_ENOUGH_GOLD);
                    } else {
                        if (match_shell_is_in_hotkey_submenu(state)) {
                            state->mode = MATCH_SHELL_MODE_NONE;
                        }
                        MatchInput input;
                        input.type = MATCH_INPUT_BUILDING_ENQUEUE;
                        input.building_enqueue.item_type = (uint8_t)item.type;
                        input.building_enqueue.item_subtype = item.type == BUILDING_QUEUE_ITEM_UNIT
                                                                ? (uint32_t)item.unit_type
                                                                : item.upgrade;
                        input.building_enqueue.building_count = (uint8_t)state->selection.size();
                        memcpy(&input.building_enqueue.building_ids, &state->selection[0], state->selection.size() * sizeof(EntityId));
                        state->input_queue.push_back(input);
                    }
                } 

                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Selection list click
    if (state->selection.size() > 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
        !(state->is_minimap_dragging || match_shell_is_selecting(state))) {
        for (uint32_t selection_index = 0; selection_index < state->selection.size(); selection_index++) {
            Rect icon_rect = match_shell_get_selection_list_item_rect(selection_index);
            if (icon_rect.has_point(input_get_mouse_position())) {
                if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                    state->selection.erase(state->selection.begin() + selection_index);
                } else {
                    std::vector<EntityId> selection;
                    selection.push_back(state->selection[selection_index]);
                    match_shell_set_selection(state, selection);
                }
                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Building queue click
    if (state->selection.size() == 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !spectator_mode &&
            !(state->is_minimap_dragging || match_shell_is_selecting(state))) {
        const Entity& building = state->match_state.entities.get_by_id(state->selection[0]);
        const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
        for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
            Rect icon_rect = (Rect) {
                .x = BUILDING_QUEUE_POSITIONS[building_queue_index].x,
                .y = BUILDING_QUEUE_POSITIONS[building_queue_index].y,
                .w = icon_sprite_info.frame_width * 2, 
                .h = icon_sprite_info.frame_height * 2
            };
            if (icon_rect.has_point(input_get_mouse_position())) {
                state->input_queue.push_back((MatchInput) {
                    .type = MATCH_INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (MatchInputBuildingDequeue) {
                        .building_id = state->selection[0],
                        .index = (uint8_t)building_queue_index
                    }
                });
                sound_play(SOUND_UI_CLICK);
                return;
            }
        }
    }

    // Garrisoned unit click
    if (state->selection.size() == 1 && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK) &&
            !spectator_mode &&
            !(state->is_minimap_dragging || match_shell_is_selecting(state))) {
        const Entity& carrier = state->match_state.entities.get_by_id(state->selection[0]);
        if (carrier.type == ENTITY_GOLDMINE || carrier.player_id == network_get_player_id()) {
            const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            int index = 0;
            for (EntityId entity_id : carrier.garrisoned_units) {
                const Entity& garrisoned_unit = state->match_state.entities.get_by_id(entity_id);
                // We have to make this check here because goldmines might have both allied and enemy units in them
                if (garrisoned_unit.player_id != network_get_player_id()) {
                    continue;
                }

                Rect icon_rect = (Rect) {
                    .x = GARRISON_ICON_POSITIONS[index].x,
                    .y = GARRISON_ICON_POSITIONS[index].y,
                    .w = icon_sprite_info.frame_width * 2,
                    .h = icon_sprite_info.frame_height * 2
                };
                if (icon_rect.has_point(input_get_mouse_position())) {
                    state->input_queue.push_back((MatchInput) {
                        .type = MATCH_INPUT_SINGLE_UNLOAD,
                        .single_unload = (MatchInputSingleUnload) {
                            .entity_id = entity_id
                        }
                    });
                    sound_play(SOUND_UI_CLICK);
                    return;
                }

                index++;
            }
        }
    }

    // Order move
    if (!spectator_mode) {
        // Check that the left or right mouse button is being pressed
        InputAction action_required = match_shell_is_targeting(state) ? INPUT_ACTION_LEFT_CLICK : INPUT_ACTION_RIGHT_CLICK;
        bool is_action_pressed = input_is_action_just_pressed(action_required);
        // Check that a move command can be executed in the current UI state
        bool is_movement_allowed = state->mode != MATCH_SHELL_MODE_BUILDING_PLACE && 
                                  !state->is_minimap_dragging && 
                                  match_shell_get_selection_type(state, state->selection) 
                                        == MATCH_SHELL_SELECTION_UNITS;
        // Check that the mouse is either in the world or in the minimap
        bool is_mouse_in_position = !match_shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position());

        if (is_action_pressed && is_movement_allowed && is_mouse_in_position) {
            match_shell_order_move(state);
            return;
        }
    }

    // Set building rally
    if (input_is_action_just_pressed(INPUT_ACTION_RIGHT_CLICK) && 
            match_shell_get_selection_type(state, state->selection) == MATCH_SHELL_SELECTION_BUILDINGS &&
            !spectator_mode &&
            !(state->is_minimap_dragging || match_shell_is_selecting(state) || match_shell_is_targeting(state)) &&
            (!match_shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position()))) {
        // Check to make sure that all buildings can rally
        for (EntityId id : state->selection) {
            const Entity& entity = state->match_state.entities.get_by_id(id);
            if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
                return;
            }
            if ((entity_get_data(entity.type).building_data.options & BUILDING_CAN_RALLY) != BUILDING_CAN_RALLY) {
                return;
            }
        }

        // Create rally input
        MatchInput input;
        input.type = MATCH_INPUT_RALLY;

        // Determine rally point
        if (match_shell_is_mouse_in_ui()) {
            ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
            input.rally.rally_point = ivec2((state->match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                                            (state->match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        } else {
            input.rally.rally_point = input_get_mouse_position() + state->camera_offset;
        }

        // Check if rallied onto gold
        Cell cell = map_get_cell(state->match_state.map, CELL_LAYER_GROUND, input.rally.rally_point / TILE_SIZE);
        if (cell.type == CELL_GOLDMINE) {
            // If they rallied onto their gold, adjust their rally point and play an animation to clearly indicate that units will rally out to the mine
            // But only do this if the goldmine has been explored, otherwise we would reveal to players that there is a goldmine where they clicked
            const Entity& goldmine = state->match_state.entities.get_by_id(cell.id);
            if (match_is_cell_rect_explored(state->match_state, state->match_state.players[network_get_player_id()].team, goldmine.cell, entity_get_data(goldmine.type).cell_size)) {
                input.rally.rally_point = (goldmine.cell * TILE_SIZE) + ivec2(46, 32);
                state->move_animation = animation_create(ANIMATION_UI_MOVE_ENTITY);
                state->move_animation_entity_id = cell.id;
                state->move_animation_position = goldmine.cell * TILE_SIZE;
            }
        }

        // Add buildings to rally point input
        input.rally.building_count = state->selection.size();
        memcpy(&input.rally.building_ids, &state->selection[0], state->selection.size() * sizeof(EntityId));

        state->input_queue.push_back(input);
        sound_play(SOUND_FLAG_THUMP);

        return;
    }

    // Building placement
    if (state->mode == MATCH_SHELL_MODE_BUILDING_PLACE && 
            !state->is_minimap_dragging && 
            !match_shell_is_mouse_in_ui() && 
            input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) {
        if (!match_shell_building_can_be_placed(state)) {
            match_shell_show_status(state, MATCH_UI_STATUS_CANT_BUILD);
            return;
        }

        MatchInput input;
        input.type = MATCH_INPUT_BUILD;
        input.build.shift_command = input_is_action_pressed(INPUT_ACTION_SHIFT);
        input.build.building_type = state->building_type;
        input.build.entity_count = state->selection.size();
        memcpy(&input.build.entity_ids, &state->selection[0], state->selection.size() * sizeof(EntityId));
        input.build.target_cell = match_shell_get_building_cell(entity_get_data(state->building_type).cell_size, state->camera_offset);
        state->input_queue.push_back(input);

        if (!input.build.shift_command) {
            state->mode = MATCH_SHELL_MODE_NONE;
        }

        sound_play(SOUND_BUILDING_PLACE);
        return;
    }

    // Control group key press
    if (!spectator_mode) {
        uint32_t number_key_pressed;
        for (number_key_pressed = 0; number_key_pressed < 10; number_key_pressed++) {
            if (input_is_action_just_pressed((InputAction)(INPUT_ACTION_NUM1 + number_key_pressed))) {
                break;
            }
        }
        if (number_key_pressed != 10) {
            int control_group_index = number_key_pressed;
            // Set control group
            if (input_is_action_pressed(INPUT_ACTION_CTRL)) {
                if (state->selection.empty() || state->match_state.entities.get_by_id(state->selection[0]).player_id != network_get_player_id()) {
                    return;
                }

                state->control_groups[control_group_index] = state->selection;
                state->control_group_selected = control_group_index;
                return;
            }

            // Append control group
            if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                if (state->selection.empty() || state->match_state.entities.get_by_id(state->selection[0]).player_id != network_get_player_id()) {
                    return;
                }
                
                for (EntityId entity_id : state->selection) {
                    if (std::find(state->control_groups[control_group_index].begin(), state->control_groups[control_group_index].end(), entity_id) 
                            == state->control_groups[control_group_index].end()) {
                        state->control_groups[control_group_index].push_back(entity_id);
                    }
                }

                return;
            }

            // Snap to control group
            if (state->control_group_double_tap_timer != 0 && state->control_group_selected == control_group_index) {
                ivec2 group_min;
                ivec2 group_max;
                for (uint32_t selection_index = 0; selection_index < state->selection.size(); selection_index++) {
                    const Entity& entity = state->match_state.entities.get_by_id(state->selection[selection_index]);
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
                match_shell_center_camera_on_cell(state, group_center);
                return;
            }

            // Select control group
            if (!state->control_groups[control_group_index].empty()) {
                match_shell_set_selection(state, state->control_groups[control_group_index]);
                if (!state->selection.empty()) {
                    state->control_group_double_tap_timer = MATCH_SHELL_DOUBLE_CLICK_DURATION;
                    state->control_group_selected = control_group_index;
                }
            }
        }
    }

    // Jump to latest alert
    if (input_is_action_just_pressed(INPUT_ACTION_SPACE) && state->latest_alert_cell.x != -1 && !spectator_mode) {
        match_shell_center_camera_on_cell(state, state->latest_alert_cell);
    }

    // Camera hotkeys
    for (int index = 0; index < MATCH_SHELL_CAMERA_HOTKEY_COUNT; index++) {
        if (input_is_action_just_pressed((InputAction)(INPUT_ACTION_F1 + index))) {
            if (input_is_action_pressed(INPUT_ACTION_SHIFT)) {
                state->camera_hotkeys[index] = state->camera_offset;
            } else if (state->camera_hotkeys[index].x != -1) {
                state->camera_offset = state->camera_hotkeys[index];
            }
        }
    }
}

void match_shell_order_move(MatchShellState* state) {
    // Determine move target
    ivec2 move_target;
    if (match_shell_is_mouse_in_ui()) {
        ivec2 minimap_pos = input_get_mouse_position() - ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y);
        move_target = ivec2((state->match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                            (state->match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
    } else {
        move_target = input_get_mouse_position() + state->camera_offset;
    }

    // Create move input
    MatchInput input;
    input.move.shift_command = input_is_action_pressed(INPUT_ACTION_SHIFT);
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;
    EntityId remembered_entity_id = ID_NULL;
    bool input_move_target_is_balloon = false;

    // Checks if clicked on entity
    uint8_t player_team = state->match_state.players[network_get_player_id()].team;
    int fog_value = match_get_fog(state->match_state, state->match_state.players[network_get_player_id()].team, input.move.target_cell);
    // If clicked on hidden fog or the minimap, then don't check for entity click
    if (fog_value != FOG_HIDDEN && !match_shell_is_mouse_in_ui()) {
        for (uint32_t entity_index = 0; entity_index < state->match_state.entities.size(); entity_index++) {
            const Entity& entity = state->match_state.entities[entity_index];
            // Can't select units under explored fog
            if (fog_value == FOG_EXPLORED && entity_is_unit(entity.type)) {
                continue;
            }
            // Non-units *can* be selected under explored fog, but only if we have explored that the building exists
            if (fog_value == FOG_EXPLORED) {
                auto it = state->match_state.remembered_entities[player_team].find(state->match_state.entities.get_id_of(entity_index));
                if (it == state->match_state.remembered_entities[player_team].end()) {
                    continue;
                }
            }
            // Don't target unselectable units
            if (!entity_is_selectable(entity)) {
                continue;
            }
            // Don't target invisible units (unless we have detection)
            if (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state->match_state.detection[player_team][input.move.target_cell.x + (input.move.target_cell.y * state->match_state.map.width)] == 0) {
                continue;
            }

            Rect entity_rect = entity_get_rect(state->match_state.entities[entity_index]);
            if (entity_rect.has_point(move_target)) {
                if (input.move.target_id == ID_NULL || 
                        (state->match_state.entities[entity_index].type == ENTITY_BALLOON && !input_move_target_is_balloon)) {
                    input.move.target_id = state->match_state.entities.get_id_of(entity_index);
                    input_move_target_is_balloon = state->match_state.entities[entity_index].type == ENTITY_BALLOON;
                }
                if (fog_value == FOG_EXPLORED) {
                    remembered_entity_id = state->match_state.entities.get_id_of(entity_index);
                }
            }
        }
    }

    if (state->mode == MATCH_SHELL_MODE_TARGET_UNLOAD) {
        input.type = MATCH_INPUT_MOVE_UNLOAD;
    } else if (state->mode == MATCH_SHELL_MODE_TARGET_REPAIR) {
        input.type = MATCH_INPUT_MOVE_REPAIR;
    } else if (state->mode == MATCH_SHELL_MODE_TARGET_MOLOTOV) {
        input.type = MATCH_INPUT_MOVE_MOLOTOV;
    } else if (input.move.target_id != ID_NULL && state->match_state.entities.get_by_id(input.move.target_id).type != ENTITY_GOLDMINE &&
               (state->mode == MATCH_SHELL_MODE_TARGET_ATTACK || 
                state->match_state.players[state->match_state.entities.get_by_id(input.move.target_id).player_id].team != state->match_state.players[network_get_player_id()].team)) {
        input.type = MATCH_INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && state->mode == MATCH_SHELL_MODE_TARGET_ATTACK) {
        input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    } else if (input.move.target_id != ID_NULL) {
        input.type = MATCH_INPUT_MOVE_ENTITY;
    } else {
        input.type = MATCH_INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = state->selection.size();
    memcpy(input.move.entity_ids, &state->selection[0], state->selection.size() * sizeof(EntityId));

    if (input.type == MATCH_INPUT_MOVE_REPAIR) {
        bool is_repair_target_valid = true;
        if (input.move.target_id == ID_NULL) {
            is_repair_target_valid = false;
        } else {
            const Entity& repair_target = state->match_state.entities.get_by_id(input.move.target_id);
            if (state->match_state.players[repair_target.player_id].team != state->match_state.players[network_get_player_id()].team || 
                    !entity_is_building(repair_target.type)) {
                is_repair_target_valid = false;
            }
        }

        if (!is_repair_target_valid) {
            match_shell_show_status(state, MATCH_UI_STATUS_REPAIR_TARGET_INVALID);
            state->mode = MATCH_SHELL_MODE_NONE;
            return;
        }
    }

    state->input_queue.push_back(input);

    // Play animation
    if (remembered_entity_id != ID_NULL) {
        EntityType remembered_entity_type = state->match_state.remembered_entities[player_team][remembered_entity_id].type;
        state->move_animation = animation_create(remembered_entity_type == ENTITY_GOLDMINE
                                                        ? ANIMATION_UI_MOVE_ENTITY 
                                                        : ANIMATION_UI_MOVE_ATTACK_ENTITY);
        state->move_animation_position = cell_center(input.move.target_cell).to_ivec2();
        state->move_animation_entity_id = remembered_entity_id;
    } else if (input.type == MATCH_INPUT_MOVE_CELL || input.type == MATCH_INPUT_MOVE_ATTACK_CELL || input.type == MATCH_INPUT_MOVE_UNLOAD || input.type == MATCH_INPUT_MOVE_MOLOTOV) {
        state->move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
        state->move_animation_position = move_target;
        state->move_animation_entity_id = ID_NULL;
    } else {
        state->move_animation = animation_create(input.type == MATCH_INPUT_MOVE_ATTACK_ENTITY ? ANIMATION_UI_MOVE_ATTACK_ENTITY : ANIMATION_UI_MOVE_ENTITY);
        state->move_animation_position = cell_center(input.move.target_cell).to_ivec2();
        state->move_animation_entity_id = input.move.target_id;
    }

    // Reset UI mode if targeting
    if (match_shell_is_targeting(state) && !input_is_action_pressed(INPUT_ACTION_SHIFT)) {
        state->mode = MATCH_SHELL_MODE_NONE;
    }
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
            state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT || 
            state->mode == MATCH_SHELL_MODE_DESYNC;
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

bool match_shell_selection_has_enough_energy(const MatchShellState* state, const std::vector<EntityId>& selection, uint32_t cost) {
    for (EntityId id : selection) {
        if (state->match_state.entities.get_by_id(id).energy >= cost) {
            return true;
            break;
        }
    }

    return false;
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

// STATUS

void match_shell_show_status(MatchShellState* state, const char* message) {
    state->status_message = std::string(message);
    state->status_timer = STATUS_DURATION;
}

// BUILDING PLACEMENT

ivec2 match_shell_get_building_cell(int building_size, ivec2 camera_offset) {
    ivec2 offset = building_size == 1 ? ivec2(0, 0) : ivec2(building_size, building_size) - ivec2(2, 2);
    ivec2 building_cell = ((input_get_mouse_position() + camera_offset) / TILE_SIZE) - offset;
    building_cell.x = std::max(0, building_cell.x);
    building_cell.y = std::max(0, building_cell.y);
    return building_cell;
}

bool match_shell_building_can_be_placed(const MatchShellState* state) {
    const EntityData& entity_data = entity_get_data(state->building_type);
    ivec2 building_cell = match_shell_get_building_cell(entity_data.cell_size, state->camera_offset);
    ivec2 miner_cell = state->match_state.entities.get_by_id(match_get_nearest_builder(state->match_state, state->selection, building_cell)).cell;

    // Check that the building is in bounds
    if (!map_is_cell_rect_in_bounds(state->match_state.map, building_cell, entity_data.cell_size)) {
        return false;
    }

    // If building a hall, check that it is not too close to goldmines
    if (state->building_type == ENTITY_HALL) {
        Rect building_rect = (Rect) {
            .x = building_cell.x, .y = building_cell.y,
            .w = entity_data.cell_size, .h = entity_data.cell_size
        };

        for (const Entity& entity : state->match_state.entities) {
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
            if (!match_shell_is_building_place_cell_valid(state, miner_cell, ivec2(x, y))) {
                return false;
            }
        }
    }

    return true;
}

bool match_shell_is_building_place_cell_valid(const MatchShellState* state, ivec2 miner_cell, ivec2 cell) {
    // Check that no entities are occupying the building rect
    Cell map_ground_cell = map_get_cell(state->match_state.map, CELL_LAYER_GROUND, cell);
    if (map_ground_cell.type == CELL_UNIT || map_ground_cell.type == CELL_MINER || map_ground_cell.type == CELL_BUILDING) {
        // If the entity is not visible to the player, then we will ignore the fact that it's blocking the building
        if (cell != miner_cell &&
                (entity_is_visible_to_player(state->match_state, state->match_state.entities.get_by_id(map_ground_cell.id), network_get_player_id()) ||
                (map_ground_cell.type == CELL_BUILDING && state->match_state.remembered_entities[network_get_player_id()].find(map_ground_cell.id) != state->match_state.remembered_entities[network_get_player_id()].end()))) {
            return false;
        }
    } else if (map_ground_cell.type != CELL_EMPTY) {
        return false;
    }

    // Check that we're not building on a ramp
    if (map_is_tile_ramp(state->match_state.map, cell)) {
        return false;
    } 

    // Check that we're not building on top of a landmine
    Cell map_underground_cell = map_get_cell(state->match_state.map, CELL_LAYER_UNDERGROUND, cell);
    if (map_underground_cell.type == CELL_BUILDING && 
            entity_is_visible_to_player(state->match_state, state->match_state.entities.get_by_id(map_underground_cell.id), network_get_player_id())) {
        return false;
    }

    // Check that we're not building on hidden fog
    if (match_get_fog(state->match_state, state->match_state.players[network_get_player_id()].team, cell) == FOG_HIDDEN) {
        return false;
    }

    return true;
}

// HOTKEY MENU

Rect match_shell_get_selection_list_item_rect(uint32_t selection_index) {
    ivec2 pos = SELECTION_LIST_TOP_LEFT + ivec2(((selection_index % 10) * 68) - 24, (selection_index / 10) * 68);
    return (Rect) { .x = pos.x, .y = pos.y, .w = 64, .h = 64 };
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
        case MATCH_SHELL_MODE_DESYNC:
            return "Desync Detected";
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

bool match_shell_is_in_single_player_game() {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == network_get_player_id()) {
            continue;
        }
        if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_READY) {
            return false;
        }
    }

    return true;
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

// DESYNC

bool match_shell_are_checksums_out_of_sync(MatchShellState* state, uint32_t frame) {
    // If we haven't gotten to this frame yet, then we can't say for sure that it's out of sync
    if (state->checksums[network_get_player_id()].size() <= frame) {
        return false;
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state->match_state.players[player_id].active) {
            continue;
        }
        if (state->checksums[player_id].size() <= frame) {
            continue;
        }
        if (state->checksums[player_id][frame] != state->checksums[network_get_player_id()][frame]) {
            return true;
        }
    }

    return false;
}

void match_shell_compare_checksums(MatchShellState* state, uint32_t frame) {
    if (match_shell_are_checksums_out_of_sync(state, frame)) {
        log_error("DESYNC on frame %u", frame);

        state->mode = MATCH_SHELL_MODE_DESYNC;

        #ifdef GOLD_DEBUG_DESYNC
            size_t state_buffer_length;
            uint8_t* state_buffer = desync_read_serialized_frame(frame, &state_buffer_length);
            if (state_buffer == NULL) {
                log_error("match_shell_compare_checksums could not read serialized frame.");
                return;
            }

            network_send_serialized_frame(state_buffer, state_buffer_length);
            free(state_buffer);
            log_info("DESYNC Sent serialized frame with size %llu.", state_buffer_length);
        #endif
    }
}

#ifdef GOLD_DEBUG_DESYNC

void match_shell_handle_serialized_frame(uint8_t* incoming_state_buffer, size_t incoming_state_buffer_length) {
        uint32_t frame;
        memcpy(&frame, incoming_state_buffer + sizeof(uint8_t), sizeof(frame));
        log_info("DESYNC Received serialized frame %u with size %llu", frame, incoming_state_buffer_length);

        size_t state_buffer_length;
        uint8_t* state_buffer = desync_read_serialized_frame(frame, &state_buffer_length);
        if (state_buffer == NULL) {
            log_error("match_shell_handle_serialized_frame could not read serialized frame.");
            return;
        }

        size_t header_size = sizeof(uint8_t) + sizeof(uint32_t);
        uint32_t checksum = desync_compute_buffer_checksum(state_buffer + header_size, state_buffer_length - header_size);
        uint32_t checksum2 = desync_compute_buffer_checksum(incoming_state_buffer + header_size, incoming_state_buffer_length - header_size);
        log_info("DESYNC Comparing serialized / incoming frame %u. size %llu / %llu. checksum %u / %u", frame, state_buffer_length - header_size, incoming_state_buffer_length - header_size, checksum, checksum2);
        desync_compare_frames(state_buffer + sizeof(uint8_t) + sizeof(uint32_t), incoming_state_buffer + sizeof(uint8_t) + sizeof(uint32_t));
        free(state_buffer);
        log_info("DESYNC Finished comparing frames.");
}

#endif

// RENDER

void match_shell_render(const MatchShellState* state) {
    std::vector<RenderSpriteParams> above_fog_sprite_params;
    std::vector<RenderSpriteParams> ysort_params;

    ivec2 base_pos = ivec2(-(state->camera_offset.x % TILE_SIZE), -(state->camera_offset.y % TILE_SIZE));
    ivec2 base_coords = ivec2(state->camera_offset.x / TILE_SIZE, state->camera_offset.y / TILE_SIZE);
    ivec2 max_visible_tiles = ivec2(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    // Begin elevation passes
    static const int ELEVATION_COUNT = 2;
    for (uint32_t elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        // Render map
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                int map_index = (base_coords.x + x) + ((base_coords.y + y) * state->match_state.map.width);
                Tile tile = state->match_state.map.tiles[map_index];

                RenderSpriteParams tile_params = (RenderSpriteParams) {
                    .sprite = tile.sprite,
                    .frame = tile.frame,
                    .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                    .options = RENDER_SPRITE_NO_CULL,
                    .recolor_id = 0
                };

                bool should_render_on_ground_level = (tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_WATER) || map_is_tile_ramp(state->match_state.map, base_coords + ivec2(x, y));
                if (!(tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_WATER) && elevation == 0) {
                    render_sprite_frame(SPRITE_TILE_SAND1, ivec2(0, 0), base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
                }
                if ((should_render_on_ground_level && elevation == 0) || (!should_render_on_ground_level && elevation == tile.elevation)) {
                    render_sprite_frame(tile_params.sprite, tile_params.frame, tile_params.position, tile_params.options, tile_params.recolor_id);
                } 

                // Decorations
                Cell cell = state->match_state.map.cells[CELL_LAYER_GROUND][map_index];
                if (cell.type >= CELL_DECORATION_1 && cell.type <= CELL_DECORATION_5 && tile.elevation == elevation) {
                    ysort_params.push_back((RenderSpriteParams) {
                        .sprite = SPRITE_DECORATION,
                        .frame = ivec2(cell.type - CELL_DECORATION_1, 0),
                        .position = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE),
                        .options = RENDER_SPRITE_NO_CULL,
                        .recolor_id = 0
                    });
                }

                #ifdef GOLD_DEBUG
                    if (state->debug_show_region_lines && elevation == tile.elevation) {
                        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
                            ivec2 neighbor = base_coords + ivec2(x, y) + DIRECTION_IVEC2[direction];
                            if (!map_is_cell_in_bounds(state->match_state.map, neighbor) || map_get_region(state->match_state.map, neighbor) == state->match_state.map.regions[map_index]) {
                                continue;
                            }
                            ivec2 tile_pos = base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE);
                            if (direction == DIRECTION_NORTH) {
                                render_draw_rect((Rect) {
                                    .x = tile_pos.x, .y = tile_pos.y,
                                    .w = TILE_SIZE, .h = 1
                                }, RENDER_COLOR_WHITE);
                            } else if (direction == DIRECTION_SOUTH) {
                                render_draw_rect((Rect) {
                                    .x = tile_pos.x, .y = tile_pos.y + TILE_SIZE - 1,
                                    .w = TILE_SIZE, .h = 1
                                }, RENDER_COLOR_WHITE);
                            } else if (direction == DIRECTION_WEST) {
                                render_draw_rect((Rect) {
                                    .x = tile_pos.x, .y = tile_pos.y, 
                                    .w = 1, .h = TILE_SIZE
                                }, RENDER_COLOR_WHITE);
                            } else if (direction == DIRECTION_EAST) {
                                render_draw_rect((Rect) {
                                    .x = tile_pos.x + TILE_SIZE - 1, .y = tile_pos.y, 
                                    .w = 1, .h = TILE_SIZE
                                }, RENDER_COLOR_WHITE);
                            }
                        }
                    }
                #endif
            }  // End for each x
        } // End for each y

        // For each cell layer
        for (int cell_layer = CELL_LAYER_UNDERGROUND; cell_layer < CELL_LAYER_GROUND + 1; cell_layer++) {
            // Dead entities
            for (uint32_t entity_index = 0; entity_index < state->match_state.entities.size(); entity_index++) {
                const Entity& entity = state->match_state.entities[entity_index];
                const EntityData& entity_data = entity_get_data(entity.type);
                if (!(entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED)) {
                    continue;
                }
                if (entity_data.cell_layer != cell_layer || 
                        entity_get_elevation(entity, state->match_state.map) != elevation) {
                    continue;
                }
                if (!match_shell_is_entity_visible(state, entity)) {
                    continue;
                }
                
                RenderSpriteParams params = match_shell_create_entity_render_params(state, entity);
                render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
            }

            // Select rings and healthbars
            for (EntityId id : state->selection) {
                const Entity& entity = state->match_state.entities.get_by_id(id);
                const EntityData& entity_data = entity_get_data(entity.type);
                if (entity_data.cell_layer != cell_layer || 
                        entity_get_elevation(entity, state->match_state.map) != elevation) {
                    continue;
                }
                match_shell_render_entity_select_rings_and_healthbars(state, entity);
            }

            // Move animation
            if (animation_is_playing(state->move_animation) &&
                    map_get_tile(state->match_state.map, state->move_animation_position / TILE_SIZE).elevation == elevation) {
                if (state->move_animation.name == ANIMATION_UI_MOVE_CELL && cell_layer == CELL_LAYER_GROUND) {
                    RenderSpriteParams params = (RenderSpriteParams) {
                        .sprite = SPRITE_UI_MOVE,
                        .frame = state->move_animation.frame,
                        .position = state->move_animation_position - state->camera_offset,
                        .options = RENDER_SPRITE_CENTERED,
                        .recolor_id = 0
                    };
                    ivec2 ui_move_cell = state->move_animation_position / TILE_SIZE;
                    if (match_get_fog(state->match_state, state->match_state.players[network_get_player_id()].team, ui_move_cell) > 0) {
                        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                    } else {
                        above_fog_sprite_params.push_back(params);
                    }
                } else if (state->move_animation.frame.x % 2 == 0) {
                    uint32_t entity_index = state->match_state.entities.get_index_of(state->move_animation_entity_id);
                    if (entity_index != INDEX_INVALID) {
                        const Entity& entity = state->match_state.entities[entity_index];
                        if (entity_get_data(entity.type).cell_layer == cell_layer) {
                            match_shell_render_entity_move_animation(state, entity);
                        }
                    } else if (cell_layer == CELL_LAYER_GROUND) {
                        auto it = state->match_state.remembered_entities[state->match_state.players[network_get_player_id()].team].find(state->move_animation_entity_id);
                        if (it != state->match_state.remembered_entities[state->match_state.players[network_get_player_id()].team].end()) {
                            ivec2 entity_center_position = (it->second.cell * TILE_SIZE) + ((ivec2(it->second.cell_size, it->second.cell_size) * TILE_SIZE) / 2);

                            render_sprite_frame(match_shell_get_entity_select_ring(it->second.type, state->move_animation.name == ANIMATION_UI_MOVE_ATTACK_ENTITY), ivec2(0, 0), entity_center_position, RENDER_SPRITE_CENTERED, 0);
                        }
                    }
                }
            }

            // Underground entities
            if (cell_layer == CELL_LAYER_UNDERGROUND) {
                for (const Entity& entity : state->match_state.entities) {
                    const EntityData& entity_data = entity_get_data(entity.type);
                    if (entity_data.cell_layer != CELL_LAYER_UNDERGROUND ||
                            entity_get_elevation(entity, state->match_state.map) != elevation) {
                        continue;
                    }
                    if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
                        continue;
                    }
                    if (!match_shell_is_entity_visible(state, entity)) {
                        continue;
                    }

                    RenderSpriteParams params = match_shell_create_entity_render_params(state, entity);
                    render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
                }
            }
        } // End for each cell layer
    } // End for each elevation

    // Fires
    for (const Fire& fire : state->match_state.fires) {
        if (match_shell_get_fire_cell_render(state, fire) == FIRE_CELL_RENDER_BELOW) {
            render_sprite_frame(SPRITE_PARTICLE_FIRE, fire.animation.frame, (fire.cell * TILE_SIZE) - state->camera_offset, 0, 0);
        }
    }

    // Entities
    for (uint32_t entity_index = 0; entity_index < state->match_state.entities.size(); entity_index++) {
        const Entity& entity = state->match_state.entities[entity_index];
        const EntityData& entity_data = entity_get_data(entity.type);
        if (entity.mode == MODE_UNIT_DEATH_FADE || 
                entity.mode == MODE_BUILDING_DESTROYED ||
                entity_data.cell_layer != CELL_LAYER_GROUND) {
            continue;
        }
        if (!match_shell_is_entity_visible(state, entity)) {
            continue;
        }

        RenderSpriteParams params = match_shell_create_entity_render_params(state, entity);
        const SpriteInfo& sprite_info = render_get_sprite_info(entity_get_sprite(state->match_state, entity));
        Rect render_rect = (Rect) {
            .x = params.position.x, .y = params.position.y,
            .w = sprite_info.frame_width * 2, .h = sprite_info.frame_height * 2
        };

        if (entity.bleed_timer != 0) {
            const SpriteInfo& bleed_sprite_info = render_get_sprite_info(SPRITE_PARTICLE_BLEED);
            RenderSpriteParams bleed_params = (RenderSpriteParams) {
                .sprite = SPRITE_PARTICLE_BLEED,
                .frame = entity.bleed_animation.frame,
                .position = ivec2(render_rect.x + (render_rect.w / 2) - bleed_sprite_info.frame_width, 
                                  render_rect.y + (render_rect.h / 2) - bleed_sprite_info.frame_height),
                .options = RENDER_SPRITE_NO_CULL,
                .recolor_id = 0
            };
            Rect bleed_render_rect = (Rect) {
                .x = bleed_params.position.x, .y = bleed_params.position.y,
                .w = bleed_sprite_info.frame_width, .h = bleed_sprite_info.frame_height
            };
            if (bleed_render_rect.intersects(SCREEN_RECT)) {
                ysort_params.push_back(bleed_params);
            }
        }

        if (!render_rect.intersects(SCREEN_RECT)) {
            continue;
        }
        params.options |= RENDER_SPRITE_NO_CULL;

        ysort_params.push_back(params);
    }

    // Remembered entities
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->replay_mode && (state->replay_fog_index == 0 || state->replay_fog_index == 1)) {
            break;
        }
        if (state->replay_mode && state->replay_fog_player_ids[state->replay_fog_index] != player_id) {
            continue;
        }
        if (!state->replay_mode && player_id != network_get_player_id()) {
            continue;
        }

        for (auto it : state->match_state.remembered_entities[state->match_state.players[player_id].team]) {
            // Don't draw the remembered entity if we can see it, otherwise we will double draw them
            if (match_shell_is_cell_rect_revealed(state, it.second.cell, it.second.cell_size)) {
                continue;
            }

            const EntityData& entity_data = entity_get_data(it.second.type);
            const SpriteInfo& sprite_info = render_get_sprite_info(entity_data.sprite);

            RenderSpriteParams params = (RenderSpriteParams) {
                .sprite = entity_data.sprite,
                .frame = it.second.frame,
                .position = (it.second.cell * TILE_SIZE) - state->camera_offset,
                .options = RENDER_SPRITE_NO_CULL,
                .recolor_id = it.second.recolor_id
            };

            Rect render_rect = (Rect) {
                .x = params.position.x, .y = params.position.y,
                .w = sprite_info.frame_width * 2, .h = sprite_info.frame_height * 2
            };
            if (!render_rect.intersects(SCREEN_RECT)) {
                continue;
            }

            ysort_params.push_back(params);
        }
    }

    // Rally points
    uint32_t selection_type = match_shell_get_selection_type(state, state->selection);
    if (selection_type == MATCH_SHELL_SELECTION_BUILDINGS || (state->replay_mode && selection_type == MATCH_SHELL_SELECTION_ENEMY_BUILDING)) {
        for (EntityId id : state->selection) {
            const Entity& building = state->match_state.entities.get_by_id(id);
            if (building.mode != MODE_BUILDING_FINISHED || building.rally_point.x == -1) {
                continue;
            }

            RenderSpriteParams params = (RenderSpriteParams) {
                .sprite = SPRITE_RALLY_FLAG,
                .frame = state->rally_flag_animation.frame,
                .position = building.rally_point - ivec2(8, 30) - state->camera_offset,
                .options = 0,
                .recolor_id = state->match_state.players[building.player_id].recolor_id
            };

            ivec2 rally_cell = building.rally_point / TILE_SIZE;
            if (match_shell_get_fog(state, rally_cell) > 0) {
                ysort_params.push_back(params);
            } else {
                above_fog_sprite_params.push_back(params);
            }
        }
    }

    // Render ysort params
    match_shell_ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
    for (const RenderSpriteParams& params : ysort_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }

    // Balloon shadows
    for (const Entity& entity : state->match_state.entities) {
        if (entity.type == ENTITY_BALLOON && entity.mode != MODE_UNIT_DEATH_FADE &&
                match_shell_is_entity_visible(state, entity)) {
            render_sprite_frame(SPRITE_UNIT_BALLOON_SHADOW, ivec2(0, 0), entity.position.to_ivec2() + ivec2(-10, 6) - state->camera_offset, 0, 0);
        }
    }

    // Building fires
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

        const EntityData& entity_data = entity_get_data(entity.type);
        for (int x = entity.cell.x; x < entity.cell.x + entity_data.cell_size; x++) {
            ivec2 fire_position = (ivec2(x, entity.cell.y + entity_data.cell_size - 1) * TILE_SIZE) - state->camera_offset;
            render_sprite_frame(SPRITE_PARTICLE_FIRE, state->building_fire_animation.frame, fire_position, 0, 0);
        }
    }

    // Fires above units
    for (const Fire& fire : state->match_state.fires) {
        if (match_shell_get_fire_cell_render(state, fire) == FIRE_CELL_RENDER_ABOVE) {
            render_sprite_frame(SPRITE_PARTICLE_FIRE, fire.animation.frame, (fire.cell * TILE_SIZE) - state->camera_offset, 0, 0);
        }
    }

    // Smith and workshop smoke animations
    for (const Entity& entity : state->match_state.entities) {
        if (!match_shell_is_entity_visible(state, entity)) {
            continue;
        }
        if (entity.animation.name == ANIMATION_WORKSHOP) {
            // Haha this is awful
            int hframe = -1;
            switch (entity.animation.frame.x) {
                case 5:
                    hframe = 0;
                    break;
                case 6:
                case 8:
                case 10:
                    hframe = 1;
                    break;
                case 7:
                case 9:
                case 11:
                    hframe = 2;
                    break;
                case 12:
                    hframe = 3;
                    break;
                case 13:
                    hframe = 4;
                    break;
                default:
                    break;
            }
            if (hframe != -1) {
                Rect building_rect = entity_get_rect(entity);
                ivec2 steam_position = ivec2(building_rect.x, building_rect.y) + (ivec2(building_rect.w, building_rect.h) / 2) - state->camera_offset;
                render_sprite_frame(SPRITE_WORKSHOP_STEAM, ivec2(hframe, 0), steam_position, RENDER_SPRITE_CENTERED, 0);
            }
        } else if (entity.type == ENTITY_SMITH && entity.animation.name != ANIMATION_UNIT_IDLE) {
            render_sprite_frame(SPRITE_BUILDING_SMITH_ANIMATION, entity.animation.frame, entity.position.to_ivec2() - ivec2(0, 32) - state->camera_offset, 0, 0);
        }
    }

    // Ground particles
    for (const Particle& particle : state->match_state.particles[PARTICLE_LAYER_GROUND]) {
        match_shell_render_particle(state, particle);
    }

    // Projectiles
    for (const Projectile& projectile : state->match_state.projectiles) {
        if (!match_shell_is_cell_rect_revealed(state, projectile.position.to_ivec2() / TILE_SIZE, 1)) {
            continue;
        }
        uint32_t options = RENDER_SPRITE_CENTERED;
        if (projectile.position.x > projectile.target.x) {
            options |= RENDER_SPRITE_FLIP_H;
        }
        render_sprite_frame(SPRITE_PROJECTILE_MOLOTOV, ivec2(0, 0), projectile.position.to_ivec2() - state->camera_offset, options, 0);
    }

    // Miners on gold counter
    for (uint32_t entity_index = 0; entity_index < state->match_state.entities.size(); entity_index++) {
        const Entity& entity = state->match_state.entities[entity_index];
        // Make sure the entity is actually a goldmine
        if (entity.type != ENTITY_GOLDMINE || entity.mode != MODE_GOLDMINE) {
            continue;
        }
        // Make sure the player can see the goldmine
        if (!match_shell_is_cell_rect_revealed(state, entity.cell, entity_get_data(entity.type).cell_size)) {
            continue;
        }
        // Make sure the goldmine is actually on screen
        Rect entity_rect = entity_get_rect(entity);
        entity_rect.x -= state->camera_offset.x;
        entity_rect.y -= state->camera_offset.y;
        if (!SCREEN_RECT.intersects(entity_rect)) {
            continue;
        }

        const SpriteInfo& miner_icon_info = render_get_sprite_info(SPRITE_UI_MINER_ICON);
        int icon_y_offset = 0;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (!state->replay_mode && player_id != network_get_player_id()) {
                continue;
            }

            // Count how many miners are mining from this mine
            EntityId entity_id = state->match_state.entities.get_id_of(entity_index);
            if (player_id == PLAYER_NONE) {
                continue;
            }
            uint32_t miner_count = match_get_miners_on_gold(state->match_state, entity_id, player_id);
            if (miner_count == 0) {
                continue;
            }

            char counter_text[4];
            sprintf(counter_text, "%u", miner_count);
            ivec2 text_size = render_get_text_size(FONT_HACK_WHITE, counter_text);
            text_size.x += miner_icon_info.frame_width * 2;
            ivec2 text_pos = ivec2(entity_rect.x + (entity_rect.w / 2) - (text_size.x / 2), entity_rect.y + 12) + ivec2(0, icon_y_offset);
            render_text(miner_count > MATCH_MAX_MINERS_ON_GOLD ? FONT_HACK_PLAYER1 : FONT_HACK_WHITE, counter_text, text_pos + ivec2((miner_icon_info.frame_width * 2) + 4, 0));
            render_sprite_frame(SPRITE_UI_MINER_ICON, ivec2(0, 0), text_pos, RENDER_SPRITE_NO_CULL, state->match_state.players[player_id].recolor_id);
            icon_y_offset -= (miner_icon_info.frame_height * 2) + 2;
        }
    }

    // Sky entities select rings
    for (EntityId entity_id : state->selection) {
        const Entity& entity = state->match_state.entities.get_by_id(entity_id);
        const EntityData& entity_data = entity_get_data(entity.type);
        if (entity_data.cell_layer != CELL_LAYER_SKY) {
            continue;
        }
        match_shell_render_entity_select_rings_and_healthbars(state, entity);
    }

    // Sky entity move animation
    if (animation_is_playing(state->move_animation) && 
            state->move_animation.name != ANIMATION_UI_MOVE_CELL &&
            state->move_animation.frame.x % 2 == 0) {
        uint32_t entity_index = state->match_state.entities.get_index_of(state->move_animation_entity_id);
        if (entity_index != INDEX_INVALID) {
            const Entity& entity = state->match_state.entities[entity_index];
            if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
                match_shell_render_entity_move_animation(state, entity);
            }
        }
    }

    // Sky entities
    ysort_params.clear();
    for (const Entity& entity : state->match_state.entities) {
        const EntityData& entity_data = entity_get_data(entity.type);
        if (entity.mode == MODE_UNIT_DEATH_FADE || 
                entity.mode == MODE_BUILDING_DESTROYED ||
                entity_data.cell_layer != CELL_LAYER_SKY) {
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
        if (!render_rect.intersects(SCREEN_RECT)) {
            continue;
        }
        params.options |= RENDER_SPRITE_NO_CULL;

        ysort_params.push_back(params);
    }
    match_shell_ysort_render_params(ysort_params, 0, ysort_params.size() - 1);
    for (const RenderSpriteParams& params : ysort_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        if (params.sprite == SPRITE_UNIT_BALLOON) {
            if ((params.frame.x == 3 || params.frame.x == 4) && params.frame.y != 22) {
                int steam_hframe = params.frame.x - 3;
                uint32_t options = RENDER_SPRITE_NO_CULL;
                if (params.frame.y == 1) {
                    options |= RENDER_SPRITE_FLIP_H;
                }
                render_sprite_frame(SPRITE_UNIT_BALLOON_STEAM, ivec2(steam_hframe, 0), params.position, options, 0);
            }
        }
    }

    // Sky particles
    for (const Particle& particle : state->match_state.particles[PARTICLE_LAYER_SKY]) {
        match_shell_render_particle(state, particle);
    }

    // Fog of War
    for (int fog_pass = 0; fog_pass < 2; fog_pass++) {
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                ivec2 fog_cell = base_coords + ivec2(x, y);
                int fog = match_shell_get_fog(state, fog_cell);
                if (fog > 0) {
                    continue;
                }
                if (fog_pass == 1 && fog == FOG_EXPLORED) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    ivec2 neighbor_cell = fog_cell + DIRECTION_IVEC2[direction];
                    if (!map_is_cell_in_bounds(state->match_state.map, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if ((fog_pass == 0 && match_shell_get_fog(state, neighbor_cell) < 1) ||
                        (fog_pass != 0 && match_shell_get_fog(state, neighbor_cell) == FOG_HIDDEN)) {
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
                    if (!map_is_cell_in_bounds(state->match_state.map, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if ((fog_pass == 0 && match_shell_get_fog(state, neighbor_cell) < 1) ||
                        (fog_pass != 0 && match_shell_get_fog(state, neighbor_cell) == FOG_HIDDEN)) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                int autotile_index = map_neighbors_to_autotile_index(neighbors);
                #ifdef GOLD_DEBUG
                    if (state->debug_fog == DEBUG_FOG_DISABLED) {
                        continue;
                    }
                #endif
                render_sprite_frame(fog_pass == 0 ? SPRITE_FOG_EXPLORED : SPRITE_FOG_HIDDEN, 
                        ivec2(autotile_index % AUTOTILE_HFRAMES, autotile_index / AUTOTILE_HFRAMES), 
                        base_pos + ivec2(x * TILE_SIZE, y * TILE_SIZE), RENDER_SPRITE_NO_CULL, 0);
            }
        }
    }

    // Above fog params
    for (const RenderSpriteParams& params : above_fog_sprite_params) {
        render_sprite_frame(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }

    // UI Building Placement
    if (state->mode == MATCH_SHELL_MODE_BUILDING_PLACE && !match_shell_is_mouse_in_ui()) {
        const EntityData& building_data = entity_get_data(state->building_type);

        // First draw the building
        ivec2 building_cell = match_shell_get_building_cell(building_data.cell_size, state->camera_offset);
        render_sprite_frame(building_data.sprite, ivec2(3, 0), (building_cell * TILE_SIZE) - state->camera_offset, RENDER_SPRITE_NO_CULL, state->match_state.players[network_get_player_id()].recolor_id);

        // Then draw the green / red squares
        ivec2 miner_cell = state->match_state.entities.get_by_id(match_get_nearest_builder(state->match_state, state->selection, building_cell)).cell;
        for (int y = building_cell.y; y < building_cell.y + building_data.cell_size; y++) {
            for (int x = building_cell.x; x < building_cell.x + building_data.cell_size; x++) {
                ivec2 cell = ivec2(x, y);
                bool is_cell_red = !match_shell_is_building_place_cell_valid(state, miner_cell, cell);
                
                // Don't allow buildings too close to goldmines
                if (state->building_type == ENTITY_HALL) {
                    for (const Entity& goldmine : state->match_state.entities) {
                        if (goldmine.type == ENTITY_GOLDMINE && entity_goldmine_get_block_building_rect(goldmine.cell).has_point(cell)) {
                            is_cell_red = true;
                            break;
                        }
                    }
                }

                RenderColor cell_color = is_cell_red ? RENDER_COLOR_RED_TRANSPARENT : RENDER_COLOR_GREEN_TRANSPARENT;
                Rect cell_rect = (Rect) {
                    .x = (x * TILE_SIZE) - state->camera_offset.x,
                    .y = (y * TILE_SIZE) - state->camera_offset.y,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                render_fill_rect(cell_rect, cell_color);
            }
        }
    }
    // End UI building placement

    // UI queued building placements
    for (EntityId entity_id : state->selection) {
        const Entity& entity = state->match_state.entities.get_by_id(entity_id);
        // If it's not an allied unit, then we can break out of this whole loop, since the rest of the selection won't be either
        if (!entity_is_unit(entity.type) || 
                (!state->replay_mode && entity.player_id != network_get_player_id())) {
            break;
        }

        if (entity.target.type == TARGET_BUILD && entity.target.id == ID_NULL) {
            match_shell_render_target_build(state, entity.target, entity.player_id);
        }
        for (const Target& target : entity.target_queue) {
            if (target.type == TARGET_BUILD) {
                match_shell_render_target_build(state, target, entity.player_id);
            }
        }
    }

    // UI Chat
    for (uint32_t chat_index = 0; chat_index < state->chat.size(); chat_index++) {
        const ChatMessage& message = state->chat[state->chat.size() - chat_index - 1];
        ivec2 message_pos = ivec2(32, MINIMAP_RECT.y - 96 - (chat_index * 32));
        if (message.player_id != PLAYER_NONE) {
            const MatchPlayer& player = state->match_state.players[message.player_id];
            char player_text[40];
            sprintf(player_text, "%s: ", player.name);
            render_text((FontName)(FONT_HACK_PLAYER0 + player.recolor_id), player_text, message_pos);
            message_pos.x += render_get_text_size(FONT_HACK_WHITE, player_text).x;
        }
        render_text(FONT_HACK_WHITE, message.message.c_str(), message_pos);
    }
    if (input_is_text_input_active()) {
        char prompt_str[128];
        sprintf(prompt_str, "Chat: %s", state->chat_message.c_str());
        render_text(FONT_HACK_WHITE, prompt_str, ivec2(32, MINIMAP_RECT.y - 64));
        if (state->chat_cursor_visible) {
            int prompt_width = render_get_text_size(FONT_HACK_WHITE, prompt_str).x;
            ivec2 cursor_pos = ivec2(32 + prompt_width - 2, MINIMAP_RECT.y - 65);
            render_text(FONT_HACK_WHITE, "|", cursor_pos);
        }
    }

    // Select rect
    if (match_shell_is_selecting(state)) {
        ivec2 mouse_world_pos = ivec2(input_get_mouse_position().x, std::min(input_get_mouse_position().y, SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT)) + state->camera_offset;
        Rect select_rect = (Rect) {
            .x = std::min(state->select_origin.x, mouse_world_pos.x) - state->camera_offset.x, 
            .y = std::min(state->select_origin.y, mouse_world_pos.y) - state->camera_offset.y,
            .w = std::abs(state->select_origin.x - mouse_world_pos.x),
            .h = std::abs(state->select_origin.y - mouse_world_pos.y)
        };
        render_draw_rect(select_rect, RENDER_COLOR_WHITE);
    }

    // UI frames
    const SpriteInfo& minimap_sprite_info = render_get_sprite_info(SPRITE_UI_MINIMAP);
    render_sprite_frame(SPRITE_UI_MINIMAP, ivec2(0, 0), ivec2(0, SCREEN_HEIGHT - minimap_sprite_info.frame_height * 2), 0, 0);
    render_ninepatch(SPRITE_UI_FRAME_BOLTS, BOTTOM_PANEL_RECT);
    if (state->replay_mode) {
        render_ninepatch(SPRITE_UI_FRAME_BOLTS, REPLAY_PANEL_RECT);
    } else {
        render_ninepatch(SPRITE_UI_FRAME_BOLTS, BUTTON_PANEL_RECT);
        render_sprite_frame(SPRITE_UI_WANTED_SIGN, ivec2(0, 0), WANTED_SIGN_POSITION, 0, 0);
    }

    // UI Control groups
    for (int control_group_index = 0; control_group_index < MATCH_SHELL_CONTROL_GROUP_COUNT; control_group_index++) {
        // Count the entities in this control group and determine which is the most common
        uint32_t entity_count = 0;
        std::unordered_map<EntityType, uint32_t> entity_occurances;
        EntityType most_common_entity_type = ENTITY_MINER;
        entity_occurances[ENTITY_MINER] = 0;

        for (EntityId id : state->control_groups[control_group_index]) {
            uint32_t entity_index = state->match_state.entities.get_index_of(id);
            if (entity_index == INDEX_INVALID || !entity_is_selectable(state->match_state.entities[entity_index])) {
                continue;
            }

            EntityType entity_type = state->match_state.entities[entity_index].type;
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
        if (state->control_group_selected != control_group_index) {
            font = FONT_M3X6_DARKBLACK;
            button_frame = 2;
        }

        SpriteName button_icon = entity_get_icon(state->match_state, most_common_entity_type, network_get_player_id());
        const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_CONTROL_GROUP);
        ivec2 render_pos = ivec2(BOTTOM_PANEL_RECT.x, BOTTOM_PANEL_RECT.y) + ivec2(4, 0) + ivec2((6 + (sprite_info.frame_width * 2)) * control_group_index, -64);
        render_sprite_frame(SPRITE_UI_CONTROL_GROUP, ivec2(button_frame, 0), render_pos, RENDER_SPRITE_NO_CULL, 0);
        render_sprite_frame(button_icon, ivec2(button_frame, 0), render_pos + ivec2(2, 0), RENDER_SPRITE_NO_CULL, 0);
        char control_group_number_text[4];
        sprintf(control_group_number_text, "%u", control_group_index == 9 ? 0 : control_group_index + 1);
        render_text(font, control_group_number_text, render_pos + ivec2(6, -18));
        char control_group_count_text[4];
        sprintf(control_group_count_text, "%u", entity_count);
        ivec2 count_text_size = render_get_text_size(font, control_group_count_text);
        render_text(font, control_group_count_text, render_pos + ivec2(64 - count_text_size.x, 46 - count_text_size.y));
    }

    // UI Status message
    if (state->status_timer != 0) {
        int status_message_width = render_get_text_size(FONT_HACK_WHITE, state->status_message.c_str()).x;
        render_text(FONT_HACK_WHITE, state->status_message.c_str(), ivec2((SCREEN_WIDTH / 2) - (status_message_width / 2), SCREEN_HEIGHT - 266));
    }
    
    // Hotkeys
    for (uint32_t hotkey_index = 0; hotkey_index < HOTKEY_GROUP_SIZE; hotkey_index++) {
        InputAction hotkey = state->hotkey_group[hotkey_index];
        if (hotkey == INPUT_HOTKEY_NONE) {
            continue;
        }

        const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
        ivec2 hotkey_position = SHELL_BUTTON_POSITIONS[hotkey_index];
        Rect hotkey_rect = (Rect) { 
            .x = hotkey_position.x, .y = hotkey_position.y,
            .w = sprite_info.frame_width * 2, .h = sprite_info.frame_height * 2
        };
        int hframe = 0;
        if (!match_does_player_meet_hotkey_requirements(state->match_state, network_get_player_id(), hotkey)) {
            hframe = 2;
        } else if (!(state->is_minimap_dragging || match_shell_is_selecting(state)) && hotkey_rect.has_point(input_get_mouse_position())) {
            hframe = 1;
            hotkey_position.y -= 2;
        }

        render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(hframe, 0), hotkey_position, RENDER_SPRITE_NO_CULL, 0);
        render_sprite_frame(match_shell_hotkey_get_sprite(state, hotkey, match_shell_should_render_hotkey_toggled(state, hotkey)), ivec2(hframe, 0), hotkey_position, RENDER_SPRITE_NO_CULL, 0);
    }

    // UI Tooltip
    if (!(state->is_minimap_dragging || match_shell_is_selecting(state))) {
        uint32_t hotkey_hovered_index;
        for (hotkey_hovered_index = 0; hotkey_hovered_index < HOTKEY_GROUP_SIZE; hotkey_hovered_index++) {
            Rect hotkey_rect = (Rect) {
                .x = SHELL_BUTTON_POSITIONS[hotkey_hovered_index].x,
                .y = SHELL_BUTTON_POSITIONS[hotkey_hovered_index].y,
                .w = 64,
                .h = 64
            };
            if (hotkey_rect.has_point(input_get_mouse_position())) {
                break;
            }
        }

        // If we are actually hovering a hotkey
        if (hotkey_hovered_index != HOTKEY_GROUP_SIZE) {
            InputAction hotkey_hovered = state->hotkey_group[hotkey_hovered_index];
            if (hotkey_hovered != INPUT_HOTKEY_NONE) {
                const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey_hovered);

                bool show_toggle = match_shell_should_render_hotkey_toggled(state, hotkey_hovered);

                // Get tooltip info
                char tooltip_text[64];
                char tooltip_desc[128];
                uint32_t tooltip_gold_cost;
                uint32_t tooltip_population_cost;
                uint32_t tooltip_energy_cost;

                sprintf(tooltip_desc, "%s", hotkey_get_desc(hotkey_hovered));

                char* tooltip_text_ptr = tooltip_text;
                tooltip_text_ptr += hotkey_get_name(tooltip_text_ptr, hotkey_hovered, show_toggle);
                tooltip_text_ptr += sprintf(tooltip_text_ptr, " (");
                tooltip_text_ptr += input_sprintf_sdl_scancode_str(tooltip_text_ptr, input_get_hotkey_mapping(hotkey_hovered));
                tooltip_text_ptr += sprintf(tooltip_text_ptr, ")");

                switch (hotkey_info.type) {
                    case HOTKEY_BUTTON_ACTION:
                    case HOTKEY_BUTTON_TOGGLED_ACTION: {
                        tooltip_gold_cost = 0;
                        tooltip_population_cost = 0;
                        if (hotkey_hovered == INPUT_HOTKEY_MOLOTOV) {
                            tooltip_energy_cost = MOLOTOV_ENERGY_COST;
                        } else if (hotkey_hovered == INPUT_HOTKEY_CAMO) {
                            tooltip_energy_cost = CAMO_ENERGY_COST;
                        } else {
                            tooltip_energy_cost = 0;
                        }
                        break;
                    }
                    case HOTKEY_BUTTON_TRAIN:
                    case HOTKEY_BUTTON_BUILD: {
                        const EntityData& entity_data = entity_get_data(hotkey_info.entity_type);
                        bool costs_energy = entity_is_building(hotkey_info.entity_type) 
                                                ? (entity_data.building_data.options & BUILDING_COSTS_ENERGY) == BUILDING_COSTS_ENERGY
                                                : false;
                        tooltip_gold_cost = costs_energy ? 0 : entity_data.gold_cost;
                        tooltip_population_cost = hotkey_info.type == HOTKEY_BUTTON_TRAIN ? entity_data.unit_data.population_cost : 0;
                        tooltip_energy_cost = costs_energy ? entity_data.gold_cost : 0;
                        break;
                    }
                    case HOTKEY_BUTTON_RESEARCH: {
                        const UpgradeData& upgrade_data = upgrade_get_data(hotkey_info.upgrade);
                        tooltip_gold_cost = upgrade_data.gold_cost;
                        tooltip_population_cost = 0;
                        tooltip_energy_cost = 0;
                        break;
                    }
                }

                if (!match_does_player_meet_hotkey_requirements(state->match_state, network_get_player_id(), hotkey_hovered)) {
                    switch (hotkey_info.requirements.type) {
                        case HOTKEY_REQUIRES_NONE: {
                            GOLD_ASSERT(false);
                            break;
                        }
                        case HOTKEY_REQUIRES_BUILDING: {
                            sprintf(tooltip_desc, "Requires %s", entity_get_data(hotkey_info.requirements.building).name);
                            break;
                        }
                        case HOTKEY_REQUIRES_UPGRADE: {
                            sprintf(tooltip_desc, "Requires %s", upgrade_get_data(hotkey_info.requirements.upgrade).name);
                            break;
                        }
                    }
                    tooltip_gold_cost = 0;
                    tooltip_population_cost = 0;
                    tooltip_energy_cost = 0;
                }

                // Render the tooltip frame
                bool tooltip_has_desc = strlen(tooltip_desc) != 0;
                int tooltip_text_width = render_get_text_size(FONT_WESTERN8_OFFBLACK, tooltip_text).x;
                if (tooltip_has_desc) {
                    tooltip_text_width = std::max(tooltip_text_width, render_get_text_size(FONT_HACK_OFFBLACK, tooltip_desc).x);
                }
                int tooltip_min_width = 20 + tooltip_text_width;
                int tooltip_cell_width = tooltip_min_width / 16;
                int tooltip_cell_height = 3;
                if (tooltip_gold_cost != 0 || tooltip_energy_cost != 0) {
                    tooltip_cell_height += 2;
                }
                if (tooltip_has_desc) {
                    tooltip_cell_height += 2;
                }
                if (tooltip_min_width % 16 != 0) {
                    tooltip_cell_width++;
                }
                ivec2 tooltip_top_left = ivec2(
                    SCREEN_WIDTH - (tooltip_cell_width * 16) - 3,
                    BUTTON_PANEL_RECT.y - (tooltip_cell_height * 16) - 4
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

                        render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, frame, tooltip_top_left + (ivec2(x, y) * 16), RENDER_SPRITE_NO_CULL, 0);
                    }
                }

                // Render tooltip text
                int tooltip_item_y = 10;
                render_text(FONT_WESTERN8_OFFBLACK, tooltip_text, tooltip_top_left + ivec2(10, tooltip_item_y));
                tooltip_item_y = 42;
                if (tooltip_has_desc) {
                    render_text(FONT_HACK_OFFBLACK, tooltip_desc, tooltip_top_left + ivec2(10, 10 + 32));
                    tooltip_item_y = 70;
                }

                // Render gold icon and text
                if (tooltip_gold_cost != 0) {
                    render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), tooltip_top_left + ivec2(10, tooltip_item_y), RENDER_SPRITE_NO_CULL, 0);
                    char gold_text[4];
                    sprintf(gold_text, "%u", tooltip_gold_cost);
                    render_text(FONT_WESTERN8_OFFBLACK, gold_text, tooltip_top_left + ivec2(46, tooltip_item_y));
                }

                // Render population icon and text
                if (tooltip_population_cost != 0) {
                    render_sprite_frame(SPRITE_UI_HOUSE_ICON, ivec2(0, 0), tooltip_top_left + ivec2(10 + 36 + 64, tooltip_item_y - 4), RENDER_SPRITE_NO_CULL, 0);
                    char population_text[4];
                    sprintf(population_text, "%u", tooltip_population_cost);
                    render_text(FONT_WESTERN8_OFFBLACK, population_text, tooltip_top_left + ivec2(10 + 36 + 64 + 44, tooltip_item_y + 4));
                }

                // Render energy text
                if (tooltip_energy_cost != 0) {
                    render_sprite_frame(SPRITE_UI_ENERGY_ICON, ivec2(0, 0), tooltip_top_left + ivec2(10, tooltip_item_y), RENDER_SPRITE_NO_CULL, 0);
                    char energy_text[4];
                    sprintf(energy_text, "%u", tooltip_energy_cost);
                    render_text(FONT_WESTERN8_OFFBLACK, energy_text, tooltip_top_left + ivec2(44, tooltip_item_y));
                }
            } 
        }
    }
    // End render tooltip

    // UI Selection list
    if (state->selection.size() == 1) {
        const Entity& entity = state->match_state.entities.get_by_id(state->selection[0]);
        const EntityData& entity_data = entity_get_data(entity.type);

        // Entity name
        const SpriteInfo& frame_sprite_info = render_get_sprite_info(SPRITE_UI_TEXT_FRAME);
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, entity_data.name);
        int frame_count = (text_size.x / (frame_sprite_info.frame_width * 2)) + 1;
        if (text_size.x % (frame_sprite_info.frame_width * 2) != 0) {
            frame_count++;
        }
        for (int frame = 0; frame < frame_count; frame++) {
            int hframe = 1;
            if (frame == 0) {
                hframe = 0;
            } else if (frame == frame_count - 1) {
                hframe = 2;
            }
            render_sprite_frame(SPRITE_UI_TEXT_FRAME, ivec2(hframe, 0), SELECTION_LIST_TOP_LEFT + ivec2(frame * frame_sprite_info.frame_width * 2, 0), RENDER_SPRITE_NO_CULL, 0);
        }
        ivec2 frame_size = ivec2(frame_count * frame_sprite_info.frame_width * 2, frame_sprite_info.frame_height * 2);
        render_text(FONT_WESTERN8_OFFBLACK, entity_data.name, SELECTION_LIST_TOP_LEFT + ivec2((frame_size.x / 2) - (text_size.x / 2), 0));

        // Entity icon
        render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(0, 36), RENDER_SPRITE_NO_CULL, 0);
        render_sprite_frame(entity_get_icon(state->match_state, entity.type, entity.player_id), ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(0, 36), RENDER_SPRITE_NO_CULL, 0);

        if (entity.type == ENTITY_GOLDMINE) {
            if (entity.mode == MODE_GOLDMINE_COLLAPSED) {
                render_text(FONT_HACK_WHITE, "Collapsed!", SELECTION_LIST_TOP_LEFT + ivec2(72, 40));
            } else {
                char gold_left_str[8];
                sprintf(gold_left_str, "%u", entity.gold_held);
                render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), SELECTION_LIST_TOP_LEFT + ivec2(72, 40), RENDER_SPRITE_NO_CULL, 0);
                render_text(FONT_HACK_WHITE, gold_left_str, SELECTION_LIST_TOP_LEFT + ivec2(72 + (render_get_sprite_info(SPRITE_UI_GOLD_ICON).frame_width * 2) + 4, 42));
            }
        } else {
            ivec2 healthbar_position = SELECTION_LIST_TOP_LEFT + ivec2(68, 36 + 4);
            ivec2 healthbar_size = ivec2(128, 24);
            match_shell_render_healthbar(RENDER_HEALTHBAR, healthbar_position, healthbar_size, entity.health, entity_data.max_health);

            char health_text[16];
            sprintf(health_text, "%i/%i", entity.health, entity_data.max_health);
            ivec2 health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
            ivec2 health_text_position = healthbar_position + (healthbar_size / 2) - (health_text_size / 2) + ivec2(0, 1); 
            render_text(FONT_HACK_WHITE, health_text, health_text_position);

            uint32_t entity_max_energy = entity_get_max_energy(state->match_state, entity);
            if (entity_is_unit(entity.type) && entity_max_energy != 0)  {
                healthbar_position += ivec2(0, healthbar_size.y + 2);
                match_shell_render_healthbar(RENDER_ENERGY_BAR, healthbar_position, healthbar_size, entity.energy, entity_max_energy);
                sprintf(health_text, "%i/%i", entity.energy, entity_max_energy);
                health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
                health_text_position = healthbar_position + (healthbar_size / 2) - (health_text_size / 2); 
                render_text(FONT_HACK_WHITE, health_text, health_text_position);
            }

            SpriteName stat_icons[4];
            int stat_count = 0;

            // Detection
            if (entity_has_detection(state->match_state, entity)) {
                stat_icons[stat_count] = SPRITE_UI_STAT_ICON_DETECTION;
                stat_count++;
            }

            // Bleeding
            if (entity.bleed_timer != 0) {
                stat_icons[stat_count] = SPRITE_UI_STAT_ICON_BLEED;
                stat_count++;
            }

            ivec2 stat_position = SELECTION_LIST_TOP_LEFT + ivec2(0, 36 + 68); 
            ivec2 stat_positions[2];
            const int STAT_ICON_SIZE = render_get_sprite_info(SPRITE_UI_STAT_ICON_DETECTION).frame_width * 2;
            int STAT_TEXT_PADDING = 2;

            for (int stat_index = 0; stat_index < stat_count; stat_index++) {
                render_sprite_frame(stat_icons[stat_index], ivec2(0, 0), stat_position, RENDER_SPRITE_NO_CULL, 0);
                stat_positions[stat_index] = stat_position;
                stat_position.y += STAT_ICON_SIZE + STAT_TEXT_PADDING; 
            }

            // Determine if a stat is being hovered
            int stat_hovered;
            for (stat_hovered = 0; stat_hovered < stat_count; stat_hovered++) {
                Rect stat_rect = (Rect) {
                    .x = stat_positions[stat_hovered].x,
                    .y = stat_positions[stat_hovered].y,
                    .w = STAT_ICON_SIZE, .h = STAT_ICON_SIZE
                };
                if (stat_rect.has_point(input_get_mouse_position())) {
                    break;
                }
            }

            // Render stat tooltip
            if (stat_hovered != stat_count) {
                const SpriteInfo& tooltip_info = render_get_sprite_info(SPRITE_UI_TOOLTIP_FRAME);
                const char* tooltip_text = match_shell_render_get_stat_tooltip(stat_icons[stat_hovered]);

                // Determine tooltip size
                ivec2 tooltip_text_size = render_get_text_size(FONT_HACK_OFFBLACK, tooltip_text);
                int tooltip_width = tooltip_text_size.x + (tooltip_info.frame_width * 2);
                int tooltip_frame_count = tooltip_width / (tooltip_info.frame_width * 2);
                if (tooltip_width % (tooltip_info.frame_width * 2) != 0) {
                    tooltip_frame_count++;
                }

                // Determine tooltip position
                ivec2 tooltip_pos = stat_positions[stat_hovered] + ivec2(STAT_ICON_SIZE - 4, -((tooltip_info.frame_height * 4) - 4));

                // Render tooltip background
                for (int frame_x = 0; frame_x < tooltip_frame_count; frame_x++) {
                    int hframe = 1;
                    if (frame_x == 0) {
                        hframe = 0;
                    } else if (frame_x == tooltip_frame_count - 1) {
                        hframe = 2;
                    }

                    render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, ivec2(hframe, 0), tooltip_pos + ivec2(tooltip_info.frame_width * 2 * frame_x, 0), RENDER_SPRITE_NO_CULL, 0);
                    render_sprite_frame(SPRITE_UI_TOOLTIP_FRAME, ivec2(hframe, 2), tooltip_pos + ivec2(tooltip_info.frame_width * 2 * frame_x, tooltip_info.frame_height * 2), RENDER_SPRITE_NO_CULL, 0);
                }

                // Render tooltip text
                render_text(FONT_HACK_OFFBLACK, tooltip_text, tooltip_pos + ivec2(8, 4));
            }
        }
    } else {
        for (uint32_t selection_index = 0; selection_index < state->selection.size(); selection_index++) {
            match_shell_render_entity_icon(
                state, 
                state->match_state.entities.get_by_id(state->selection[selection_index]), 
                match_shell_get_selection_list_item_rect(selection_index));
        }
    }
    // End UI Selection List

    // UI Building queues
    if (state->selection.size() == 1) {
        const Entity& building = state->match_state.entities.get_by_id(state->selection[0]);
        if (entity_is_building(building.type) && 
                !building.queue.empty() &&
                (state->replay_mode || building.player_id == network_get_player_id())) {
            // Render building queue icon buttons
            const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
                Rect icon_rect = (Rect) {
                    .x = BUILDING_QUEUE_POSITIONS[building_queue_index].x,
                    .y = BUILDING_QUEUE_POSITIONS[building_queue_index].y,
                    .w = icon_sprite_info.frame_width * 2,
                    .h = icon_sprite_info.frame_height * 2
                };
                SpriteName item_sprite;
                switch (building.queue[building_queue_index].type) {
                    case BUILDING_QUEUE_ITEM_UNIT: {
                        item_sprite = entity_get_icon(state->match_state, building.queue[building_queue_index].unit_type, building.player_id);
                        break;
                    }
                    case BUILDING_QUEUE_ITEM_UPGRADE: {
                        item_sprite = upgrade_get_data(building.queue[building_queue_index].upgrade).icon;
                        break;
                    }
                }
                bool hovered = !(state->is_minimap_dragging || match_shell_is_selecting(state)) && 
                                    icon_rect.has_point(input_get_mouse_position());
                render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - ((int)hovered * 2)), RENDER_SPRITE_NO_CULL, 0);
                render_sprite_frame(item_sprite, ivec2(hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - ((int)hovered * 2)), RENDER_SPRITE_NO_CULL, 0);
            }

            // Render building queue progress bar
            if (building.timer == BUILDING_QUEUE_BLOCKED) {
                render_text(FONT_WESTERN8_GOLD, "Build more houses.", ivec2(BUILDING_QUEUE_PROGRESS_BAR_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_RECT.y - 24));
            } else if (building.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                render_text(FONT_WESTERN8_GOLD, "Exit is blocked.", ivec2(BUILDING_QUEUE_PROGRESS_BAR_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_RECT.y - 24));
            } else {
                int item_duration = (int)building_queue_item_duration(building.queue[0]);
                Rect building_queue_progress_bar_subrect = BUILDING_QUEUE_PROGRESS_BAR_RECT;
                building_queue_progress_bar_subrect.w = (BUILDING_QUEUE_PROGRESS_BAR_RECT.w * (item_duration - (int)building.timer) / item_duration);
                render_fill_rect(building_queue_progress_bar_subrect, RENDER_COLOR_WHITE);
                render_draw_rect(BUILDING_QUEUE_PROGRESS_BAR_RECT, RENDER_COLOR_OFFBLACK);
            }
        }
    }

    // UI Garrisoned units
    if (state->selection.size() == 1) {
        const Entity& carrier = state->match_state.entities.get_by_id(state->selection[0]);
        if (carrier.type == ENTITY_GOLDMINE || 
                state->replay_mode ||
                carrier.player_id == network_get_player_id()) {
            const SpriteInfo& icon_sprite_info = render_get_sprite_info(SPRITE_UI_ICON_BUTTON);
            int index = 0;
            for (EntityId entity_id : carrier.garrisoned_units) {
                const Entity& garrisoned_unit = state->match_state.entities.get_by_id(entity_id);
                // We have to make this check here because goldmines might have both allied and enemy units in them
                if (!state->replay_mode && garrisoned_unit.player_id != network_get_player_id()) {
                    continue;
                }

                Rect icon_rect = (Rect) {
                    .x = GARRISON_ICON_POSITIONS[index].x,
                    .y = GARRISON_ICON_POSITIONS[index].y,
                    .w = icon_sprite_info.frame_width * 2,
                    .h = icon_sprite_info.frame_height * 2
                };
                match_shell_render_entity_icon(state, garrisoned_unit, icon_rect);
                index++;
            }
        }
    }

    // Resource counters
    const SpriteInfo& population_icon_sprite_info = render_get_sprite_info(SPRITE_UI_HOUSE_ICON);
    const SpriteInfo& gold_icon_sprite_info = render_get_sprite_info(SPRITE_UI_GOLD_ICON);
    int resource_base_y = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if ((!state->replay_mode && player_id != network_get_player_id()) || 
                (state->replay_mode && !state->match_state.players[player_id].active)) {
            continue;
        }

        int render_x = SCREEN_WIDTH;

        // Rendering from right to left here

        // Population text
        render_x -= (render_get_text_size(FONT_HACK_WHITE, "200/200").x + 4);
        char population_text[8];
        sprintf(population_text, "%u/%u", match_get_player_population(state->match_state, player_id), match_get_player_max_population(state->match_state, player_id));
        render_text(FONT_HACK_WHITE, population_text, ivec2(render_x, resource_base_y + 6));

        // Population icon
        render_x -= ((population_icon_sprite_info.frame_width * 2) + 4);
        render_sprite_frame(SPRITE_UI_HOUSE_ICON, ivec2(0, 0), ivec2(render_x, resource_base_y), RENDER_SPRITE_NO_CULL, 0);

        // Gold text
        render_x -= (render_get_text_size(FONT_HACK_WHITE, "99999").x + 4);
        char gold_text[8];
        sprintf(gold_text, "%u", state->displayed_gold_amounts[player_id]);
        render_text(FONT_HACK_WHITE, gold_text, ivec2(render_x, resource_base_y + 6));

        // Gold icon
        render_x -= ((gold_icon_sprite_info.frame_width * 2) + 4);
        render_sprite_frame(SPRITE_UI_GOLD_ICON, ivec2(0, 0), ivec2(render_x, resource_base_y + 4), RENDER_SPRITE_NO_CULL, 0);

        // Player name
        if (state->replay_mode) {
            render_x -= (render_get_text_size(FONT_HACK_WHITE, state->match_state.players[player_id].name).x + 4);
            render_text((FontName)(FONT_HACK_PLAYER0 + state->match_state.players[player_id].recolor_id), state->match_state.players[player_id].name, ivec2(render_x, resource_base_y + 6));
        }

        resource_base_y += population_icon_sprite_info.frame_height * 2;
    }

    // Menu button icon
    {
        const SpriteInfo& sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_BURGER);
        Rect menu_button_rect = (Rect) {
            .x = MENU_BUTTON_POSITION.x, .y = MENU_BUTTON_POSITION.y,
            .w = sprite_info.frame_width * 2, .h = sprite_info.frame_height * 2
        };
        bool hovered = state->mode == MATCH_SHELL_MODE_MENU || state->mode == MATCH_SHELL_MODE_MENU_SURRENDER || (
                            !(state->is_minimap_dragging || match_shell_is_selecting(state)) && menu_button_rect.has_point(input_get_mouse_position()));
        render_sprite_frame(SPRITE_UI_BUTTON_BURGER, ivec2((int)hovered, 0), MENU_BUTTON_POSITION, RENDER_SPRITE_NO_CULL, 0);
    }

    // UI Disconnect frame
    if (state->disconnect_timer > DISCONNECT_GRACE) {
        render_ninepatch(SPRITE_UI_FRAME, DISCONNECT_FRAME_RECT);
        ivec2 text_size = render_get_text_size(FONT_HACK_GOLD, "Waiting for players...");
        render_text(FONT_HACK_GOLD, "Waiting for players...", ivec2(DISCONNECT_FRAME_RECT.x + (DISCONNECT_FRAME_RECT.w / 2) - (text_size.x / 2), DISCONNECT_FRAME_RECT.y + 16));
        int player_text_y = 64;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == NETWORK_PLAYER_STATUS_NONE || !(state->inputs[player_id].empty() || state->inputs[player_id].front().empty())) {
                continue;
            }

            render_text(FONT_HACK_GOLD, network_get_player(player_id).name, ivec2(DISCONNECT_FRAME_RECT.x + 16, DISCONNECT_FRAME_RECT.y + player_text_y));
            player_text_y += 32;
        }
    }

    // MINIMAP
    // Minimap tiles
    for (int y = 0; y < state->match_state.map.height; y++) {
        for (int x = 0; x < state->match_state.map.width; x++) {
            MinimapPixel pixel; 
            Tile tile = map_get_tile(state->match_state.map, ivec2(x, y));
            if (tile.sprite >= SPRITE_TILE_SAND1 && tile.sprite <= SPRITE_TILE_SAND3) {
                pixel = MINIMAP_PIXEL_SAND;
            } else if (tile.sprite == SPRITE_TILE_WATER) {
                pixel = MINIMAP_PIXEL_WATER;
            } else {
                pixel = MINIMAP_PIXEL_WALL;
            }
            render_minimap_putpixel(MINIMAP_LAYER_TILE, ivec2(x, y), pixel);
        }
    }
    // Minimap entities
    for (const Entity& entity : state->match_state.entities) {
        if (!entity_is_selectable(entity) || !match_shell_is_entity_visible(state, entity)) {
            continue;
        }

        MinimapPixel pixel;
        if (entity.type == ENTITY_GOLDMINE) {
            pixel = MINIMAP_PIXEL_GOLD;
        } else if (entity_check_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER)) {
            pixel = MINIMAP_PIXEL_WHITE;
        } else {
            pixel = (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + state->match_state.players[entity.player_id].recolor_id);
        }
        int entity_cell_size = entity_get_data(entity.type).cell_size;
        Rect entity_rect = (Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size, .h = entity_cell_size
        };
        render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, pixel);
    }
    // Minimap remembered entities
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->replay_mode && (state->replay_fog_index == 0 || state->replay_fog_index == 0)) {
            break;
        }
        if (state->replay_mode && state->replay_fog_player_ids[state->replay_fog_index] != player_id) {
            continue;
        }
        if (!state->replay_mode && player_id != network_get_player_id()) {
            continue;
        }

        for (auto it : state->match_state.remembered_entities[state->match_state.players[player_id].team]) {
            Rect entity_rect = (Rect) {
                .x = it.second.cell.x, .y = it.second.cell.y,
                .w = it.second.cell_size, .h = it.second.cell_size
            };
            MinimapPixel pixel = it.second.type == ENTITY_GOLDMINE ? MINIMAP_PIXEL_GOLD : (MinimapPixel)(MINIMAP_PIXEL_PLAYER0 + it.second.recolor_id);
            render_minimap_fill_rect(MINIMAP_LAYER_TILE, entity_rect, pixel);
        }
    }
    // Minimap fog of war
    for (int y = 0; y < state->match_state.map.height; y++) {
        for (int x = 0; x < state->match_state.map.width; x++) {
            int fog_value = match_shell_get_fog(state, ivec2(x, y));
            #ifdef GOLD_DEBUG
                if (state->debug_fog == DEBUG_FOG_DISABLED) {
                    fog_value = 1;
                }
            #endif
            MinimapPixel pixel;
            if (fog_value > 0) {
                pixel = MINIMAP_PIXEL_TRANSPARENT;
            } else if (fog_value == 0) {
                pixel = MINIMAP_PIXEL_OFFBLACK_TRANSPARENT;
            } else {
                pixel = MINIMAP_PIXEL_OFFBLACK;
            }

            render_minimap_putpixel(MINIMAP_LAYER_FOG, ivec2(x, y), pixel);
        }
    }
    // Minimap alerts
    for (const Alert& alert : state->alerts) {
        if (alert.timer <= ALERT_LINGER_DURATION) {
            continue;
        }

        int alert_timer = alert.timer - ALERT_LINGER_DURATION;
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
        render_minimap_draw_rect(MINIMAP_LAYER_FOG, alert_rect, alert.pixel);
    }
    // Minimap camera rect
    Rect camera_rect = (Rect) {
        .x = state->camera_offset.x / TILE_SIZE,
        .y = state->camera_offset.y / TILE_SIZE,
        .w = (SCREEN_WIDTH / TILE_SIZE) - 1,
        .h = ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / TILE_SIZE) - 1
    };
    render_minimap_draw_rect(MINIMAP_LAYER_FOG, camera_rect, MINIMAP_PIXEL_WHITE);
    render_minimap(ivec2(MINIMAP_RECT.x, MINIMAP_RECT.y), ivec2(state->match_state.map.width, state->match_state.map.height), ivec2(MINIMAP_RECT.w, MINIMAP_RECT.h));

    ui_render(state->ui);
    if (state->replay_mode) {
        ui_render(state->replay_ui);
    }
}

SpriteName match_shell_get_entity_select_ring(EntityType type, bool attacking) {
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

SpriteName match_shell_hotkey_get_sprite(const MatchShellState* state, InputAction hotkey, bool show_toggled) {
    const HotkeyButtonInfo& info = hotkey_get_button_info(hotkey);
    if (info.type == HOTKEY_BUTTON_BUILD || info.type == HOTKEY_BUTTON_TRAIN) {
        return entity_get_icon(state->match_state, info.entity_type, network_get_player_id());
    } else {
        return hotkey_get_sprite(hotkey, show_toggled);
    }
}

int match_shell_ysort_render_params_partition(std::vector<RenderSpriteParams>& params, int low, int high) {
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

void match_shell_ysort_render_params(std::vector<RenderSpriteParams>& params, int low, int high) {
    if (low < high) {
        int partition_index = match_shell_ysort_render_params_partition(params, low, high);
        match_shell_ysort_render_params(params, low, partition_index - 1);
        match_shell_ysort_render_params(params, partition_index + 1, high);
    }
}

void match_shell_render_healthbar(RenderHealthbarType type, ivec2 position, ivec2 size, int amount, int max) {
    Rect healthbar_rect = (Rect) { 
        .x = position.x, 
        .y = position.y, 
        .w = size.x, 
        .h = size.y 
    };

    // Cull the healthbar
    if (!healthbar_rect.intersects(SCREEN_RECT)) {
        return;
    }

    Rect healthbar_subrect = healthbar_rect;
    healthbar_subrect.w = (healthbar_subrect.w * amount) / max;
    
    RenderColor healthbar_color;
    if (type == RENDER_GARRISON_BAR) {
        healthbar_color = RENDER_COLOR_WHITE;
    } else if (type == RENDER_ENERGY_BAR) {
        healthbar_color = RENDER_COLOR_BLUE;
    } else if (healthbar_subrect.w <= healthbar_rect.w / 3) {
        healthbar_color = RENDER_COLOR_RED;
    } else {
        healthbar_color = RENDER_COLOR_GREEN;
    }

    render_fill_rect(healthbar_subrect, healthbar_color);
    render_draw_rect(healthbar_rect, RENDER_COLOR_OFFBLACK);

    if (type == RENDER_GARRISON_BAR) {
        for (int line_index = 1; line_index < max; line_index++) {
            int line_x = healthbar_rect.x + ((healthbar_rect.w * line_index) / max);
            render_vertical_line(line_x, healthbar_rect.y, healthbar_rect.y + healthbar_rect.h, RENDER_COLOR_OFFBLACK);
        }
    }
}

void match_shell_render_target_build(const MatchShellState* state, const Target& target, uint8_t player_id) {
    const EntityData& building_data = entity_get_data(target.build.building_type);
    Rect building_rect = (Rect) {
        .x = (target.build.building_cell.x * TILE_SIZE) - state->camera_offset.x,
        .y = (target.build.building_cell.y * TILE_SIZE) - state->camera_offset.y,
        .w = building_data.cell_size * TILE_SIZE,
        .h = building_data.cell_size * TILE_SIZE
    };
    render_sprite_frame(building_data.sprite, ivec2(3, 0), ivec2(building_rect.x, building_rect.y), 0, state->match_state.players[player_id].recolor_id);
    render_fill_rect(building_rect, RENDER_COLOR_GREEN_TRANSPARENT);
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

void match_shell_render_entity_select_rings_and_healthbars(const MatchShellState* state, const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    Rect entity_rect = entity_get_rect(entity);

    // Render select ring
    bool use_red_select_ring = state->replay_mode || entity.type == ENTITY_GOLDMINE 
                                    ? false 
                                    : state->match_state.players[entity.player_id].team != state->match_state.players[network_get_player_id()].team;
    SpriteName select_ring_sprite = match_shell_get_entity_select_ring(entity.type, use_red_select_ring);
    ivec2 entity_center_position = entity_is_unit(entity.type) 
            ? entity.position.to_ivec2()
            : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2)); 
    entity_center_position -= state->camera_offset;
    if (entity_data.cell_layer == CELL_LAYER_SKY) {
        entity_center_position.y += ENTITY_SKY_POSITION_Y_OFFSET;
    }
    render_sprite_frame(select_ring_sprite, ivec2(0, 0), entity_center_position, RENDER_SPRITE_CENTERED, 0);

    // Render healthbar
    ivec2 healthbar_position = ivec2(entity_rect.x, entity_rect.y + entity_rect.h + HEALTHBAR_PADDING) - state->camera_offset;
    if (entity_data.max_health != 0) {
        match_shell_render_healthbar(RENDER_HEALTHBAR, healthbar_position, ivec2(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, entity_data.max_health);
        healthbar_position.y += HEALTHBAR_HEIGHT + 2;
    }

    // Render garrison bar
    if (entity_data.garrison_capacity != 0 && (entity.type == ENTITY_GOLDMINE || state->replay_mode || 
            state->match_state.players[entity.player_id].team == state->match_state.players[network_get_player_id()].team)) {
        match_shell_render_healthbar(RENDER_GARRISON_BAR, healthbar_position, ivec2(entity_rect.w, HEALTHBAR_HEIGHT), (int)entity.garrisoned_units.size(), (int)entity_data.garrison_capacity);
        healthbar_position.y += HEALTHBAR_HEIGHT + 2;
    } 
    // Render energy bar
    uint32_t entity_max_energy = entity_get_max_energy(state->match_state, entity);
    if (entity_is_unit(entity.type) && entity_max_energy != 0) {
        match_shell_render_healthbar(RENDER_ENERGY_BAR, healthbar_position, ivec2(entity_rect.w, HEALTHBAR_HEIGHT), (int)entity.energy, (int)entity_max_energy);
    }
}

void match_shell_render_entity_icon(const MatchShellState* state, const Entity& entity, Rect icon_rect) {
    const EntityData& entity_data = entity_get_data(entity.type);
    bool icon_hovered = !(state->is_minimap_dragging || match_shell_is_selecting(state)) &&
                            icon_rect.has_point(input_get_mouse_position());

    render_sprite_frame(SPRITE_UI_ICON_BUTTON, ivec2(icon_hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - ((int)icon_hovered * 2)), RENDER_SPRITE_NO_CULL, 0);
    render_sprite_frame(entity_get_icon(state->match_state, entity.type, entity.player_id), ivec2(icon_hovered ? 1 : 0, 0), ivec2(icon_rect.x, icon_rect.y - ((int)icon_hovered * 2)), RENDER_SPRITE_NO_CULL, 0);
    ivec2 healthbar_position = ivec2(icon_rect.x + 1, icon_rect.y + 54 - ((int)icon_hovered * 2));
    ivec2 healthbar_size = ivec2(60, 8);
    uint32_t entity_max_energy = entity_get_max_energy(state->match_state, entity);
    if (entity_is_unit(entity.type) && entity_max_energy != 0) {
        match_shell_render_healthbar(RENDER_ENERGY_BAR, healthbar_position, healthbar_size, entity.energy, entity_max_energy);
        healthbar_position -= ivec2(0, healthbar_size.y + 2);
    }
    match_shell_render_healthbar(RENDER_HEALTHBAR, healthbar_position, healthbar_size, entity.health, entity_data.max_health);
}

void match_shell_render_entity_move_animation(const MatchShellState* state, const Entity& entity) {
    Rect entity_rect = entity_get_rect(entity);
    ivec2 entity_center_position = entity_is_unit(entity.type) 
            ? entity.position.to_ivec2()
            : ivec2(entity_rect.x + (entity_rect.w / 2), entity_rect.y + (entity_rect.h / 2)); 
    entity_center_position -= state->camera_offset;
    if (entity_get_data(entity.type).cell_layer == CELL_LAYER_SKY) {
        entity_center_position.y += ENTITY_SKY_POSITION_Y_OFFSET;
    }

    render_sprite_frame(match_shell_get_entity_select_ring(entity.type, state->move_animation.name == ANIMATION_UI_MOVE_ATTACK_ENTITY), ivec2(0, 0), entity_center_position, RENDER_SPRITE_CENTERED, 0);
}

void match_shell_render_particle(const MatchShellState* state, const Particle& particle) {
    // Check if particle can be seen by player
    if (!match_shell_is_cell_rect_revealed(state, particle.position / TILE_SIZE, 1)) {
        return;
    }

    render_sprite_frame(particle.sprite, ivec2(particle.animation.frame.x, particle.vframe), particle.position - state->camera_offset, RENDER_SPRITE_CENTERED, 0);
}

bool match_shell_should_render_hotkey_toggled(const MatchShellState* state, InputAction hotkey) {
    const HotkeyButtonInfo& hotkey_info = hotkey_get_button_info(hotkey);
    if (hotkey_info.type != HOTKEY_BUTTON_TOGGLED_ACTION) {
        return false;
    }

    const Entity& entity = state->match_state.entities.get_by_id(state->selection[0]);
    if (hotkey == INPUT_HOTKEY_CAMO && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE)) {
        return true;
    }

    return false;
}

const char* match_shell_render_get_stat_tooltip(SpriteName sprite) {
    switch (sprite) {
        case SPRITE_UI_STAT_ICON_DETECTION:
            return "Detection";
        case SPRITE_UI_STAT_ICON_BLEED:
            return "Bleeding";
        default:
            log_warn("Unhandled stat tooltip icon of %u", sprite);
            return "";
    }
}

FireCellRender match_shell_get_fire_cell_render(const MatchShellState* state, const Fire& fire) {
    if (!match_shell_is_cell_rect_revealed(state, fire.cell, 1)) {
        return FIRE_CELL_DO_NOT_RENDER;
    }
    Cell map_fire_cell = map_get_cell(state->match_state.map, CELL_LAYER_GROUND, fire.cell);
    if (map_fire_cell.type == CELL_EMPTY) {
        return FIRE_CELL_RENDER_BELOW;
    }
    if (map_fire_cell.type == CELL_BUILDING) {
        return FIRE_CELL_DO_NOT_RENDER;
    }
    if (map_fire_cell.type >= CELL_DECORATION_1 && map_fire_cell.type <= CELL_DECORATION_5) {
        return FIRE_CELL_RENDER_ABOVE;
    }
    if (!(map_fire_cell.type == CELL_UNIT || map_fire_cell.type == CELL_MINER)) {
        return FIRE_CELL_DO_NOT_RENDER;
    }

    const Entity& unit = state->match_state.entities.get_by_id(map_fire_cell.id);

    if (unit.mode == MODE_UNIT_DEATH_FADE) {
        return FIRE_CELL_RENDER_ABOVE;
    }
    bool is_bottom_row = fire.cell.y == unit.cell.y + (entity_get_data(unit.type).cell_size - 1);
    if (!is_bottom_row) {
        return FIRE_CELL_RENDER_BELOW;
    }
    if (unit.mode == MODE_UNIT_MOVE && DIRECTION_IVEC2[unit.direction].y == -1) {
        return FIRE_CELL_RENDER_BELOW;
    }
    return FIRE_CELL_RENDER_ABOVE;
}
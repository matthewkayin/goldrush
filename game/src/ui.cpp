#include "ui.h"

#include "logger.h"
#include "network.h"
#include "lcg.h"
#include <algorithm>

static const uint32_t TURN_DURATION = 4;
static const uint32_t TURN_OFFSET = 3;
static const uint32_t MATCH_DISCONNECT_GRACE = 10;
static const uint32_t UI_STATUS_DURATION = 60;

static const int CAMERA_DRAG_MARGIN = 4;

static const SDL_Rect UI_DISCONNECT_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 100, .y = 32,
    .w = 200, .h = 200
};
static const SDL_Rect UI_MATCH_OVER_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (250 / 2), .y = 96,
    .w = 250, .h = 90
};
static const SDL_Rect UI_MATCH_OVER_CONTINUE_BUTTON_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 60, .y = UI_MATCH_OVER_FRAME_RECT.y + 32,
    .w = 120, .h = 21
};
static const SDL_Rect UI_MATCH_OVER_EXIT_BUTTON_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 60, .y = UI_MATCH_OVER_CONTINUE_BUTTON_RECT.y + UI_MATCH_OVER_CONTINUE_BUTTON_RECT.h + 4,
    .w = 120, .h = 21
};
static const SDL_Rect UI_MENU_BUTTON_RECT = (SDL_Rect) {
    .x = 1, .y = 1, .w = 19, .h = 18
};
static const SDL_Rect UI_MENU_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (150 / 2), .y = 64,
    .w = 150, .h = 125
};
static const int UI_MENU_BUTTON_COUNT = 3;
static const int UI_MENU_SURRENDER_BUTTON_COUNT = 2;
static const SDL_Rect UI_MENU_BUTTON_RECTS[UI_MENU_BUTTON_COUNT] = {
    (SDL_Rect) {
        .x = (SCREEN_WIDTH / 2) - 60, .y = UI_MENU_RECT.y + 32,
        .w = 120, .h = 21
    },
    (SDL_Rect) {
        .x = (SCREEN_WIDTH / 2) - 60, .y = UI_MENU_RECT.y + 32 + 21 + 5,
        .w = 120, .h = 21
    },
    (SDL_Rect) {
        .x = (SCREEN_WIDTH / 2) - 60, .y = UI_MENU_RECT.y + 32 + 21 + 5 + 21 + 5,
        .w = 120, .h = 21
    },
};
static const char* UI_MENU_BUTTON_TEXT[UI_MENU_BUTTON_COUNT] = { "LEAVE MATCH", "OPTIONS", "BACK" };
static const char* UI_MENU_SURRENDER_BUTTON_TEXT[UI_MENU_SURRENDER_BUTTON_COUNT] = { "YES", "BACK" };
const xy UI_FRAME_BOTTOM_POSITION = xy(136, SCREEN_HEIGHT - UI_HEIGHT);
const xy SELECTION_LIST_TOP_LEFT = UI_FRAME_BOTTOM_POSITION + xy(12 + 16, 12);
const xy BUILDING_QUEUE_TOP_LEFT = xy(164, 12);
const xy UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX] = {
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT,
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(0, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36 * 2, 35),
    UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(36 * 3, 35)
};
static const uint32_t UI_DOUBLE_CLICK_DURATION = 16;

const uint32_t UI_ALERT_DURATION = 90;
const uint32_t UI_ALERT_LINGER_DURATION = 60 * 20;
const uint32_t UI_ALERT_TOTAL_DURATION = UI_ALERT_DURATION + UI_ALERT_LINGER_DURATION;
const uint32_t UI_ATTACK_ALERT_DISTANCE = 20;

static const int HEALTHBAR_HEIGHT = 4;
static const int HEALTHBAR_PADDING = 3;
static const int BUILDING_HEALTHBAR_PADDING = 5;

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
const SDL_Rect UI_MINIMAP_RECT = (SDL_Rect) {
    .x = 4,
    .y = SCREEN_HEIGHT - 132,
    .w = 128,
    .h = 128
};
const SDL_Rect UI_CHAT_RECT = (SDL_Rect) { .x = 16, .y = UI_MINIMAP_RECT.y - 32, .w = 128, .h = 16 };

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
    }},
    { UI_BUTTON_BUILD_SMITH, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_SALOON
    }},
    { UI_BUTTON_BUILD_COOP, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_SMITH
    }},
    { UI_BUTTON_BUILD_BARRACKS, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_SMITH
    }},
    { UI_BUTTON_UNIT_TINKER, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_SMITH
    }},
    { UI_BUTTON_UNIT_SAPPER, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_UPGRADE,
        .upgrade = UPGRADE_EXPLOSIVES
    }},
    { UI_BUTTON_SMOKE, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_UPGRADE,
        .upgrade = UPGRADE_SMOKE
    }},
    { UI_BUTTON_BUILD_MINE, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_UPGRADE,
        .upgrade = UPGRADE_EXPLOSIVES
    }},
    { UI_BUTTON_RESEARCH_BAYONETS, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_BARRACKS
    }},
    { UI_BUTTON_BUILD_SHERIFFS, (ui_button_requirements_t) {
        .type = UI_BUTTON_REQUIRES_BUILDING,
        .building_type = ENTITY_BARRACKS
    }},
};

ui_state_t ui_init(int32_t lcg_seed, const noise_t& noise) {
    ui_state_t state;

    state.mode = UI_MODE_MATCH_NOT_STARTED;

    for (int button_index = 0; button_index < UI_BUTTONSET_SIZE; button_index++) {
        state.buttons[button_index] = UI_BUTTON_NONE;
    }

    state.turn_timer = 0;
    state.disconnect_timer = 0;

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const player_t& player = network_get_player(player_id);
        if (player.status == PLAYER_STATUS_NONE) {
            continue;
        }

        // Init input queues
        input_t empty_input;
        empty_input.type = INPUT_NONE;
        std::vector<input_t> empty_input_list = { empty_input };
        for (uint8_t i = 0; i < TURN_OFFSET - 1; i++) {
            state.inputs[player_id].push_back(empty_input_list);
        }

    }

    // INIT MATCH STATE HERE
    state.match_state = match_init(lcg_seed, noise);
    ui_center_camera_on_cell(state, state.match_state.map_player_spawns[network_get_player_id()]);
    state.is_minimap_dragging = false;

    state.select_rect_origin = xy(-1, -1);
    state.select_rect = (SDL_Rect) { .x = 0, .y = 0, . w = 1, .h = 1 };

    state.rally_flag_animation = animation_create(ANIMATION_RALLY_FLAG);

    state.status_timer = 0;

    state.control_group_selected = UI_CONTROL_GROUP_NONE_SELECTED;
    state.double_click_timer = 0;
    state.control_group_double_tap_timer = 0;

    memset(state.sound_cooldown_timers, 0, sizeof(state.sound_cooldown_timers));

    state.options_menu_state.mode = OPTION_MENU_CLOSED;


    // Destroy minimap texture if already created
    if (engine.minimap_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_texture);
        engine.minimap_texture = NULL;
    }
    if (engine.minimap_tiles_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_tiles_texture);
        engine.minimap_tiles_texture = NULL;
    }
    ui_create_minimap_texture(state);

    network_toggle_ready();
    if (network_are_all_players_ready()) {
        log_info("Match started.");
        state.mode = UI_MODE_NONE;
    }

    return state;
}

void ui_handle_input(ui_state_t& state, SDL_Event event) {
    if (state.mode == UI_MODE_MATCH_NOT_STARTED || state.disconnect_timer > 0) {
        return;
    }

    // Options menu
    if (state.options_menu_state.mode != OPTION_MENU_CLOSED) {
        options_menu_handle_input(state.options_menu_state, event);
        if (engine.minimap_texture == NULL) {
            ui_create_minimap_texture(state);
        }
        return;
    }

    // Match over button press
    if ((state.mode == UI_MODE_MATCH_OVER_VICTORY || state.mode == UI_MODE_MATCH_OVER_DEFEAT)) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && sdl_rect_has_point(UI_MATCH_OVER_CONTINUE_BUTTON_RECT, engine.mouse_position)) {
            state.mode = UI_MODE_NONE;
            return;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && sdl_rect_has_point(UI_MATCH_OVER_EXIT_BUTTON_RECT, engine.mouse_position)) {
            network_disconnect();
            state.mode = UI_MODE_LEAVE_MATCH;
            return;
        }
        return;
    }

    // Open menu button
    if ((event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !ui_is_targeting(state) && !ui_is_selecting(state) && !state.is_minimap_dragging && sdl_rect_has_point(UI_MENU_BUTTON_RECT, engine.mouse_position)) || 
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F10)) {
        if (state.mode == UI_MODE_MENU || state.mode == UI_MODE_MENU_SURRENDER) {
            state.mode = UI_MODE_NONE;
        } else {
            state.mode = UI_MODE_MENU;
        }
        sound_play(SOUND_UI_SELECT);
        return;
    }

    // Menu button press
    if (state.mode == UI_MODE_MENU || state.mode == UI_MODE_MENU_SURRENDER) {
        if (!(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            return;
        }

        int button_pressed;
        int button_count = state.mode == UI_MODE_MENU ? UI_MENU_BUTTON_COUNT : UI_MENU_SURRENDER_BUTTON_COUNT;
        for (button_pressed = 0; button_pressed < button_count; button_pressed++) {
            if (sdl_rect_has_point(UI_MENU_BUTTON_RECTS[button_pressed], engine.mouse_position)) {
                break;
            }
        }
        if (button_pressed == button_count) {
            return;
        }

        sound_play(SOUND_UI_SELECT);
        if (state.mode == UI_MODE_MENU && button_pressed == 2) {
            state.mode = UI_MODE_NONE;
        } else if (state.mode == UI_MODE_MENU && button_pressed == 1) {
            state.options_menu_state = options_menu_open();
        } else if (state.mode == UI_MODE_MENU_SURRENDER && button_pressed == 1) {
            state.mode = UI_MODE_MENU;
        } else if (state.mode == UI_MODE_MENU && button_pressed == 0) {
            bool is_another_player_in_match = false;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (!(network_get_player_id() == player_id || network_get_player(player_id).status == PLAYER_STATUS_NONE)) {
                    is_another_player_in_match = true;
                    break;
                }
            }

            if (is_another_player_in_match) {
                state.mode = UI_MODE_MENU_SURRENDER;
            } else {
                network_disconnect();
                state.mode = UI_MODE_LEAVE_MATCH;
            }
        } else if (state.mode == UI_MODE_MENU_SURRENDER && button_pressed == 0) {
            network_disconnect();
            state.mode = UI_MODE_LEAVE_MATCH;
        }

        return;
    }

    // Begin chat
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN && !SDL_IsTextInputActive()) {
        state.chat_message = "";
        SDL_SetTextInputRect(&UI_CHAT_RECT);
        SDL_StartTextInput();
        sound_play(SOUND_UI_SELECT);
        return;
    }

    // Chat enter message
    if (SDL_IsTextInputActive() && event.type == SDL_TEXTINPUT) {
        state.chat_message += std::string(event.text.text);
        if (state.chat_message.length() > MATCH_CHAT_MESSAGE_MAX_LENGTH) {
            state.chat_message = state.chat_message.substr(0, MATCH_CHAT_MESSAGE_MAX_LENGTH);
        }

        return;
    } 

    // Chat non-text input key handle
    if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            // Chat delete character
            case SDLK_BACKSPACE: {
                if (state.chat_message.length() > 0) {
                    state.chat_message.pop_back();
                }
                break;
            }
            // Chat send message
            case SDLK_RETURN: {
                if (state.chat_message.length() != 0) {
                    input_t input;
                    input.type = INPUT_CHAT;
                    input.chat.message_length = (uint8_t)state.chat_message.length() + 1; // The +1 is for the null character
                    strcpy(input.chat.message, &state.chat_message[0]);
                    state.input_queue.push_back(input);
                    sound_play(SOUND_UI_SELECT);
                }

                SDL_StopTextInput();
                break;
            }
            // Cancel chat message
            case SDLK_ESCAPE: {
                SDL_StopTextInput();
                break;
            }
            default:
                break;
        }

        return;
    }

    // UI button press
    if (event.type == SDL_MOUSEBUTTONDOWN && ui_get_ui_button_hovered(state) != -1) {
        ui_handle_ui_button_press(state, state.buttons[ui_get_ui_button_hovered(state)]);
        return;
    }

    // UI button hotkey press
    if (event.type == SDL_KEYDOWN) {
        for (int button_index = 0; button_index < UI_BUTTONSET_SIZE; button_index++) {
            UiButton button = state.buttons[button_index];
            if (button == UI_BUTTON_NONE) {
                continue;
            }
            if (engine.hotkeys.at(button) == event.key.keysym.sym) {
                ui_handle_ui_button_press(state, button);
                return;
            }
        }
    }

    // Garrisoned unit icon press
    if (ui_get_garrisoned_index_hovered(state) != -1 && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        sound_play(SOUND_UI_SELECT);
        state.input_queue.push_back((input_t) {
            .type = INPUT_SINGLE_UNLOAD,
            .single_unload = (input_single_unload_t) {
                .unit_id = state.match_state.entities.get_by_id(state.selection[0]).garrisoned_units[ui_get_garrisoned_index_hovered(state)]
            }
        });
    }

    // Selected unit icon press
    if (ui_get_selected_unit_hovered(state) != -1 && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        sound_play(SOUND_UI_SELECT);
        if (engine.keystate[SDL_SCANCODE_LSHIFT] || engine.keystate[SDL_SCANCODE_RSHIFT]) {
            ui_deselect_entity_if_selected(state, state.selection[ui_get_selected_unit_hovered(state)]);
        } else {
            std::vector<entity_id> selection;
            selection.push_back(state.selection[ui_get_selected_unit_hovered(state)]);
            ui_set_selection(state, selection);
        }
    }

    // Building queue icon press
    if (ui_get_building_queue_item_hovered(state) != -1 && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        sound_play(SOUND_UI_SELECT);
        state.input_queue.push_back((input_t) {
            .type = INPUT_BUILDING_DEQUEUE,
            .building_dequeue = (input_building_dequeue_t) {
                .building_id = state.selection[0],
                .index = (uint16_t)ui_get_building_queue_item_hovered(state)
            }
        });
    }

    // UI building place
    if (state.mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui() && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (!ui_building_can_be_placed(state)) {
            ui_show_status(state, UI_STATUS_CANT_BUILD);
            return;
        }

        input_t input;
        input.type = INPUT_BUILD;
        input.build.shift_command = engine.keystate[SDL_SCANCODE_LSHIFT] || engine.keystate[SDL_SCANCODE_RSHIFT];
        input.build.building_type = state.building_type;
        input.build.entity_count = state.selection.size();
        memcpy(&input.build.entity_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));
        input.build.target_cell = ui_get_building_cell(state);
        state.input_queue.push_back(input);

        if (!input.build.shift_command) {
            state.mode = UI_MODE_NONE;
        }

        sound_play(SOUND_BUILDING_PLACE);
        return;
    }

    // Order movement
    if (event.type == SDL_MOUSEBUTTONDOWN && ui_get_selection_type(state, state.selection) == UI_SELECTION_TYPE_UNITS && state.mode != UI_MODE_BUILDING_PLACE &&
            (sdl_rect_has_point(UI_MINIMAP_RECT, engine.mouse_position) || !ui_is_mouse_in_ui()) &&
            ((event.button.button == SDL_BUTTON_LEFT && ui_is_targeting(state)) ||
            (event.button.button == SDL_BUTTON_RIGHT && !ui_is_selecting(state) && !state.is_minimap_dragging))) {
        input_t move_input = ui_create_move_input(state);

        if (move_input.type == INPUT_MOVE_REPAIR) {
            bool is_repair_target_valid = true;
            if (move_input.move.target_id == ID_NULL) {
                is_repair_target_valid = false;
            } else {
                const entity_t& repair_target = state.match_state.entities.get_by_id(move_input.move.target_id);
                if (repair_target.player_id != network_get_player_id() || !entity_is_building(repair_target.type)) {
                    is_repair_target_valid = false;
                }
            }

            if (!is_repair_target_valid) {
                ui_show_status(state, UI_STATUS_REPAIR_TARGET_INVALID);
                state.mode = UI_MODE_NONE;
                return;
            }
        }

        state.input_queue.push_back(move_input);

        // Check if clicked on remembered building
        entity_id remembered_entity_id = ID_NULL;
        if ((move_input.type == INPUT_MOVE_CELL || move_input.type == INPUT_MOVE_ATTACK_CELL) &&
                state.match_state.map_fog[network_get_player_id()][move_input.move.target_cell.x + (move_input.move.target_cell.y * state.match_state.map_width)] == FOG_EXPLORED) {
            xy move_target;
            if (ui_is_mouse_in_ui()) {
                xy minimap_pos = engine.mouse_position - xy(UI_MINIMAP_RECT.x, UI_MINIMAP_RECT.y);
                move_target = xy((state.match_state.map_width * TILE_SIZE * minimap_pos.x) / UI_MINIMAP_RECT.w,
                                    (state.match_state.map_height * TILE_SIZE * minimap_pos.y) / UI_MINIMAP_RECT.h);
            } else {
                move_target = ui_get_mouse_world_pos(state);
            }

            for (auto it : state.match_state.remembered_entities[network_get_player_id()]) {
                SDL_Rect remembered_entity_rect = (SDL_Rect) {
                    .x = it.second.cell.x * TILE_SIZE, .y = it.second.cell.y * TILE_SIZE,
                    .w = it.second.cell_size * TILE_SIZE, .h = it.second.cell_size * TILE_SIZE
                };
                if (sdl_rect_has_point(remembered_entity_rect, engine.mouse_position + state.camera_offset)) {
                    remembered_entity_id = it.first;
                    break;
                }
            }
        }

        // Provide instant user feedback
        if (remembered_entity_id != ID_NULL) {
            state.move_animation = animation_create(state.match_state.remembered_entities[network_get_player_id()][remembered_entity_id].sprite_params.recolor_id != RECOLOR_NONE 
                                                            ? ANIMATION_UI_MOVE_ATTACK_ENTITY 
                                                            : ANIMATION_UI_MOVE_ENTITY);
            state.move_animation_position = cell_center(move_input.move.target_cell).to_xy();
            state.move_animation_entity_id = remembered_entity_id;
        } else if (move_input.type == INPUT_MOVE_CELL || move_input.type == INPUT_MOVE_ATTACK_CELL || move_input.type == INPUT_MOVE_UNLOAD || move_input.type == INPUT_MOVE_SMOKE) {
            state.move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
            state.move_animation_position = ui_get_mouse_world_pos(state);
            state.move_animation_entity_id = ID_NULL;
        } else {
            state.move_animation = animation_create(move_input.type == INPUT_MOVE_ATTACK_ENTITY ? ANIMATION_UI_MOVE_ATTACK_ENTITY : ANIMATION_UI_MOVE_ENTITY);
            state.move_animation_position = cell_center(move_input.move.target_cell).to_xy();
            state.move_animation_entity_id = move_input.move.target_id;
        }

        // Reset UI mode if targeting
        if (ui_is_targeting(state)) {
            state.mode = UI_MODE_NONE;
        }
        return;
    } 

    // Set building rally
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT &&
            ui_get_selection_type(state, state.selection) == UI_SELECTION_TYPE_BUILDINGS && 
            !ui_is_targeting(state) && !ui_is_selecting(state) && !state.is_minimap_dragging &&
            (!ui_is_mouse_in_ui() || sdl_rect_has_point(UI_MINIMAP_RECT, engine.mouse_position))) {
        // Check to make sure that all buildings can rally
        for (entity_id id : state.selection) {
            entity_t& entity = state.match_state.entities.get_by_id(id);
            if (entity.mode == MODE_BUILDING_IN_PROGRESS || !ENTITY_DATA.at(entity.type).building_data.can_rally) {
                return;
            }
        }

        input_t input;
        input.type = INPUT_RALLY;
        if (ui_is_mouse_in_ui()) {
            xy minimap_pos = engine.mouse_position - xy(UI_MINIMAP_RECT.x, UI_MINIMAP_RECT.y);
            input.rally.rally_point = xy((state.match_state.map_width * TILE_SIZE * minimap_pos.x) / UI_MINIMAP_RECT.w,
                                (state.match_state.map_height * TILE_SIZE * minimap_pos.y) / UI_MINIMAP_RECT.h);
        } else {
            input.rally.rally_point = ui_get_mouse_world_pos(state);
        }
        input.rally.building_count = (uint16_t)state.selection.size();
        memcpy(&input.rally.building_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));
        state.input_queue.push_back(input);
        sound_play(SOUND_FLAG_THUMP);
        return;
    }

    // Begin minimap drag
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && 
            (state.mode == UI_MODE_NONE || state.mode == UI_MODE_BUILD || state.mode == UI_MODE_BUILD2) && sdl_rect_has_point(UI_MINIMAP_RECT, engine.mouse_position)) {
        state.is_minimap_dragging = true;
        return;
    }

    // End minimap drag
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && state.is_minimap_dragging) {
        state.is_minimap_dragging = false;
        return;
    }

    // Begin selecting
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
            (state.mode == UI_MODE_NONE || state.mode == UI_MODE_BUILD || state.mode == UI_MODE_BUILD2) && !ui_is_mouse_in_ui()) {
        state.select_rect_origin = ui_get_mouse_world_pos(state);
        return;
    }

    // End selecting
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && ui_is_selecting(state)) {
        state.select_rect_origin = xy(-1, -1);
        std::vector<entity_id> selection = ui_create_selection_from_rect(state);

        // Append selection
        if (engine.keystate[SDL_SCANCODE_LCTRL] || engine.keystate[SDL_SCANCODE_RCTRL]) { 
            // Don't append selection if units are same type
            if (ui_get_selection_type(state, state.selection) != ui_get_selection_type(state, selection)) {
                return;
            }
            for (entity_id incoming_id : selection) {
                if (std::find(state.selection.begin(), state.selection.end(), incoming_id) == state.selection.end()) {
                    state.selection.push_back(incoming_id);
                }
            }

            ui_set_selection(state, state.selection);
            state.control_group_selected = -1;
            return;
        } 

        // Double-click selection
        if (selection.size() == 1) {
            if (state.double_click_timer == 0) {
                state.double_click_timer = UI_DOUBLE_CLICK_DURATION;
            } else if (state.selection.size() == 1 && state.selection[0] == selection[0] && state.match_state.entities.get_by_id(state.selection[0]).player_id == network_get_player_id()) {
                EntityType selected_type = state.match_state.entities.get_by_id(state.selection[0]).type;
                selection.clear();
                for (uint32_t entity_index = 0; entity_index < state.match_state.entities.size(); entity_index++) {
                    if (state.match_state.entities[entity_index].type != selected_type || state.match_state.entities[entity_index].player_id != network_get_player_id()) {
                        continue;
                    }
                    SDL_Rect entity_rect = entity_get_rect(state.match_state.entities[entity_index]);
                    entity_rect.x -= state.camera_offset.x;
                    entity_rect.y -= state.camera_offset.y;
                    if (SDL_HasIntersection(&SCREEN_RECT, &entity_rect) == SDL_TRUE) {
                        selection.push_back(state.match_state.entities.get_id_of(entity_index));
                    }
                }
            }
        }

        // Regular selection
        ui_set_selection(state, selection);
        state.control_group_selected = -1;

        return;
    }


    // Control group key press
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym >= SDLK_0 && event.key.keysym.sym <= SDLK_9) {
        uint32_t control_group_index = event.key.keysym.sym == SDLK_0 ? 9 : (uint32_t)(event.key.keysym.sym - SDLK_1);
        // Set control group
        if (engine.keystate[SDL_SCANCODE_LCTRL] || engine.keystate[SDL_SCANCODE_RCTRL]) {
            if (state.selection.empty() || state.match_state.entities.get_by_id(state.selection[0]).player_id != network_get_player_id()) {
                return;
            }
            state.control_groups[control_group_index] = state.selection;
            state.control_group_selected = control_group_index;
            return;
        }

        // Append control group
        if (engine.keystate[SDL_SCANCODE_LSHIFT] || engine.keystate[SDL_SCANCODE_RSHIFT]) {
            if (state.selection.empty() || state.match_state.entities.get_by_id(state.selection[0]).player_id != network_get_player_id() || 
                    ui_get_selection_type(state, state.selection) != ui_get_selection_type(state, state.control_groups[control_group_index])) {
                return;
            }
            for (entity_id id : state.selection) {
                if (std::find(state.control_groups[control_group_index].begin(), state.control_groups[control_group_index].end(), id) == state.control_groups[control_group_index].end()) {
                    state.control_groups[control_group_index].push_back(id);
                }
            }
            return;
        }

        // Snap to control group
        if (state.control_group_double_tap_timer != 0 && state.control_group_double_tap_key == control_group_index) {
            xy group_min;
            xy group_max;
            for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
                const entity_t& entity = state.match_state.entities.get_by_id(state.selection[selection_index]);
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
            SDL_Rect group_rect = (SDL_Rect) { 
                .x = group_min.x, .y = group_min.y,
                .w = group_max.x - group_min.x, .h = group_max.y - group_min.y
            };
            xy group_center = xy(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));
            ui_center_camera_on_cell(state, group_center);
            return;
        }

        // Select control group
        if (!state.control_groups[control_group_index].empty()) {
            ui_set_selection(state, state.control_groups[control_group_index]);
            if (!state.selection.empty()) {
                state.control_group_double_tap_timer = UI_DOUBLE_CLICK_DURATION;
                state.control_group_double_tap_key = control_group_index;
                state.control_group_selected = control_group_index;
            }
        }
        return;
    }

    // Jump to latest alert
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE && !state.alerts.empty()) {
        ui_center_camera_on_cell(state, state.alerts[state.alerts.size() - 1].cell);
        return;
    }
}

void ui_handle_network_event(ui_state_t& state, const network_event_t& network_event) {
    switch (network_event.type) {
        case NETWORK_EVENT_INPUT: {
            // Deserialize input
            std::vector<input_t> tick_inputs;

            const uint8_t* in_buffer = network_event.input.in_buffer;
            size_t in_buffer_head = 1;

            while (in_buffer_head < network_event.input.in_buffer_length) {
                tick_inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
            }

            state.inputs[network_event.input.player_id].push_back(tick_inputs);
            break;
        }
        case NETWORK_EVENT_PLAYER_DISCONNECTED: {
            match_add_chat_message(state.match_state, std::string(network_get_player(network_event.player_disconnected.player_id).name) + " disconnected.");

            // Determine if we should exit the match
            uint32_t player_count = 0;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (network_get_player(player_id).status != PLAYER_STATUS_NONE) {
                    player_count++;
                }
            }
            if (player_count < 2) {
                state.mode = UI_MODE_MATCH_OVER_VICTORY;
            }
            break;
        }
        default: 
            break;
    }
}

void ui_update(ui_state_t& state) {
    if (state.mode == UI_MODE_MATCH_NOT_STARTED && network_are_all_players_ready()) {
        log_trace("Match started.");
        state.mode = UI_MODE_NONE;
    }
    if (state.mode == UI_MODE_MATCH_NOT_STARTED) {
        return;
    }

    if (state.options_menu_state.mode != OPTION_MENU_CLOSED) {
        options_menu_update(state.options_menu_state);
    }

    // Turn loop
    if (state.turn_timer == 0) {
        bool all_inputs_received = true;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            // We didn't receive inputs, start the timer
            if (state.inputs[player_id].empty() || state.inputs[player_id][0].empty()) {
                all_inputs_received = false;
                continue;
            }
        }

        if (!all_inputs_received) {
            state.disconnect_timer++;
            return;
        }

        // Reset the disconnect timer if we received inputs
        state.disconnect_timer = 0;

        // All inputs received. Begin next turn
        // HANDLE INPUT
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            for (const input_t& input : state.inputs[player_id][0]) {
                match_input_handle(state.match_state, player_id, input);
            }
            state.inputs[player_id].erase(state.inputs[player_id].begin());
        }

        state.turn_timer = TURN_DURATION;

        // FLUSH INPUT
        // Always send at least one input per tick
        if (state.input_queue.empty()) {
            input_t empty_input;
            empty_input.type = INPUT_NONE;
            state.input_queue.push_back(empty_input);
        }

        // Serialize the inputs
        uint8_t out_buffer[INPUT_BUFFER_SIZE];
        size_t out_buffer_length = 1; // Leaves space in the buffer for the network message type
        for (const input_t& input : state.input_queue) {
            match_input_serialize(out_buffer, out_buffer_length, input);
        }
        state.inputs[network_get_player_id()].push_back(state.input_queue);
        state.input_queue.clear();

        // Send them to other players
        network_send_input(out_buffer, out_buffer_length);
    } // End if tick timer is 0

    state.turn_timer--;

    // MATCH UPDATE
    match_update(state.match_state);
    while ((!state.match_state.events.empty())) {
        match_event_t event = state.match_state.events[0];
        state.match_state.events.erase(state.match_state.events.begin());
        switch (event.type) {
            case MATCH_EVENT_SOUND: {
                if (state.sound_cooldown_timers[event.sound.sound] != 0) {
                    break;
                }
                SDL_Rect camera_rect = (SDL_Rect) { .x = state.camera_offset.x, .y = state.camera_offset.y, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
                if (!sdl_rect_has_point(camera_rect, event.sound.position)) {
                    break;
                }

                sound_play(event.sound.sound);
                state.sound_cooldown_timers[event.sound.sound] = 5;
                break;
            }
            case MATCH_EVENT_ALERT: {
                if (event.alert.player_id != network_get_player_id()) {
                    break;
                }

                SDL_Rect camera_rect = (SDL_Rect) { .x = state.camera_offset.x, .y = state.camera_offset.y, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
                SDL_Rect alert_rect = (SDL_Rect) { 
                    .x = event.alert.cell.x * TILE_SIZE, 
                    .y = event.alert.cell.y * TILE_SIZE, 
                    .w = event.alert.cell_size * TILE_SIZE, 
                    .h = event.alert.cell_size * TILE_SIZE 
                };

                // Check if an existing attack alert already exists nearby
                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    bool is_existing_attack_alert_nearby = false;
                    for (const alert_t& existing_alert : state.alerts) {
                        if (existing_alert.color == ALERT_COLOR_RED && xy::manhattan_distance(existing_alert.cell, event.alert.cell) < UI_ATTACK_ALERT_DISTANCE) {
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
                    default:
                        break;
                }

                if (SDL_HasIntersection(&camera_rect, &alert_rect) == SDL_TRUE) {
                    break;
                }

                UiAlertColor color;
                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    color = ALERT_COLOR_RED;
                } else {
                    color = ALERT_COLOR_GREEN;
                }

                state.alerts.push_back((alert_t) {
                    .color = color,
                    .cell = event.alert.cell,
                    .cell_size = event.alert.cell_size,
                    .timer = UI_ALERT_TOTAL_DURATION
                });

                if (event.alert.type == MATCH_ALERT_TYPE_ATTACK) {
                    ui_show_status(state, UI_STATUS_UNDER_ATTACK);
                    sound_play(SOUND_ALERT_BELL);
                }
                break;
            }
            case MATCH_EVENT_SELECTION_HANDOFF: {
                if (event.selection_handoff.player_id != network_get_player_id()) {
                    break;
                }

                if (state.selection.size() == 1 && state.selection[0] == event.selection_handoff.to_deselect) {
                    std::vector<entity_id> new_selection;
                    new_selection.push_back(event.selection_handoff.to_select);
                    ui_set_selection(state, new_selection);
                    state.mode = UI_MODE_NONE;
                }
                break;
            }
            case MATCH_EVENT_SMOKE_COOLDOWN:
            case MATCH_EVENT_CANT_BUILD:
            case MATCH_EVENT_BUILDING_EXIT_BLOCKED:
            case MATCH_EVENT_MINE_EXIT_BLOCKED:
            case MATCH_EVENT_NOT_ENOUGH_GOLD:
            case MATCH_EVENT_NOT_ENOUGH_HOUSE: {
                if (event.player_id != network_get_player_id()) {
                    break;
                }
                switch (event.type) {
                    case MATCH_EVENT_SMOKE_COOLDOWN:
                        ui_show_status(state, UI_STATUS_SMOKE_COOLDOWN);
                        break;
                    case MATCH_EVENT_CANT_BUILD:
                        ui_show_status(state, UI_STATUS_CANT_BUILD);
                        break;
                    case MATCH_EVENT_BUILDING_EXIT_BLOCKED:
                        ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
                        break;
                    case MATCH_EVENT_MINE_EXIT_BLOCKED:
                        ui_show_status(state, UI_STATUS_MINE_EXIT_BLOCKED);
                        break;
                    case MATCH_EVENT_NOT_ENOUGH_GOLD:
                        ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                        break;
                    case MATCH_EVENT_NOT_ENOUGH_HOUSE:
                        ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
                        break;
                    default:
                        log_warn("Unhandled match event %u", event.type);
                        break;
                }

                break;
            }
            case MATCH_EVENT_RESEARCH_COMPLETE: {
                if (event.research_complete.player_id != network_get_player_id()) {
                    break;
                }

                char message[128];
                sprintf(message, "%s research complete.", UPGRADE_DATA.at(event.research_complete.upgrade).name);
                ui_show_status(state, message);
                break;
            }
        }
    }

    // CAMERA DRAG
    if (!ui_is_selecting(state) && !state.is_minimap_dragging && state.options_menu_state.mode == OPTION_MENU_CLOSED) {
        xy camera_drag_direction = xy(0, 0);
        if (engine.mouse_position.x < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = -1;
        } else if (engine.mouse_position.x > SCREEN_WIDTH - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.x = 1;
        }
        if (engine.mouse_position.y < CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = -1;
        } else if (engine.mouse_position.y > SCREEN_HEIGHT - CAMERA_DRAG_MARGIN) {
            camera_drag_direction.y = 1;
        }
        state.camera_offset += camera_drag_direction * engine.options[OPTION_CAMERA_SPEED];
        ui_clamp_camera(state);
    }

    // MINIMAP DRAG
    if (state.is_minimap_dragging) {
        xy minimap_pos = xy(
            std::clamp(engine.mouse_position.x - UI_MINIMAP_RECT.x, 0, UI_MINIMAP_RECT.w), 
            std::clamp(engine.mouse_position.y - UI_MINIMAP_RECT.y, 0, UI_MINIMAP_RECT.h));
        xy map_pos = xy(
            (state.match_state.map_width * TILE_SIZE * minimap_pos.x) / UI_MINIMAP_RECT.w,
            (state.match_state.map_height * TILE_SIZE * minimap_pos.y) / UI_MINIMAP_RECT.h);
        ui_center_camera_on_cell(state, map_pos / TILE_SIZE);
    }

    // SELECT RECT
    if (ui_is_selecting(state) && state.options_menu_state.mode == OPTION_MENU_CLOSED) {
        // Update select rect
        state.select_rect = (SDL_Rect) {
            .x = std::min(state.select_rect_origin.x, ui_get_mouse_world_pos(state).x),
            .y = std::min(state.select_rect_origin.y, ui_get_mouse_world_pos(state).y),
            .w = std::max(1, std::abs(state.select_rect_origin.x - ui_get_mouse_world_pos(state).x)), 
            .h = std::max(1, std::abs(state.select_rect_origin.y - ui_get_mouse_world_pos(state).y))
        };
    }

    // Update timers
    if (animation_is_playing(state.move_animation)) {
        animation_update(state.move_animation);
    }
    animation_update(state.rally_flag_animation);
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
    if (state.status_timer > 0) {
        state.status_timer--;
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

    // Update cursor
    engine_set_cursor(ui_is_targeting(state) ? CURSOR_TARGET : CURSOR_DEFAULT);
    if ((ui_is_targeting(state) || state.mode == UI_MODE_BUILDING_PLACE || state.mode == UI_MODE_BUILD || state.mode == UI_MODE_BUILD2) && state.selection.empty()) {
        state.mode = UI_MODE_NONE;
    }

    // Update buttons
    ui_update_buttons(state);

    // Clear hidden units from selection
    {
        int selection_index = 0;
        while (selection_index < state.selection.size()) {
            entity_t& selected_entity = state.match_state.entities.get_by_id(state.selection[selection_index]);
            if (!entity_is_selectable(selected_entity) || 
                    !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), selected_entity.cell, entity_cell_size(selected_entity.type))) {
                state.selection.erase(state.selection.begin() + selection_index);
            } else {
                selection_index++;
            }
        }
    }
}

void ui_create_minimap_texture(const ui_state_t& state) {
    // Create new minimap texture
    engine.minimap_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, state.match_state.map_width, state.match_state.map_height);

    SDL_Texture* minimap_tiles_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, state.match_state.map_width, state.match_state.map_height);
    SDL_SetRenderTarget(engine.renderer, minimap_tiles_texture);
    for (int y = 0; y < state.match_state.map_height; y++) {
        for (int x = 0; x < state.match_state.map_width; x++) {
            SDL_Color color;
            int tile_index = state.match_state.map_tiles[x + (y * state.match_state.map_width)].index;
            if (tile_index == 0) {
                color = COLOR_WHITE;
            } else if (tile_index >= engine.tile_index[TILE_SAND] && tile_index <= engine.tile_index[TILE_SAND3]) {
                color = COLOR_SAND_DARK;
            } else if (tile_index >= engine.tile_index[TILE_WATER] && tile_index < engine.tile_index[TILE_WATER] + 47) {
                color = COLOR_DIM_BLUE;
            } else {
                color = COLOR_DARK_GRAY;
            }
            SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
            SDL_RenderDrawPoint(engine.renderer, x, y);
        }
    }

    engine.minimap_tiles_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, state.match_state.map_width, state.match_state.map_height);
    const int pitch = state.match_state.map_width * 4;
    void* pixels = malloc(state.match_state.map_height * pitch);
    SDL_RenderReadPixels(engine.renderer, NULL, SDL_PIXELFORMAT_ARGB8888, pixels, pitch);
    SDL_UpdateTexture(engine.minimap_tiles_texture, NULL, pixels, pitch);

    SDL_SetRenderTarget(engine.renderer, NULL);
    SDL_DestroyTexture(minimap_tiles_texture);
    free(pixels);
    log_trace("Created minimap texture.");
}

input_t ui_create_move_input(const ui_state_t& state) {
    // Determine move target
    xy move_target;
    if (ui_is_mouse_in_ui()) {
        xy minimap_pos = engine.mouse_position - xy(UI_MINIMAP_RECT.x, UI_MINIMAP_RECT.y);
        move_target = xy((state.match_state.map_width * TILE_SIZE * minimap_pos.x) / UI_MINIMAP_RECT.w,
                            (state.match_state.map_height * TILE_SIZE * minimap_pos.y) / UI_MINIMAP_RECT.h);
    } else {
        move_target = ui_get_mouse_world_pos(state);
    }

    // Create move input
    input_t input;
    input.move.shift_command = engine.keystate[SDL_SCANCODE_LSHIFT] || engine.keystate[SDL_SCANCODE_RSHIFT];
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;
    int fog_value = state.match_state.map_fog[network_get_player_id()][input.move.target_cell.x + (input.move.target_cell.y * state.match_state.map_width)];
    // don't target enemies in hidden fog or when minimap clicking
    if (fog_value != FOG_HIDDEN && !ui_is_mouse_in_ui()) {
        for (uint32_t entity_index = 0; entity_index < state.match_state.entities.size(); entity_index++) {
            const entity_t& entity = state.match_state.entities[entity_index];
            // don't target unselectable entities, unless it's gold and the player doesn't know that the gold is unselectable
            // It's also saying, don't target a hidden mine
            if (!entity_is_selectable(entity) || 
                    (fog_value == FOG_EXPLORED && entity.type != ENTITY_GOLD_MINE) ||
                    (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && entity.player_id != network_get_player_id() && state.match_state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.match_state.map_width)] == 0)) {
                continue;
            }

            SDL_Rect entity_rect = entity_get_rect(state.match_state.entities[entity_index]);
            if (sdl_rect_has_point(entity_rect, move_target)) {
                input.move.target_id = state.match_state.entities.get_id_of(entity_index);
                break;
            }
        }
    }

    if (state.mode == UI_MODE_TARGET_UNLOAD) {
        input.type = INPUT_MOVE_UNLOAD;
    } else if (state.mode == UI_MODE_TARGET_REPAIR) {
        input.type = INPUT_MOVE_REPAIR;
    } else if (state.mode == UI_MODE_TARGET_SMOKE) {
        input.type = INPUT_MOVE_SMOKE;
    } else if (input.move.target_id != ID_NULL && state.match_state.entities.get_by_id(input.move.target_id).type != ENTITY_GOLD_MINE &&
               (state.mode == UI_MODE_TARGET_ATTACK || 
                state.match_state.entities.get_by_id(input.move.target_id).player_id != network_get_player_id())) {
        input.type = INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && state.mode == UI_MODE_TARGET_ATTACK) {
        input.type = INPUT_MOVE_ATTACK_CELL;
    } else if (input.move.target_id != ID_NULL) {
        input.type = INPUT_MOVE_ENTITY;
    } else {
        input.type = INPUT_MOVE_CELL;
    }

    // Populate move input entity ids
    input.move.entity_count = (uint16_t)state.selection.size();
    memcpy(input.move.entity_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));

    return input;
}

bool ui_is_mouse_in_ui() {
    return (engine.mouse_position.y >= SCREEN_HEIGHT - UI_HEIGHT) ||
           (engine.mouse_position.x <= 136 && engine.mouse_position.y >= SCREEN_HEIGHT - 136) ||
           (engine.mouse_position.x >= SCREEN_WIDTH - 132 && engine.mouse_position.y >= SCREEN_HEIGHT - 106);
}

bool ui_is_selecting(const ui_state_t& state) {
    return state.select_rect_origin.x != -1;
}

xy ui_get_mouse_world_pos(const ui_state_t& state) {
    return engine.mouse_position + state.camera_offset;
}

void ui_clamp_camera(ui_state_t& state) {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, ((int)state.match_state.map_width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, ((int)state.match_state.map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT);
}

void ui_center_camera_on_cell(ui_state_t& state, xy cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    ui_clamp_camera(state);
}

std::vector<entity_id> ui_create_selection_from_rect(const ui_state_t& state) {
    std::vector<entity_id> selection;

    // Select player units
    for (uint32_t index = 0; index < state.match_state.entities.size(); index++) {
        const entity_t& entity = state.match_state.entities[index];
        if (entity.player_id != network_get_player_id() || 
            !entity_is_unit(entity.type) || 
            !entity_is_selectable(entity)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select player buildings
    for (uint32_t index = 0; index < state.match_state.entities.size(); index++) {
        const entity_t& entity = state.match_state.entities[index];
        if (entity.player_id != network_get_player_id() || 
            !entity_is_building(entity.type) || 
            !entity_is_selectable(entity)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.match_state.entities.get_id_of(index));
        }
    }
    if (!selection.empty()) {
        return selection;
    }

    // Select enemy unit
    for (uint32_t index = 0; index < state.match_state.entities.size(); index++) {
        const entity_t& entity = state.match_state.entities[index];
        if (entity.player_id == network_get_player_id() || 
            !entity_is_unit(entity.type) || 
            !entity_is_selectable(entity) || 
            !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type)) ||
            (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state.match_state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.match_state.map_width)] == 0)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.match_state.entities.get_id_of(index));
            return selection;
        }
    }

    // Select enemy buildings
    for (uint32_t index = 0; index < state.match_state.entities.size(); index++) {
        const entity_t& entity = state.match_state.entities[index];
        if (entity.player_id == network_get_player_id() || 
            !entity_is_building(entity.type) || 
            !entity_is_selectable(entity) || 
            !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type)) ||
            (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state.match_state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.match_state.map_width)] == 0)) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.match_state.entities.get_id_of(index));
            return selection;
        }
    }

    for (uint32_t index = 0; index < state.match_state.entities.size(); index++) {
        const entity_t& entity = state.match_state.entities[index];
        if (entity.type != ENTITY_GOLD_MINE || !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            continue;
        }

        SDL_Rect entity_rect = entity_get_rect(entity);
        if (SDL_HasIntersection(&entity_rect, &state.select_rect) == SDL_TRUE) {
            selection.push_back(state.match_state.entities.get_id_of(index));
            return selection;
        }
    }

    return selection;
}

void ui_set_selection(ui_state_t& state, const std::vector<entity_id>& selection) {
    state.selection = selection;

    for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
        uint32_t entity_index = state.match_state.entities.get_index_of(state.selection[selection_index]);
        if (entity_index == INDEX_INVALID || !entity_is_selectable(state.match_state.entities[entity_index])) {
            state.selection.erase(state.selection.begin() + selection_index);
            selection_index--;
            continue;
        }
    }

    while (state.selection.size() > SELECTION_LIMIT) {
        state.selection.pop_back();
    }

    state.mode = UI_MODE_NONE;
}

void ui_update_buttons(ui_state_t& state) {
    for (int i = 0; i < 6; i++) {
        state.buttons[i] = UI_BUTTON_NONE;
    }

    if (ui_is_targeting(state) || state.mode == UI_MODE_BUILDING_PLACE) {
        state.buttons[5] = UI_BUTTON_CANCEL;
        return;
    }
    if (state.mode == UI_MODE_BUILD) {
        state.buttons[0] = UI_BUTTON_BUILD_HALL;
        state.buttons[1] = UI_BUTTON_BUILD_HOUSE;
        state.buttons[2] = UI_BUTTON_BUILD_CAMP;
        state.buttons[3] = UI_BUTTON_BUILD_SALOON;
        state.buttons[4] = UI_BUTTON_BUILD_BUNKER;
        state.buttons[5] = UI_BUTTON_CANCEL;
        return;
    }
    if (state.mode == UI_MODE_BUILD2) {
        state.buttons[0] = UI_BUTTON_BUILD_SMITH;
        state.buttons[1] = UI_BUTTON_BUILD_COOP;
        state.buttons[2] = UI_BUTTON_BUILD_BARRACKS;
        state.buttons[3] = UI_BUTTON_BUILD_SHERIFFS;
        state.buttons[5] = UI_BUTTON_CANCEL;
        return;
    }

    if (state.selection.empty()) {
        return;
    }

    const entity_t entity = state.match_state.entities.get_by_id(state.selection[0]);
    // This covers enemy selection and mine selection
    if (entity.player_id != network_get_player_id()) {
        return;
    }

    if (state.selection.size() == 1 && entity.mode == MODE_BUILDING_IN_PROGRESS) {
        state.buttons[5] = UI_BUTTON_CANCEL;
        return;
    }

    if (entity_is_unit(entity.type)) {
        state.buttons[0] = UI_BUTTON_ATTACK;
        state.buttons[1] = UI_BUTTON_STOP;
        state.buttons[2] = UI_BUTTON_DEFEND;
    }

    // This block returns if selected entities are not all the same type
    uint32_t garrison_count = entity.garrisoned_units.size();
    for (uint32_t id_index = 1; id_index < state.selection.size(); id_index++) {
        const entity_t& other = state.match_state.entities.get_by_id(state.selection[id_index]);
        garrison_count += other.garrisoned_units.size();
        if (other.type != entity.type || (!entity_is_unit(entity.type) && other.mode != entity.mode)) {
            return;
        }
    }

    if (garrison_count != 0) {
        state.buttons[3] = UI_BUTTON_UNLOAD;
    }

    if (!entity.queue.empty() && state.selection.size() == 1) {
        state.buttons[5] = UI_BUTTON_CANCEL;
    }

    switch (entity.type) {
        case ENTITY_MINER: {
            state.buttons[3] = UI_BUTTON_REPAIR;
            state.buttons[4] = UI_BUTTON_BUILD;
            state.buttons[5] = UI_BUTTON_BUILD2;
            break;
        }
        case ENTITY_SAPPER: {
            state.buttons[3] = UI_BUTTON_EXPLODE;
            break;
        }
        case ENTITY_TINKER: {
            state.buttons[3] = UI_BUTTON_BUILD_MINE;
            state.buttons[4] = UI_BUTTON_SMOKE;
            break;
        }
        case ENTITY_HALL: {
            state.buttons[0] = UI_BUTTON_UNIT_MINER;
            break;
        }
        case ENTITY_SALOON: {
            state.buttons[0] = UI_BUTTON_UNIT_COWBOY;
            state.buttons[1] = UI_BUTTON_UNIT_BANDIT;
            state.buttons[2] = UI_BUTTON_UNIT_TINKER;
            state.buttons[3] = UI_BUTTON_UNIT_SAPPER;
            break;
        }
        case ENTITY_COOP: {
            state.buttons[0] = UI_BUTTON_UNIT_JOCKEY;
            state.buttons[1] = match_player_has_upgrade(state.match_state, network_get_player_id(), UPGRADE_WAR_WAGON) ? UI_BUTTON_UNIT_WAR_WAGON : UI_BUTTON_UNIT_WAGON;
            break;
        }
        case ENTITY_SMITH: {
            if (match_player_upgrade_is_available(state.match_state, network_get_player_id(), UPGRADE_WAR_WAGON)) {
                state.buttons[0] = UI_BUTTON_RESEARCH_WAR_WAGON;
            }
            if (match_player_upgrade_is_available(state.match_state, network_get_player_id(), UPGRADE_EXPLOSIVES)) {
                state.buttons[1] = UI_BUTTON_RESEARCH_EXPLOSIVES;
            }
            if (match_player_upgrade_is_available(state.match_state, network_get_player_id(), UPGRADE_BAYONETS)) {
                state.buttons[2] = UI_BUTTON_RESEARCH_BAYONETS;
            }
            if (match_player_upgrade_is_available(state.match_state, network_get_player_id(), UPGRADE_SMOKE)) {
                state.buttons[3] = UI_BUTTON_RESEARCH_SMOKE;
            }
            break;
        }
        case ENTITY_BARRACKS: {
            state.buttons[0] = UI_BUTTON_UNIT_SOLDIER;
            state.buttons[1] = UI_BUTTON_UNIT_CANNON;
            break;
        }
        case ENTITY_SHERIFFS: {
            state.buttons[0] = UI_BUTTON_UNIT_SPY;
            break;
        }
        default:
            break;
    }
}

UiSelectionType ui_get_selection_type(const ui_state_t& state, const std::vector<entity_id>& selection) {
    if (selection.empty()) {
        return UI_SELECTION_TYPE_NONE;
    }

    const entity_t& entity = state.match_state.entities.get_by_id(state.selection[0]);
    if (entity_is_unit(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return UI_SELECTION_TYPE_UNITS;
        } else {
            return UI_SELECTION_TYPE_ENEMY_UNIT;
        }
    } else if (entity_is_building(entity.type)) {
        if (entity.player_id == network_get_player_id()) {
            return UI_SELECTION_TYPE_BUILDINGS;
        } else {
            return UI_SELECTION_TYPE_ENEMY_BUILDING;
        }
    } else {
        return UI_SELECTION_TYPE_GOLD;
    }
}

bool ui_is_targeting(const ui_state_t& state) {
    return state.mode >= UI_MODE_TARGET_ATTACK && state.mode < UI_MODE_CHAT;
}

int ui_get_ui_button_hovered(const ui_state_t& state) {
    if (ui_is_selecting(state) || state.is_minimap_dragging || state.mode >= UI_MODE_CHAT) {
        return -1;
    } 

    for (int i = 0; i < 6; i++) {
        if (state.buttons[i] == UI_BUTTON_NONE) {
            continue;
        }

        if (sdl_rect_has_point(UI_BUTTON_RECT[i], engine.mouse_position)) {
            return i;
        }
    }

    return -1;
}

bool ui_button_requirements_met(const ui_state_t& state, UiButton button) {
    auto ui_button_requirements_it = UI_BUTTON_REQUIREMENTS.find(button);

    // Buttons with no defined requirements are enabled by default
    if (ui_button_requirements_it == UI_BUTTON_REQUIREMENTS.end()) {
        return true;
    }

    switch (ui_button_requirements_it->second.type) {
        case UI_BUTTON_REQUIRES_BUILDING: {
            for (const entity_t& building : state.match_state.entities) {
                if (building.player_id == network_get_player_id() && building.mode == MODE_BUILDING_FINISHED && building.type == ui_button_requirements_it->second.building_type) {
                    return true;
                }
            }
            return false;
        }
        case UI_BUTTON_REQUIRES_UPGRADE: {
            return match_player_has_upgrade(state.match_state, network_get_player_id(), ui_button_requirements_it->second.upgrade);
        }
    }
}

void ui_handle_ui_button_press(ui_state_t& state, UiButton button) {
    if (!ui_button_requirements_met(state, button)) {
        return;
    }

    sound_play(SOUND_UI_SELECT);

    switch (button) {
        case UI_BUTTON_ATTACK: {
            state.mode = UI_MODE_TARGET_ATTACK;
            break;
        }
        case UI_BUTTON_REPAIR: {
            state.mode = UI_MODE_TARGET_REPAIR;
            break;
        }
        case UI_BUTTON_UNLOAD: {
            if (ui_get_selection_type(state, state.selection) == UI_SELECTION_TYPE_UNITS) {
                state.mode = UI_MODE_TARGET_UNLOAD;
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
            state.mode = UI_MODE_BUILD;
            break;
        }
        case UI_BUTTON_BUILD2: {
            state.mode = UI_MODE_BUILD2;
            break;
        }
        case UI_BUTTON_CANCEL: {
            if (state.mode == UI_MODE_BUILDING_PLACE) {
                switch (state.building_type) {
                    case ENTITY_HALL:
                    case ENTITY_CAMP:
                    case ENTITY_HOUSE:
                    case ENTITY_SALOON:
                    case ENTITY_BUNKER:
                        state.mode = UI_MODE_BUILD;
                        break;
                    case ENTITY_COOP:
                        state.mode = UI_MODE_BUILD2;
                        break;
                    case ENTITY_LAND_MINE:
                        state.mode = UI_MODE_NONE;
                        break;
                    default:
                        log_warn("Tried to cancel with unhandled ui_building_type of %u", state.building_type);
                        state.mode = UI_MODE_NONE;
                        break;
                }
            } else if (state.selection.size() == 1 && state.match_state.entities.get_by_id(state.selection[0]).mode == MODE_BUILDING_IN_PROGRESS) {
                state.input_queue.push_back((input_t) {
                    .type = INPUT_BUILD_CANCEL,
                    .build_cancel = (input_build_cancel_t) {
                        .building_id = state.selection[0]
                    }
                });
            } else if (state.selection.size() == 1 && !state.match_state.entities.get_by_id(state.selection[0]).queue.empty()) {
                state.input_queue.push_back((input_t) {
                    .type = INPUT_BUILDING_DEQUEUE,
                    .building_dequeue = (input_building_dequeue_t) {
                        .building_id = state.selection[0],
                        .index = BUILDING_DEQUEUE_POP_FRONT
                    }
                });
            } else if (state.mode == UI_MODE_BUILD || state.mode == UI_MODE_BUILD2) {
                state.mode = UI_MODE_NONE;
            } else if (ui_is_targeting(state)) {
                state.mode = UI_MODE_NONE;
            } 
            break;
        }
        case UI_BUTTON_EXPLODE: {
            input_t input;
            input.type = INPUT_EXPLODE;
            input.explode.entity_count = state.selection.size();
            memcpy(&input.explode.entity_ids, &state.selection[0], input.explode.entity_count * sizeof(entity_id));
            state.input_queue.push_back(input);
            break;
        }
        case UI_BUTTON_SMOKE: {
            bool is_smoke_off_cooldown = false;
            for (entity_id id : state.selection) {
                if (state.match_state.entities.get_by_id(id).cooldown_timer == 0) {
                    is_smoke_off_cooldown = true;
                    break;
                }
            }
            if (!is_smoke_off_cooldown) {
                ui_show_status(state, UI_STATUS_SMOKE_COOLDOWN);
                break;
            }
            state.mode = UI_MODE_TARGET_SMOKE;
            break;
        }
        default:
            break;
    }

    for (auto it : UPGRADE_DATA) {
        if (it.second.ui_button == button) {
            if (!match_player_upgrade_is_available(state.match_state, network_get_player_id(), it.first)) {
                return;
            }

            if (state.match_state.player_gold[network_get_player_id()] < it.second.gold_cost) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                return;
            }

            state.input_queue.push_back((input_t) {
                .type = INPUT_BUILDING_ENQUEUE,
                .building_enqueue = (input_building_enqueue_t) {
                    .building_id = state.selection[0],
                    .item = (building_queue_item_t) {
                        .type = BUILDING_QUEUE_ITEM_UPGRADE,
                        .upgrade = it.first
                    }
                }
            });

            return;
        }
    }

    for (auto it : ENTITY_DATA) {
        if (it.second.ui_button == button) {
            EntityType entity_type = it.first;
            // Enqueue unit into building
            if (entity_is_unit(entity_type)) {
                 // Decide which building to enqueue
                uint32_t selected_building_index = state.match_state.entities.get_index_of(state.selection[0]);
                for (int id_index = 1; id_index < state.selection.size(); id_index++) {
                    if (state.match_state.entities.get_by_id(state.selection[id_index]).queue.size() < state.match_state.entities[selected_building_index].queue.size()) {
                        selected_building_index = state.match_state.entities.get_index_of(state.selection[id_index]);
                    }
                }
                entity_t& building = state.match_state.entities[selected_building_index];

                building_queue_item_t item = (building_queue_item_t) {
                    .type = BUILDING_QUEUE_ITEM_UNIT,
                    .unit_type = entity_type
                };

                if (building.queue.size() == BUILDING_QUEUE_MAX) {
                    ui_show_status(state, UI_STATUS_BUILDING_QUEUE_FULL);
                } else if (state.match_state.player_gold[network_get_player_id()] < building_queue_item_cost(item)) {
                    ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                } else {
                    input_t input = (input_t) {
                        .type = INPUT_BUILDING_ENQUEUE,
                        .building_enqueue = (input_building_enqueue_t) {
                            .building_id = state.match_state.entities.get_id_of(selected_building_index),
                            .item = item
                        }
                    };
                    state.input_queue.push_back(input);
                }
            } else {
                if (state.match_state.player_gold[network_get_player_id()] < it.second.gold_cost) {
                    ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                    return;
                }

                state.mode = UI_MODE_BUILDING_PLACE;
                state.building_type = entity_type;
            }

            return;
        }
    }
}

void ui_deselect_entity_if_selected(ui_state_t& state, entity_id id) {
    for (uint32_t id_index = 0; id_index < state.selection.size(); id_index++) {
        if (state.selection[id_index] == id) {
            state.selection.erase(state.selection.begin() + id_index);
            return;
        }
    }
}

void ui_show_status(ui_state_t& state, const char* message) {
    state.status_message = std::string(message);
    state.status_timer = UI_STATUS_DURATION;
}

xy ui_get_building_cell(const ui_state_t& state) {
    int building_cell_size = entity_cell_size(state.building_type);
    xy offset = building_cell_size == 1 ? xy(0, 0) : xy(building_cell_size, building_cell_size) - xy(2, 2);
    xy building_cell = ((engine.mouse_position + state.camera_offset) / TILE_SIZE) - offset;
    building_cell.x = std::max(0, building_cell.x);
    building_cell.y = std::max(0, building_cell.y);
    return building_cell;
}

bool ui_building_can_be_placed(const ui_state_t& state) {
    xy building_cell = ui_get_building_cell(state);
    int building_cell_size = ENTITY_DATA.at(state.building_type).cell_size;
    xy miner_cell = state.match_state.entities.get_by_id(match_get_nearest_builder(state.match_state, state.selection, building_cell)).cell;

    if (!map_is_cell_rect_in_bounds(state.match_state, building_cell, building_cell_size)) {
        return false;
    }

    SDL_Rect building_rect = (SDL_Rect) { .x = building_cell.x, .y = building_cell.y, .w = building_cell_size, .h = building_cell_size };
    for (const entity_t& entity : state.match_state.entities) {
        if (entity.type == ENTITY_GOLD_MINE) {
            SDL_Rect gold_block_rect = entity_gold_get_block_building_rect(entity.cell);
            if ((state.building_type == ENTITY_CAMP || state.building_type == ENTITY_HALL) && SDL_HasIntersection(&building_rect, &gold_block_rect) == SDL_TRUE) {
                return false;
            }
        } 
    }

    for (int x = building_cell.x; x < building_cell.x + building_cell_size; x++) {
        for (int y = building_cell.y; y < building_cell.y + building_cell_size; y++) {
            if ((xy(x, y) != miner_cell && state.match_state.map_cells[x + (y * state.match_state.map_width)] != CELL_EMPTY) || 
                    state.match_state.map_fog[network_get_player_id()][x + (y * state.match_state.map_width)] == FOG_HIDDEN || 
                    map_is_tile_ramp(state.match_state, xy(x, y)) ||
                    state.match_state.map_mine_cells[x + (y * state.match_state.map_width)] != ID_NULL) {
                return false;
            }
        }
    }

    return true;
}

ui_tooltip_info_t ui_get_hovered_tooltip_info(const ui_state_t& state) {
    ui_tooltip_info_t info;
    memset(&info, 0, sizeof(info));
    char* info_text_ptr = info.text;

    UiButton button = state.buttons[ui_get_ui_button_hovered(state)];
    if (!ui_button_requirements_met(state, button)) {
        auto button_requirements_it = UI_BUTTON_REQUIREMENTS.find(button);
        info_text_ptr += sprintf(info_text_ptr, "Requires ");
        switch (button_requirements_it->second.type) {
            case UI_BUTTON_REQUIRES_BUILDING: {
                info_text_ptr += sprintf(info_text_ptr, "%s", ENTITY_DATA.at(button_requirements_it->second.building_type).name);
                break;
            }
            case UI_BUTTON_REQUIRES_UPGRADE: {
                info_text_ptr += sprintf(info_text_ptr, "%s", UPGRADE_DATA.at(button_requirements_it->second.upgrade).name);
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
        case UI_BUTTON_BUILD2:
            info_text_ptr += sprintf(info_text_ptr, "Advanced Build");
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
        case UI_BUTTON_EXPLODE:
            info_text_ptr += sprintf(info_text_ptr, "Explode");
            break;
        case UI_BUTTON_SMOKE:
            info_text_ptr += sprintf(info_text_ptr, "Smoke Bomb");
            break;
        case UI_BUTTON_CANCEL:
            info_text_ptr += sprintf(info_text_ptr, "Cancel");
            break;
        default: {
            for (auto upgrade_data_it : UPGRADE_DATA) {
                if (upgrade_data_it.second.ui_button == button) {
                    info_text_ptr += sprintf(info_text_ptr, "Research %s", upgrade_data_it.second.name);
                    info.gold_cost = upgrade_data_it.second.gold_cost;
                    break; // breaks for upgrade_data_it
                }
            }

            for (auto entity_data_it : ENTITY_DATA) {
                if (entity_data_it.second.ui_button == button) {
                    if (button == UI_BUTTON_BUILD_MINE) {
                        info_text_ptr += sprintf(info_text_ptr, "Lay %s", entity_data_it.second.name);
                    } else {
                        info_text_ptr += sprintf(info_text_ptr, "%s %s", entity_is_unit(entity_data_it.first) ? "Hire" : "Build", entity_data_it.second.name);
                    }
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
        SDL_Keycode hotkey = engine.hotkeys.at(button);
        info_text_ptr += engine_sdl_key_str(info_text_ptr, hotkey);
        info_text_ptr += sprintf(info_text_ptr, ")");
    }

    return info;
}

xy ui_garrisoned_icon_position(int index) {
    return UI_FRAME_BOTTOM_POSITION + BUILDING_QUEUE_TOP_LEFT + xy(index * 34, 0);
}

int ui_get_garrisoned_index_hovered(const ui_state_t& state) {
    UiSelectionType selection_type = ui_get_selection_type(state, state.selection);
    if (!(selection_type == UI_SELECTION_TYPE_UNITS || selection_type == UI_SELECTION_TYPE_BUILDINGS) ||
            state.selection.size() != 1 || ui_is_selecting(state) || state.is_minimap_dragging || 
            !(state.mode == UI_MODE_NONE || state.mode == UI_MODE_BUILD || state.mode == UI_MODE_BUILD2)) {
        return -1;
    } 

    const entity_t& entity = state.match_state.entities.get_by_id(state.selection[0]);
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

int ui_get_selected_unit_hovered(const ui_state_t& state) {
    if (state.selection.size() < 2 || ui_is_selecting(state) || state.is_minimap_dragging || !(state.mode == UI_MODE_NONE || state.mode == UI_MODE_BUILD || state.mode == UI_MODE_BUILD2)) {
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

int ui_get_building_queue_item_hovered(const ui_state_t& state) {
    if (state.selection.size() != 1 || ui_get_selection_type(state, state.selection) != UI_SELECTION_TYPE_BUILDINGS || 
                ui_is_selecting(state) || state.is_minimap_dragging || !(state.mode == UI_MODE_NONE)) {
        return -1;
    } 

    for (int index = 0; index < state.match_state.entities.get_by_id(state.selection[0]).queue.size(); index++) {
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

// RENDER

void ui_render(const ui_state_t& state) {
    // Prepare map render
    xy base_pos = xy(-(state.camera_offset.x % TILE_SIZE), -(state.camera_offset.y % TILE_SIZE));
    xy base_coords = xy(state.camera_offset.x / TILE_SIZE, state.camera_offset.y / TILE_SIZE);
    xy max_visible_tiles = xy(SCREEN_WIDTH / TILE_SIZE, (SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE);
    if (base_pos.x != 0) {
        max_visible_tiles.x++;
    }
    if (base_pos.y != 0) {
        max_visible_tiles.y++;
    }

    std::vector<render_sprite_params_t> above_fog_render_params;

    // Begin elevation passes
    for (uint16_t elevation = 0; elevation < 3; elevation++) {
        std::vector<render_sprite_params_t> ysorted_render_params;

        // Render map
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.match_state.map_width);
                // If current elevation is above the tile, then skip it
                if (state.match_state.map_tiles[map_index].elevation < elevation) {
                    continue;
                }

                // If current elevation is equal to the tile, then render the tile, otherwise render default ground tile beneath it
                uint16_t map_tile_index = state.match_state.map_tiles[map_index].elevation == elevation
                                            ? state.match_state.map_tiles[map_index].index
                                            : 1;
                SDL_Rect tile_src_rect = (SDL_Rect) {
                    .x = map_tile_index * TILE_SIZE,
                    .y = 0,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                SDL_Rect tile_dst_rect = (SDL_Rect) {
                    .x = base_pos.x + (x * TILE_SIZE),
                    .y = base_pos.y + (y * TILE_SIZE),
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                // Render a sand tile below front walls
                if (map_tile_index != 1 && elevation == 0) {
                    SDL_Rect below_tile_src_rect = tile_src_rect;
                    below_tile_src_rect.x = TILE_SIZE;
                    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILESET_ARIZONA].texture, &below_tile_src_rect, &tile_dst_rect);
                }
                SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_TILESET_ARIZONA].texture, &tile_src_rect, &tile_dst_rect);

                // Render decorations
                if (state.match_state.map_cells[map_index] >= CELL_DECORATION_1 && state.match_state.map_cells[map_index] <= CELL_DECORATION_5) {
                    ysorted_render_params.push_back((render_sprite_params_t) {
                        .sprite = SPRITE_TILE_DECORATION,
                        .frame = xy(state.match_state.map_cells[map_index] - CELL_DECORATION_1, 0),
                        .position = xy(tile_dst_rect.x, tile_dst_rect.y),
                        .options = RENDER_SPRITE_NO_CULL,
                        .recolor_id = RECOLOR_NONE
                    });
                }
            } // End for x of visible tiles
        } // End for y of visible tiles
        // End render map

        // Mine select rings and healthbars
        for (entity_id id : state.selection) {
            const entity_t& entity = state.match_state.entities.get_by_id(id);
            if (entity.type != ENTITY_LAND_MINE || entity_get_elevation(state.match_state, entity) != elevation) {
                continue;
            }

            // Select ring
            render_sprite(entity_get_select_ring(entity, entity.player_id == PLAYER_NONE || entity.player_id == network_get_player_id()), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);

            // Determine the healthbar rect
            SDL_Rect entity_rect = entity_get_rect(entity);
            entity_rect.x -= state.camera_offset.x;
            entity_rect.y -= state.camera_offset.y;
            xy healthbar_position = xy(entity_rect.x, entity_rect.y + entity_rect.h + HEALTHBAR_PADDING);
            ui_render_healthbar(healthbar_position, xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, ENTITY_DATA.at(entity.type).max_health);
        }

        // Mine UI move animation
        if (animation_is_playing(state.move_animation) && 
                map_get_tile(state.match_state, state.move_animation_position / TILE_SIZE).elevation == elevation &&
                state.move_animation.name != ANIMATION_UI_MOVE_CELL && state.move_animation.frame.x % 2 == 0) {
            uint32_t entity_index = state.match_state.entities.get_index_of(state.move_animation_entity_id);
            if (entity_index != INDEX_INVALID && state.match_state.entities[entity_index].type == ENTITY_LAND_MINE) {
                const entity_t& entity = state.match_state.entities[entity_index];
                render_sprite(entity_get_select_ring(entity, state.move_animation.name == ANIMATION_UI_MOVE_ENTITY), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);
            } else {
                auto remembered_entities_it = state.match_state.remembered_entities[network_get_player_id()].find(state.move_animation_entity_id);
                if (remembered_entities_it != state.match_state.remembered_entities[network_get_player_id()].end()) {
                    if (remembered_entities_it->second.sprite_params.sprite == SPRITE_BUILDING_MINE) {
                        render_sprite(SPRITE_SELECT_RING_MINE, xy(0, 0), (remembered_entities_it->second.cell * TILE_SIZE) + (xy(remembered_entities_it->second.cell_size, remembered_entities_it->second.cell_size) * TILE_SIZE / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                }
            }
        }

        // Render entity corpses
        for (const entity_t& entity : state.match_state.entities) {
            if (!(entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_LAND_MINE) || 
                    entity_get_elevation(state.match_state, entity) != elevation || 
                    (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && entity.player_id != network_get_player_id() && state.match_state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.match_state.map_width)] == 0) ||
                    !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
                continue;
            }

            render_sprite_params_t render_params = ui_create_entity_render_params(state, entity);
            render_sprite(render_params.sprite, render_params.frame, render_params.position, render_params.options, render_params.recolor_id);
        }

        // Remembered entity corpses
        for (auto it : state.match_state.remembered_entities[network_get_player_id()]) {
            if (it.second.sprite_params.sprite < SPRITE_BUILDING_DESTROYED_2 || it.second.sprite_params.sprite > SPRITE_BUILDING_DESTROYED_MINE ||
                    map_get_tile(state.match_state, it.second.cell).elevation != elevation || map_is_cell_rect_revealed(state.match_state, network_get_player_id(), it.second.cell, it.second.cell_size)) {
                continue;
            }

            render_sprite_params_t render_params = it.second.sprite_params;
            render_params.position -= state.camera_offset;
            render_sprite(render_params.sprite, render_params.frame, render_params.position, render_params.options, render_params.recolor_id);
        }

        // Select rings and healthbars
        for (entity_id id : state.selection) {
            const entity_t& entity = state.match_state.entities.get_by_id(id);
            if (entity.type == ENTITY_LAND_MINE || entity_get_elevation(state.match_state, entity) != elevation) {
                continue;
            }

            // Select ring
            render_sprite(entity_get_select_ring(entity, entity.player_id == PLAYER_NONE || entity.player_id == network_get_player_id()), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);

            if (entity.type == ENTITY_GOLD_MINE) {
                continue;
            }
            // Determine the healthbar rect
            SDL_Rect entity_rect = entity_get_rect(entity);
            entity_rect.x -= state.camera_offset.x;
            entity_rect.y -= state.camera_offset.y;
            xy healthbar_position = xy(entity_rect.x, entity_rect.y + entity_rect.h + (entity_is_unit(entity.type) ? HEALTHBAR_PADDING : BUILDING_HEALTHBAR_PADDING));
            ui_render_healthbar(healthbar_position, xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, ENTITY_DATA.at(entity.type).max_health);
            if (ENTITY_DATA.at(entity.type).garrison_capacity != 0) {
                ui_render_garrisoned_units_healthbar(healthbar_position + xy(0, HEALTHBAR_HEIGHT + 1), xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.garrisoned_units.size(), ENTITY_DATA.at(entity.type).garrison_capacity);
            }
        }

        // Entities
        for (const entity_t& entity : state.match_state.entities) {
            if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_LAND_MINE ||
                    !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type)) || 
                    (entity.player_id != network_get_player_id() && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state.match_state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.match_state.map_width)] == 0) ||
                    entity.garrison_id != ID_NULL || entity_get_elevation(state.match_state, entity) != elevation) {
                continue;
            }

            render_sprite_params_t render_params = ui_create_entity_render_params(state, entity);
            SDL_Rect render_rect = (SDL_Rect) {
                .x = render_params.position.x,
                .y = render_params.position.y,
                .w = engine.sprites[render_params.sprite].frame_size.x,
                .h = engine.sprites[render_params.sprite].frame_size.y
            };
            // Perform culling in advance so that we can minimize y-sorting
            if (SDL_HasIntersection(&render_rect, &SCREEN_RECT) != SDL_TRUE) {
                continue;
            }
            render_params.options |= RENDER_SPRITE_NO_CULL;

            ysorted_render_params.push_back(render_params);
        }

        // Remembered entities
        for (auto it : state.match_state.remembered_entities[network_get_player_id()]) {
            if ((it.second.sprite_params.sprite >= SPRITE_BUILDING_DESTROYED_2 && it.second.sprite_params.sprite <= SPRITE_BUILDING_DESTROYED_MINE) || it.second.sprite_params.sprite == SPRITE_BUILDING_MINE ||
                    map_get_tile(state.match_state, it.second.cell).elevation != elevation || map_is_cell_rect_revealed(state.match_state, network_get_player_id(), it.second.cell, it.second.cell_size)) {
                continue;
            }

            SDL_Rect render_rect = (SDL_Rect) {
                .x = it.second.sprite_params.position.x - state.camera_offset.x,
                .y = it.second.sprite_params.position.y - state.camera_offset.y,
                .w = engine.sprites[it.second.sprite_params.sprite].frame_size.x,
                .h = engine.sprites[it.second.sprite_params.sprite].frame_size.y
            };
            // Perform culling in advance so that we can minimize y-sorting
            if (SDL_HasIntersection(&render_rect, &SCREEN_RECT) != SDL_TRUE) {
                continue;
            }

            render_sprite_params_t render_params = it.second.sprite_params;
            render_params.position -= state.camera_offset;
            render_params.options |= RENDER_SPRITE_NO_CULL;

            ysorted_render_params.push_back(render_params);
        }

        // UI move animation
        if (animation_is_playing(state.move_animation) && 
                map_get_tile(state.match_state, state.move_animation_position / TILE_SIZE).elevation == elevation) {
            if (state.move_animation.name == ANIMATION_UI_MOVE_CELL) {
                render_sprite_params_t ui_move_params = (render_sprite_params_t) {
                    .sprite = SPRITE_UI_MOVE,
                    .frame = state.move_animation.frame,
                    .position = state.move_animation_position - state.camera_offset,
                    .options = RENDER_SPRITE_CENTERED,
                    .recolor_id = RECOLOR_NONE
                };
                xy ui_move_cell = state.move_animation_position / TILE_SIZE;
                if (state.match_state.map_fog[network_get_player_id()][ui_move_cell.x + (ui_move_cell.y * state.match_state.map_width)] > 0) {
                    render_sprite(ui_move_params.sprite, ui_move_params.frame, ui_move_params.position, ui_move_params.options);
                } else {
                    above_fog_render_params.push_back(ui_move_params);
                }
            } else if (state.move_animation.frame.x % 2 == 0) {
                uint32_t entity_index = state.match_state.entities.get_index_of(state.move_animation_entity_id);
                if (entity_index != INDEX_INVALID) {
                    if (state.match_state.entities[entity_index].type != ENTITY_LAND_MINE) {
                        const entity_t& entity = state.match_state.entities[entity_index];
                        render_sprite(entity_get_select_ring(entity, state.move_animation.name == ANIMATION_UI_MOVE_ENTITY), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                } else {
                    auto remembered_entities_it = state.match_state.remembered_entities[network_get_player_id()].find(state.move_animation_entity_id);
                    if (remembered_entities_it != state.match_state.remembered_entities[network_get_player_id()].end()) {
                        Sprite select_ring_sprite;
                        if (remembered_entities_it->second.sprite_params.sprite != SPRITE_BUILDING_MINE) {
                            select_ring_sprite = (Sprite)(SPRITE_SELECT_RING_BUILDING_2 + ((remembered_entities_it->second.cell_size - 2) * 2) + 1);
                            render_sprite(select_ring_sprite, xy(0, 0), (remembered_entities_it->second.cell * TILE_SIZE) + (xy(remembered_entities_it->second.cell_size, remembered_entities_it->second.cell_size) * TILE_SIZE / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                        }
                    }
                }
            }
        }

        // Rally points
        if (ui_get_selection_type(state, state.selection) == UI_SELECTION_TYPE_BUILDINGS) {
            for (entity_id id : state.selection) {
                const entity_t& building = state.match_state.entities.get_by_id(id);
                xy rally_cell = building.rally_point / TILE_SIZE;
                if (building.mode == MODE_BUILDING_FINISHED && building.rally_point.x != -1 &&
                        map_get_tile(state.match_state, rally_cell).elevation == elevation) {
                    // Determine whether to render above or below the fog of war
                    std::vector<render_sprite_params_t>& sprite_params_vector = state.match_state.map_fog[building.player_id][rally_cell.x + (rally_cell.y * state.match_state.map_width)] > 0
                                                                                    ? ysorted_render_params
                                                                                    : above_fog_render_params;
                    sprite_params_vector.push_back((render_sprite_params_t) {
                        .sprite = SPRITE_RALLY_FLAG,
                        .frame = state.rally_flag_animation.frame,
                        .position = building.rally_point - xy(4, 15) - state.camera_offset,
                        .options = 0,
                        .recolor_id = network_get_player(building.player_id).recolor_id
                    });
                } // End if should render rally point
            } // End for each id
        } // End if selection type is buildings

        // End Ysort
        ysort_render_params(ysorted_render_params, 0, ysorted_render_params.size() - 1);
        for (const render_sprite_params_t& params : ysorted_render_params) {
            render_sprite(params.sprite, params.frame, params.position, params.options, params.recolor_id);
        }
    } // End for each elevation

    // Smith animation
    for (const entity_t& entity : state.match_state.entities) {
        if (entity.type == ENTITY_SMITH && entity.animation.name != ANIMATION_UNIT_IDLE && map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            render_sprite(SPRITE_BUILDING_SMITH_ANIMATION, entity.animation.frame, entity.position.to_xy() - xy(0, 16) - state.camera_offset);
        }
    }

    // Particles
    for (const particle_t& particle : state.match_state.particles) {
        if (particle.animation.name == ANIMATION_PARTICLE_SMOKE) {
            xy particle_top_left_cell = (particle.position / TILE_SIZE) - xy((PARTICLE_SMOKE_CELL_SIZE - 1) / 2, (PARTICLE_SMOKE_CELL_SIZE - 1) / 2);
            bool particle_is_revealed = false;
            for (int y = particle_top_left_cell.y; y < particle_top_left_cell.y + PARTICLE_SMOKE_CELL_SIZE; y++) {
                for (int x = particle_top_left_cell.x; x < particle_top_left_cell.x + PARTICLE_SMOKE_CELL_SIZE; x++) {
                    if (map_is_cell_in_bounds(state.match_state, xy(x, y)) && state.match_state.map_fog[network_get_player_id()][x + (y * state.match_state.map_width)] > 0) {
                        particle_is_revealed = true;
                        break;
                    }
                }
                if (particle_is_revealed) {
                    break;
                }
            }
            if (!particle_is_revealed) {
                continue;
            }
        } else {
            if (!map_is_cell_rect_revealed(state.match_state, network_get_player_id(), particle.position / TILE_SIZE, 1)) {
                continue;
            }
        }
        render_sprite(particle.sprite, xy(particle.animation.frame.x, particle.vframe), particle.position - state.camera_offset, RENDER_SPRITE_CENTERED);
    }

    // Projectiles
    for (const projectile_t& projectile : state.match_state.projectiles) {
        if (!map_is_cell_rect_revealed(state.match_state, network_get_player_id(), projectile.position.to_xy() / TILE_SIZE, 1)) {
            continue;
        }
        render_sprite(SPRITE_PROJECTILE_SMOKE, xy(0, 0), projectile.position.to_xy() - state.camera_offset, RENDER_SPRITE_CENTERED);
    }

    // Fog of War
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    for (int fog_pass = 0; fog_pass < 2; fog_pass++) {
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                xy fog_cell = base_coords + xy(x, y);
                int fog = state.match_state.map_fog[network_get_player_id()][fog_cell.x + (fog_cell.y * state.match_state.map_width)];
                if (fog > 0) {
                    continue;
                }
                if (fog_pass == 1 && fog == FOG_EXPLORED) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    xy neighbor_cell = fog_cell + DIRECTION_XY[direction];
                    if (!map_is_cell_in_bounds(state.match_state, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if (fog_pass == 0 
                            ? state.match_state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.match_state.map_width)] < 1 
                            : state.match_state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.match_state.map_width)] == FOG_HIDDEN) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }

                for (int direction = 1; direction < DIRECTION_COUNT; direction += 2) {
                    xy neighbor_cell = fog_cell + DIRECTION_XY[direction];
                    int prev_direction = direction - 1;
                    int next_direction = (direction + 1) % DIRECTION_COUNT;
                    if ((neighbors & DIRECTION_MASK[prev_direction]) != DIRECTION_MASK[prev_direction] ||
                        (neighbors & DIRECTION_MASK[next_direction]) != DIRECTION_MASK[next_direction]) {
                        continue;
                    }
                    if (!map_is_cell_in_bounds(state.match_state, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if (fog_pass == 0 
                            ? state.match_state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.match_state.map_width)] < 1 
                            : state.match_state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.match_state.map_width)] == FOG_HIDDEN) {
                        neighbors += DIRECTION_MASK[direction];
                    }
                }
                int hframe = engine.neighbors_to_autotile_index.at(neighbors);
                if (fog_pass == 0) {
                    hframe += 47;
                }

                SDL_Rect fog_dst_rect = (SDL_Rect) {
                    .x = base_pos.x + (x * TILE_SIZE),
                    .y = base_pos.y + (y * TILE_SIZE),
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                SDL_Rect fog_src_rect = (SDL_Rect) {
                    .x = hframe * TILE_SIZE,
                    .y = 0,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                #ifndef GOLD_DEBUG_FOG_DISABLED
                    SDL_RenderCopy(engine.renderer, engine.sprites[SPRITE_FOG_OF_WAR].texture, &fog_src_rect, &fog_dst_rect);
                #endif
            }
        }
    }
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);

    // Above fog of war sprites
    for (render_sprite_params_t params : above_fog_render_params) {
        render_sprite(params.sprite, params.frame, params.position, params.options, params.recolor_id);
    }

    // Debug pathing 
    #ifdef GOLD_DEBUG_UNIT_PATHS
    for (const entity_t& entity : state.match_state.entities) {
        if (!entity_is_unit(entity.type) || entity.path.empty()) {
            continue;
        }
        SDL_Color line_color = entity_is_mining(state.match_state, entity) ? COLOR_GOLD : COLOR_WHITE;
        SDL_SetRenderDrawColor(engine.renderer, line_color.r, line_color.g, line_color.b, line_color.a);
        for (uint32_t path_index = 0; path_index < entity.path.size(); path_index++) {
            xy start = (path_index == 0 ? entity.position.to_xy() : cell_center(entity.path[path_index - 1]).to_xy()) - state.camera_offset;
            xy end = cell_center(entity.path[path_index]).to_xy() - state.camera_offset;
            SDL_RenderDrawLine(engine.renderer, start.x, start.y, end.x, end.y);
        }
    }
    #endif

    // Select rect
    if (ui_is_selecting(state)) {
        SDL_Rect select_rect = state.select_rect;
        select_rect.x -= state.camera_offset.x;
        select_rect.y -= state.camera_offset.y;
        SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(engine.renderer, &select_rect);
    }

    // UI show queued building placements
    for (entity_id id : state.selection) {
        const entity_t& entity = state.match_state.entities.get_by_id(id);
        // If it's not an allied unit, then we can break out of this whole loop, since the rest of the selection won't be either
        if (!entity_is_unit(entity.type) || entity.player_id != network_get_player_id()) {
            break;
        }

        if (entity.target.type == TARGET_BUILD && entity.target.id == ID_NULL) {
            ui_render_target_build(state, entity.target);
        }
        for (const target_t& target : entity.target_queue) {
            if (target.type == TARGET_BUILD) {
                ui_render_target_build(state, target);
            }
        }
    }

    // UI Building Placement
    if (state.mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui()) {
        const entity_data_t& building_data = ENTITY_DATA.at(state.building_type);
        xy ui_building_cell = ui_get_building_cell(state);
        render_sprite(building_data.sprite, xy(3, 0), (ui_building_cell * TILE_SIZE) - state.camera_offset, 0, network_get_player(network_get_player_id()).recolor_id);

        bool is_placement_out_of_bounds = ui_building_cell.x + building_data.cell_size > state.match_state.map_width ||
                                          ui_building_cell.y + building_data.cell_size > state.match_state.map_height;
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
        xy miner_cell = state.match_state.entities.get_by_id(match_get_nearest_builder(state.match_state, state.selection, ui_building_cell)).cell;
        for (int x = ui_building_cell.x; x < ui_building_cell.x + building_data.cell_size; x++) {
            for (int y = ui_building_cell.y; y < ui_building_cell.y + building_data.cell_size; y++) {
                bool is_cell_green = true;
                if (is_placement_out_of_bounds) {
                    is_cell_green = false;
                }
                if (state.match_state.map_fog[network_get_player_id()][x + (y * state.match_state.map_width)] == FOG_HIDDEN || map_is_tile_ramp(state.match_state, xy(x, y)) ||
                        (xy(x, y) != miner_cell && state.match_state.map_cells[x + (y * state.match_state.map_width)] != CELL_EMPTY) ||
                        state.match_state.map_mine_cells[x + (y * state.match_state.map_width)] != ID_NULL) {
                    is_cell_green = false;
                }

                if (state.building_type == ENTITY_CAMP || state.building_type == ENTITY_HALL) {
                    for (const entity_t& gold : state.match_state.entities) {
                        if (gold.type != ENTITY_GOLD_MINE) {
                            continue;
                        }
                        if (sdl_rect_has_point(entity_gold_get_block_building_rect(gold.cell), xy(x, y))) {
                            is_cell_green = false;
                            break;
                        }
                    }
                }

                SDL_Color cell_color = is_cell_green ? COLOR_GREEN : COLOR_RED;
                SDL_SetRenderDrawColor(engine.renderer, cell_color.r, cell_color.g, cell_color.b, 128);
                SDL_Rect cell_rect = (SDL_Rect) { 
                    .x = (x * TILE_SIZE) - state.camera_offset.x, 
                    .y = (y * TILE_SIZE) - state.camera_offset.y,
                    .w = TILE_SIZE,
                    .h = TILE_SIZE
                };
                SDL_RenderFillRect(engine.renderer, &cell_rect);
            }
        }
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);
    }

    // UI Chat
    for (uint32_t chat_index = 0; chat_index < state.match_state.chat.size(); chat_index++) {
        render_text(FONT_HACK_WHITE, state.match_state.chat[state.match_state.chat.size() - chat_index - 1].message.c_str(), xy(16, UI_MINIMAP_RECT.y - 48 - (chat_index * 16)));
    }
    // UI Chat message
    if (SDL_IsTextInputActive()) {
        char prompt_str[128];
        sprintf(prompt_str, "Message: %s", state.chat_message.c_str());
        render_text(FONT_HACK_WHITE, prompt_str, xy(UI_CHAT_RECT.x, UI_CHAT_RECT.y));
    }

    // UI Status message
    if (state.status_timer != 0) {
        render_text(FONT_HACK_WHITE, state.status_message.c_str(), xy(RENDER_TEXT_CENTERED, SCREEN_HEIGHT - 148));
    }

    // UI Disconnect frame
    if (state.disconnect_timer > MATCH_DISCONNECT_GRACE) {
        render_ninepatch(SPRITE_UI_FRAME, UI_DISCONNECT_FRAME_RECT, 16);
        render_text(FONT_WESTERN8_GOLD, "Waiting for players...", xy(UI_DISCONNECT_FRAME_RECT.x + 16, UI_DISCONNECT_FRAME_RECT.y + 8));
        int player_text_y = 32;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == PLAYER_STATUS_NONE || !(state.inputs[player_id].empty() || state.inputs[player_id][0].empty())) {
                continue;
            }

            render_text(FONT_WESTERN8_GOLD, network_get_player(player_id).name, xy(UI_DISCONNECT_FRAME_RECT.x + 24, UI_DISCONNECT_FRAME_RECT.y + player_text_y));
            player_text_y += 24;
        }
    }

    // UI Match over
    if (state.mode == UI_MODE_MATCH_OVER_VICTORY || state.mode == UI_MODE_MATCH_OVER_DEFEAT) {
        render_ninepatch(SPRITE_UI_FRAME, UI_MATCH_OVER_FRAME_RECT, 16);
        render_text(FONT_WESTERN8_GOLD, state.mode == UI_MODE_MATCH_OVER_VICTORY ? "Victory!" : "Defeat!", xy(RENDER_TEXT_CENTERED, UI_MATCH_OVER_FRAME_RECT.y + 10));

        bool continue_button_hovered = sdl_rect_has_point(UI_MATCH_OVER_CONTINUE_BUTTON_RECT, engine.mouse_position);
        xy continue_button_position = xy(UI_MATCH_OVER_CONTINUE_BUTTON_RECT.x, UI_MATCH_OVER_CONTINUE_BUTTON_RECT.y + (continue_button_hovered ? -1 : 0));
        render_sprite(SPRITE_UI_MENU_PARCHMENT_BUTTONS, xy(0, continue_button_hovered ? 1 : 0), continue_button_position, RENDER_SPRITE_NO_CULL);
        render_text(continue_button_hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, "CONTINUE", continue_button_position + xy(UI_MATCH_OVER_EXIT_BUTTON_RECT.w / 2, 5), TEXT_ANCHOR_TOP_CENTER);


        bool exit_button_hovered = sdl_rect_has_point(UI_MATCH_OVER_EXIT_BUTTON_RECT, engine.mouse_position);
        xy exit_button_position = xy(UI_MATCH_OVER_EXIT_BUTTON_RECT.x, UI_MATCH_OVER_EXIT_BUTTON_RECT.y + (exit_button_hovered ? -1 : 0));
        render_sprite(SPRITE_UI_MENU_PARCHMENT_BUTTONS, xy(0, exit_button_hovered ? 1 : 0), exit_button_position, RENDER_SPRITE_NO_CULL);
        render_text(exit_button_hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, "EXIT", exit_button_position + xy(UI_MATCH_OVER_EXIT_BUTTON_RECT.w / 2, 5), TEXT_ANCHOR_TOP_CENTER);
    }

    // Menu button
    render_sprite(SPRITE_UI_MENU_BUTTON, xy(sdl_rect_has_point(UI_MENU_BUTTON_RECT, engine.mouse_position) || state.mode == UI_MODE_MENU ? 1 : 0, 0), xy(UI_MENU_BUTTON_RECT.x, UI_MENU_BUTTON_RECT.y), RENDER_SPRITE_NO_CULL);
    if (state.mode == UI_MODE_MENU || state.mode == UI_MODE_MENU_SURRENDER) {
        render_ninepatch(SPRITE_UI_FRAME, UI_MENU_RECT, 16);
        render_text(FONT_WESTERN8_GOLD, state.mode == UI_MODE_MENU ? "Game Menu" : "Surrender?", xy(RENDER_TEXT_CENTERED, UI_MENU_RECT.y + 10));
        int button_count = state.mode == UI_MODE_MENU ? UI_MENU_BUTTON_COUNT : UI_MENU_SURRENDER_BUTTON_COUNT;
        for (int i = 0; i < button_count; i++) {
            bool button_hovered = sdl_rect_has_point(UI_MENU_BUTTON_RECTS[i], engine.mouse_position);
            xy button_position = xy(UI_MENU_BUTTON_RECTS[i].x, UI_MENU_BUTTON_RECTS[i].y + (button_hovered ? -1 : 0));
            render_sprite(SPRITE_UI_MENU_PARCHMENT_BUTTONS, xy(0, button_hovered ? 1 : 0), button_position, RENDER_SPRITE_NO_CULL);
            render_text(button_hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, state.mode == UI_MODE_MENU ? UI_MENU_BUTTON_TEXT[i] : UI_MENU_SURRENDER_BUTTON_TEXT[i], button_position + xy(UI_MENU_BUTTON_RECTS[i].w / 2, 5), TEXT_ANCHOR_TOP_CENTER);
        }
    }

    // UI Control Groups
    for (uint32_t control_group_index = 0; control_group_index < 10; control_group_index++) {
        // Count the entities in this control group and determine which is the most common
        uint32_t entity_count = 0;
        std::unordered_map<EntityType, uint32_t> entity_occurances;
        EntityType most_common_entity_type = ENTITY_MINER;
        entity_occurances[ENTITY_MINER] = 0;

        for (entity_id id : state.control_groups[control_group_index]) {
            uint32_t entity_index = state.match_state.entities.get_index_of(id);
            if (entity_index == INDEX_INVALID || !entity_is_selectable(state.match_state.entities[entity_index])) {
                continue;
            }

            EntityType entity_type = state.match_state.entities[entity_index].type;
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
        Font font = FONT_M3X6_OFFBLACK;
        if (state.control_group_selected != control_group_index) {
            font = FONT_M3X6_DARKBLACK;
            button_frame = 2;
        }

        int button_icon = ENTITY_DATA.at(most_common_entity_type).ui_button;
        xy render_pos = UI_FRAME_BOTTOM_POSITION + xy(2, 0) + xy((3 + engine.sprites[SPRITE_UI_CONTROL_GROUP_FRAME].frame_size.x) * control_group_index, -32);
        render_sprite(SPRITE_UI_CONTROL_GROUP_FRAME, xy(button_frame, 0), render_pos, RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(button_icon - 1, button_frame), render_pos + xy(1, 0), RENDER_SPRITE_NO_CULL);
        char control_group_number_text[4];
        sprintf(control_group_number_text, "%u", control_group_index == 9 ? 0 : control_group_index + 1);
        render_text(font, control_group_number_text, render_pos + xy(3, -9));
        char control_group_count_text[4];
        sprintf(control_group_count_text, "%u", entity_count);
        render_text(font, control_group_count_text, render_pos + xy(32, 23), TEXT_ANCHOR_BOTTOM_RIGHT);
    }

    // UI frames
    render_sprite(SPRITE_UI_MINIMAP, xy(0, 0), xy(0, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_MINIMAP].frame_size.y));
    render_sprite(SPRITE_UI_FRAME_BOTTOM, xy(0, 0), UI_FRAME_BOTTOM_POSITION);
    render_sprite(SPRITE_UI_FRAME_BUTTONS, xy(0, 0), xy(engine.sprites[SPRITE_UI_MINIMAP].frame_size.x + engine.sprites[SPRITE_UI_FRAME_BOTTOM].frame_size.x, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y));

    // UI Buttons
    for (int i = 0; i < UI_BUTTONSET_SIZE; i++) {
        UiButton ui_button = state.buttons[i];
        if (ui_button == UI_BUTTON_NONE) {
            continue;
        }

        bool is_button_hovered = ui_get_ui_button_hovered(state) == i && ui_button_requirements_met(state, ui_button);
        bool is_button_pressed = is_button_hovered && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LEFT) != 0;
        xy offset = is_button_pressed ? xy(1, 1) : (is_button_hovered ? xy(0, -1) : xy(0, 0));
        int button_state = 0;
        if (!ui_button_requirements_met(state, ui_button)) {
            button_state = 2;
        } else if (is_button_hovered) {
            button_state = 1;
        }
        render_sprite(SPRITE_UI_BUTTON, xy(button_state, 0), xy(UI_BUTTON_RECT[i].x, UI_BUTTON_RECT[i].y) + offset);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(ui_button - 1, button_state), xy(UI_BUTTON_RECT[i].x, UI_BUTTON_RECT[i].y) + offset);
    }

    // UI Selection list
    if (state.selection.size() == 1) {
        const entity_t& entity = state.match_state.entities.get_by_id(state.selection[0]);
        const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

        render_text_with_text_frame(SPRITE_UI_TEXT_FRAME, FONT_WESTERN8_OFFBLACK, entity_data.name, SELECTION_LIST_TOP_LEFT, xy(0, 2));
        render_sprite(SPRITE_UI_BUTTON, xy(0, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(entity_data.ui_button - 1, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);

        if (entity.type != ENTITY_GOLD_MINE) {
            xy healthbar_position = SELECTION_LIST_TOP_LEFT + xy(0, 18 + 35);
            xy healthbar_size = xy(64, 12);
            ui_render_healthbar(healthbar_position, healthbar_size, entity.health, entity_data.max_health);

            char health_text[10];
            sprintf(health_text, "%i/%i", entity.health, entity_data.max_health);
            xy health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
            xy health_text_position = healthbar_position + xy((healthbar_size.x / 2) - (health_text_size.x / 2), (healthbar_size.y / 2) - (health_text_size.y / 2));
            render_text(FONT_HACK_WHITE, health_text, health_text_position, TEXT_ANCHOR_BOTTOM_LEFT);

            if (ENTITY_DATA.at(entity.type).has_detection) {
                render_text(FONT_WESTERN8_GOLD, "Detection", SELECTION_LIST_TOP_LEFT + xy(36, 22));
            }
            if (entity.type == ENTITY_TINKER && entity.cooldown_timer != 0) {
                char cooldown_text[64];
                sprintf(cooldown_text, "Smoke Bomb cooldown: %us", entity.cooldown_timer / 60);
                render_text(FONT_WESTERN8_GOLD, cooldown_text, SELECTION_LIST_TOP_LEFT + xy(36, 36));
            }
        } else {
            char gold_left_str[17];
            sprintf(gold_left_str, "Gold Left: %u", entity.gold_held);
            render_text(FONT_WESTERN8_GOLD, gold_left_str, SELECTION_LIST_TOP_LEFT + xy(36, 22));
        }
    } else {
        for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
            const entity_t& entity = state.match_state.entities.get_by_id(state.selection[selection_index]);
            const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

            bool icon_hovered = ui_get_selected_unit_hovered(state) == selection_index;
            xy icon_position = ui_get_selected_unit_icon_position(selection_index) + (icon_hovered ? xy(0, -1) : xy(0, 0));
            render_sprite(SPRITE_UI_BUTTON, xy(icon_hovered ? 1 : 0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(entity_data.ui_button - 1, icon_hovered ? 1 : 0), icon_position, RENDER_SPRITE_NO_CULL);
            ui_render_healthbar(icon_position + xy(1, 32 - 5), xy(32- 2, 4), entity.health, entity_data.max_health);
        }
    }

    UiSelectionType ui_selection_type = ui_get_selection_type(state, state.selection);

    // UI Building queues
    if (state.selection.size() == 1 && ui_selection_type == UI_SELECTION_TYPE_BUILDINGS && !state.match_state.entities.get_by_id(state.selection[0]).queue.empty()) {
        const entity_t& building = state.match_state.entities.get_by_id(state.selection[0]);
        for (uint32_t building_queue_index = 0; building_queue_index < building.queue.size(); building_queue_index++) {
            bool icon_hovered = ui_get_building_queue_item_hovered(state) == building_queue_index;
            xy icon_position = UI_BUILDING_QUEUE_POSITIONS[building_queue_index] + (icon_hovered ? xy(0, -1) : xy(0, 0));
            render_sprite(SPRITE_UI_BUTTON, xy(icon_hovered ? 1 : 0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(building_queue_item_icon(building.queue[building_queue_index]) - 1, icon_hovered ? 1 : 0), icon_position, RENDER_SPRITE_NO_CULL);
        }

        static const SDL_Rect BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT = (SDL_Rect) {
            .x = UI_FRAME_BOTTOM_POSITION.x + BUILDING_QUEUE_TOP_LEFT.x + 32 + 4,
            .y = UI_FRAME_BOTTOM_POSITION.y + BUILDING_QUEUE_TOP_LEFT.y + 32 - 8,
            .w = 32 * 3 + (4 * 2), .h = 6
        };
        if (building.timer == BUILDING_QUEUE_BLOCKED) {
            render_text(FONT_WESTERN8_GOLD, "Build more houses.", xy(BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y - 12));
        } else if (building.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
            render_text(FONT_WESTERN8_GOLD, "Exit is blocked.", xy(BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x + 2, BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y - 12));
        } else {
            SDL_Rect building_queue_progress_bar_rect = (SDL_Rect) {
                .x = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.x,
                .y = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.y,
                .w = (BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.w * (int)(building_queue_item_duration(building.queue[0]) - building.timer)) / (int)building_queue_item_duration(building.queue[0]),
                .h = BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT.h,
            };
            SDL_SetRenderDrawColor(engine.renderer, COLOR_WHITE.r, COLOR_WHITE.g, COLOR_WHITE.b, COLOR_WHITE.a);
            SDL_RenderFillRect(engine.renderer, &building_queue_progress_bar_rect);
            SDL_SetRenderDrawColor(engine.renderer, COLOR_BLACK.r, COLOR_BLACK.g, COLOR_BLACK.b, COLOR_BLACK.a);
            SDL_RenderDrawRect(engine.renderer, &BUILDING_QUEUE_PROGRESS_BAR_FRAME_RECT);
        }
    }

    // UI Garrisoned units
    if (state.selection.size() == 1 && (ui_selection_type == UI_SELECTION_TYPE_BUILDINGS || ui_selection_type == UI_SELECTION_TYPE_UNITS)) {
        const entity_t& entity = state.match_state.entities.get_by_id(state.selection[0]);
        for (int index = 0; index < entity.garrisoned_units.size(); index++) {
            const entity_t& garrisoned_unit = state.match_state.entities.get_by_id(entity.garrisoned_units[index]);
            bool icon_hovered = ui_get_garrisoned_index_hovered(state) == index;
            xy icon_position = ui_garrisoned_icon_position(index) + (icon_hovered ? xy(0, -1) : xy(0, 0));

            render_sprite(SPRITE_UI_BUTTON, xy(icon_hovered ? 1 : 0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(ENTITY_DATA.at(garrisoned_unit.type).ui_button - 1, icon_hovered ? 1 : 0), icon_position, RENDER_SPRITE_NO_CULL);
            ui_render_healthbar(icon_position + xy(1, 32 - 5), xy(32 - 2, 4), garrisoned_unit.health, ENTITY_DATA.at(garrisoned_unit.type).max_health);
        }
    }

    // UI Tooltip
    if (ui_get_ui_button_hovered(state) != -1) {
        ui_tooltip_info_t tooltip = ui_get_hovered_tooltip_info(state);
        xy tooltip_text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, tooltip.text);

        int tooltip_min_width = 10 + tooltip_text_size.x;
        int tooltip_cell_width = tooltip_min_width / 8;
        int tooltip_cell_height = tooltip.gold_cost != 0 ? 5 : 3;
        if (tooltip_min_width % 8 != 0) {
            tooltip_cell_width++;
        }
        xy tooltip_top_left = xy(SCREEN_WIDTH - (tooltip_cell_width * 8) - 2, SCREEN_HEIGHT - engine.sprites[SPRITE_UI_FRAME_BUTTONS].frame_size.y - (tooltip_cell_height * 8) - 2);
        for (int tooltip_x_index = 0; tooltip_x_index < tooltip_cell_width; tooltip_x_index++) {
            for (int tooltip_y_index = 0; tooltip_y_index < tooltip_cell_height; tooltip_y_index++) {
                xy tooltip_frame;
                if (tooltip_x_index == 0) {
                    tooltip_frame.x = 0;
                } else if (tooltip_x_index == tooltip_cell_width - 1) {
                    tooltip_frame.x = 2;
                } else {
                    tooltip_frame.x = 1;
                }
                if (tooltip_y_index == 0) {
                    tooltip_frame.y = 0;
                } else if (tooltip_y_index == tooltip_cell_height - 1) {
                    tooltip_frame.y = 2;
                } else {
                    tooltip_frame.y = 1;
                }
                render_sprite(SPRITE_UI_TOOLTIP_FRAME, tooltip_frame, tooltip_top_left + (xy(tooltip_x_index * 8, tooltip_y_index * 8)), RENDER_SPRITE_NO_CULL);
            }
        }

        render_text(FONT_WESTERN8_OFFBLACK, tooltip.text, tooltip_top_left + xy(5, 5));

        if (tooltip.gold_cost != 0) {
            render_sprite(SPRITE_UI_GOLD, xy(0, 0), tooltip_top_left + xy(5, 21), RENDER_SPRITE_NO_CULL);
            char gold_text[4];
            sprintf(gold_text, "%u", tooltip.gold_cost);
            render_text(FONT_WESTERN8_OFFBLACK, gold_text, tooltip_top_left + xy(5 + 18, 23));
        }
        if (tooltip.population_cost != 0) {
            render_sprite(SPRITE_UI_HOUSE, xy(0, 0), tooltip_top_left + xy(5 + 18 + 32, 19), RENDER_SPRITE_NO_CULL);
            char population_text[4];
            sprintf(population_text, "%u", tooltip.population_cost);
            render_text(FONT_WESTERN8_OFFBLACK, population_text, tooltip_top_left + xy(5 + 18 + 32 + 22, 23));
        }
    }

    // Resource counters
    char gold_text[8];
    sprintf(gold_text, "%u", state.match_state.player_gold[network_get_player_id()]);
    render_text(FONT_WESTERN8_WHITE, gold_text, xy(SCREEN_WIDTH - 172 + 18, 4));
    render_sprite(SPRITE_UI_GOLD, xy(0, 0), xy(SCREEN_WIDTH - 172, 2), RENDER_SPRITE_NO_CULL);

    char population_text[8];
    sprintf(population_text, "%u/%u", match_get_player_population(state.match_state, network_get_player_id()), match_get_player_max_population(state.match_state, network_get_player_id()));
    render_text(FONT_WESTERN8_WHITE, population_text, xy(SCREEN_WIDTH - 88 + 22, 4));
    render_sprite(SPRITE_UI_HOUSE, xy(0, 0), xy(SCREEN_WIDTH - 88, 0), RENDER_SPRITE_NO_CULL);

    // Minimap prepare texture
    SDL_SetRenderTarget(engine.renderer, engine.minimap_texture);
    SDL_RenderCopy(engine.renderer, engine.minimap_tiles_texture, NULL, NULL);

    // Minimap entities
    for (const entity_t& entity : state.match_state.entities) {
        if (!entity_is_selectable(entity) || !map_is_cell_rect_revealed(state.match_state, network_get_player_id(), entity.cell, entity_cell_size(entity.type)) ||
                (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state.match_state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.match_state.map_width)] == 0)) {
            continue;
        }
        SDL_Rect entity_rect = (SDL_Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size(entity.type) + 1, .h = entity_cell_size(entity.type) + 1
        };
        SDL_Color color;
        if (entity.type == ENTITY_GOLD_MINE) {
            color = COLOR_GOLD;
        } else if (entity_check_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER)) {
           color = COLOR_WHITE;
        } else {
            color = PLAYER_COLORS[network_get_player(entity.player_id).recolor_id].clothes_color;
        }
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(engine.renderer, &entity_rect);
    }

    // Minimap remembered entities
    for (auto it : state.match_state.remembered_entities[network_get_player_id()]) {
        if (map_is_cell_rect_revealed(state.match_state, network_get_player_id(), it.second.cell, it.second.cell_size)) {
            continue;
        }

        SDL_Rect entity_rect = (SDL_Rect) {
            .x = it.second.cell.x, .y = it.second.cell.y,
            .w = it.second.cell_size, .h = it.second.cell_size
        };
        // I'm assuming here that we will never render the player's own units in remembered units
        SDL_Color color;
        if (it.second.sprite_params.recolor_id == RECOLOR_NONE) {
            color = COLOR_GOLD;
        } else {
            color = PLAYER_COLORS[it.second.sprite_params.recolor_id].clothes_color;
        }
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(engine.renderer, &entity_rect);
    }

    // Minimap alerts
    for (const alert_t& alert : state.alerts) {
        if (alert.timer <= UI_ALERT_LINGER_DURATION) {
            continue;
        }

        int alert_timer = alert.timer - UI_ALERT_LINGER_DURATION;
        int alert_rect_margin = 3 + (alert_timer <= 60 ? 0 : ((alert_timer - 60) / 3));
        SDL_Rect alert_rect = (SDL_Rect) {
            .x = alert.cell.x - alert_rect_margin,
            .y = alert.cell.y - alert_rect_margin,
            .w = alert.cell_size + 1 + (alert_rect_margin * 2),
            .h = alert.cell_size + 1 + (alert_rect_margin * 2)
        };

        SDL_Color color = alert.color == ALERT_COLOR_GREEN ? COLOR_GREEN : COLOR_RED;
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawRect(engine.renderer, &alert_rect);
    }

    // Minimap Fog of War
#ifndef GOLD_DEBUG_FOG_DISABLED
    std::vector<SDL_Point> fog_hidden_points;
    fog_hidden_points.reserve(state.match_state.map_width * state.match_state.map_height);
    std::vector<SDL_Point> fog_explored_points;
    fog_explored_points.reserve(state.match_state.map_width * state.match_state.map_height);
    for (int y = 0; y < state.match_state.map_height; y++) {
        for (int x = 0; x < state.match_state.map_width; x++) {
            int fog = state.match_state.map_fog[network_get_player_id()][x + (y * state.match_state.map_width)];
            if (fog == FOG_HIDDEN) {
                fog_hidden_points.push_back((SDL_Point) { .x = x, .y = y });
            } else if (fog == FOG_EXPLORED) {
                fog_explored_points.push_back((SDL_Point) { .x = x, .y = y });
            }
        }
    }
    SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, 255);
    SDL_RenderDrawPoints(engine.renderer, &fog_hidden_points[0], fog_hidden_points.size());

    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, 128);
    SDL_RenderDrawPoints(engine.renderer, &fog_explored_points[0], fog_explored_points.size());
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);
#endif

    // Minimap camera rect
    SDL_Rect camera_rect = (SDL_Rect) {
        .x = state.camera_offset.x / TILE_SIZE,
        .y = state.camera_offset.y / TILE_SIZE,
        .w = SCREEN_WIDTH / TILE_SIZE,
        .h = 1 + ((SCREEN_HEIGHT - UI_HEIGHT) / TILE_SIZE)
    };
    SDL_SetRenderDrawColor(engine.renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(engine.renderer, &camera_rect);

    SDL_SetRenderTarget(engine.renderer, NULL);

    // Minimap render texture
    SDL_RenderCopy(engine.renderer, engine.minimap_texture, NULL, &UI_MINIMAP_RECT);

    if (!ui_is_mouse_in_ui() && engine.render_debug_info) {
        xy mouse_cell = (engine.mouse_position + state.camera_offset) / TILE_SIZE;
        entity_id cell_value = map_get_cell(state.match_state, mouse_cell);
        char cell_text[128];
        switch (cell_value) {
            case CELL_EMPTY:
                sprintf(cell_text, "%i,%i: EMPTY", mouse_cell.x, mouse_cell.y);
                break;
            case CELL_BLOCKED:
                sprintf(cell_text, "%i,%i: BLOCKED", mouse_cell.x, mouse_cell.y);
                break;
            case CELL_UNREACHABLE:
                sprintf(cell_text, "%i,%i: UNREACHABLE", mouse_cell.x, mouse_cell.y);
                break;
            case CELL_DECORATION_1:
            case CELL_DECORATION_2:
            case CELL_DECORATION_3:
            case CELL_DECORATION_4:
            case CELL_DECORATION_5:
                sprintf(cell_text, "%i,%i: DECORATION", mouse_cell.x, mouse_cell.y);
                break;
            default:
                sprintf(cell_text, "%i,%i: %u", mouse_cell.x, mouse_cell.y, cell_value);
                break;
        }
        render_text(FONT_HACK_WHITE, cell_text, xy(0, 12));

        for (uint32_t entity_index = 0; entity_index < state.match_state.entities.size(); entity_index++) {
            if (state.match_state.entities[entity_index].type == ENTITY_GOLD_MINE) {
                continue;
            }

            SDL_Rect entity_rect = entity_get_rect(state.match_state.entities[entity_index]);
            if (sdl_rect_has_point(entity_rect, engine.mouse_position + state.camera_offset)) {
                const entity_t& entity = state.match_state.entities[entity_index];
                const char* mode_text[] = {
                    "Idle", "Move", "Blocked", "Finished", "Build", "Repair", "Attack", "Soldier Attack",
                    "Mine", "Throw", "Death", "Death Fade", "In Progress", "Finish", "Destroyed", "Arm", "Prime", "Gold", "Mined Out"
                };
                const char* target_text[] = {
                    "None", "Cell", "Entity", "Attack Cell", "Attack Entity", "Repair", "Unload",
                    "Smoke", "Build", "Build Assist", "Gold"
                };
                const char* anim_name[] = {
                    "Ui Move Cell",
                    "Ui Move Entity",
                    "Ui Move Attack Entity",
                    "Unit Idle",
                    "Unit Move",
                    "Unit Move Slow",
                    "Unit Move Cannon",
                    "Unit Attack",
                    "Soldier Ranged Attack",
                    "Cannon Attack",
                    "Unit Mine",
                    "Unit Build",
                    "Unit Death",
                    "Unit Death Fade",
                    "Cannon Death",
                    "Cannon Death Fade",
                    "Rally Flag",
                    "Particle Sparks",
                    "Particle Bunker Cowboy",
                    "Particle Explosion",
                    "Particle Cannon Explosion",
                    "Particle Smoke Start",
                    "Particle Smoke",
                    "Particle Smoke End",
                    "Smith Begin",
                    "Smith Loop",
                    "Smith End",
                    "Mine Prime"
                };
                char debug_text[128];
                sprintf(debug_text, "%u: type %s player %u mode %s flags %u", state.match_state.entities.get_id_of(entity_index), ENTITY_DATA.at(entity.type).name, entity.player_id, mode_text[entity.mode], entity.flags);
                render_text(FONT_HACK_WHITE, debug_text, xy(0, 24));
                sprintf(debug_text, "hp %u target %s cell %i,%i", entity.health, target_text[entity.target.type], entity.cell.x, entity.cell.y);
                render_text(FONT_HACK_WHITE, debug_text, xy(0, 36));
                sprintf(debug_text, "anim %s frame %i,%i loops %i", anim_name[entity.animation.name], entity.animation.frame.x, entity.animation.frame.y, entity.animation.loops_remaining);
                render_text(FONT_HACK_WHITE, debug_text, xy(0, 48));
                break;
            }
        }
    }

    if (state.options_menu_state.mode != OPTION_MENU_CLOSED) {
        options_menu_render(state.options_menu_state);
    }
}

render_sprite_params_t ui_create_entity_render_params(const ui_state_t& state, const entity_t& entity) {
    render_sprite_params_t render_params = (render_sprite_params_t) {
        .sprite = entity_get_sprite(entity),
        .frame = entity_get_animation_frame(entity),
        .position = entity.position.to_xy() - state.camera_offset,
        .options = 0,
        .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLD_MINE? (uint8_t)RECOLOR_NONE : network_get_player(entity.player_id).recolor_id
    };
    // Adjust render position for units because they are centered
    if (entity_is_unit(entity.type)) {
        render_params.position.x -= engine.sprites[render_params.sprite].frame_size.x / 2;
        render_params.position.y -= engine.sprites[render_params.sprite].frame_size.y / 2;

        bool should_flip_h = entity_should_flip_h(entity);
        if (entity.mode == MODE_UNIT_BUILD) {
            const entity_t& building = state.match_state.entities.get_by_id(entity.target.id);
            const entity_data_t& building_data = ENTITY_DATA.at(building.type);
            int building_hframe = entity_get_animation_frame(building).x;
            render_params.position = building.position.to_xy() + xy(building_data.building_data.builder_positions_x[building_hframe], building_data.building_data.builder_positions_y[building_hframe]) - state.camera_offset;
            should_flip_h = building_data.building_data.builder_flip_h[building_hframe];
        }

        if (should_flip_h) {
            render_params.options |= RENDER_SPRITE_FLIP_H;
        }
    } 

    return render_params;
}

void ui_render_healthbar(xy position, xy size, int health, int max_health) {
    GOLD_ASSERT(max_health != 0);

    SDL_Rect healthbar_rect = (SDL_Rect) { .x = position.x, .y = position.y, .w = size.x, .h = size.y };
    if (SDL_HasIntersection(&healthbar_rect, &SCREEN_RECT) != SDL_TRUE) {
        return;
    }

    SDL_Rect healthbar_subrect = healthbar_rect;
    healthbar_subrect.w = (healthbar_rect.w * health) / max_health;
    SDL_Color subrect_color = healthbar_subrect.w <= healthbar_rect.w / 3 ? COLOR_RED : COLOR_GREEN;

    SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
    SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
    SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
    SDL_RenderDrawRect(engine.renderer, &healthbar_rect);
}

void ui_render_garrisoned_units_healthbar(xy position, xy size, int garrisoned_size, int garrisoned_capacity) {
    SDL_Rect healthbar_rect = (SDL_Rect) { .x = position.x, .y = position.y, .w = size.x, .h = size.y };
    if (SDL_HasIntersection(&healthbar_rect, &SCREEN_RECT) != SDL_TRUE) {
        return;
    }

    SDL_Rect healthbar_subrect = healthbar_rect;
    healthbar_subrect.w = (healthbar_rect.w * garrisoned_size) / garrisoned_capacity;
    SDL_Color subrect_color = COLOR_WHITE;

    SDL_SetRenderDrawColor(engine.renderer, subrect_color.r, subrect_color.g, subrect_color.b, subrect_color.a);
    SDL_RenderFillRect(engine.renderer, &healthbar_subrect);
    SDL_SetRenderDrawColor(engine.renderer, COLOR_OFFBLACK.r, COLOR_OFFBLACK.g, COLOR_OFFBLACK.b, COLOR_OFFBLACK.a);
    SDL_RenderDrawRect(engine.renderer, &healthbar_rect);

    for (int line_index = 1; line_index < garrisoned_capacity; line_index++) {
        int line_x = healthbar_rect.x + ((healthbar_rect.w * line_index) / garrisoned_capacity);
        SDL_RenderDrawLine(engine.renderer, line_x, healthbar_rect.y, line_x, healthbar_rect.y + healthbar_rect.h - 1);
    }
}

void ui_render_target_build(const ui_state_t& state, const target_t& target) {
    SDL_SetRenderDrawColor(engine.renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 128);
    xy building_pos = (target.build.building_cell * TILE_SIZE) - state.camera_offset;
    render_sprite(ENTITY_DATA.at(target.build.building_type).sprite, xy(3, 0), building_pos, 0, network_get_player(network_get_player_id()).recolor_id);
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect building_rect = (SDL_Rect) {
        .x = building_pos.x, .y = building_pos.y,
        .w = entity_cell_size(target.build.building_type) * TILE_SIZE,
        .h = entity_cell_size(target.build.building_type) * TILE_SIZE
    };
    SDL_RenderFillRect(engine.renderer, &building_rect);
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_NONE);
}
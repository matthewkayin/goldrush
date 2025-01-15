#include "match.h"

#include "engine.h"
#include "network.h"
#include "logger.h"
#include "lcg.h"
#include <algorithm>

static const uint32_t TURN_DURATION = 4;
static const uint32_t TURN_OFFSET = 4;
static const uint32_t MATCH_DISCONNECT_GRACE = 10;
static const uint32_t PLAYER_STARTING_GOLD = 5000;

static const int CAMERA_DRAG_MARGIN = 4;
static const int CAMERA_DRAG_SPEED = 16;

static const SDL_Rect UI_DISCONNECT_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 100, .y = 32,
    .w = 200, .h = 200
};
static const SDL_Rect UI_MATCH_OVER_FRAME_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (250 / 2), .y = 96,
    .w = 250, .h = 90
};
static const SDL_Rect UI_MATCH_OVER_CONTINUE_BUTTON_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 45, .y = UI_MATCH_OVER_FRAME_RECT.y + 32,
    .w = 89, .h = 21
};
static const SDL_Rect UI_MATCH_OVER_EXIT_BUTTON_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - 45, .y = UI_MATCH_OVER_CONTINUE_BUTTON_RECT.y + UI_MATCH_OVER_CONTINUE_BUTTON_RECT.h + 4,
    .w = 89, .h = 21
};
static const SDL_Rect UI_MENU_BUTTON_RECT = (SDL_Rect) {
    .x = 1, .y = 1, .w = 19, .h = 18
};
static const SDL_Rect UI_MENU_RECT = (SDL_Rect) {
    .x = (SCREEN_WIDTH / 2) - (150 / 2), .y = 64,
    .w = 150, .h = 100
};
static const int UI_MENU_BUTTON_COUNT = 2;
static const SDL_Rect UI_MENU_BUTTON_RECTS[UI_MENU_BUTTON_COUNT] = {
    (SDL_Rect) {
        .x = (SCREEN_WIDTH / 2) - 45, .y = UI_MENU_RECT.y + 32,
        .w = 86, .h = 21
    },
    (SDL_Rect) {
        .x = (SCREEN_WIDTH / 2) - 45, .y = UI_MENU_RECT.y + 32 + 21 + 5,
        .w = 86, .h = 21
    },
};
static const char* UI_MENU_BUTTON_TEXT[UI_MENU_BUTTON_COUNT] = { "EXIT GAME", "BACK" };
static const char* UI_MENU_SURRENDER_BUTTON_TEXT[UI_MENU_BUTTON_COUNT] = { "YES", "BACK" };
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
const uint32_t MATCH_TAKING_DAMAGE_TIMER_DURATION = 30;
const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION = 10;
const uint32_t MATCH_ALERT_DURATION = 90;
const uint32_t MATCH_ALERT_LINGER_DURATION = 60 * 20;
const uint32_t MATCH_ALERT_TOTAL_DURATION = MATCH_ALERT_DURATION + MATCH_ALERT_LINGER_DURATION;
const uint32_t MATCH_ATTACK_ALERT_DISTANCE = 20;

static const int HEALTHBAR_HEIGHT = 4;
static const int HEALTHBAR_PADDING = 3;
static const int BUILDING_HEALTHBAR_PADDING = 5;

const std::unordered_map<UiButton, SDL_Keycode> hotkeys = {
    { UI_BUTTON_STOP, SDLK_s },
    { UI_BUTTON_ATTACK, SDLK_a },
    { UI_BUTTON_DEFEND, SDLK_d },
    { UI_BUTTON_BUILD, SDLK_b },
    { UI_BUTTON_BUILD2, SDLK_v },
    { UI_BUTTON_REPAIR, SDLK_r },
    { UI_BUTTON_CANCEL, SDLK_ESCAPE },
    { UI_BUTTON_UNLOAD, SDLK_x },
    { UI_BUTTON_EXPLODE, SDLK_e },
    { UI_BUTTON_BUILD_HALL, SDLK_t },
    { UI_BUTTON_BUILD_HOUSE, SDLK_e },
    { UI_BUTTON_BUILD_CAMP, SDLK_c },
    { UI_BUTTON_BUILD_SALOON, SDLK_s },
    { UI_BUTTON_BUILD_BUNKER, SDLK_b },
    { UI_BUTTON_BUILD_COOP, SDLK_c },
    { UI_BUTTON_BUILD_SMITH, SDLK_s },
    { UI_BUTTON_BUILD_BARRACKS, SDLK_b },
    { UI_BUTTON_BUILD_MINE, SDLK_e },
    { UI_BUTTON_UNIT_MINER, SDLK_r },
    { UI_BUTTON_UNIT_MINER, SDLK_e },
    { UI_BUTTON_UNIT_COWBOY, SDLK_c },
    { UI_BUTTON_UNIT_WAGON, SDLK_w },
    { UI_BUTTON_UNIT_WAR_WAGON, SDLK_w },
    { UI_BUTTON_UNIT_BANDIT, SDLK_b },
    { UI_BUTTON_UNIT_JOCKEY, SDLK_e },
    { UI_BUTTON_UNIT_SAPPER, SDLK_s },
    { UI_BUTTON_UNIT_TINKER, SDLK_t },
    { UI_BUTTON_UNIT_SOLDIER, SDLK_d },
    { UI_BUTTON_UNIT_CANNON, SDLK_c },
    { UI_BUTTON_UNIT_SPY, SDLK_s },
    { UI_BUTTON_RESEARCH_WAR_WAGON, SDLK_w },
    { UI_BUTTON_RESEARCH_EXPLOSIVES, SDLK_e },
    { UI_BUTTON_RESEARCH_BAYONETS, SDLK_b }
};

const std::unordered_map<uint32_t, upgrade_data_t> UPGRADE_DATA = {
    { UPGRADE_WAR_WAGON, (upgrade_data_t) {
            .name = "Wagon Armor",
            .ui_button = UI_BUTTON_RESEARCH_WAR_WAGON,
            .gold_cost = 200,
            .research_duration = 60
    }},
    { UPGRADE_EXPLOSIVES, (upgrade_data_t) {
            .name = "Explosives",
            .ui_button = UI_BUTTON_RESEARCH_EXPLOSIVES,
            .gold_cost = 200,
            .research_duration = 60
    }},
    { UPGRADE_BAYONETS, (upgrade_data_t) {
            .name = "Bayonets",
            .ui_button = UI_BUTTON_RESEARCH_BAYONETS,
            .gold_cost = 200,
            .research_duration = 60
    }}
};

match_state_t match_init() {
    match_state_t state;

    state.ui_mode = UI_MODE_MATCH_NOT_STARTED;
    state.ui_status_timer = 0;
    state.ui_is_minimap_dragging = false;
    state.select_rect_origin = xy(-1, -1);
    state.control_group_selected = -1;
    state.ui_double_click_timer = 0;
    state.control_group_double_tap_timer = 0;
    state.ui_rally_flag_animation = animation_create(ANIMATION_RALLY_FLAG);
    for (int i = 0; i < 6; i++) {
        state.ui_buttons[i] = UI_BUTTON_NONE;
    }

    std::vector<xy> player_spawns = map_init(state);
    
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

        // Determine player spawn
        int spawn_index = lcg_rand() % player_spawns.size();
        xy player_spawn = player_spawns[spawn_index];
        player_spawns.erase(player_spawns.begin() + spawn_index);
        if (player_id == network_get_player_id()) {
            match_camera_center_on_cell(state, player_spawn);
        }

        state.player_gold[player_id] = PLAYER_STARTING_GOLD;
        state.player_upgrades[player_id] = 0;
        state.player_upgrades_in_progress[player_id] = 0;
        state.map_fog[player_id] = std::vector<int>(state.map_width * state.map_height, FOG_HIDDEN);
        state.map_detection[player_id] = std::vector<int>(state.map_width * state.map_height, 0);

        entity_create(state, ENTITY_WAGON, player_id, player_spawn + xy(1, 0));
        entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(0, 0));
        entity_create(state, ENTITY_CANNON, player_id, player_spawn + xy(-2, 1));
        // entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(0, 1));
        // entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(3, 0));
        // entity_create(state, ENTITY_MINER, player_id, player_spawn + xy(3, 1));
    }
    state.turn_timer = 0;
    state.ui_disconnect_timer = 0;

    // Destroy minimap texture if already created
    if (engine.minimap_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_texture);
        engine.minimap_texture = NULL;
    }
    if (engine.minimap_tiles_texture != NULL) {
        SDL_DestroyTexture(engine.minimap_tiles_texture);
        engine.minimap_tiles_texture = NULL;
    }
    // Create new minimap texture
    engine.minimap_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, state.map_width, state.map_height);
    engine.minimap_tiles_texture = SDL_CreateTexture(engine.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, state.map_width, state.map_height);
    SDL_SetRenderTarget(engine.renderer, engine.minimap_tiles_texture);
    for (int y = 0; y < state.map_height; y++) {
        for (int x = 0; x < state.map_width; x++) {
            SDL_Color color;
            int tile_index = state.map_tiles[x + (y * state.map_width)].index;
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
    SDL_SetRenderTarget(engine.renderer, NULL);
    log_trace("Created minimap texture.");

    network_toggle_ready();
    if (network_are_all_players_ready()) {
        log_info("Match started.");
        state.ui_mode = UI_MODE_NONE;
    }

    return state;
}

void match_handle_input(match_state_t& state, SDL_Event event) {
    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED || state.ui_disconnect_timer > 0) {
        return;
    }

    // Match over button press
    if ((state.ui_mode == UI_MODE_MATCH_OVER_VICTORY || state.ui_mode == UI_MODE_MATCH_OVER_DEFEAT)) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && sdl_rect_has_point(UI_MATCH_OVER_CONTINUE_BUTTON_RECT, engine.mouse_position)) {
            state.ui_mode = UI_MODE_NONE;
            return;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && sdl_rect_has_point(UI_MATCH_OVER_EXIT_BUTTON_RECT, engine.mouse_position)) {
            network_disconnect();
            state.ui_mode = UI_MODE_LEAVE_MATCH;
            return;
        }
        return;
    }

    // Open menu button
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !ui_is_targeting(state) && !ui_is_selecting(state) && !state.ui_is_minimap_dragging && sdl_rect_has_point(UI_MENU_BUTTON_RECT, engine.mouse_position)) {
        if (state.ui_mode == UI_MODE_MENU || state.ui_mode == UI_MODE_MENU_SURRENDER) {
            state.ui_mode = UI_MODE_NONE;
        } else {
            state.ui_mode = UI_MODE_MENU;
        }
        return;
    }

    // Menu button press
    if (state.ui_mode == UI_MODE_MENU || state.ui_mode == UI_MODE_MENU_SURRENDER) {
        if (!(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)) {
            return;
        }

        int button_pressed;
        for (button_pressed = 0; button_pressed < UI_MENU_BUTTON_COUNT; button_pressed++) {
            if (sdl_rect_has_point(UI_MENU_BUTTON_RECTS[button_pressed], engine.mouse_position)) {
                break;
            }
        }
        if (button_pressed == UI_MENU_BUTTON_COUNT) {
            return;
        }

        if (state.ui_mode == UI_MODE_MENU && button_pressed == 1) {
            state.ui_mode = UI_MODE_NONE;
        } else if (state.ui_mode == UI_MODE_MENU_SURRENDER && button_pressed == 1) {
            state.ui_mode = UI_MODE_MENU;
        } else if (state.ui_mode == UI_MODE_MENU && button_pressed == 0) {
            bool is_another_player_in_match = false;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (!(network_get_player_id() == player_id || network_get_player(player_id).status == PLAYER_STATUS_NONE)) {
                    is_another_player_in_match = true;
                    break;
                }
            }

            if (is_another_player_in_match) {
                state.ui_mode = UI_MODE_MENU_SURRENDER;
            } else {
                network_disconnect();
                state.ui_mode = UI_MODE_LEAVE_MATCH;
            }
        } else if (state.ui_mode == UI_MODE_MENU_SURRENDER && button_pressed == 0) {
            network_disconnect();
            state.ui_mode = UI_MODE_LEAVE_MATCH;
        }

        return;
    }

    // Begin chat
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN && !SDL_IsTextInputActive()) {
        state.ui_chat_message = "";
        SDL_SetTextInputRect(&UI_CHAT_RECT);
        SDL_StartTextInput();
        return;
    }

    // Chat enter message
    if (SDL_IsTextInputActive() && event.type == SDL_TEXTINPUT) {
        state.ui_chat_message += std::string(event.text.text);
        if (state.ui_chat_message.length() > UI_CHAT_MESSAGE_MAX_LENGTH) {
            state.ui_chat_message = state.ui_chat_message.substr(0, UI_CHAT_MESSAGE_MAX_LENGTH);
        }

        return;
    } 

    // Chat non-text input key handle
    if (SDL_IsTextInputActive() && event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            // Chat delete character
            case SDLK_BACKSPACE: {
                if (state.ui_chat_message.length() > 0) {
                    state.ui_chat_message.pop_back();
                }
                break;
            }
            // Chat send message
            case SDLK_RETURN: {
                if (state.ui_chat_message.length() != 0) {
                    input_t input;
                    input.type = INPUT_CHAT;
                    input.chat.message_length = (uint8_t)state.ui_chat_message.length() + 1; // The +1 is for the null character
                    strcpy(input.chat.message, &state.ui_chat_message[0]);
                    state.input_queue.push_back(input);
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
        ui_handle_ui_button_press(state, state.ui_buttons[ui_get_ui_button_hovered(state)]);
        return;
    }
    // UI button hotkey press
    if (event.type == SDL_KEYDOWN) {
        for (int button_index = 0; button_index < UI_BUTTONSET_SIZE; button_index++) {
            UiButton button = state.ui_buttons[button_index];
            if (button == UI_BUTTON_NONE) {
                continue;
            }
            if (hotkeys.at(button) == event.key.keysym.sym) {
                ui_handle_ui_button_press(state, button);
                return;
            }
        }
    }

    // Garrisoned unit icon press
    if (ui_get_garrisoned_index_hovered(state) != -1 && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        state.input_queue.push_back((input_t) {
            .type = INPUT_SINGLE_UNLOAD,
            .single_unload = (input_single_unload_t) {
                .unit_id = state.entities.get_by_id(state.selection[0]).garrisoned_units[ui_get_garrisoned_index_hovered(state)]
            }
        });
    }

    // Selected unit icon press
    if (ui_get_selected_unit_hovered(state) != -1 && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (engine.keystate[SDL_SCANCODE_LSHIFT]) {
            ui_deselect_entity_if_selected(state, state.selection[ui_get_selected_unit_hovered(state)]);
        } else {
            std::vector<entity_id> selection;
            selection.push_back(state.selection[ui_get_selected_unit_hovered(state)]);
            ui_set_selection(state, selection);
        }
    }

    // Building queue icon press
    if (ui_get_building_queue_item_hovered(state) != -1 && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        state.input_queue.push_back((input_t) {
            .type = INPUT_BUILDING_DEQUEUE,
            .building_dequeue = (input_building_dequeue_t) {
                .building_id = state.selection[0],
                .index = (uint16_t)ui_get_building_queue_item_hovered(state)
            }
        });
    }

    // UI building place
    if (state.ui_mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui() && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (!ui_building_can_be_placed(state)) {
            ui_show_status(state, UI_STATUS_CANT_BUILD);
            return;
        }

        input_t input;
        input.type = INPUT_BUILD;
        input.build.building_type = state.ui_building_type;
        input.build.entity_count = state.selection.size();
        memcpy(&input.build.entity_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));
        input.build.target_cell = ui_get_building_cell(state);
        state.input_queue.push_back(input);

        state.ui_mode = UI_MODE_NONE;
        return;
    }

    // Order movement
    if (event.type == SDL_MOUSEBUTTONDOWN && ui_get_selection_type(state, state.selection) == SELECTION_TYPE_UNITS && 
            (sdl_rect_has_point(MINIMAP_RECT, engine.mouse_position) || !ui_is_mouse_in_ui()) &&
            ((event.button.button == SDL_BUTTON_LEFT && ui_is_targeting(state)) ||
            (event.button.button == SDL_BUTTON_RIGHT && !ui_is_selecting(state) && !state.ui_is_minimap_dragging))) {
        input_t move_input = match_create_move_input(state);

        if (move_input.type == INPUT_MOVE_REPAIR) {
            bool is_repair_target_valid = true;
            if (move_input.move.target_id == ID_NULL) {
                is_repair_target_valid = false;
            } else {
                const entity_t& repair_target = state.entities.get_by_id(move_input.move.target_id);
                if (repair_target.player_id != network_get_player_id() || !entity_is_building(repair_target.type)) {
                    is_repair_target_valid = false;
                }
            }

            if (!is_repair_target_valid) {
                ui_show_status(state, UI_STATUS_REPAIR_TARGET_INVALID);
                state.ui_mode = UI_MODE_NONE;
                return;
            }
        }

        state.input_queue.push_back(move_input);

        // Check if clicked on remembered building
        entity_id remembered_entity_id = ID_NULL;
        if ((move_input.type == INPUT_MOVE_CELL || move_input.type == INPUT_MOVE_ATTACK_CELL) &&
                state.map_fog[network_get_player_id()][move_input.move.target_cell.x + (move_input.move.target_cell.y * state.map_width)] == FOG_EXPLORED) {
            xy move_target;
            if (ui_is_mouse_in_ui()) {
                xy minimap_pos = engine.mouse_position - xy(MINIMAP_RECT.x, MINIMAP_RECT.y);
                move_target = xy((state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                                    (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
            } else {
                move_target = match_get_mouse_world_pos(state);
            }

            for (auto it : state.remembered_entities[network_get_player_id()]) {
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
            state.ui_move_animation = animation_create(state.remembered_entities[network_get_player_id()][remembered_entity_id].sprite_params.recolor_id != RECOLOR_NONE 
                                                            ? ANIMATION_UI_MOVE_ATTACK_ENTITY 
                                                            : ANIMATION_UI_MOVE_ENTITY);
            state.ui_move_position = cell_center(move_input.move.target_cell).to_xy();
            state.ui_move_entity_id = remembered_entity_id;
        } else if (move_input.type == INPUT_MOVE_CELL || move_input.type == INPUT_MOVE_ATTACK_CELL || move_input.type == INPUT_MOVE_UNLOAD) {
            state.ui_move_animation = animation_create(ANIMATION_UI_MOVE_CELL);
            state.ui_move_position = match_get_mouse_world_pos(state);
            state.ui_move_entity_id = ID_NULL;
        } else {
            state.ui_move_animation = animation_create(move_input.type == INPUT_MOVE_ATTACK_ENTITY ? ANIMATION_UI_MOVE_ATTACK_ENTITY : ANIMATION_UI_MOVE_ENTITY);
            state.ui_move_position = cell_center(move_input.move.target_cell).to_xy();
            state.ui_move_entity_id = move_input.move.target_id;
        }

        // Reset UI mode if targeting
        if (ui_is_targeting(state)) {
            state.ui_mode = UI_MODE_NONE;
        }
        return;
    } 

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT &&
            ui_get_selection_type(state, state.selection) == SELECTION_TYPE_BUILDINGS && 
            !ui_is_targeting(state) && !ui_is_selecting(state) && !state.ui_is_minimap_dragging) {
        // Check to make sure that all buildings can rally
        for (entity_id id : state.selection) {
            if (!ENTITY_DATA.at(state.entities.get_by_id(id).type).building_data.can_rally) {
                return;
            }
        }

        input_t input;
        input.type = INPUT_RALLY;
        input.rally.rally_point = match_get_mouse_world_pos(state);
        input.rally.building_count = (uint16_t)state.selection.size();
        memcpy(&input.rally.building_ids, &state.selection[0], state.selection.size() * sizeof(entity_id));
        state.input_queue.push_back(input);
        return;
    }

    // Begin minimap drag
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && 
            (state.ui_mode == UI_MODE_NONE || state.ui_mode == UI_MODE_BUILD || state.ui_mode == UI_MODE_BUILD2) && sdl_rect_has_point(MINIMAP_RECT, engine.mouse_position)) {
        state.ui_is_minimap_dragging = true;
        return;
    }

    // End minimap drag
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && state.ui_is_minimap_dragging) {
        state.ui_is_minimap_dragging = false;
        return;
    }

    // Begin selecting
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
            (state.ui_mode == UI_MODE_NONE || state.ui_mode == UI_MODE_BUILD || state.ui_mode == UI_MODE_BUILD2) && !ui_is_mouse_in_ui()) {
        state.select_rect_origin = match_get_mouse_world_pos(state);
        return;
    }

    // End selecting
    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && ui_is_selecting(state)) {
        state.select_rect_origin = xy(-1, -1);
        std::vector<entity_id> selection = ui_create_selection_from_rect(state);

        // Append selection
        if (engine.keystate[SDL_SCANCODE_LCTRL]) { 
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
            if (state.ui_double_click_timer == 0) {
                state.ui_double_click_timer = UI_DOUBLE_CLICK_DURATION;
            } else if (state.selection.size() == 1 && state.selection[0] == selection[0] && state.entities.get_by_id(state.selection[0]).player_id == network_get_player_id()) {
                EntityType selected_type = state.entities.get_by_id(state.selection[0]).type;
                selection.clear();
                for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                    if (state.entities[entity_index].type != selected_type || state.entities[entity_index].player_id != network_get_player_id()) {
                        continue;
                    }
                    SDL_Rect entity_rect = entity_get_rect(state.entities[entity_index]);
                    entity_rect.x -= state.camera_offset.x;
                    entity_rect.y -= state.camera_offset.y;
                    if (SDL_HasIntersection(&SCREEN_RECT, &entity_rect) == SDL_TRUE) {
                        selection.push_back(state.entities.get_id_of(entity_index));
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
        if (engine.keystate[SDL_SCANCODE_LCTRL]) {
            if (state.selection.empty() || state.entities.get_by_id(state.selection[0]).player_id != network_get_player_id()) {
                return;
            }
            state.control_groups[control_group_index] = state.selection;
            state.control_group_selected = control_group_index;
            return;
        }

        // Append control group
        if (engine.keystate[SDL_SCANCODE_LSHIFT]) {
            if (state.selection.empty() || state.entities.get_by_id(state.selection[0]).player_id != network_get_player_id() || 
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
                const entity_t& entity = state.entities.get_by_id(state.selection[selection_index]);
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
            match_camera_center_on_cell(state, group_center);
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
        match_camera_center_on_cell(state, state.alerts[state.alerts.size() - 1].cell);
        return;
    }
}

void match_update(match_state_t& state) {
    network_service();

    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED && network_are_all_players_ready()) {
        log_trace("Match started.");
        state.ui_mode = UI_MODE_NONE;
    }
    if (state.ui_mode == UI_MODE_MATCH_NOT_STARTED) {
        return;
    }

    // Poll network events
    network_event_t network_event;
    while (network_poll_events(&network_event)) {
        switch (network_event.type) {
            case NETWORK_EVENT_INPUT: {
                // Deserialize input
                std::vector<input_t> tick_inputs;

                uint8_t* in_buffer = network_event.input.in_buffer;
                size_t in_buffer_head = 1;

                while (in_buffer_head < network_event.input.in_buffer_length) {
                    tick_inputs.push_back(match_input_deserialize(in_buffer, in_buffer_head));
                }

                state.inputs[network_event.input.player_id].push_back(tick_inputs);
                break;
            }
            case NETWORK_EVENT_PLAYER_DISCONNECTED: {
                ui_add_chat_message(state, std::string(network_get_player(network_event.player_disconnected.player_id).name) + " disconnected.");

                // Determine if we should exit the match
                uint32_t player_count = 0;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (network_get_player(player_id).status != PLAYER_STATUS_NONE) {
                        player_count++;
                    }
                }
                if (player_count < 2) {
                    state.ui_mode = UI_MODE_MATCH_OVER_VICTORY;
                }
                break;
            }
            default: 
                break;
        }
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
            state.ui_disconnect_timer++;
            return;
        }

        // Reset the disconnect timer if we received inputs
        state.ui_disconnect_timer = 0;

        // All inputs received. Begin next turn
        // HANDLE INPUT
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            const player_t& player = network_get_player(player_id);
            if (player.status == PLAYER_STATUS_NONE) {
                continue;
            }

            for (const input_t& input : state.inputs[player_id][0]) {
                match_input_handle(state, player_id, input);
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

    // CAMERA DRAG
    if (!ui_is_selecting(state) && !state.ui_is_minimap_dragging) {
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
        state.camera_offset += camera_drag_direction * CAMERA_DRAG_SPEED;
        match_camera_clamp(state);
    }

    // MINIMAP DRAG
    if (state.ui_is_minimap_dragging) {
        xy minimap_pos = xy(
            std::clamp(engine.mouse_position.x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w), 
            std::clamp(engine.mouse_position.y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        xy map_pos = xy(
            (state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        match_camera_center_on_cell(state, map_pos / TILE_SIZE);
    }

    // SELECT RECT
    if (ui_is_selecting(state)) {
        // Update select rect
        state.select_rect = (SDL_Rect) {
            .x = std::min(state.select_rect_origin.x, match_get_mouse_world_pos(state).x),
            .y = std::min(state.select_rect_origin.y, match_get_mouse_world_pos(state).y),
            .w = std::max(1, std::abs(state.select_rect_origin.x - match_get_mouse_world_pos(state).x)), 
            .h = std::max(1, std::abs(state.select_rect_origin.y - match_get_mouse_world_pos(state).y))
        };
    }

    // Update timers
    if (animation_is_playing(state.ui_move_animation)) {
        animation_update(state.ui_move_animation);
    }
    animation_update(state.ui_rally_flag_animation);
    if (state.ui_double_click_timer != 0) {
        state.ui_double_click_timer--;
    }
    if (state.control_group_double_tap_timer != 0) {
        state.control_group_double_tap_timer--;
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

    // Update particles
    {
        uint32_t particle_index = 0;
        while (particle_index < state.particles.size()) {
            animation_update(state.particles[particle_index].animation);
            if (!animation_is_playing(state.particles[particle_index].animation)) {
                state.particles.erase(state.particles.begin() + particle_index);
            } else {
                particle_index++;
            }
        }
    }

    // Update entities
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        entity_update(state, entity_index);
    }

    // Remove any dead entities
    {
        uint32_t entity_index = 0;
        while (entity_index < state.entities.size()) {
            if ((state.entities[entity_index].mode == MODE_UNIT_DEATH_FADE && !animation_is_playing(state.entities[entity_index].animation)) || 
                    (state.entities[entity_index].garrison_id != ID_NULL && state.entities[entity_index].health == 0) ||
                    (state.entities[entity_index].mode == MODE_BUILDING_DESTROYED && state.entities[entity_index].timer == 0) ||
                    (state.entities[entity_index].type == ENTITY_GOLD && state.entities[entity_index].mode == MODE_GOLD_MINED_OUT)) {
                // Remove this entity's fog but only if they are not gold and not garrisoned
                if (state.entities[entity_index].player_id != PLAYER_NONE && state.entities[entity_index].garrison_id == ID_NULL) {
                    map_fog_update(state, state.entities[entity_index].player_id, state.entities[entity_index].cell, entity_cell_size(state.entities[entity_index].type), ENTITY_DATA.at(state.entities[entity_index].type).sight, false, ENTITY_DATA.at(state.entities[entity_index].type).has_detection);
                }
                state.entities.remove_at(entity_index);
            } else {
                entity_index++;
            }
        }
    }

    if (state.ui_status_timer > 0) {
        state.ui_status_timer--;
    }
    for (uint32_t chat_index = 0; chat_index < state.ui_chat.size(); chat_index++) {
        state.ui_chat[chat_index].timer--;
        if (state.ui_chat[chat_index].timer == 0) {
            state.ui_chat.erase(state.ui_chat.begin() + chat_index);
            chat_index--;
        }
    }

    engine_set_cursor(ui_is_targeting(state) ? CURSOR_TARGET : CURSOR_DEFAULT);
    ui_update_buttons(state);

    if (state.map_is_fog_dirty) {
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (network_get_player(player_id).status == PLAYER_NONE) {
                continue;
            }
            // Remove any remembered entities (but only if this player can see that they should be removed)
            auto it = state.remembered_entities[player_id].begin();
            while (it != state.remembered_entities[player_id].end()) {
                if ((state.entities.get_index_of(it->first) == INDEX_INVALID || state.entities.get_by_id(it->first).health == 0) && 
                        (map_is_cell_rect_revealed(state, player_id, it->second.cell, it->second.cell_size) || it->second.sprite_params.recolor_id == player_id)) {
                    it = state.remembered_entities[player_id].erase(it);
                } else {
                    it++;
                }
            }
        }
        if (state.selection.size() == 1) {
            entity_t& selected_entity = state.entities.get_by_id(state.selection[0]);
            if (!map_is_cell_rect_revealed(state, network_get_player_id(), selected_entity.cell, entity_cell_size(selected_entity.type))) {
                state.selection.clear();
            }
        }
        state.map_is_fog_dirty = false;
    }
}

void match_camera_clamp(match_state_t& state) {
    state.camera_offset.x = std::clamp(state.camera_offset.x, 0, ((int)state.map_width * TILE_SIZE) - SCREEN_WIDTH);
    state.camera_offset.y = std::clamp(state.camera_offset.y, 0, ((int)state.map_height * TILE_SIZE) - SCREEN_HEIGHT + UI_HEIGHT);
}

void match_camera_center_on_cell(match_state_t& state, xy cell) {
    state.camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state.camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_HEIGHT / 2);
    match_camera_clamp(state);
}

xy match_get_mouse_world_pos(const match_state_t& state) {
    return engine.mouse_position + state.camera_offset;
}

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id) {
    uint32_t population = 0;
    for (const entity_t& entity : state.entities) {
        if (entity.player_id == player_id && entity_is_unit(entity.type) && entity.health != 0) {
            population += ENTITY_DATA.at(entity.type).unit_data.population_cost;
        }
    }
    
    return population;
}

uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id) {
    uint32_t max_population = 0;
    for (const entity_t& building : state.entities) {
        if (building.player_id == player_id && building.mode == MODE_BUILDING_FINISHED && (building.type == ENTITY_HOUSE || building.type == ENTITY_HALL)) {
            max_population += 10;
        }
    }

    return max_population;
}

bool match_player_has_upgrade(const match_state_t& state, uint8_t player_id, uint32_t upgrade) {
    return (state.player_upgrades[player_id] & upgrade) == upgrade;
}

bool match_player_upgrade_is_available(const match_state_t& state, uint8_t player_id, uint32_t upgrade) {
    return ((state.player_upgrades[player_id] | state.player_upgrades_in_progress[player_id]) & upgrade) == 0;
}

void match_grant_player_upgrade(match_state_t& state, uint8_t player_id, uint32_t upgrade) {
    state.player_upgrades[player_id] |= upgrade;
}

input_t match_create_move_input(const match_state_t& state) {
    // Determine move target
    xy move_target;
    if (ui_is_mouse_in_ui()) {
        xy minimap_pos = engine.mouse_position - xy(MINIMAP_RECT.x, MINIMAP_RECT.y);
        move_target = xy((state.map_width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
                            (state.map_height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
    } else {
        move_target = match_get_mouse_world_pos(state);
    }

    // Create move input
    input_t input;
    input.move.target_cell = move_target / TILE_SIZE;
    input.move.target_id = ID_NULL;
    int fog_value = state.map_fog[network_get_player_id()][input.move.target_cell.x + (input.move.target_cell.y * state.map_width)];
    if (fog_value != FOG_HIDDEN) {
        for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
            const entity_t& entity = state.entities[entity_index];
            // I think this is saying, don't target unselectable entities, unless it's gold and the player doesn't know that the gold is unselectable
            // It's also saying, don't target a hidden mine
            if (!entity_is_selectable(entity) || 
                    (fog_value == FOG_EXPLORED && entity.type != ENTITY_GOLD) ||
                    (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && entity.player_id != network_get_player_id() && state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.map_width)] == 0)) {
                continue;
            }

            SDL_Rect entity_rect = entity_get_rect(state.entities[entity_index]);
            if (sdl_rect_has_point(entity_rect, move_target)) {
                input.move.target_id = state.entities.get_id_of(entity_index);
                break;
            }
        }
    }

    if (state.ui_mode == UI_MODE_TARGET_UNLOAD) {
        input.type = INPUT_MOVE_UNLOAD;
    } else if (state.ui_mode == UI_MODE_TARGET_REPAIR) {
        input.type = INPUT_MOVE_REPAIR;
    } else if (input.move.target_id != ID_NULL && state.entities.get_by_id(input.move.target_id).type != ENTITY_GOLD &&
               (state.ui_mode == UI_MODE_TARGET_ATTACK || 
                state.entities.get_by_id(input.move.target_id).player_id != network_get_player_id())) {
        input.type = INPUT_MOVE_ATTACK_ENTITY;
    } else if (input.move.target_id == ID_NULL && state.ui_mode == UI_MODE_TARGET_ATTACK) {
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

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input) {
    out_buffer[out_buffer_length] = input.type;
    out_buffer_length++;

    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.move.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.move.target_id, sizeof(entity_id));
            out_buffer_length += sizeof(entity_id);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.move.entity_ids, input.move.entity_count * sizeof(entity_id));
            out_buffer_length += input.move.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            memcpy(out_buffer + out_buffer_length, &input.stop.entity_count, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.stop.entity_ids, input.stop.entity_count * sizeof(entity_id));
            out_buffer_length += input.stop.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD: {
            memcpy(out_buffer + out_buffer_length, &input.build.building_type, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.build.target_cell, sizeof(xy));
            out_buffer_length += sizeof(xy);
            
            memcpy(out_buffer + out_buffer_length, &input.build.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.build.entity_ids, input.build.entity_count * sizeof(entity_id));
            out_buffer_length += input.build.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD_CANCEL: {
            memcpy(out_buffer + out_buffer_length, &input.build_cancel, sizeof(input_build_cancel_t));
            out_buffer_length += sizeof(input_build_cancel_t);
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_enqueue, sizeof(input_building_enqueue_t));
            out_buffer_length += sizeof(input_building_enqueue_t);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            memcpy(out_buffer + out_buffer_length, &input.building_dequeue, sizeof(input_building_dequeue_t));
            out_buffer_length += sizeof(input_building_dequeue_t);
            break;
        }
        case INPUT_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.unload.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.unload.entity_ids, input.unload.entity_count * sizeof(entity_id));
            out_buffer_length += input.unload.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_SINGLE_UNLOAD: {
            memcpy(out_buffer + out_buffer_length, &input.single_unload, sizeof(input_single_unload_t));
            out_buffer_length += sizeof(input_single_unload_t);
            break;
        }
        case INPUT_RALLY: {
            memcpy(out_buffer + out_buffer_length, &input.rally.rally_point, sizeof(xy));
            out_buffer_length += sizeof(xy);

            memcpy(out_buffer + out_buffer_length, &input.rally.building_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);
            
            memcpy(out_buffer + out_buffer_length, &input.rally.building_ids, input.rally.building_count * sizeof(entity_id));
            out_buffer_length += input.rally.building_count * sizeof(entity_id);
            break;
        }
        case INPUT_EXPLODE: {
            memcpy(out_buffer + out_buffer_length, &input.explode.entity_count, sizeof(uint16_t));
            out_buffer_length += sizeof(uint16_t);

            memcpy(out_buffer + out_buffer_length, &input.explode.entity_ids, input.explode.entity_count * sizeof(entity_id));
            out_buffer_length += input.explode.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_CHAT: {
            memcpy(out_buffer + out_buffer_length, &input.chat.message_length, sizeof(uint8_t));
            out_buffer_length += sizeof(uint8_t);

            memcpy(out_buffer + out_buffer_length, &input.chat.message, input.chat.message_length);
            out_buffer_length += input.chat.message_length;
            break;
        }
        default:
            break;
    }
}

input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head) {
    input_t input;
    input.type = in_buffer[in_buffer_head];
    in_buffer_head++;

    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            memcpy(&input.move.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.move.target_id, in_buffer + in_buffer_head, sizeof(entity_id));
            in_buffer_head += sizeof(entity_id);

            memcpy(&input.move.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.move.entity_ids, in_buffer + in_buffer_head, input.move.entity_count * sizeof(entity_id));
            in_buffer_head += input.move.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_STOP:
        case INPUT_DEFEND: {
            memcpy(&input.stop.entity_count, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.stop.entity_ids, in_buffer + in_buffer_head, input.stop.entity_count * sizeof(entity_id));
            in_buffer_head += input.stop.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD: {
            memcpy(&input.build.building_type, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.build.target_cell, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.build.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.build.entity_ids, in_buffer + in_buffer_head, input.build.entity_count * sizeof(entity_id));
            in_buffer_head += input.build.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_BUILD_CANCEL: {
            memcpy(&input.build_cancel, in_buffer + in_buffer_head, sizeof(input_build_cancel_t));
            in_buffer_head += sizeof(input_build_cancel_t);
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            memcpy(&input.building_enqueue, in_buffer + in_buffer_head, sizeof(input_building_enqueue_t));
            in_buffer_head += sizeof(input_building_enqueue_t);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            memcpy(&input.building_dequeue, in_buffer + in_buffer_head, sizeof(input_building_dequeue_t));
            in_buffer_head += sizeof(input_building_dequeue_t);
            break;
        }
        case INPUT_UNLOAD: {
            memcpy(&input.unload.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.unload.entity_ids, in_buffer + in_buffer_head, input.unload.entity_count * sizeof(entity_id));
            in_buffer_head += input.unload.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_SINGLE_UNLOAD: {
            memcpy(&input.single_unload, in_buffer + in_buffer_head, sizeof(input_single_unload_t));
            in_buffer_head += sizeof(input_single_unload_t);
            break;
        }
        case INPUT_RALLY: {
            memcpy(&input.rally.rally_point, in_buffer + in_buffer_head, sizeof(xy));
            in_buffer_head += sizeof(xy);

            memcpy(&input.rally.building_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.rally.building_ids, in_buffer + in_buffer_head, input.rally.building_count * sizeof(entity_id));
            in_buffer_head += input.rally.building_count * sizeof(entity_id);
            break;
        }
        case INPUT_EXPLODE: {
            memcpy(&input.explode.entity_count, in_buffer + in_buffer_head, sizeof(uint16_t));
            in_buffer_head += sizeof(uint16_t);

            memcpy(&input.explode.entity_ids, in_buffer + in_buffer_head, input.explode.entity_count * sizeof(entity_id));
            in_buffer_head += input.explode.entity_count * sizeof(entity_id);
            break;
        }
        case INPUT_CHAT: {
            memcpy(&input.chat.message_length, in_buffer + in_buffer_head, sizeof(uint8_t));
            in_buffer_head += sizeof(uint8_t);

            memcpy(&input.chat.message, in_buffer + in_buffer_head, input.chat.message_length);
            in_buffer_head += input.chat.message_length;
            break;
        }
        default:
            break;
    }

    return input;
}

void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input) {
    switch (input.type) {
        case INPUT_MOVE_CELL:
        case INPUT_MOVE_ENTITY:
        case INPUT_MOVE_ATTACK_CELL:
        case INPUT_MOVE_ATTACK_ENTITY:
        case INPUT_MOVE_REPAIR:
        case INPUT_MOVE_UNLOAD: {
            // Determine the target index
            uint32_t target_index = INDEX_INVALID;
            if (input.type == INPUT_MOVE_ENTITY || input.type == INPUT_MOVE_ATTACK_ENTITY || input.type == INPUT_MOVE_REPAIR) {
                target_index = state.entities.get_index_of(input.move.target_id);
                if (target_index != INDEX_INVALID && !entity_is_selectable(state.entities[target_index])) {
                    target_index = INDEX_INVALID;
                }
            }

            // Calculate group center
            xy group_center;
            bool should_move_as_group = target_index == INDEX_INVALID;
            uint32_t unit_count = 0;
            if (should_move_as_group) {
                xy group_min;
                xy group_max;
                for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                    uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                    if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                        continue;
                    }

                    xy entity_cell = state.entities[entity_index].cell;
                    if (unit_count == 0) {
                        group_min = entity_cell;
                        group_max = entity_cell;
                    } else {
                        group_min.x = std::min(group_min.x, entity_cell.x);
                        group_min.y = std::min(group_min.y, entity_cell.y);
                        group_max.x = std::max(group_max.x, entity_cell.x);
                        group_max.y = std::max(group_max.y, entity_cell.y);
                    }

                    unit_count++;
                }

                SDL_Rect group_rect = (SDL_Rect) { 
                    .x = group_min.x, .y = group_min.y,
                    .w = group_max.x - group_min.x, .h = group_max.y - group_min.y
                };
                group_center = xy(group_rect.x + (group_rect.w / 2), group_rect.y + (group_rect.h / 2));

                // Don't move as group if we're not in a group
                // Also don't move as a group if the target is inside the group rect (this allows units to converge in on a cell)
                if (unit_count < 2 || sdl_rect_has_point(group_rect, input.move.target_cell)) {
                    should_move_as_group = false;
                }
            } // End calculate group center

            // Give each unit the move command
            for (uint32_t id_index = 0; id_index < input.move.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.move.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                entity_t& entity = state.entities[entity_index];

                // Set the unit's target
                target_t target;
                target.type = (TargetType)input.type;
                if (target_index == INDEX_INVALID) {
                    target.cell = input.move.target_cell;
                    // If group-moving, use the group move cell, but only if the cell is valid
                    if (should_move_as_group) {
                        xy group_move_cell = input.move.target_cell + (entity.cell - group_center);
                        if (map_is_cell_in_bounds(state, group_move_cell) && 
                                xy::manhattan_distance(group_move_cell, input.move.target_cell) <= 3 &&
                                map_get_tile(state, group_move_cell).elevation == map_get_tile(state, input.move.target_cell).elevation) {
                            target.cell = group_move_cell;
                        }
                    }
                } else if (entity.type == ENTITY_MINER && state.entities[target_index].type == ENTITY_GOLD) {
                    target = entity_target_gold(state, entity, input.move.target_id);
                } else if (target.type == TARGET_REPAIR) {
                    target.id = input.move.target_id;
                    target.repair = (target_repair_t) {
                        .health_repaired = 0
                    };
                } else if (input.move.target_id == input.move.entity_ids[id_index]) {
                    target = (target_t) { .type = TARGET_NONE };
                } else {
                    target.id = input.move.target_id;
                }
                entity_set_target(entity, target);

                if (entity.target.type == TARGET_GOLD) {
                    entity.gold_patch_id = state.entities.get_by_id(entity.target.id).gold_patch_id;
                } else if (entity.type == ENTITY_MINER && entity.target.type == TARGET_ENTITY && (state.entities[target_index].type == ENTITY_CAMP || state.entities[target_index].type == ENTITY_HALL) && entity.gold_held) {
                    target_t nearest_gold_target = entity_target_nearest_gold(state, state.entities[target_index].player_id, state.entities[target_index].cell, GOLD_PATCH_ID_NULL);
                    entity.gold_patch_id = nearest_gold_target.type == TARGET_NONE
                                            ? GOLD_PATCH_ID_NULL
                                            : state.entities.get_by_id(nearest_gold_target.id).gold_patch_id;
                }
            } // End for each unit in move input
            break;
        } // End handle INPUT_MOVE
        case INPUT_STOP:
        case INPUT_DEFEND: {
            for (uint8_t id_index = 0; id_index < input.stop.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.stop.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                entity_t& entity = state.entities[entity_index];

                entity.path.clear();
                entity_set_target(entity, (target_t) {
                    .type = TARGET_NONE
                });
                if (input.type == INPUT_DEFEND) {
                    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, true);
                }
            }
            break;
        }
        case INPUT_BUILD: {
            // Determine the list of viable builders
            std::vector<entity_id> builder_ids;
            for (uint32_t id_index = 0; id_index < input.build.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.build.entity_ids[id_index]);
                if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                    continue;
                }
                builder_ids.push_back(input.build.entity_ids[id_index]);
            }

            // Assign the lead builder's target
            entity_id lead_builder_id = ui_get_nearest_builder(state, builder_ids, input.build.target_cell);
            entity_t& lead_builder = state.entities.get_by_id(lead_builder_id);
            entity_set_target(lead_builder, (target_t) {
                .type = TARGET_BUILD,
                .id = ID_NULL,
                .build = (target_build_t) {
                    .unit_cell = input.build.building_type == ENTITY_MINE ? input.build.target_cell : get_nearest_cell_in_rect(lead_builder.cell, input.build.target_cell, entity_cell_size((EntityType)input.build.building_type)),
                    .building_cell = input.build.target_cell,
                    .building_type = (EntityType)input.build.building_type
                }
            });

            // Assign the helpers' target
            if (input.build.building_type != ENTITY_MINE) {
                for (entity_id builder_id : builder_ids) {
                    if (builder_id == lead_builder_id) {
                        continue;
                    } 
                    entity_set_target(state.entities.get_by_id(builder_id), (target_t) {
                        .type = TARGET_BUILD_ASSIST,
                        .id = lead_builder_id
                    });
                }
            }
            break;
        }
        case INPUT_BUILD_CANCEL: {
            uint32_t building_index = state.entities.get_index_of(input.build_cancel.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                break;
            }

            const entity_data_t& building_data = ENTITY_DATA.at(state.entities[building_index].type);
            uint32_t gold_refund = building_data.gold_cost - (((uint32_t)state.entities[building_index].health * building_data.gold_cost) / (uint32_t)building_data.max_health);
            state.player_gold[state.entities[building_index].player_id] += gold_refund;

            // Tell the builder to stop building
            for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
                if (state.entities[entity_index].target.type == TARGET_BUILD && state.entities[entity_index].target.id == input.build_cancel.building_id) {
                    entity_t& builder = state.entities[entity_index];
                    builder.cell = builder.target.build.building_cell;
                    builder.position = entity_get_target_position(builder);
                    builder.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    builder.mode = MODE_UNIT_IDLE;
                    map_set_cell_rect(state, builder.cell, entity_cell_size(builder.type), state.entities.get_id_of(entity_index));
                    map_fog_update(state, builder.player_id, builder.cell, entity_cell_size(builder.type), ENTITY_DATA.at(builder.type).sight, true, ENTITY_DATA.at(builder.type).has_detection);
                    break;
                }
            }

            // Destroy the building
            state.entities[building_index].health = 0;
            break;
        }
        case INPUT_BUILDING_ENQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_enqueue.building_id);
            building_queue_item_t item = input.building_enqueue.item;
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }
            if (state.player_gold[player_id] < building_queue_item_cost(item)) {
                return;
            }
            if (state.entities[building_index].queue.size() == BUILDING_QUEUE_MAX) {
                return;
            }
            // Reject this enqueue if the upgrade is already being researched
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE && !match_player_upgrade_is_available(state, player_id, item.upgrade)) {
                return;
            }

            // Check if player has the war wagon upgrade
            if (item.type == BUILDING_QUEUE_ITEM_UNIT && item.unit_type == ENTITY_WAGON && match_player_has_upgrade(state, player_id, UPGRADE_WAR_WAGON)) {
                item.unit_type = ENTITY_WAR_WAGON;
            }

            // Mark upgrades as in-progress when we enqueue them
            if (item.type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state.player_upgrades_in_progress[player_id] |= item.upgrade;
            }

            state.player_gold[player_id] -= building_queue_item_cost(input.building_enqueue.item);
            entity_building_enqueue(state, state.entities[building_index], input.building_enqueue.item);
            break;
        }
        case INPUT_BUILDING_DEQUEUE: {
            uint32_t building_index = state.entities.get_index_of(input.building_dequeue.building_id);
            if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                return;
            }
            entity_t& building = state.entities[building_index];
            if (building.queue.empty()) {
                return;
            }

            uint32_t index = input.building_dequeue.index == BUILDING_DEQUEUE_POP_FRONT
                                    ? building.queue.size() - 1
                                    : input.building_dequeue.index;
            
            state.player_gold[player_id] += building_queue_item_cost(building.queue[index]);
            if (building.queue[index].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                state.player_upgrades_in_progress[building.player_id] &= ~building.queue[index].upgrade;
            }

            if (index == 0) {
                entity_building_dequeue(state, building);
            } else {
                building.queue.erase(building.queue.begin() + index);
            }
            break;
        }
        case INPUT_UNLOAD: {
            for (uint32_t input_index = 0; input_index < input.unload.entity_count; input_index++) {
                uint32_t carrier_index = state.entities.get_index_of(input.unload.entity_ids[input_index]);
                if (carrier_index == INDEX_INVALID || !entity_is_selectable(state.entities[carrier_index]) || state.entities[carrier_index].garrisoned_units.empty()) {
                    continue;
                }

                entity_t& carrier = state.entities[carrier_index];
                entity_unload_unit(state, carrier, ENTITY_UNLOAD_ALL);
            }
            break;
        }
        case INPUT_SINGLE_UNLOAD: {
            uint32_t garrisoned_unit_index = state.entities.get_index_of(input.single_unload.unit_id);
            if (garrisoned_unit_index == INDEX_INVALID || state.entities[garrisoned_unit_index].health == 0 || state.entities[garrisoned_unit_index].garrison_id == ID_NULL) {
                return;
            }
            entity_t& garrisoned_unit = state.entities[garrisoned_unit_index];
            entity_id carrier_id = garrisoned_unit.garrison_id;
            entity_t& carrier = state.entities.get_by_id(carrier_id);
            entity_unload_unit(state, carrier, input.single_unload.unit_id);
            break;
        }
        case INPUT_RALLY: {
            for (uint32_t id_index = 0; id_index < input.rally.building_count; id_index++) {
                uint32_t building_index = state.entities.get_index_of(input.rally.building_ids[id_index]);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index])) {
                    continue;
                }
                state.entities[building_index].rally_point = input.rally.rally_point;
            }
            break;
        }
        case INPUT_EXPLODE: {
            // Determine all the exploding entities up front
            // This is because if an entity has died in the four frames since this input was sent, we don't want it to explode
            // But otherwise we want them all to explode at once and if we handle them sequentially then one might kill the other before it gets the chance to explode
            entity_id entities_to_explode[SELECTION_LIMIT];
            uint32_t entity_count = 0;
            for (uint32_t id_index = 0; id_index < input.explode.entity_count; id_index++) {
                uint32_t entity_index = state.entities.get_index_of(input.explode.entity_ids[id_index]);
                if (entity_index != INDEX_INVALID && entity_is_selectable(state.entities[entity_index])) {
                    entities_to_explode[entity_count] = input.explode.entity_ids[id_index];
                    entity_count++;
                }
            }
            for (uint32_t explode_index = 0; explode_index < entity_count; explode_index++) {
                entity_explode(state, entities_to_explode[explode_index]);
            }
            break;
        }
        case INPUT_CHAT: {
            ui_add_chat_message(state, std::string(network_get_player(player_id).name) + ": " + std::string(input.chat.message));
            break;
        }
        default:
            break;
    }
}

void match_render(const match_state_t& state) {
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
                int map_index = (base_coords.x + x) + ((base_coords.y + y) * state.map_width);
                // If current elevation is above the tile, then skip it
                if (state.map_tiles[map_index].elevation < elevation) {
                    continue;
                }

                // If current elevation is equal to the tile, then render the tile, otherwise render default ground tile beneath it
                uint16_t map_tile_index = state.map_tiles[map_index].elevation == elevation
                                            ? state.map_tiles[map_index].index
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
                if (state.map_cells[map_index] >= CELL_DECORATION_1 && state.map_cells[map_index] <= CELL_DECORATION_5) {
                    ysorted_render_params.push_back((render_sprite_params_t) {
                        .sprite = SPRITE_TILE_DECORATION,
                        .frame = xy(state.map_cells[map_index] - CELL_DECORATION_1, 0),
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
            const entity_t& entity = state.entities.get_by_id(id);
            if (entity.type != ENTITY_MINE || entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            // Select ring
            render_sprite(entity_get_select_ring(entity, entity.player_id == PLAYER_NONE || entity.player_id == network_get_player_id()), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);

            // Determine the healthbar rect
            SDL_Rect entity_rect = entity_get_rect(entity);
            entity_rect.x -= state.camera_offset.x;
            entity_rect.y -= state.camera_offset.y;
            xy healthbar_position = xy(entity_rect.x, entity_rect.y + entity_rect.h + HEALTHBAR_PADDING);
            match_render_healthbar(healthbar_position, xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, ENTITY_DATA.at(entity.type).max_health);
        }

        // Mine UI move animation
        if (animation_is_playing(state.ui_move_animation) && 
                map_get_tile(state, state.ui_move_position / TILE_SIZE).elevation == elevation &&
                state.ui_move_animation.name != ANIMATION_UI_MOVE_CELL && state.ui_move_animation.frame.x % 2 == 0) {
            uint32_t entity_index = state.entities.get_index_of(state.ui_move_entity_id);
            if (entity_index != INDEX_INVALID && state.entities[entity_index].type == ENTITY_MINE) {
                const entity_t& entity = state.entities[entity_index];
                render_sprite(entity_get_select_ring(entity, state.ui_move_animation.name == ANIMATION_UI_MOVE_ENTITY), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);
            } else {
                auto remembered_entities_it = state.remembered_entities[network_get_player_id()].find(state.ui_move_entity_id);
                if (remembered_entities_it != state.remembered_entities[network_get_player_id()].end()) {
                    if (remembered_entities_it->second.sprite_params.sprite == SPRITE_BUILDING_MINE) {
                        render_sprite(SPRITE_SELECT_RING_MINE, xy(0, 0), (remembered_entities_it->second.cell * TILE_SIZE) + (xy(remembered_entities_it->second.cell_size, remembered_entities_it->second.cell_size) * TILE_SIZE / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                }
            }
        }

        // Render entity corpses
        for (const entity_t& entity : state.entities) {
            if (!(entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_MINE) || 
                    entity_get_elevation(state, entity) != elevation || 
                    (entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && entity.player_id != network_get_player_id() && state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.map_width)] == 0) ||
                    !map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
                continue;
            }

            render_sprite_params_t render_params = match_create_entity_render_params(state, entity);
            render_sprite(render_params.sprite, render_params.frame, render_params.position, render_params.options, render_params.recolor_id);
        }

        // Remembered entity corpses
        for (auto it : state.remembered_entities[network_get_player_id()]) {
            if (it.second.sprite_params.sprite < SPRITE_BUILDING_DESTROYED_2 || it.second.sprite_params.sprite > SPRITE_BUILDING_DESTROYED_MINE ||
                    map_get_tile(state, it.second.cell).elevation != elevation || map_is_cell_rect_revealed(state, network_get_player_id(), it.second.cell, it.second.cell_size)) {
                continue;
            }

            render_sprite_params_t render_params = it.second.sprite_params;
            render_params.position -= state.camera_offset;
            render_sprite(render_params.sprite, render_params.frame, render_params.position, render_params.options, render_params.recolor_id);
        }

        // Select rings and healthbars
        for (entity_id id : state.selection) {
            const entity_t& entity = state.entities.get_by_id(id);
            if (entity.type == ENTITY_MINE || entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            // Select ring
            render_sprite(entity_get_select_ring(entity, entity.player_id == PLAYER_NONE || entity.player_id == network_get_player_id()), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);

            if (entity.type == ENTITY_GOLD) {
                continue;
            }
            // Determine the healthbar rect
            SDL_Rect entity_rect = entity_get_rect(entity);
            entity_rect.x -= state.camera_offset.x;
            entity_rect.y -= state.camera_offset.y;
            xy healthbar_position = xy(entity_rect.x, entity_rect.y + entity_rect.h + (entity_is_unit(entity.type) ? HEALTHBAR_PADDING : BUILDING_HEALTHBAR_PADDING));
            match_render_healthbar(healthbar_position, xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.health, ENTITY_DATA.at(entity.type).max_health);
            if (ENTITY_DATA.at(entity.type).garrison_capacity != 0) {
                match_render_garrisoned_units_healthbar(healthbar_position + xy(0, HEALTHBAR_HEIGHT + 1), xy(entity_rect.w, HEALTHBAR_HEIGHT), entity.garrisoned_units.size(), ENTITY_DATA.at(entity.type).garrison_capacity);
            }
        }

        // Entities
        for (const entity_t& entity : state.entities) {
            if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_MINE ||
                    !map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type)) || 
                    (entity.player_id != network_get_player_id() && entity_check_flag(entity, ENTITY_FLAG_INVISIBLE) && state.map_detection[network_get_player_id()][entity.cell.x + (entity.cell.y * state.map_width)] == 0) ||
                    entity.garrison_id != ID_NULL || entity_get_elevation(state, entity) != elevation) {
                continue;
            }

            render_sprite_params_t render_params = match_create_entity_render_params(state, entity);
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
        for (auto it : state.remembered_entities[network_get_player_id()]) {
            if ((it.second.sprite_params.sprite >= SPRITE_BUILDING_DESTROYED_2 && it.second.sprite_params.sprite <= SPRITE_BUILDING_DESTROYED_MINE) || it.second.sprite_params.sprite == SPRITE_BUILDING_MINE ||
                    map_get_tile(state, it.second.cell).elevation != elevation || map_is_cell_rect_revealed(state, network_get_player_id(), it.second.cell, it.second.cell_size)) {
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
        if (animation_is_playing(state.ui_move_animation) && 
                map_get_tile(state, state.ui_move_position / TILE_SIZE).elevation == elevation) {
            if (state.ui_move_animation.name == ANIMATION_UI_MOVE_CELL) {
                render_sprite_params_t ui_move_params = (render_sprite_params_t) {
                    .sprite = SPRITE_UI_MOVE,
                    .frame = state.ui_move_animation.frame,
                    .position = state.ui_move_position - state.camera_offset,
                    .options = RENDER_SPRITE_CENTERED,
                    .recolor_id = RECOLOR_NONE
                };
                xy ui_move_cell = state.ui_move_position / TILE_SIZE;
                if (state.map_fog[network_get_player_id()][ui_move_cell.x + (ui_move_cell.y * state.map_width)] > 0) {
                    render_sprite(ui_move_params.sprite, ui_move_params.frame, ui_move_params.position, ui_move_params.options);
                } else {
                    above_fog_render_params.push_back(ui_move_params);
                }
            } else if (state.ui_move_animation.frame.x % 2 == 0) {
                uint32_t entity_index = state.entities.get_index_of(state.ui_move_entity_id);
                if (entity_index != INDEX_INVALID) {
                    if (state.entities[entity_index].type != ENTITY_MINE) {
                        const entity_t& entity = state.entities[entity_index];
                        render_sprite(entity_get_select_ring(entity, state.ui_move_animation.name == ANIMATION_UI_MOVE_ENTITY), xy(0, 0), entity_get_center_position(entity) - state.camera_offset, RENDER_SPRITE_CENTERED);
                    }
                } else {
                    auto remembered_entities_it = state.remembered_entities[network_get_player_id()].find(state.ui_move_entity_id);
                    if (remembered_entities_it != state.remembered_entities[network_get_player_id()].end()) {
                        Sprite select_ring_sprite;
                        if (remembered_entities_it->second.sprite_params.sprite != SPRITE_BUILDING_MINE) {
                            if (remembered_entities_it->second.sprite_params.sprite == SPRITE_TILE_GOLD) {
                                select_ring_sprite = SPRITE_SELECT_RING_GOLD;
                            } else {
                                select_ring_sprite = (Sprite)(SPRITE_SELECT_RING_BUILDING_2 + ((remembered_entities_it->second.cell_size - 2) * 2) + 1);
                            }
                            render_sprite(select_ring_sprite, xy(0, 0), (remembered_entities_it->second.cell * TILE_SIZE) + (xy(remembered_entities_it->second.cell_size, remembered_entities_it->second.cell_size) * TILE_SIZE / 2) - state.camera_offset, RENDER_SPRITE_CENTERED);
                        }
                    }
                }
            }
        }

        // Rally points
        if (ui_get_selection_type(state, state.selection) == SELECTION_TYPE_BUILDINGS) {
            for (entity_id id : state.selection) {
                const entity_t& building = state.entities.get_by_id(id);
                xy rally_cell = building.rally_point / TILE_SIZE;
                if (building.mode == MODE_BUILDING_FINISHED && building.rally_point.x != -1 &&
                        map_get_tile(state, rally_cell).elevation == elevation) {
                    // Determine whether to render above or below the fog of war
                    std::vector<render_sprite_params_t>& sprite_params_vector = state.map_fog[building.player_id][rally_cell.x + (rally_cell.y * state.map_width)] > 0
                                                                                    ? ysorted_render_params
                                                                                    : above_fog_render_params;
                    sprite_params_vector.push_back((render_sprite_params_t) {
                        .sprite = SPRITE_RALLY_FLAG,
                        .frame = state.ui_rally_flag_animation.frame,
                        .position = building.rally_point - xy(4, 15) - state.camera_offset,
                        .options = 0,
                        .recolor_id = building.player_id
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
    for (const entity_t& entity : state.entities) {
        if (entity.type == ENTITY_SMITH && entity.animation.name != ANIMATION_UNIT_IDLE && map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            render_sprite(SPRITE_BUILDING_SMITH_ANIMATION, entity.animation.frame, entity.position.to_xy() - xy(0, 16) - state.camera_offset);
        }
    }

    // Particles
    for (const particle_t& particle : state.particles) {
        if (!map_is_cell_rect_revealed(state, network_get_player_id(), particle.position / TILE_SIZE, 1)) {
            continue;
        }
        render_sprite(particle.sprite, xy(particle.animation.frame.x, particle.vframe), particle.position - state.camera_offset, RENDER_SPRITE_CENTERED);
    }

    // Fog of War
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    for (int fog_pass = 0; fog_pass < 2; fog_pass++) {
        for (int y = 0; y < max_visible_tiles.y; y++) {
            for (int x = 0; x < max_visible_tiles.x; x++) {
                xy fog_cell = base_coords + xy(x, y);
                int fog = state.map_fog[network_get_player_id()][fog_cell.x + (fog_cell.y * state.map_width)];
                if (fog > 0) {
                    continue;
                }
                if (fog_pass == 1 && fog == FOG_EXPLORED) {
                    continue;
                }

                uint32_t neighbors = 0;
                for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
                    xy neighbor_cell = fog_cell + DIRECTION_XY[direction];
                    if (!map_is_cell_in_bounds(state, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if (fog_pass == 0 
                            ? state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.map_width)] < 1 
                            : state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.map_width)] == FOG_HIDDEN) {
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
                    if (!map_is_cell_in_bounds(state, neighbor_cell)) {
                        neighbors += DIRECTION_MASK[direction];
                        continue;
                    }
                    if (fog_pass == 0 
                            ? state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.map_width)] < 1 
                            : state.map_fog[network_get_player_id()][neighbor_cell.x + (neighbor_cell.y * state.map_width)] == FOG_HIDDEN) {
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
    for (const entity_t& entity : state.entities) {
        if (!entity_is_unit(entity.type) || entity.path.empty()) {
            continue;
        }
        SDL_Color line_color = entity_should_gold_walk(state, entity) ? COLOR_GOLD : COLOR_WHITE;
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

    // UI Building Placement
    if (state.ui_mode == UI_MODE_BUILDING_PLACE && !ui_is_mouse_in_ui()) {
        const entity_data_t& building_data = ENTITY_DATA.at(state.ui_building_type);
        xy ui_building_cell = ui_get_building_cell(state);
        render_sprite(building_data.sprite, xy(3, 0), (ui_building_cell * TILE_SIZE) + - state.camera_offset, 0, network_get_player_id());

        bool is_placement_out_of_bounds = ui_building_cell.x + building_data.cell_size >= state.map_width ||
                                          ui_building_cell.y + building_data.cell_size >= state.map_height;
        SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
        xy miner_cell = state.entities.get_by_id(ui_get_nearest_builder(state, state.selection, ui_building_cell)).cell;
        for (int x = ui_building_cell.x; x < ui_building_cell.x + building_data.cell_size; x++) {
            for (int y = ui_building_cell.y; y < ui_building_cell.y + building_data.cell_size; y++) {
                bool is_cell_green = true;
                if (is_placement_out_of_bounds) {
                    is_cell_green = false;
                }
                if (state.map_fog[network_get_player_id()][x + (y * state.map_width)] == FOG_HIDDEN || map_is_tile_ramp(state, xy(x, y)) ||
                        (xy(x, y) != miner_cell && state.map_cells[x + (y * state.map_width)] != CELL_EMPTY) ||
                        state.map_mine_cells[x + (y * state.map_width)] != ID_NULL) {
                    is_cell_green = false;
                }

                if (state.ui_building_type == ENTITY_CAMP || state.ui_building_type == ENTITY_HALL) {
                    for (const entity_t& gold : state.entities) {
                        if (gold.type != ENTITY_GOLD || gold.gold_held == 0) {
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
    for (uint32_t chat_index = 0; chat_index < state.ui_chat.size(); chat_index++) {
        render_text(FONT_HACK_WHITE, state.ui_chat[state.ui_chat.size() - chat_index - 1].message.c_str(), xy(16, MINIMAP_RECT.y - 48 - (chat_index * 16)));
    }
    // UI Chat message
    if (SDL_IsTextInputActive()) {
        char prompt_str[128];
        sprintf(prompt_str, "Message: %s", state.ui_chat_message.c_str());
        render_text(FONT_HACK_WHITE, prompt_str, xy(UI_CHAT_RECT.x, UI_CHAT_RECT.y));
    }

    // UI Status message
    if (state.ui_status_timer != 0) {
        render_text(FONT_HACK_WHITE, state.ui_status_message.c_str(), xy(RENDER_TEXT_CENTERED, SCREEN_HEIGHT - 148));
    }

    // UI Disconnect frame
    if (state.ui_disconnect_timer > MATCH_DISCONNECT_GRACE) {
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
    if (state.ui_mode == UI_MODE_MATCH_OVER_VICTORY || state.ui_mode == UI_MODE_MATCH_OVER_DEFEAT) {
        render_ninepatch(SPRITE_UI_FRAME, UI_MATCH_OVER_FRAME_RECT, 16);
        render_text(FONT_WESTERN8_GOLD, state.ui_mode == UI_MODE_MATCH_OVER_VICTORY ? "Victory!" : "Defeat!", xy(RENDER_TEXT_CENTERED, UI_MATCH_OVER_FRAME_RECT.y + 10));

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
    render_sprite(SPRITE_UI_MENU_BUTTON, xy(sdl_rect_has_point(UI_MENU_BUTTON_RECT, engine.mouse_position) || state.ui_mode == UI_MODE_MENU ? 1 : 0, 0), xy(UI_MENU_BUTTON_RECT.x, UI_MENU_BUTTON_RECT.y), RENDER_SPRITE_NO_CULL);
    if (state.ui_mode == UI_MODE_MENU || state.ui_mode == UI_MODE_MENU_SURRENDER) {
        render_ninepatch(SPRITE_UI_FRAME, UI_MENU_RECT, 16);
        render_text(FONT_WESTERN8_GOLD, state.ui_mode == UI_MODE_MENU ? "Game Menu" : "Surrender?", xy(RENDER_TEXT_CENTERED, UI_MENU_RECT.y + 10));
        for (int i = 0; i < UI_MENU_BUTTON_COUNT; i++) {
            bool button_hovered = sdl_rect_has_point(UI_MENU_BUTTON_RECTS[i], engine.mouse_position);
            xy button_position = xy(UI_MENU_BUTTON_RECTS[i].x, UI_MENU_BUTTON_RECTS[i].y + (button_hovered ? -1 : 0));
            render_sprite(SPRITE_UI_MENU_PARCHMENT_BUTTONS, xy(0, button_hovered ? 1 : 0), button_position, RENDER_SPRITE_NO_CULL);
            render_text(button_hovered ? FONT_WESTERN8_WHITE : FONT_WESTERN8_OFFBLACK, state.ui_mode == UI_MODE_MENU ? UI_MENU_BUTTON_TEXT[i] : UI_MENU_SURRENDER_BUTTON_TEXT[i], button_position + xy(UI_MENU_BUTTON_RECTS[i].w / 2, 5), TEXT_ANCHOR_TOP_CENTER);
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
            uint32_t entity_index = state.entities.get_index_of(id);
            if (entity_index == INDEX_INVALID || !entity_is_selectable(state.entities[entity_index])) {
                continue;
            }

            EntityType entity_type = state.entities[entity_index].type;
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
        UiButton ui_button = state.ui_buttons[i];
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
        const entity_t& entity = state.entities.get_by_id(state.selection[0]);
        const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

        match_render_text_with_text_frame(entity_data.name, SELECTION_LIST_TOP_LEFT);
        render_sprite(SPRITE_UI_BUTTON, xy(0, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);
        render_sprite(SPRITE_UI_BUTTON_ICON, xy(entity_data.ui_button - 1, 0), SELECTION_LIST_TOP_LEFT + xy(0, 18), RENDER_SPRITE_NO_CULL);

        if (entity.type != ENTITY_GOLD) {
            xy healthbar_position = SELECTION_LIST_TOP_LEFT + xy(0, 18 + 35);
            xy healthbar_size = xy(64, 12);
            match_render_healthbar(healthbar_position, healthbar_size, entity.health, entity_data.max_health);

            char health_text[10];
            sprintf(health_text, "%i/%i", entity.health, entity_data.max_health);
            xy health_text_size = render_get_text_size(FONT_HACK_WHITE, health_text);
            xy health_text_position = healthbar_position + xy((healthbar_size.x / 2) - (health_text_size.x / 2), (healthbar_size.y / 2) - (health_text_size.y / 2));
            render_text(FONT_HACK_WHITE, health_text, health_text_position, TEXT_ANCHOR_BOTTOM_LEFT);
        } else {
            char gold_left_str[17];
            sprintf(gold_left_str, "Gold Left: %u", entity.gold_held);
            render_text(FONT_WESTERN8_GOLD, gold_left_str, SELECTION_LIST_TOP_LEFT + xy(36, 22));
        }
    } else {
        for (uint32_t selection_index = 0; selection_index < state.selection.size(); selection_index++) {
            const entity_t& entity = state.entities.get_by_id(state.selection[selection_index]);
            const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

            bool icon_hovered = ui_get_selected_unit_hovered(state) == selection_index;
            xy icon_position = ui_get_selected_unit_icon_position(selection_index) + (icon_hovered ? xy(0, -1) : xy(0, 0));
            render_sprite(SPRITE_UI_BUTTON, xy(icon_hovered ? 1 : 0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(entity_data.ui_button - 1, icon_hovered ? 1 : 0), icon_position, RENDER_SPRITE_NO_CULL);
            match_render_healthbar(icon_position + xy(1, 32 - 5), xy(32- 2, 4), entity.health, entity_data.max_health);
        }
    }

    SelectionType ui_selection_type = ui_get_selection_type(state, state.selection);

    // UI Building queues
    if (state.selection.size() == 1 && ui_selection_type == SELECTION_TYPE_BUILDINGS && !state.entities.get_by_id(state.selection[0]).queue.empty()) {
        const entity_t& building = state.entities.get_by_id(state.selection[0]);
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
    if (state.selection.size() == 1 && (ui_selection_type == SELECTION_TYPE_BUILDINGS || ui_selection_type == SELECTION_TYPE_UNITS)) {
        const entity_t& entity = state.entities.get_by_id(state.selection[0]);
        for (int index = 0; index < entity.garrisoned_units.size(); index++) {
            const entity_t& garrisoned_unit = state.entities.get_by_id(entity.garrisoned_units[index]);
            bool icon_hovered = ui_get_garrisoned_index_hovered(state) == index;
            xy icon_position = ui_garrisoned_icon_position(index) + (icon_hovered ? xy(0, -1) : xy(0, 0));

            render_sprite(SPRITE_UI_BUTTON, xy(icon_hovered ? 1 : 0, 0), icon_position, RENDER_SPRITE_NO_CULL);
            render_sprite(SPRITE_UI_BUTTON_ICON, xy(ENTITY_DATA.at(garrisoned_unit.type).ui_button - 1, icon_hovered ? 1 : 0), icon_position, RENDER_SPRITE_NO_CULL);
            match_render_healthbar(icon_position + xy(1, 32 - 5), xy(32 - 2, 4), garrisoned_unit.health, ENTITY_DATA.at(garrisoned_unit.type).max_health);
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
    sprintf(gold_text, "%u", state.player_gold[network_get_player_id()]);
    render_text(FONT_WESTERN8_WHITE, gold_text, xy(SCREEN_WIDTH - 172 + 18, 4));
    render_sprite(SPRITE_UI_GOLD, xy(0, 0), xy(SCREEN_WIDTH - 172, 2), RENDER_SPRITE_NO_CULL);

    char population_text[8];
    sprintf(population_text, "%u/%u", match_get_player_population(state, network_get_player_id()), match_get_player_max_population(state, network_get_player_id()));
    render_text(FONT_WESTERN8_WHITE, population_text, xy(SCREEN_WIDTH - 88 + 22, 4));
    render_sprite(SPRITE_UI_HOUSE, xy(0, 0), xy(SCREEN_WIDTH - 88, 0), RENDER_SPRITE_NO_CULL);

    // Minimap prepare texture
    SDL_SetRenderTarget(engine.renderer, engine.minimap_texture);
    SDL_RenderCopy(engine.renderer, engine.minimap_tiles_texture, NULL, NULL);

    // Minimap entities
    for (const entity_t& entity : state.entities) {
        if (!entity_is_selectable(entity) || !map_is_cell_rect_revealed(state, network_get_player_id(), entity.cell, entity_cell_size(entity.type))) {
            continue;
        }
        SDL_Rect entity_rect = (SDL_Rect) {
            .x = entity.cell.x, .y = entity.cell.y,
            .w = entity_cell_size(entity.type) + 1, .h = entity_cell_size(entity.type) + 1
        };
        SDL_Color color;
        if (entity.type == ENTITY_GOLD) {
            color = COLOR_GOLD;
        } else if (entity.player_id == network_get_player_id()) {
            /*
             * This code is too smart for it's own good so let me explain:
             * entity_damage_timer will begin at 30 and it should cause us to flicker between red and green in intervals of 10 (MATCH_TAKING_DAMAGE_FLICKER_DURATION)
             * if you take entity_damage_timer / 10 you get an int depending on where in the range you are. 5 / 10 = 0, 15 / 10 = 1, 25 / 10 = 2, so the even numbers
             * in this range is where we want the flicker to occur and the odd numbers is where we want to not flicker
            */
            if (entity.taking_damage_timer != 0 && (entity.taking_damage_timer / MATCH_TAKING_DAMAGE_FLICKER_DURATION) % 2 == 0) {
                color = COLOR_RED;
            } else {
                color = COLOR_GREEN;
            }
        } else {
            color = PLAYER_COLORS[entity.player_id];
        }
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(engine.renderer, &entity_rect);
    }

    // Minimap remembered entities
    for (auto it : state.remembered_entities[network_get_player_id()]) {
        if (map_is_cell_rect_revealed(state, network_get_player_id(), it.second.cell, it.second.cell_size)) {
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
            color = PLAYER_COLORS[it.second.sprite_params.recolor_id];
        }
        SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(engine.renderer, &entity_rect);
    }

    // Minimap alerts
    for (const alert_t& alert : state.alerts) {
        if (alert.timer <= MATCH_ALERT_LINGER_DURATION) {
            continue;
        }

        int alert_timer = alert.timer - MATCH_ALERT_LINGER_DURATION;
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
    fog_hidden_points.reserve(state.map_width * state.map_height);
    std::vector<SDL_Point> fog_explored_points;
    fog_explored_points.reserve(state.map_width * state.map_height);
    for (int y = 0; y < state.map_height; y++) {
        for (int x = 0; x < state.map_width; x++) {
            int fog = state.map_fog[network_get_player_id()][x + (y * state.map_width)];
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
    SDL_RenderCopy(engine.renderer, engine.minimap_texture, NULL, &MINIMAP_RECT);
}

render_sprite_params_t match_create_entity_render_params(const match_state_t& state, const entity_t& entity) {
    render_sprite_params_t render_params = (render_sprite_params_t) {
        .sprite = entity_get_sprite(entity),
        .frame = entity_get_animation_frame(entity),
        .position = entity.position.to_xy() - state.camera_offset,
        .options = 0,
        .recolor_id = entity.mode == MODE_BUILDING_DESTROYED || entity.type == ENTITY_GOLD ? (uint8_t)RECOLOR_NONE : entity.player_id
    };
    // Adjust render position for units because they are centered
    if (entity_is_unit(entity.type)) {
        render_params.position.x -= engine.sprites[render_params.sprite].frame_size.x / 2;
        render_params.position.y -= engine.sprites[render_params.sprite].frame_size.y / 2;

        bool should_flip_h = entity_should_flip_h(entity);
        if (entity.mode == MODE_UNIT_BUILD) {
            const entity_t& building = state.entities.get_by_id(entity.target.id);
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

void match_render_healthbar(xy position, xy size, int health, int max_health) {
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

void match_render_garrisoned_units_healthbar(xy position, xy size, int garrisoned_size, int garrisoned_capacity) {
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

void match_render_text_with_text_frame(const char* text, xy position) {
    xy text_size = render_get_text_size(FONT_WESTERN8_OFFBLACK, text);
    int frame_width = (text_size.x / 15) + 1;
    if (text_size.x % 15 != 0) {
        frame_width++;
    }
    for (int frame_x = 0; frame_x < frame_width; frame_x++) {
        int x_frame = 1;
        if (frame_x == 0) {
            x_frame = 0;
        } else if (frame_x == frame_width - 1) {
            x_frame = 2;
        }
        render_sprite(SPRITE_UI_TEXT_FRAME, xy(x_frame, 0), position + xy(frame_x * 15, 0), RENDER_SPRITE_NO_CULL);
    }

    render_text(FONT_WESTERN8_OFFBLACK, text, position + xy(((frame_width * 15) / 2) - (text_size.x / 2), 2));
}
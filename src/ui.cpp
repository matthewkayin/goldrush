#include "ui.h"

#include "logger.h"
#include "network.h"

static const uint32_t TURN_DURATION = 4;
static const uint32_t TURN_OFFSET = 4;
static const uint32_t MATCH_DISCONNECT_GRACE = 10;

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

const uint32_t MATCH_ALERT_DURATION = 90;
const uint32_t MATCH_ALERT_LINGER_DURATION = 60 * 20;
const uint32_t MATCH_ALERT_TOTAL_DURATION = MATCH_ALERT_DURATION + MATCH_ALERT_LINGER_DURATION;
const uint32_t MATCH_ATTACK_ALERT_DISTANCE = 20;

static const int HEALTHBAR_HEIGHT = 4;
static const int HEALTHBAR_PADDING = 3;
static const int BUILDING_HEALTHBAR_PADDING = 5;

ui_state_t ui_init(int32_t lcg_seed, const noise_t& noise) {
    ui_state_t state;

    lcg_srand(lcg_seed);
    log_trace("Set random seed to %i", lcg_seed);

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
    map_t map = map_init(noise);
    ui_center_camera_on_cell(state, player_spawns[network_get_player_id()]);
    state.match_state = match_init()
    if (player_id == network_get_player_id()) {
        match_camera_center_on_cell(state, player_spawn);
    }
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

void ui_handle_input(ui_state_t& state, SDL_Event e) {

}

void ui_update(ui_state_t& state) {

}

void ui_render(const ui_state_t& state) {

}
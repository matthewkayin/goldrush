#include "base.h"

#include "core/cursor.h"

#include <algorithm>

static const int SHELL_UI_HEIGHT = 176;
static const Rect SCREEN_RECT = (Rect) { .x = 0, .y = 0, .w = SCREEN_WIDTH, .h = SCREEN_HEIGHT };
static const Rect MINIMAP_RECT = (Rect) { .x = 8, .y = SCREEN_HEIGHT - 264, .w = 256, .h = 256 };
static const ivec2 SELECTION_LIST_TOP_LEFT = ivec2(328, 568);
static const uint32_t STATUS_DURATION = 60;
static const int CAMERA_DRAG_MARGIN = 4;

// Sound
static const int SOUND_LISTEN_MARGIN = 128;
static const Rect SOUND_LISTEN_RECT = (Rect) {
    .x = -SOUND_LISTEN_MARGIN,
    .y = -SOUND_LISTEN_MARGIN,
    .w = SCREEN_WIDTH + (SOUND_LISTEN_MARGIN * 2),
    .h = SCREEN_HEIGHT + (SOUND_LISTEN_MARGIN * 2),
};
static const uint32_t SOUND_COOLDOWN_DURATION = 5;

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


Shell::Shell() {
    camera_offset = ivec2(0, 0);
    select_origin = ivec2(-1, -1);
    is_minimap_dragging = false;
    status_timer = 0;
    chat_cursor_blink_timer = 0;
    chat_cursor_visible = false;
    latest_alert_cell = ivec2(-1, -1);
    rally_flag_animation = animation_create(ANIMATION_RALLY_FLAG);
    building_fire_animation = animation_create(ANIMATION_FIRE_BURN);
    is_paused = false;

    options_menu.mode = OPTIONS_MENU_CLOSED;

    memset(sound_cooldown_timers, 0, sizeof(sound_cooldown_timers));
    memset(displayed_gold_amounts, 0, sizeof(displayed_gold_amounts));
}

void Shell::base_update() {
    // Open menu
    const SpriteInfo& menu_button_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_BURGER);
    Rect menu_button_rect = (Rect) {
        .x = MENU_BUTTON_POSITION.x, .y = MENU_BUTTON_POSITION.y,
        .w = menu_button_sprite_info.frame_width * 2, .h = menu_button_sprite_info.frame_height * 2
    };
    if (!(is_minimap_dragging || is_selecting()) && 
            ((menu_button_rect.has_point(input_get_mouse_position()) && input_is_action_just_pressed(INPUT_ACTION_LEFT_CLICK)) || input_is_action_just_pressed(INPUT_ACTION_MATCH_MENU))) {
        if (mode == SHELL_MODE_MENU || mode == SHELL_MODE_MENU_SURRENDER || mode == SHELL_MODE_MENU_SURRENDER_TO_DESKTOP) {
            options_menu.mode = OPTIONS_MENU_CLOSED;
            mode = SHELL_MODE_NONE;
        } else if (mode != SHELL_MODE_MATCH_OVER_DEFEAT && mode != SHELL_MODE_MATCH_OVER_VICTORY) {
            mode = SHELL_MODE_MENU;
            input_stop_text_input();
        }
        sound_play(SOUND_UI_CLICK);

        return;
    }

    // Menu
    if (is_in_menu()) {
        // This cancels minimap dragging if we open the menu with F10
        // It also cancels it if the victory / defeat menu pops up during minimap dragging
        if (is_minimap_dragging) {
            is_minimap_dragging = false;
        }

        ui.input_enabled = options_menu.mode == OPTIONS_MENU_CLOSED;
        ui_frame_rect(ui, MENU_RECT);

        const char* header_text = shell_get_menu_header_text(mode);
        ivec2 text_size = render_get_text_size(FONT_WESTERN8_GOLD, header_text);
        ivec2 text_pos = ivec2(MENU_RECT.x + (MENU_RECT.w / 2) - (text_size.x / 2), MENU_RECT.y + 20);
        ui_element_position(ui, text_pos);
        ui_text(ui, FONT_WESTERN8_GOLD, header_text);

        ivec2 column_position = ivec2(MENU_RECT.x + (MENU_RECT.w / 2), MENU_RECT.y + 64);
        if (mode != SHELL_MODE_MENU) {
            column_position.y += 22;
        }
        ui_begin_column(ui, column_position, 10);
            ivec2 button_size = ui_button_size("Return to Menu");
            if (mode == SHELL_MODE_MENU) {
                if (ui_button(ui, "Leave Match", button_size, true)) {
                    if (is_surrender_required_to_leave()) {
                        mode = SHELL_MODE_MENU_SURRENDER;
                    } else {
                        leave_match(false);
                    }
                }
                if (ui_button(ui, "Exit Program", button_size, true)) {
                    if (is_surrender_required_to_leave()) {
                        mode = SHELL_MODE_MENU_SURRENDER_TO_DESKTOP;
                    } else {
                        leave_match(true);
                    }
                }
                if (ui_button(ui, "Options", button_size, true)) {
                    options_menu = options_menu_open();
                }
                if (ui_button(ui, "Back", button_size, true)) {
                    mode = SHELL_MODE_NONE;
                }
            } else if (mode == SHELL_MODE_MENU_SURRENDER || mode == SHELL_MODE_MENU_SURRENDER_TO_DESKTOP) {
                if (ui_button(ui, "Yes", button_size, true)) {
                    leave_match(mode == SHELL_MODE_MENU_SURRENDER_TO_DESKTOP);
                }
                if (ui_button(ui, "Back", button_size, true)) {
                    mode = SHELL_MODE_MENU;
                }
            } else if (mode == SHELL_MODE_MATCH_OVER_VICTORY || mode == SHELL_MODE_MATCH_OVER_DEFEAT) {
                if (ui_button(ui, "Keep Playing", button_size, true)) {
                    mode = SHELL_MODE_NONE;
                }
                if (ui_button(ui, "Return to Menu", button_size, true)) {
                    leave_match(false);
                }
                if (ui_button(ui, "Exit Program", button_size, true)) {
                    leave_match(true);
                }
            }
        ui_end_container(ui);

        if (options_menu.mode != OPTIONS_MENU_CLOSED) {
            options_menu_update(options_menu, ui);
        }
    }

    // Match update
    if (is_paused) {
        match_update(match_state);
    }

    // Match events
    for (const MatchEvent& event : match_state.events) {
        if (event.type == MATCH_EVENT_SOUND) {
            if (sound_cooldown_timers[event.sound.sound] != 0) {
                break;
            }
            if (!is_cell_rect_revealed(event.sound.position / TILE_SIZE, 1)) {
                break;
            }
            if (SOUND_LISTEN_RECT.has_point(event.sound.position - camera_offset)) {
                sound_play(event.sound.sound);
                sound_cooldown_timers[event.sound.sound] = SOUND_COOLDOWN_DURATION;
            }
            break;
        }

        handle_match_event(event);

        switch (event.type) {
           
        }
    }
    match_state.events.clear();

    // Camera drag
    if (!is_selecting()) {
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
        camera_offset += camera_drag_direction * option_get_value(OPTION_CAMERA_SPEED);
        clamp_camera();
    }

    // Minimap drag
    if (is_minimap_dragging) {
        ivec2 minimap_pos = ivec2(
            std::clamp(input_get_mouse_position().x - MINIMAP_RECT.x, 0, MINIMAP_RECT.w),
            std::clamp(input_get_mouse_position().y - MINIMAP_RECT.y, 0, MINIMAP_RECT.h));
        ivec2 map_pos = ivec2(
            (match_state.map.width * TILE_SIZE * minimap_pos.x) / MINIMAP_RECT.w,
            (match_state.map.height * TILE_SIZE * minimap_pos.y) / MINIMAP_RECT.h);
        center_camera_on_cell(map_pos / TILE_SIZE);
    }

    // Update timers and animations
    if (animation_is_playing(move_animation)) {
        animation_update(move_animation);
    }
    animation_update(rally_flag_animation);
    animation_update(building_fire_animation);
    if (status_timer != 0) {
        status_timer--;
    }
    if (double_click_timer != 0) {
        double_click_timer--;
    }
    if (control_group_double_tap_timer != 0) {
        control_group_double_tap_timer--;
    }
    for (int sound = 0; sound < SOUND_COUNT; sound++) {
        if (sound_cooldown_timers[sound] != 0) {
            sound_cooldown_timers[sound]--;
        }
    }
    if (input_is_text_input_active()) {
        chat_cursor_blink_timer--;
        if (chat_cursor_blink_timer == 0) {
            chat_cursor_visible = !chat_cursor_visible;
            chat_cursor_blink_timer = CHAT_CURSOR_BLINK_DURATION;
        }
    }

    // Update displayed gold amounts
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!match_state.players[player_id].active) {
            continue;
        } 
        int difference = std::abs((int)displayed_gold_amounts[player_id] - (int)match_state.players[player_id].gold);
        int step = 1;
        if (difference > 100) {
            step = 10;
        } else if (difference > 10) {
            step = 5;
        } else if (difference > 1) {
            step = 2;
        }
        if (displayed_gold_amounts[player_id] < match_state.players[player_id].gold) {
            displayed_gold_amounts[player_id] += step;
        } else if (displayed_gold_amounts[player_id] > match_state.players[player_id].gold) {
            displayed_gold_amounts[player_id] -= step;
        }
    }

    // Update alerts
    {
        uint32_t alert_index = 0;
        while (alert_index < alerts.size()) {
            alerts[alert_index].timer--;
            if (alerts[alert_index].timer == 0) {
                alerts.erase(alerts.begin() + alert_index);
            } else {
                alert_index++;
            }
        }
    }

    // Update chat
    for (uint32_t chat_index = 0; chat_index < chat.size(); chat_index++) {
        chat[chat_index].timer--;
        if (chat[chat_index].timer == 0) {
            chat.erase(chat.begin() + chat_index);
            chat_index--;
        }
    }

    // Clear hidden units from selection
    {
        uint32_t selection_index = 0;
        while (selection_index < selection.size()) {
            Entity& selected_entity = match_state.entities.get_by_id(selection[selection_index]);
            if (!entity_is_selectable(selected_entity) || !is_entity_visible(selected_entity)) {
                selection.erase(selection.begin() + selection_index);
            } else {
                selection_index++;
            }
        }
    }

    // Update cursor
    cursor_set(is_targeting() && (!shell_is_mouse_in_ui() || MINIMAP_RECT.has_point(input_get_mouse_position())) ? CURSOR_TARGET : CURSOR_DEFAULT);
    if ((is_targeting() || mode == SHELL_MODE_BUILDING_PLACE || is_in_submenu()) && selection.empty()) {
        mode = SHELL_MODE_NONE;
    }

    // Play fire sound effects
    bool is_fire_on_screen = false;
    for (const Fire& fire : match_state.fires) {
        if (!is_cell_rect_revealed(fire.cell, 1)) {
            continue;
        }
        if (map_get_cell(match_state.map, CELL_LAYER_GROUND, fire.cell).type != CELL_EMPTY) {
            continue;
        }
        Rect fire_rect = (Rect) {
            .x = (fire.cell.x * TILE_SIZE) - camera_offset.x,
            .y = (fire.cell.y * TILE_SIZE) - camera_offset.y,
            .w = TILE_SIZE,
            .h = TILE_SIZE
        };
        if (SOUND_LISTEN_RECT.intersects(fire_rect)) {
            is_fire_on_screen = true;
            break;
        }
    }
    if (!is_fire_on_screen) {
        for (const Entity& entity : match_state.entities) {
            if (entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_BUILDING_DESTROYED) {
                continue;
            }
            if (!entity_is_building(entity.type) || !entity_check_flag(entity, ENTITY_FLAG_ON_FIRE)) {
                continue;
            }
            if (!is_entity_visible(entity)) {
                continue;
            }

            RenderSpriteParams params = match_ui_create_entity_render_params(state, entity);
            const SpriteInfo& sprite_info = render_get_sprite_info(match_entity_get_sprite(match, entity));
            Rect render_rect = (Rect) {
                .x = params.position.x, .y = params.position.y,
                .w = sprite_info.frame_width, .h = sprite_info.frame_height
            };

            // Entity is on fire and on screen
            if (SOUND_LISTEN_RECT.intersects(render_rect)) {
                is_fire_on_screen = true;
                break;
            }
        }
    }
    if (is_fire_on_screen && !sound_is_fire_loop_playing()) {
        sound_begin_fire_loop();
    } else if (!is_fire_on_screen && sound_is_fire_loop_playing()) {
        sound_end_fire_loop();
    }
}

void Shell::render() const {

}

void Shell::clamp_camera() {
    camera_offset.x = std::clamp(camera_offset.x, 0, (match_state.map.width * TILE_SIZE) - SCREEN_WIDTH);
    camera_offset.y = std::clamp(camera_offset.y, 0, (match_state.map.height * TILE_SIZE) - SCREEN_HEIGHT + SHELL_UI_HEIGHT);
}

void Shell::center_camera_on_cell(ivec2 cell) {
    camera_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    camera_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - SHELL_UI_HEIGHT) / 2);
    clamp_camera();
}

void Shell::show_status(const char* message) {
    status_message = message;
    status_timer = STATUS_DURATION;
}

void Shell::set_selection(std::vector<EntityId>& selection) {
    this->selection = selection;

    for (uint32_t selection_index = 0; selection_index < selection.size(); selection_index++) {
        uint32_t entity_index = match_state.entities.get_index_of(selection[selection_index]);
        if (entity_index == INDEX_INVALID || !entity_is_selectable(match_state.entities[entity_index])) {
            selection.erase(selection.begin() + selection_index);
            selection_index--;
            continue;
        }
    }

    while (selection.size() > SELECTION_LIMIT) {
        selection.pop_back();
    }

    mode = SHELL_MODE_NONE;
}

bool Shell::is_selecting() const {
    return select_origin.x != -1;
}

bool Shell::is_in_menu() const {
    return mode == SHELL_MODE_MENU || 
                mode == SHELL_MODE_MENU_SURRENDER ||
                mode == SHELL_MODE_MENU_SURRENDER_TO_DESKTOP ||
                mode == SHELL_MODE_MATCH_OVER_VICTORY ||
                mode == SHELL_MODE_MATCH_OVER_DEFEAT;
}

bool Shell::is_in_submenu() const {
    return mode == SHELL_MODE_BUILD || mode == SHELL_MODE_BUILD2;
}

bool Shell::is_targeting() const {
    return mode >= SHELL_MODE_TARGET_ATTACK && mode < SHELL_MODE_CHAT;
}

// Stateless helpers

const char* shell_get_menu_header_text(ShellMode mode) {
    switch (mode) {
        case SHELL_MODE_MENU:
            return "Game Menu";
        case SHELL_MODE_MENU_SURRENDER:
        case SHELL_MODE_MENU_SURRENDER_TO_DESKTOP:
            return "Surrender?";
        case SHELL_MODE_MATCH_OVER_VICTORY:
            return "Victory!";
        case SHELL_MODE_MATCH_OVER_DEFEAT:
            return "Defeat!";
        default:
            return "";
    }
}

bool shell_is_mouse_in_ui() {
    ivec2 mouse_position = input_get_mouse_position();
    return (mouse_position.y >= SCREEN_HEIGHT - SHELL_UI_HEIGHT) ||
           (mouse_position.x <= 272 && mouse_position.y >= SCREEN_HEIGHT - 272) ||
           (mouse_position.x >= SCREEN_WIDTH - 264 && mouse_position.y >= SCREEN_HEIGHT - 212);
}
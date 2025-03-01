#pragma once

#include "defines.h"
#include "noise.h"
#include "util.h"
#include "options.h"
#include "match.h"
#include "network.h"
#include <SDL2/SDL.h>
#include <unordered_map>
#include <string>

#define UI_HEIGHT 88
#define UI_BUTTONSET_SIZE 6
#define UI_CONTROL_GROUP_COUNT 10
#define UI_CONTROL_GROUP_NONE_SELECTED -1

extern const xy UI_SELECTION_LIST_TOP_LEFT; 
extern const SDL_Rect UI_BUTTON_RECT[UI_BUTTONSET_SIZE];
extern const xy UI_FRAME_BOTTOM_POSITION; 
extern const xy UI_BUILDING_QUEUE_TOP_LEFT; 
extern const xy UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX];
extern const SDL_Rect UI_CHAT_RECT;
extern const SDL_Rect UI_MINIMAP_RECT;

enum UiMode {
    UI_MODE_MATCH_NOT_STARTED,
    UI_MODE_NONE,
    UI_MODE_BUILD,
    UI_MODE_BUILD2,
    UI_MODE_BUILDING_PLACE,
    UI_MODE_TARGET_ATTACK,
    UI_MODE_TARGET_UNLOAD,
    UI_MODE_TARGET_REPAIR,
    UI_MODE_TARGET_SMOKE,
    UI_MODE_CHAT,
    UI_MODE_MENU,
    UI_MODE_MENU_SURRENDER,
    UI_MODE_MATCH_OVER_VICTORY,
    UI_MODE_MATCH_OVER_DEFEAT,
    UI_MODE_LEAVE_MATCH
};

enum ui_button_requirements_type {
    UI_BUTTON_REQUIRES_BUILDING,
    UI_BUTTON_REQUIRES_UPGRADE
};

struct ui_button_requirements_t {
    ui_button_requirements_type type;
    union {
        EntityType building_type;
        uint32_t upgrade;
    };
};
extern const std::unordered_map<UiButton, ui_button_requirements_t> UI_BUTTON_REQUIREMENTS;

struct ui_tooltip_info_t {
    char text[64];
    uint32_t gold_cost;
    uint32_t population_cost;
};

enum UiSelectionType {
    UI_SELECTION_TYPE_NONE,
    UI_SELECTION_TYPE_UNITS,
    UI_SELECTION_TYPE_BUILDINGS,
    UI_SELECTION_TYPE_ENEMY_UNIT,
    UI_SELECTION_TYPE_ENEMY_BUILDING,
    UI_SELECTION_TYPE_GOLD
};

enum UiAlertColor {
    ALERT_COLOR_PLAYER,
    ALERT_COLOR_WHITE,
    ALERT_COLOR_GOLD
};

struct alert_t {
    UiAlertColor color;
    xy cell;
    int cell_size;
    uint32_t timer;
};

// Chat

struct chat_message_t {
    char message[MATCH_CHAT_MESSAGE_MAX_LENGTH + 1];
    uint8_t player_id;
    uint32_t timer;
};

struct ui_state_t {
    UiMode mode;
    UiButton buttons[UI_BUTTONSET_SIZE];

    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t turn_timer;
    uint32_t disconnect_timer;
    match_state_t match_state;

    bool is_minimap_dragging;
    xy camera_offset;

    xy select_rect_origin;
    SDL_Rect select_rect;
    std::vector<entity_id> selection;

    animation_t move_animation;
    animation_t rally_flag_animation;
    xy move_animation_position;
    entity_id move_animation_entity_id;

    std::string status_message;
    uint32_t status_timer;

    EntityType building_type;

    uint32_t double_click_timer;
    std::vector<entity_id> control_groups[UI_CONTROL_GROUP_COUNT];
    uint32_t control_group_double_tap_timer;
    SDL_Keycode control_group_double_tap_key;
    int control_group_selected;

    std::string chat_message;
    std::vector<chat_message_t> chat;

    std::vector<alert_t> alerts;

    uint32_t sound_cooldown_timers[SOUND_COUNT];

    options_menu_state_t options_menu_state;
};

ui_state_t ui_init(int32_t lcg_seed, const noise_t& noise);
void ui_handle_input(ui_state_t& state, SDL_Event e);
void ui_handle_network_event(ui_state_t& state, const network_event_t& network_event);
void ui_update(ui_state_t& state);

void ui_create_minimap_texture(const ui_state_t& state);
input_t ui_create_move_input(const ui_state_t& state);
bool ui_is_mouse_in_ui();
bool ui_is_selecting(const ui_state_t& state);
xy ui_get_mouse_world_pos(const ui_state_t& state);
void ui_clamp_camera(ui_state_t& state);
void ui_center_camera_on_cell(ui_state_t& state, xy cell);

std::vector<entity_id> ui_create_selection_from_rect(const ui_state_t& state);
void ui_set_selection(ui_state_t& state, const std::vector<entity_id>& selection);
void ui_update_buttons(ui_state_t& state);
UiSelectionType ui_get_selection_type(const ui_state_t& state, const std::vector<entity_id>& selection);
bool ui_is_targeting(const ui_state_t& state);
int ui_get_ui_button_hovered(const ui_state_t& state);
bool ui_button_requirements_met(const ui_state_t& state, UiButton button);
void ui_handle_ui_button_press(ui_state_t& state, UiButton button);
void ui_deselect_entity_if_selected(ui_state_t& state, entity_id id);
void ui_show_status(ui_state_t& state, const char* message);
xy ui_get_building_cell(const ui_state_t& state);
bool ui_building_can_be_placed(const ui_state_t& state);
ui_tooltip_info_t ui_get_hovered_tooltip_info(const ui_state_t& state);
xy ui_garrisoned_icon_position(int index);
int ui_get_garrisoned_index_hovered(const ui_state_t& state);
xy ui_get_selected_unit_icon_position(uint32_t unit_index);
int ui_get_selected_unit_hovered(const ui_state_t& state);
int ui_get_building_queue_item_hovered(const ui_state_t& state);

// Render

void ui_render(const ui_state_t& state);
render_sprite_params_t ui_create_entity_render_params(const ui_state_t& state, const entity_t& entity);
void ui_render_healthbar(xy position, xy size, int health, int max_health);
void ui_render_garrisoned_units_healthbar(xy position, xy size, int garrisoned_size, int garrisoned_capacity);
void ui_render_target_build(const ui_state_t& state, const target_t& target);
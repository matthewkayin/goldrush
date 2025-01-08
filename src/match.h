#pragma once

#include "defines.h"
#include "util.h"
#include "id_array.h"
#include "engine.h"
#include "animation.h"
#include <SDL2/SDL.h>
#include <queue>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>

#define UI_HEIGHT 88
#define PLAYER_NONE UINT8_MAX
#define UI_BUTTONSET_SIZE 6
#define BUILDING_QUEUE_MAX 5
#define BUILDING_DEQUEUE_POP_FRONT BUILDING_QUEUE_MAX
#define BUILDING_QUEUE_BLOCKED UINT32_MAX
#define BUILDING_QUEUE_EXIT_BLOCKED UINT32_MAX - 1
#define BUILDING_FADE_DURATION 300
#define GOLD_PATCH_ID_NULL UINT32_MAX
#define ENTITY_UNLOAD_ALL ID_NULL

#define UI_STATUS_CANT_BUILD "You can't build there."
#define UI_STATUS_NOT_ENOUGH_GOLD "Not enough gold."
#define UI_STATUS_NOT_ENOUGH_HOUSE "Not enough houses."
#define UI_STATUS_BUILDING_QUEUE_FULL "Building queue is full."
#define UI_STATUS_MINE_COLLAPSED "Your gold mine collapsed!"
#define UI_STATUS_UNDER_ATTACK "You're under attack!"
#define UI_STATUS_MINE_EXIT_BLOCKED "Mine exit is blocked."
#define UI_STATUS_BUILDING_EXIT_BLOCKED "Building exit is blocked."
#define UI_STATUS_REPAIR_TARGET_INVALID "Must target an allied building."

// Map

struct tile_t {
    uint16_t index;
    uint16_t elevation;
};

const entity_id CELL_EMPTY = ID_NULL;
const entity_id CELL_BLOCKED = ID_MAX + 2;
const entity_id CELL_UNREACHABLE = ID_MAX + 3;
const entity_id CELL_DECORATION_1 = ID_MAX + 4;
const entity_id CELL_DECORATION_2 = ID_MAX + 5;
const entity_id CELL_DECORATION_3 = ID_MAX + 6;
const entity_id CELL_DECORATION_4 = ID_MAX + 7;
const entity_id CELL_DECORATION_5 = ID_MAX + 8;

const int FOG_HIDDEN = -1;
const int FOG_EXPLORED = 0;

// Entities

// If you change this, make sure that entity_is_unit() and entity_is_building() still work
enum EntityType {
    ENTITY_MINER,
    ENTITY_COWBOY,
    ENTITY_BANDIT,
    ENTITY_WAGON,
    ENTITY_WAR_WAGON,
    ENTITY_JOCKEY,
    ENTITY_SAPPER,
    ENTITY_HALL,
    ENTITY_CAMP,
    ENTITY_HOUSE,
    ENTITY_SALOON,
    ENTITY_BUNKER,
    ENTITY_COOP,
    ENTITY_SMITH,
    ENTITY_GOLD
};

enum EntityMode {
    MODE_UNIT_IDLE,
    MODE_UNIT_MOVE,
    MODE_UNIT_MOVE_BLOCKED,
    MODE_UNIT_MOVE_FINISHED,
    MODE_UNIT_BUILD,
    MODE_UNIT_REPAIR,
    MODE_UNIT_ATTACK_WINDUP,
    MODE_UNIT_ATTACK_COOLDOWN,
    MODE_UNIT_MINE,
    MODE_UNIT_DEATH,
    MODE_UNIT_DEATH_FADE,
    MODE_BUILDING_IN_PROGRESS,
    MODE_BUILDING_FINISHED,
    MODE_BUILDING_DESTROYED,
    MODE_GOLD,
    MODE_GOLD_MINED_OUT
};

enum TargetType {
    TARGET_NONE,
    // Make sure that move targets stay in the same order as their inputs
    TARGET_CELL, 
    TARGET_ENTITY,
    TARGET_ATTACK_CELL,
    TARGET_ATTACK_ENTITY,
    TARGET_REPAIR,
    TARGET_UNLOAD,
    TARGET_BUILD,
    TARGET_BUILD_ASSIST,
    TARGET_GOLD
};

struct target_build_t {
    xy unit_cell;
    xy building_cell;
    EntityType building_type;
};

struct target_repair_t {
    uint32_t health_repaired;
};

struct target_t {
    TargetType type;
    entity_id id;
    xy cell;
    union {
        target_build_t build;
        target_repair_t repair;
    };
};

enum BuildingQueueItemType: uint8_t {
    BUILDING_QUEUE_ITEM_UNIT,
    BUILDING_QUEUE_ITEM_UPGRADE
};

struct building_queue_item_t {
    BuildingQueueItemType type;
    union {
        EntityType unit_type;
        uint32_t upgrade;
    };
};

const uint32_t ENTITY_FLAG_HOLD_POSITION = 1;
const uint32_t ENTITY_FLAG_DAMAGE_FLICKER = 2;

struct entity_t {
    EntityType type;
    EntityMode mode;
    uint8_t player_id;
    uint32_t flags;

    xy cell;
    xy_fixed position;
    Direction direction;

    int health;
    target_t target;
    target_t remembered_gold_target;
    std::vector<xy> path;
    std::vector<building_queue_item_t> queue;
    xy rally_point;
    uint32_t timer;

    animation_t animation;

    std::vector<entity_id> garrisoned_units;
    entity_id garrison_id;
    uint32_t bunker_cooldown_timer;

    uint32_t gold_held;
    uint32_t gold_patch_id;

    uint32_t taking_damage_timer;
    uint32_t health_regen_timer;
};

struct unit_data_t {
    uint32_t population_cost;
    fixed speed;

    int damage;
    int attack_cooldown;
    int range_squared;
};

struct building_data_t {
    int builder_positions_x[3];
    int builder_positions_y[3];
    int builder_flip_h[3];
    bool can_rally; // If you add any more flags, use a uint32
};

struct entity_data_t {
    const char* name;
    Sprite sprite;
    UiButton ui_button;
    int cell_size;

    uint32_t gold_cost;
    uint32_t train_duration;
    int max_health;
    int sight;
    int armor;
    uint32_t attack_priority;

    uint32_t garrison_capacity;
    uint32_t garrison_size;

    union {
        unit_data_t unit_data;
        building_data_t building_data;
    };
};
extern const std::unordered_map<EntityType, entity_data_t> ENTITY_DATA;

struct remembered_entity_t {
    render_sprite_params_t sprite_params;
    xy cell;
    int cell_size;
};

// UI

const SDL_Rect MINIMAP_RECT = (SDL_Rect) {
    .x = 4,
    .y = SCREEN_HEIGHT - 132,
    .w = 128,
    .h = 128
};
extern const xy SELECTION_LIST_TOP_LEFT; 

enum UiMode {
    UI_MODE_MATCH_NOT_STARTED,
    UI_MODE_NONE,
    UI_MODE_BUILD,
    UI_MODE_BUILD2,
    UI_MODE_BUILDING_PLACE,
    UI_MODE_TARGET_ATTACK,
    UI_MODE_TARGET_UNLOAD,
    UI_MODE_TARGET_REPAIR,
    UI_MODE_CHAT,
    UI_MODE_MENU,
    UI_MODE_MENU_SURRENDER,
    UI_MODE_MATCH_OVER_VICTORY,
    UI_MODE_MATCH_OVER_DEFEAT,
    UI_MODE_LEAVE_MATCH
};

enum UiButtonRequirementsType {
    UI_BUTTON_REQUIRES_BUILDING,
    UI_BUTTON_REQUIRES_UPGRADE
};

struct ui_button_requirements_t {
    UiButtonRequirementsType type;
    union {
        EntityType building_type;
        uint32_t upgrade;
    };
};

struct ui_tooltip_info_t {
    char text[64];
    uint32_t gold_cost;
    uint32_t population_cost;
};

enum SelectionType {
    SELECTION_TYPE_NONE,
    SELECTION_TYPE_UNITS,
    SELECTION_TYPE_BUILDINGS,
    SELECTION_TYPE_ENEMY_UNIT,
    SELECTION_TYPE_ENEMY_BUILDING,
    SELECTION_TYPE_GOLD
};

struct chat_message_t {
    std::string message;
    uint32_t timer;
};

enum AlertColor {
    ALERT_COLOR_GREEN,
    ALERT_COLOR_RED
};

struct alert_t {
    AlertColor color;
    xy cell;
    int cell_size;
    uint32_t timer;
};

extern const SDL_Rect UI_BUTTON_RECT[UI_BUTTONSET_SIZE];
extern const xy UI_FRAME_BOTTOM_POSITION; 
extern const xy BUILDING_QUEUE_TOP_LEFT; 
extern const std::unordered_map<UiButton, ui_button_requirements_t> UI_BUTTON_REQUIREMENTS;
extern const std::unordered_map<UiButton, SDL_Keycode> hotkeys;
extern const uint32_t MATCH_TAKING_DAMAGE_TIMER_DURATION;
extern const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION;
extern const uint32_t MATCH_ALERT_DURATION;
extern const uint32_t MATCH_ALERT_LINGER_DURATION;
extern const uint32_t MATCH_ALERT_TOTAL_DURATION;
extern const uint32_t MATCH_ATTACK_ALERT_DISTANCE;
extern const xy UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX];

// Particles

struct particle_t {
    Sprite sprite;
    animation_t animation;
    int vframe;
    xy position;
};

// Upgrades

const uint32_t UPGRADE_WAR_WAGON = 1;
const uint32_t UPGRADE_EXPLOSIVES = 2;

struct upgrade_data_t {
    const char* name;
    UiButton ui_button;
    uint32_t gold_cost;
    uint32_t research_duration;
};

extern const std::unordered_map<uint32_t, upgrade_data_t> UPGRADE_DATA;

// Input

enum InputType: uint8_t {
    INPUT_NONE,
    // Make sure that move inputs stay in the same order as their targets
    INPUT_MOVE_CELL,
    INPUT_MOVE_ENTITY,
    INPUT_MOVE_ATTACK_CELL,
    INPUT_MOVE_ATTACK_ENTITY,
    INPUT_MOVE_REPAIR,
    INPUT_MOVE_UNLOAD,
    INPUT_STOP,
    INPUT_DEFEND,
    INPUT_BUILD,
    INPUT_BUILD_CANCEL,
    INPUT_BUILDING_ENQUEUE,
    INPUT_BUILDING_DEQUEUE,
    INPUT_RALLY,
    INPUT_UNLOAD,
    INPUT_SINGLE_UNLOAD,
    INPUT_CHAT,
    INPUT_EXPLODE
};

struct input_move_t {
    xy target_cell;
    entity_id target_id;
    uint8_t entity_count;
    entity_id entity_ids[SELECTION_LIMIT];
};

struct input_stop_t {
    uint8_t entity_count;
    entity_id entity_ids[SELECTION_LIMIT];
};

struct input_build_t {
    uint8_t building_type;
    xy target_cell;
    uint16_t entity_count;
    entity_id entity_ids[SELECTION_LIMIT];
};

struct input_build_cancel_t {
    entity_id building_id;
};

struct input_building_enqueue_t {
    entity_id building_id;
    building_queue_item_t item;
};

struct input_building_dequeue_t {
    entity_id building_id;
    uint16_t index;
};

struct input_unload_t {
    uint16_t entity_count;
    entity_id entity_ids[SELECTION_LIMIT];
};

struct input_single_unload_t {
    entity_id unit_id;
};

struct input_rally_t {
    xy rally_point;
    uint16_t building_count;
    entity_id building_ids[SELECTION_LIMIT];
};

struct input_explode_t {
    uint16_t entity_count;
    entity_id entity_ids[SELECTION_LIMIT];
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
        input_stop_t stop;
        input_stop_t defend;
        input_build_t build;
        input_build_cancel_t build_cancel;
        input_building_enqueue_t building_enqueue;
        input_building_dequeue_t building_dequeue;
        input_unload_t unload;
        input_single_unload_t single_unload;
        input_rally_t rally;
        input_explode_t explode;
    };
};

struct match_state_t {
    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t turn_timer;

    // UI
    UiMode ui_mode;
    UiButton ui_buttons[6];
    bool ui_is_minimap_dragging;
    xy camera_offset;
    xy select_rect_origin;
    SDL_Rect select_rect;
    std::vector<entity_id> selection;
    animation_t ui_move_animation;
    animation_t ui_rally_flag_animation;
    xy ui_move_position;
    entity_id ui_move_entity_id;
    std::string ui_status_message;
    uint32_t ui_status_timer;
    std::vector<chat_message_t> ui_chat;
    uint32_t ui_disconnect_timer;
    EntityType ui_building_type;
    uint32_t ui_double_click_timer;
    std::vector<entity_id> control_groups[10];
    uint32_t control_group_double_tap_timer;
    SDL_Keycode control_group_double_tap_key;
    int control_group_selected;
    std::vector<alert_t> alerts;

    // Map
    uint32_t map_width;
    uint32_t map_height;
    std::vector<tile_t> map_tiles;
    std::vector<entity_id> map_cells;
    std::vector<int> map_fog[MAX_PLAYERS];
    std::unordered_map<entity_id, remembered_entity_t> remembered_entities[MAX_PLAYERS];
    bool map_is_fog_dirty;

    // Entities
    id_array<entity_t, 400 * MAX_PLAYERS> entities;
    std::vector<particle_t> particles;

    uint32_t player_gold[MAX_PLAYERS];
    uint32_t player_upgrades[MAX_PLAYERS];
    uint32_t player_upgrades_in_progress[MAX_PLAYERS];
};

match_state_t match_init();
void match_handle_input(match_state_t& state, SDL_Event e);
void match_update(match_state_t& state);
uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id);
uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id);
bool match_player_has_upgrade(const match_state_t& state, uint8_t player_id, uint32_t upgrade);
bool match_player_upgrade_is_available(const match_state_t& state, uint8_t player_id, uint32_t upgrade);
void match_grant_player_upgrade(match_state_t& state, uint8_t player_id, uint32_t upgrade);
void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);
void match_render(const match_state_t& state);
render_sprite_params_t match_create_entity_render_params(const match_state_t& state, const entity_t& entity);
void match_render_healthbar(xy position, xy size, int health, int max_health);
void match_render_garrisoned_units_healthbar(xy position, xy size, int garrisoned_size, int garrisoned_capacity);
void match_render_text_with_text_frame(const char* text, xy position);

// Generic
void match_camera_clamp(match_state_t& state);
void match_camera_center_on_cell(match_state_t& state, xy cell);
xy match_get_mouse_world_pos(const match_state_t& state);
input_t match_create_move_input(const match_state_t& state);

// UI
bool ui_is_mouse_in_ui();
bool ui_is_selecting(const match_state_t& state);
std::vector<entity_id> ui_create_selection_from_rect(const match_state_t& state);
void ui_set_selection(match_state_t& state, const std::vector<entity_id>& selection);
void ui_update_buttons(match_state_t& state);
SelectionType ui_get_selection_type(const match_state_t& state, const std::vector<entity_id>& selection);
bool ui_is_targeting(const match_state_t& state);
void ui_add_chat_message(match_state_t& state, std::string message);
int ui_get_ui_button_hovered(const match_state_t& state);
bool ui_button_requirements_met(const match_state_t& state, UiButton button);
void ui_handle_ui_button_press(match_state_t& state, UiButton button);
void ui_deselect_entity_if_selected(match_state_t& state, entity_id id);
void ui_show_status(match_state_t& state, const char* message);
xy ui_get_building_cell(const match_state_t& state);
entity_id ui_get_nearest_builder(const match_state_t& state, const std::vector<entity_id>& builders, xy cell);
bool ui_building_can_be_placed(const match_state_t& state);
ui_tooltip_info_t ui_get_hovered_tooltip_info(const match_state_t& state);
xy ui_garrisoned_icon_position(int index);
int ui_get_garrisoned_index_hovered(const match_state_t& state);
xy ui_get_selected_unit_icon_position(uint32_t unit_index);
int ui_get_selected_unit_hovered(const match_state_t& state);
int ui_get_building_queue_item_hovered(const match_state_t& state);

// Map
std::vector<xy> map_init(match_state_t& state);
void map_calculate_unreachable_cells(match_state_t& state);
void map_recalculate_unreachable_cells(match_state_t& state, xy cell);
bool map_is_cell_in_bounds(const match_state_t& state, xy cell);
bool map_is_cell_rect_in_bounds(const match_state_t& state, xy cell, int cell_size);
tile_t map_get_tile(const match_state_t& state, xy cell);
entity_id map_get_cell(const match_state_t& state, xy cell);
bool map_is_cell_rect_equal_to(const match_state_t& state, xy cell, int cell_size, entity_id value);
void map_set_cell_rect(match_state_t& state, xy cell, int cell_size, entity_id value);
bool map_is_cell_rect_occupied(const match_state_t& state, xy cell, int cell_size, xy origin = xy(-1, -1), bool ignore_miners = false);
bool map_is_cell_rect_occupied(const match_state_t& state, xy cell, xy cell_size, xy origin = xy(-1, -1), bool ignore_miners = false);
xy map_get_nearest_cell_around_rect(const match_state_t& state, xy start, int start_size, xy rect_position, int rect_size, bool allow_blocked_cells);
bool map_is_tile_ramp(const match_state_t& state, xy cell);
bool map_is_cell_rect_same_elevation(const match_state_t& state, xy cell, xy size);
void map_pathfind(const match_state_t& state, xy from, xy to, int cell_size, std::vector<xy>* path, bool gold_walk);
bool map_is_cell_rect_revealed(const match_state_t& state, uint8_t player_id, xy cell, int cell_size);
void map_fog_update(match_state_t& state, uint8_t player_id, xy cell, int cell_size, int sight, bool increment);

// Entities
entity_id entity_create(match_state_t& state, EntityType type, uint8_t player_id, xy cell);
entity_id entity_create_gold(match_state_t& state, xy cell, uint32_t gold_left, uint32_t gold_patch_id);
void entity_update(match_state_t& state, uint32_t entity_index);

bool entity_is_unit(EntityType entity);
bool entity_is_building(EntityType type);
int entity_cell_size(EntityType entity);
SDL_Rect entity_get_rect(const entity_t& entity);
xy entity_get_center_position(const entity_t& entity);
Sprite entity_get_sprite(const entity_t entity);
Sprite entity_get_select_ring(const entity_t entity, bool is_ally);
uint16_t entity_get_elevation(const match_state_t& state, const entity_t& entity);
bool entity_is_selectable(const entity_t& entity);
bool entity_check_flag(const entity_t& entity, uint32_t flag);
void entity_set_flag(entity_t& entity, uint32_t flag, bool value);
bool entity_is_target_invalid(const match_state_t& state, const entity_t& entity);
bool entity_has_reached_target(const match_state_t& state, const entity_t& entity);
xy entity_get_target_cell(const match_state_t& state, const entity_t& entity);
xy_fixed entity_get_target_position(const entity_t& entity);
AnimationName entity_get_expected_animation(const entity_t& entity);
xy entity_get_animation_frame(const entity_t& entity);
bool entity_should_flip_h(const entity_t& entity);
bool entity_should_die(const entity_t& entity);
SDL_Rect entity_get_sight_rect(const entity_t& entity);
bool entity_can_see_rect(const entity_t& entity, xy rect_position, int rect_size);
target_t entity_target_gold(const match_state_t& state, const entity_t& entity, entity_id gold_id);
target_t entity_target_nearest_enemy(const match_state_t& state, const entity_t& entity);
target_t entity_target_nearest_gold(const match_state_t& state, uint8_t entity_player_id, xy start_cell, uint32_t gold_patch_id);
target_t entity_target_nearest_camp(const match_state_t& state, const entity_t& entity);
bool entity_should_gold_walk(const match_state_t& state, const entity_t& entity);
SDL_Rect entity_gold_get_block_building_rect(xy cell);
uint32_t entity_get_garrisoned_occupancy(const match_state_t& state, const entity_t& entity);

void entity_set_target(entity_t& entity, target_t target);
void entity_attack_target(match_state_t& state, entity_id attacker_id, entity_t& defender);
void entity_explode(match_state_t& state, entity_id id);
xy entity_get_exit_cell(const match_state_t& state, xy building_cell, int building_size, int unit_size, xy rally_cell);
void entity_unload_unit(match_state_t& state, entity_t& entity, entity_id garrisoned_unit_id);
void entity_stop_building(match_state_t& state, entity_id id);
void entity_building_finish(match_state_t& state, entity_id building_id);

void entity_building_enqueue(match_state_t& state, entity_t& building, building_queue_item_t item);
void entity_building_dequeue(match_state_t& state, entity_t& building);
bool entity_building_is_supply_blocked(const match_state_t& state, const entity_t& building);

UiButton building_queue_item_icon(const building_queue_item_t& item);
uint32_t building_queue_item_duration(const building_queue_item_t& item);
uint32_t building_queue_item_cost(const building_queue_item_t& item);
uint32_t building_queue_population_cost(const building_queue_item_t& item);
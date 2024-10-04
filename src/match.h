#pragma once

#include "defines.h"
#include "util.h"
#include "sprite.h"
#include "container.h"
#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>
#include <array>
#include <string>

#define MAX_UNITS 200
#define MAX_PARTICLES 256

#define UI_STATUS_CANT_BUILD "You can't build there."
#define UI_STATUS_NOT_ENOUGH_GOLD "Not enough gold."
#define UI_STATUS_NOT_ENOUGH_HOUSE "Not enough houses."
#define UI_STATUS_BUILDING_QUEUE_FULL "Building queue is full."
#define UI_STATUS_MINE_COLLAPSED "Your gold mine collapsed!"
#define UI_STATUS_UNDER_ATTACK "You're under attack!"
#define UI_STATUS_MINE_EXIT_BLOCKED "Mine exit is blocked."
#define UI_STATUS_BUILDING_EXIT_BLOCKED "Building exit is blocked."

const rect_t MINIMAP_RECT = rect_t(xy(4, SCREEN_HEIGHT - 132), xy(128, 128));

extern const uint32_t UNIT_PATH_PAUSE_DURATION;
extern const uint32_t UNIT_BUILD_TICK_DURATION;
extern const uint32_t UNIT_MINE_TICK_DURATION; 
extern const uint32_t UNIT_MAX_GOLD_HELD; 
extern const uint32_t UNIT_CANT_BE_FERRIED; 
extern const uint32_t UNIT_IN_DURATION; 

const uint32_t BUILDING_QUEUE_BLOCKED = UINT32_MAX;
const uint32_t BUILDING_QUEUE_EXIT_BLOCKED = UINT32_MAX - 1;
const uint32_t BUILDING_QUEUE_MAX = 5;
const uint32_t BUILDING_FADE_DURATION = 300;
const uint32_t MINE_SIZE = 3;

extern const xy UI_FRAME_BOTTOM_POSITION;
extern const xy BUILDING_QUEUE_TOP_LEFT;
extern const xy UI_BUILDING_QUEUE_POSITIONS[BUILDING_QUEUE_MAX];

extern const uint32_t MATCH_WINNING_GOLD_AMOUNT;
extern const uint32_t MATCH_TAKING_DAMAGE_TIMER_DURATION;
extern const uint32_t MATCH_TAKING_DAMAGE_FLICKER_TIMER_DURATION;
extern const uint32_t MATCH_ALERT_DURATION;
extern const uint32_t MATCH_ATTACK_ALERT_DURATION;
extern const uint32_t MATCH_ATTACK_ALERT_DISTANCE;

const uint8_t BLOCKED_COMPLETELY = 255;
const uint8_t BLOCKED_NONE = 0;

// Map

struct tile_t {
    uint16_t index;
    int8_t elevation;
    uint8_t blocked;
};

struct decoration_t {
    uint32_t index;
    xy cell;
};

enum CellType: uint16_t {
    CELL_EMPTY,
    CELL_BLOCKED,
    CELL_UNIT,
    CELL_BUILDING,
    CELL_MINE
};

struct cell_t {
    CellType type;
    uint16_t value;
};

enum FogType: uint16_t {
    FOG_HIDDEN,
    FOG_EXPLORED,
    FOG_REVEALED
};

struct mine_t {
    xy cell;
    uint32_t gold_left;
    bool is_occupied;
};

// UI

enum UiMode {
    UI_MODE_NOT_STARTED,
    UI_MODE_NONE,
    UI_MODE_SELECTING,
    UI_MODE_MINIMAP_DRAG,
    UI_MODE_BUILDING_PLACE,
    UI_MODE_ATTACK_MOVE,
    UI_MODE_MATCH_OVER,
    UI_MODE_LEAVE_MATCH
};

enum UiButtonset {
    UI_BUTTONSET_NONE,
    UI_BUTTONSET_UNIT,
    UI_BUTTONSET_MINER,
    UI_BUTTONSET_BUILD,
    UI_BUTTONSET_CANCEL,
    UI_BUTTONSET_CAMP,
    UI_BUTTONSET_SALOON,
    UI_BUTTONSET_WAGON,
    UI_BUTTONSET_COUNT
};

enum SelectionType {
    SELECTION_TYPE_NONE,
    SELECTION_TYPE_UNITS,
    SELECTION_TYPE_BUILDINGS,
    SELECTION_TYPE_ENEMY_UNIT,
    SELECTION_TYPE_ENEMY_BUILDING,
    SELECTION_TYPE_MINE
};

struct selection_t {
    SelectionType type;
    std::vector<entity_id> ids;
};

enum AlertType {
    ALERT_UNIT_ATTACKED,
    ALERT_BUILDING_ATTACKED,
    ALERT_UNIT_FINISHED,
    ALERT_BUILDING_FINISHED,
    ALERT_MINE_COLLAPSED
};

struct alert_t {
    AlertType type;
    entity_id id;
    uint32_t timer;
};

struct attack_alert_t {
    xy cell;
    uint32_t timer;
};

// Unit

enum UnitType: uint8_t {
    UNIT_MINER,
    UNIT_COWBOY,
    UNIT_WAGON
};

enum BuildingType {
    BUILDING_NONE,
    BUILDING_HOUSE,
    BUILDING_CAMP,
    BUILDING_SALOON
};

struct path_t {
    std::vector<xy> points;
    xy target;
};

enum UnitTargetType {
    UNIT_TARGET_NONE,
    UNIT_TARGET_CELL,
    UNIT_TARGET_BUILD,
    UNIT_TARGET_BUILD_ASSIST,
    UNIT_TARGET_UNIT,
    UNIT_TARGET_BUILDING,
    UNIT_TARGET_CAMP,
    UNIT_TARGET_MINE,
    UNIT_TARGET_ATTACK
};

enum UnitMode {
    UNIT_MODE_IDLE,
    UNIT_MODE_MOVE,
    UNIT_MODE_MOVE_BLOCKED,
    UNIT_MODE_MOVE_FINISHED,
    UNIT_MODE_BUILD,
    UNIT_MODE_REPAIR,
    UNIT_MODE_IN_MINE,
    UNIT_MODE_ATTACK_WINDUP,
    UNIT_MODE_ATTACK_COOLDOWN,
    UNIT_MODE_FERRY,
    UNIT_MODE_DEATH,
    UNIT_MODE_DEATH_FADE
};

struct unit_target_build_t {
    xy unit_cell;
    xy building_cell;
    BuildingType building_type;
    entity_id building_id;
};

struct unit_target_t {
    UnitTargetType type;
    union {
        xy cell;
        entity_id id;
        unit_target_build_t build;
    };
};

struct unit_t {
    UnitType type;
    UnitMode mode;
    uint8_t player_id;

    animation_t animation;
    int health;

    Direction direction;
    xy_fixed position;
    xy cell;

    unit_target_t target;
    std::vector<xy> path;

    entity_id garrison_id;
    std::vector<entity_id> ferried_units;
    uint32_t gold_held;
    uint32_t timer;

    uint32_t taking_damage_timer;
    uint32_t taking_damage_flicker_timer;
    bool taking_damage_flicker;
};

struct unit_data_t {
    const char* name;
    Sprite sprite;
    int cell_size;
    int max_health;
    int damage;
    int armor;
    int range_squared;
    int attack_cooldown;
    fixed speed;
    int sight;
    uint32_t cost;
    uint32_t population_cost;
    uint32_t train_duration;
    uint32_t ferry_capacity;
    uint32_t ferry_size;
};

// Building

enum BuildingQueueItemType: uint8_t {
    BUILDING_QUEUE_ITEM_UNIT
};

struct building_queue_item_t {
    BuildingQueueItemType type;
    union {
        UnitType unit_type;
    };
};

enum BuildingMode {
    BUILDING_MODE_IN_PROGRESS,
    BUILDING_MODE_FINISHED,
    BUILDING_MODE_DESTROYED
};

struct building_t {
    uint8_t player_id;
    BuildingType type;
    int health;

    xy cell;
    BuildingMode mode;

    std::vector<building_queue_item_t> queue;
    uint32_t queue_timer;
    xy rally_point;

    uint32_t taking_damage_timer;
    uint32_t taking_damage_flicker_timer;
    bool taking_damage_flicker;
};

struct remembered_building_t {
    uint8_t player_id;
    BuildingType type;
    int health;
    xy cell;
    BuildingMode mode;
};

struct building_data_t {
    const char* name;
    int cell_size;
    uint32_t cost;
    int max_health;
    int builder_positions_x[3];
    int builder_positions_y[3];
    int builder_flip_h[3];
    bool can_rally;
};

// More UI

enum UiButtonRequirementsType {
    UI_BUTTON_REQUIRES_BUILDING
};

struct ui_button_requirements_t {
    UiButtonRequirementsType type;
    union {
        BuildingType building_type;
    };
};

enum SelectionModeType {
    SELECTION_MODE_NONE,
    SELECTION_MODE_UNIT,
    SELECTION_MODE_BUILDING
};

struct selection_mode_t {
    SelectionModeType type;
    uint32_t count;
    union {
        UnitType unit_type;
        BuildingType building_type;
    };
};

// Input

enum InputType {
    INPUT_NONE,
    INPUT_MOVE,
    INPUT_MOVE_UNIT,
    INPUT_MOVE_BUILDING,
    INPUT_MOVE_MINE,
    INPUT_BLIND_MOVE,
    INPUT_ATTACK_MOVE,
    INPUT_STOP,
    INPUT_BUILD,
    INPUT_BUILD_CANCEL,
    INPUT_BUILDING_ENQUEUE,
    INPUT_BUILDING_DEQUEUE,
    INPUT_UNLOAD_ALL,
    INPUT_RALLY
};

struct input_move_t {
    xy target_cell;
    entity_id target_entity_id;
    uint16_t unit_count;
    entity_id unit_ids[MAX_UNITS];
};

struct input_stop_t {
    uint8_t unit_count;
    entity_id unit_ids[MAX_UNITS];
};

struct input_build_t {
    uint16_t unit_count;
    entity_id unit_ids[MAX_UNITS];
    uint8_t building_type;
    xy target_cell;
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

struct input_unload_all_t {
    uint16_t unit_count;
    entity_id unit_ids[MAX_UNITS];
};

struct input_rally_t {
    entity_id building_id;
    xy rally_point;
};

struct input_t {
    uint8_t type;
    union {
        input_move_t move;
        input_stop_t stop;
        input_build_t build;
        input_build_cancel_t build_cancel;
        input_building_enqueue_t building_enqueue;
        input_building_dequeue_t building_dequeue;
        input_unload_all_t unload_all;
        input_rally_t rally;
    };
};

struct match_state_t {
    // UI
    UiMode ui_mode;
    UiButtonset ui_buttonset;
    xy camera_offset;
    std::string ui_status_message;
    uint32_t ui_status_timer;
    xy select_origin;
    rect_t select_rect;
    selection_t selection;
    BuildingType ui_building_type;
    cell_t ui_move_cell;
    animation_t ui_move_animation;
    animation_t ui_rally_animation;
    xy ui_move_position;
    selection_t control_groups[10];
    uint32_t control_group_double_click_timer;
    uint32_t control_group_double_click_key;
    uint32_t ui_double_click_timer;
    int ui_selected_control_group;
    std::vector<alert_t> alerts;
    std::vector<attack_alert_t> attack_alerts;

    // Inputs
    std::vector<std::vector<input_t>> inputs[MAX_PLAYERS];
    std::vector<input_t> input_queue;
    uint32_t tick_timer;

    // Map
    uint32_t map_width;
    uint32_t map_height;
    int8_t map_lowest_elevation;
    int8_t map_highest_elevation;
    std::vector<tile_t> map_tiles;
    std::vector<decoration_t> map_decorations;
    std::vector<cell_t> map_cells;
    std::vector<FogType> player_fog[MAX_PLAYERS];
    bool is_fog_dirty;

    // Entities
    id_array<unit_t> units;
    id_array<building_t> buildings;
    id_array<mine_t> mines;
    std::unordered_map<entity_id, remembered_building_t> remembered_buildings;
    std::unordered_map<entity_id, mine_t> remembered_mines;

    // Players
    uint32_t player_gold[MAX_PLAYERS];
};

match_state_t match_init();
void match_update(match_state_t& state);

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id);
uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id);

// Input
void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);

// Misc
xy_fixed cell_center(xy cell);
xy get_nearest_free_cell_within_rect(xy start_cell, rect_t rect);
xy get_first_empty_cell_around_rect(const match_state_t& state, xy cell_size, rect_t rect, xy preferred_cell = xy(-1, -1));
xy get_nearest_cell_around_rect(const match_state_t& state, rect_t start, rect_t rect, bool allow_blocked_cells = false);
xy get_exit_cell(const match_state_t& state, rect_t building_rect, xy unit_size, xy rally_cell);
rect_t mine_get_rect(xy mine_cell);
rect_t mine_get_block_building_rect(xy mine_cell);
uint32_t mine_get_worker_count(const match_state_t& state, entity_id mine_id, uint8_t player_id);

// UI
void ui_show_status(match_state_t& state, const char* message);
UiButton ui_get_ui_button(const match_state_t& state, int index);
int ui_get_ui_button_hovered(const match_state_t& state);
const rect_t& ui_get_ui_button_rect(int index);
bool ui_button_requirements_met(const match_state_t& state, UiButton button);
void ui_handle_button_pressed(match_state_t& state, UiButton button);
bool ui_is_mouse_in_ui();
xy ui_get_building_cell(const match_state_t& state);
selection_t ui_create_selection_from_rect(const match_state_t& state);
void ui_set_selection(match_state_t& state, selection_t& selection);
selection_mode_t ui_get_mode_of_selection(const match_state_t& state, const selection_t& selection);
xy ui_alert_get_cell(const match_state_t& state, const alert_t& alert);
entity_id ui_get_nearest_builder(const match_state_t& state, xy cell);
int ui_get_building_queue_index_hovered(const match_state_t& state);

// Map
void map_init(match_state_t& state, uint32_t width, uint32_t height);
bool map_is_cell_in_bounds(const match_state_t& state, xy cell);
bool map_is_cell_rect_in_bounds(const match_state_t& state, rect_t cell_rect);
bool map_is_cell_blocked(const match_state_t& state, xy cell);
bool map_is_cell_rect_occupied(const match_state_t& state, rect_t cell_rect, xy origin = xy(-1, -1), bool ignore_miners = false);
bool map_is_cell_rect_same_elevation(const match_state_t& state, rect_t cell_rect);
bool map_can_cell_rect_step_in_direction(const match_state_t& state, rect_t from, int direction);
cell_t map_get_cell(const match_state_t& state, xy cell);
void map_set_cell(match_state_t& state, xy cell, CellType type, uint16_t value = 0);
void map_set_cell_rect(match_state_t& state, rect_t cell_rect, CellType type, uint16_t id = 0);
FogType map_get_fog(const match_state_t& state, uint8_t player_id, xy cell);
bool map_is_cell_rect_revealed(const match_state_t& state, uint8_t player_id, rect_t rect);
void map_fog_reveal_at_cell(match_state_t& state, uint8_t player_id, xy cell, xy size, int sight);
int8_t map_get_elevation(const match_state_t& state, xy cell);
bool map_is_cell_blocked_in_direction(const match_state_t& state, xy cell, int direction);
void map_pathfind(const match_state_t& state, xy from, xy to, xy cell_size, std::vector<xy>* path, bool should_ignore_miners);
void map_fog_reveal(match_state_t& state, uint8_t player_id);
void map_update_fog(match_state_t& state, uint8_t player_id);

// Unit
entity_id unit_create(match_state_t& state, uint8_t player_id, UnitType type, const xy& cell);
void unit_destroy(match_state_t& state, uint32_t unit_index);
void unit_update(match_state_t& state, uint32_t unit_index);
xy unit_cell_size(UnitType type);
rect_t unit_get_rect(const unit_t& unit);
int8_t unit_get_elevation(const match_state_t& state, const unit_t& unit);
void unit_set_target(const match_state_t& state, unit_t& unit, unit_target_t target);
xy unit_get_target_cell(const match_state_t& state, const unit_t& unit);
xy_fixed unit_get_target_position(UnitType type, xy cell);
bool unit_has_reached_target(const match_state_t& state, const unit_t& unit);
bool unit_is_target_dead_or_ferried(const match_state_t& state, const unit_t& unit);
bool unit_can_see_rect(const unit_t& unit, rect_t rect);
int unit_get_damage(const match_state_t& state, const unit_t& unit);
int unit_get_armor(const match_state_t& state, const unit_t& unit);
AnimationName unit_get_expected_animation(const unit_t& unit);
int unit_get_animation_vframe(const unit_t& unit);
bool unit_sprite_should_flip_h(const unit_t& unit);
Sprite unit_get_select_ring(UnitType type, bool is_enemy);
void unit_stop_building(match_state_t& state, entity_id unit_id);
unit_target_t unit_target_nearest_camp(const match_state_t& state, xy unit_cell, uint8_t unit_player_id);
unit_target_t unit_target_nearest_mine(const match_state_t& state, const unit_t& unit);
unit_target_t unit_target_nearest_insight_enemy(const match_state_t& state, const unit_t& unit);
xy unit_get_best_unload_cell(const match_state_t& state, const unit_t& unit, xy cell_size);

// Building
entity_id building_create(match_state_t& state, uint8_t player_id, BuildingType type, xy cell);
void building_destroy(match_state_t& state, uint32_t building_index);
void building_on_finish(match_state_t& state, entity_id building_id);
void building_update(match_state_t& state, building_t& building);
void building_enqueue(match_state_t& state, building_t& building, building_queue_item_t item);
void building_dequeue(match_state_t& state, building_t& building);
bool building_is_supply_blocked(const match_state_t& state, const building_t& building);
xy building_cell_size(BuildingType type);
rect_t building_get_rect(const building_t& building);
bool building_is_finished(const building_t& building);
bool building_can_be_placed(const match_state_t& state, BuildingType type, xy cell, xy miner_cell);
Sprite building_get_select_ring(BuildingType type, bool is_enemy);
uint32_t building_queue_item_duration(const building_queue_item_t& item);
UiButton building_queue_item_icon(const building_queue_item_t& item);
uint32_t building_queue_item_cost(const building_queue_item_t& item);
uint32_t building_queue_population_cost(const building_queue_item_t& item);

extern const std::unordered_map<UiButtonset, std::array<UiButton, 6>> UI_BUTTONS;
extern const std::unordered_map<UiButton, ui_button_requirements_t> UI_BUTTON_REQUIREMENTS;
extern const std::unordered_map<uint32_t, unit_data_t> UNIT_DATA;
extern const std::unordered_map<uint32_t, building_data_t> BUILDING_DATA;
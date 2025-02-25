#pragma once

#include "defines.h"
#include "util.h"
#include "id_array.h"
#include "engine_types.h"
#include "engine.h"
#include "animation.h"
#include "options.h"
#include "noise.h"
#include <vector>
#include <array>
#include <string>
#include <unordered_map>

#define BUILDING_QUEUE_MAX 5
#define BUILDING_DEQUEUE_POP_FRONT BUILDING_QUEUE_MAX
#define BUILDING_QUEUE_BLOCKED UINT32_MAX
#define BUILDING_QUEUE_EXIT_BLOCKED UINT32_MAX - 1
#define BUILDING_FADE_DURATION 300
#define ENTITY_UNLOAD_ALL ID_NULL

extern const uint32_t MATCH_TAKING_DAMAGE_TIMER_DURATION;
extern const uint32_t MATCH_TAKING_DAMAGE_FLICKER_DURATION;

// Map

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

struct tile_t {
    uint16_t index;
    uint16_t elevation;
};

struct remembered_entity_t {
    render_sprite_params_t sprite_params;
    xy cell;
    int cell_size;
};

struct map_reveal_t {
    int player_id;
    xy cell;
    int cell_size;
    int sight;
    int timer;
};

// Chat

struct chat_message_t {
    std::string message;
    uint32_t timer;
};

const size_t MATCH_CHAT_MESSAGE_MAX_LENGTH = 64;

// Particles

struct particle_t {
    Sprite sprite;
    animation_t animation;
    int vframe;
    xy position;
};

enum ProjectileType {
    PROJECTILE_SMOKE
};

struct projectile_t {
    ProjectileType type;
    xy_fixed position;
    xy_fixed target;
};

extern const int PARTICLE_SMOKE_CELL_SIZE;

// Upgrades

const uint32_t UPGRADE_WAR_WAGON = 1;
const uint32_t UPGRADE_EXPLOSIVES = 2;
const uint32_t UPGRADE_BAYONETS = 4;
const uint32_t UPGRADE_SMOKE = 8;

struct upgrade_data_t {
    const char* name;
    UiButton ui_button;
    uint32_t gold_cost;
    uint32_t research_duration;
};

extern const std::unordered_map<uint32_t, upgrade_data_t> UPGRADE_DATA;

// Entity

// If you change this, make sure that entity_is_unit() and entity_is_building() still work
enum EntityType {
    ENTITY_MINER,
    ENTITY_COWBOY,
    ENTITY_BANDIT,
    ENTITY_WAGON,
    ENTITY_WAR_WAGON,
    ENTITY_JOCKEY,
    ENTITY_SAPPER,
    ENTITY_TINKER,
    ENTITY_SOLDIER,
    ENTITY_CANNON,
    ENTITY_SPY,
    ENTITY_HALL,
    ENTITY_CAMP,
    ENTITY_HOUSE,
    ENTITY_SALOON,
    ENTITY_BUNKER,
    ENTITY_COOP,
    ENTITY_SMITH,
    ENTITY_BARRACKS,
    ENTITY_SHERIFFS,
    ENTITY_LAND_MINE,
    ENTITY_GOLD_MINE
};

enum EntityMode {
    MODE_UNIT_IDLE,
    MODE_UNIT_MOVE,
    MODE_UNIT_MOVE_BLOCKED,
    MODE_UNIT_MOVE_FINISHED,
    MODE_UNIT_BUILD,
    MODE_UNIT_REPAIR,
    MODE_UNIT_ATTACK_WINDUP,
    MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP,
    MODE_UNIT_IN_MINE,
    MODE_UNIT_OUT_MINE,
    MODE_UNIT_TINKER_THROW,
    MODE_UNIT_DEATH,
    MODE_UNIT_DEATH_FADE,
    MODE_BUILDING_IN_PROGRESS,
    MODE_BUILDING_FINISHED,
    MODE_BUILDING_DESTROYED,
    MODE_MINE_ARM,
    MODE_MINE_PRIME,
    MODE_GOLD_MINE,
    MODE_GOLD_MINE_COLLAPSED
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
    TARGET_SMOKE,
    TARGET_BUILD,
    TARGET_BUILD_ASSIST
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
const uint32_t ENTITY_FLAG_INVISIBLE = 4;

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
    std::vector<target_t> target_queue;
    std::vector<xy> path;
    std::vector<building_queue_item_t> queue;
    xy rally_point;
    uint32_t pathfind_attempts;
    uint32_t timer;

    animation_t animation;

    std::vector<entity_id> garrisoned_units;
    entity_id garrison_id;
    uint32_t cooldown_timer;

    uint32_t gold_held;
    entity_id gold_mine_id;

    uint32_t taking_damage_counter;
    uint32_t taking_damage_timer;
    uint32_t health_regen_timer;
};

struct unit_data_t {
    uint32_t population_cost;
    fixed speed;

    int damage;
    int attack_cooldown;
    int range_squared;
    int min_range_squared;
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

    bool has_detection;

    union {
        unit_data_t unit_data;
        building_data_t building_data;
    };
};
extern const std::unordered_map<EntityType, entity_data_t> ENTITY_DATA;

// Match Event

/**
 * These are events that bubble up to the UI
 * They represent things that we would like the UI to do, 
 * but the UI may or may not do them depending on its own state
 */

enum MatchEventType {
    MATCH_EVENT_SOUND,
    MATCH_EVENT_ALERT,
    MATCH_EVENT_SELECTION_HANDOFF,
    MATCH_EVENT_RESEARCH_COMPLETE,
    MATCH_EVENT_SMOKE_COOLDOWN,
    MATCH_EVENT_CANT_BUILD,
    MATCH_EVENT_BUILDING_EXIT_BLOCKED,
    MATCH_EVENT_MINE_EXIT_BLOCKED,
    MATCH_EVENT_NOT_ENOUGH_GOLD,
    MATCH_EVENT_NOT_ENOUGH_HOUSE
};

struct match_event_sound_t {
    xy position;
    Sound sound;
};

enum MatchAlertType {
    MATCH_ALERT_TYPE_BUILDING,
    MATCH_ALERT_TYPE_UNIT,
    MATCH_ALERT_TYPE_RESEARCH,
    MATCH_ALERT_TYPE_ATTACK
};

struct match_event_alert_t {
    MatchAlertType type;
    uint8_t player_id;
    xy cell;
    int cell_size;
};

struct match_event_research_complete_t {
    uint32_t upgrade;
    uint8_t player_id;
};

struct match_event_selection_handoff_t {
    uint8_t player_id;
    entity_id to_deselect;
    entity_id to_select;
};

struct match_event_t {
    MatchEventType type;
    union {
        match_event_sound_t sound;
        match_event_alert_t alert;
        match_event_selection_handoff_t selection_handoff;
        match_event_research_complete_t research_complete;
        uint8_t player_id;
    };
};

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
    INPUT_MOVE_SMOKE,
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
    uint8_t shift_command;
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
    uint8_t shift_command;
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

struct input_chat_t {
    uint8_t message_length;
    char message[MATCH_CHAT_MESSAGE_MAX_LENGTH];
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
        input_chat_t chat;
    };
};

struct match_state_t {
    // Map
    uint32_t map_width;
    uint32_t map_height;
    std::vector<tile_t> map_tiles;
    std::vector<entity_id> map_cells;
    std::vector<entity_id> map_mine_cells;
    std::vector<int> map_fog[MAX_PLAYERS];
    std::vector<int> map_detection[MAX_PLAYERS];
    std::unordered_map<entity_id, remembered_entity_t> remembered_entities[MAX_PLAYERS];
    bool map_is_fog_dirty;
    std::vector<map_reveal_t> map_reveals;

    xy map_player_spawns[MAX_PLAYERS];

    // Entities
    id_array<entity_t, 400 * MAX_PLAYERS> entities;
    std::vector<particle_t> particles;
    std::vector<projectile_t> projectiles;

    // Player
    uint32_t player_gold[MAX_PLAYERS];
    uint32_t player_upgrades[MAX_PLAYERS];
    uint32_t player_upgrades_in_progress[MAX_PLAYERS];

    std::vector<chat_message_t> chat;

    std::vector<match_event_t> events;
};

match_state_t match_init(int32_t lcg_seed, const noise_t& noise);

// Input

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(const uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);

// Update

void match_update(match_state_t& state);

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id);
uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id);
bool match_player_has_upgrade(const match_state_t& state, uint8_t player_id, uint32_t upgrade);
bool match_player_upgrade_is_available(const match_state_t& state, uint8_t player_id, uint32_t upgrade);
void match_grant_player_upgrade(match_state_t& state, uint8_t player_id, uint32_t upgrade);
entity_id match_get_nearest_builder(const match_state_t& state, const std::vector<entity_id>& builders, xy cell);
uint32_t match_get_miners_on_gold_mine(const match_state_t& state, uint8_t player_id, entity_id gold_mine_id);
void match_add_chat_message(match_state_t& state, std::string message);

// Event

void match_event_play_sound(match_state_t& state, Sound sound, xy position);
void match_event_alert(match_state_t& state, MatchAlertType type, uint8_t player_id, xy cell, int cell_size);

// Map

void map_init(match_state_t& state, const noise_t& noise);

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
xy map_get_nearest_cell_around_rect(const match_state_t& state, xy start, int start_size, xy rect_position, int rect_size, bool allow_blocked_cells, xy ignore_cell = xy(-1, -1));
bool map_is_tile_ramp(const match_state_t& state, xy cell);
bool map_is_cell_rect_same_elevation(const match_state_t& state, xy cell, xy size);
void map_pathfind(const match_state_t& state, xy from, xy to, int cell_size, std::vector<xy>* path, bool gold_walk, std::vector<xy>* ignored_cells = NULL);
bool map_is_cell_rect_revealed(const match_state_t& state, uint8_t player_id, xy cell, int cell_size);
void map_fog_update(match_state_t& state, uint8_t player_id, xy cell, int cell_size, int sight, bool increment, bool has_detection);

// Entity

entity_id entity_create(match_state_t& state, EntityType type, uint8_t player_id, xy cell);
entity_id entity_create_gold_mine(match_state_t& state, xy cell, uint32_t gold_left);

void entity_update(match_state_t& state, uint32_t entity_index);

bool entity_is_unit(EntityType entity);
bool entity_is_building(EntityType type);
int entity_cell_size(EntityType entity);
SDL_Rect entity_get_rect(const entity_t& entity);
xy entity_get_center_position(const entity_t& entity);
Sprite entity_get_sprite(const entity_t& entity);
Sprite entity_get_select_ring(const entity_t& entity, bool is_ally);
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
Sound entity_get_attack_sound(const entity_t& entity);
Sound entity_get_death_sound(EntityType type);
bool entity_should_die(const entity_t& entity);
SDL_Rect entity_get_sight_rect(const entity_t& entity);
bool entity_can_see_rect(const entity_t& entity, xy rect_position, int rect_size);

target_t entity_target_nearest_enemy(const match_state_t& state, const entity_t& entity);
target_t entity_target_nearest_gold_mine(const match_state_t& state, const entity_t& entity);
target_t entity_target_nearest_camp(const match_state_t& state, const entity_t& entity);
bool entity_is_mining(const match_state_t& state, const entity_t& entity);

SDL_Rect entity_gold_get_block_building_rect(xy cell);
uint32_t entity_get_garrisoned_occupancy(const match_state_t& state, const entity_t& entity);
void entity_on_death_release_garrisoned_units(match_state_t& state, entity_t& entity);

void entity_set_target(entity_t& entity, target_t target);
void entity_attack_target(match_state_t& state, entity_id attacker_id, entity_t& defender);
void entity_on_attack(match_state_t& state, entity_id attacker_id, entity_t& defender);
void entity_explode(match_state_t& state, entity_id id);
xy entity_get_exit_cell(const match_state_t& state, xy building_cell, int building_size, int unit_size, xy rally_cell, bool allow_blocked_cells = false);
void entity_remove_garrisoned_unit(entity_t& entity, entity_id garrisoned_unit_id);
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
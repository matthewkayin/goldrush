#pragma once

#include "defines.h"
#include "util.h"
#include "id_array.h"
#include "engine.h"
#include "animation.h"
#include "options.h"
#include "types.h"
#include "map.h"
#include <vector>
#include <array>
#include <string>
#include <unordered_map>

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
    target_t remembered_gold_target;
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
    uint32_t gold_patch_id;

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
    map_t map;

    // Entities
    id_array<entity_t, 400 * MAX_PLAYERS> entities;
    std::vector<particle_t> particles;
    std::vector<projectile_t> projectiles;

    // Player
    uint32_t player_gold[MAX_PLAYERS];
    uint32_t player_upgrades[MAX_PLAYERS];
    uint32_t player_upgrades_in_progress[MAX_PLAYERS];

    std::vector<chat_message_t> chat;
};

match_state_t match_init(const std::vector<xy>& player_spawns);

// Input

void match_input_serialize(uint8_t* out_buffer, size_t& out_buffer_length, const input_t& input);
input_t match_input_deserialize(uint8_t* in_buffer, size_t& in_buffer_head);
void match_input_handle(match_state_t& state, uint8_t player_id, const input_t& input);

// Update

void match_update(match_state_t& state);

uint32_t match_get_player_population(const match_state_t& state, uint8_t player_id);
uint32_t match_get_player_max_population(const match_state_t& state, uint8_t player_id);
bool match_player_has_upgrade(const match_state_t& state, uint8_t player_id, uint32_t upgrade);
bool match_player_upgrade_is_available(const match_state_t& state, uint8_t player_id, uint32_t upgrade);
void match_grant_player_upgrade(match_state_t& state, uint8_t player_id, uint32_t upgrade);

void match_camera_clamp(match_state_t& state);
void match_camera_center_on_cell(match_state_t& state, xy cell);
xy match_get_mouse_world_pos(const match_state_t& state);
input_t match_create_move_input(const match_state_t& state);
void match_add_chat_message(match_state_t& state, std::string message);

// Render

void match_render(const match_state_t& state);
render_sprite_params_t match_create_entity_render_params(const match_state_t& state, const entity_t& entity);
void match_render_healthbar(xy position, xy size, int health, int max_health);
void match_render_garrisoned_units_healthbar(xy position, xy size, int garrisoned_size, int garrisoned_capacity);
void match_render_target_build(const match_state_t& state, const target_t& target);
void match_play_sound_at(match_state_t& state, Sound sound, xy position);

// Entity

entity_id entity_create(match_state_t& state, EntityType type, uint8_t player_id, xy cell);
entity_id entity_create_gold(match_state_t& state, xy cell, uint32_t gold_left, uint32_t gold_patch_id);

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
target_t entity_target_gold(const match_state_t& state, const entity_t& entity, entity_id gold_id);
target_t entity_target_nearest_enemy(const match_state_t& state, const entity_t& entity);
target_t entity_target_nearest_gold(const match_state_t& state, uint8_t entity_player_id, xy start_cell, uint32_t gold_patch_id);
target_t entity_target_nearest_camp(const match_state_t& state, const entity_t& entity);
bool entity_should_gold_walk(const match_state_t& state, const entity_t& entity);
SDL_Rect entity_gold_get_block_building_rect(xy cell);
uint32_t entity_get_garrisoned_occupancy(const match_state_t& state, const entity_t& entity);
void entity_on_death_release_garrisoned_units(match_state_t& state, entity_t& entity);

void entity_set_target(entity_t& entity, target_t target);
void entity_attack_target(match_state_t& state, entity_id attacker_id, entity_t& defender);
void entity_on_attack(match_state_t& state, entity_id attacker_id, entity_t& defender);
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
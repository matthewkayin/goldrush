#pragma once

#include "defines.h"
#include "util.h"
#include "id_array.h"
#include "engine.h"
#include "animation.h"
#include "options.h"
#include "entity_types.h"
#include <SDL2/SDL.h>
#include <queue>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>

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
    std::vector<chat_message_t> chat;

    // Map
    uint32_t map_width;
    uint32_t map_height;
    std::vector<tile_t> map_tiles;
    std::vector<entity_id> map_cells;
    std::vector<entity_id> map_mine_cells;
    std::vector<int> map_fog[MAX_PLAYERS];
    std::vector<int> map_detection[MAX_PLAYERS];
    std::unordered_map<entity_id, remembered_entity_t> remembered_entities[MAX_PLAYERS];
    std::vector<map_reveal_t> map_reveals;
    bool map_is_fog_dirty;

    // Entities
    id_array<entity_t, 400 * MAX_PLAYERS> entities;
    std::vector<particle_t> particles;
    std::vector<projectile_t> projectiles;

    uint32_t player_gold[MAX_PLAYERS];
    uint32_t player_upgrades[MAX_PLAYERS];
    uint32_t player_upgrades_in_progress[MAX_PLAYERS];
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
void map_fog_update(match_state_t& state, uint8_t player_id, xy cell, int cell_size, int sight, bool increment, bool has_detection);
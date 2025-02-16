#pragma once

#include "defines.h"
#include "animation.h"
#include "engine_types.h"
#include "util.h"
#include <vector>

#define BUILDING_QUEUE_MAX 5
#define BUILDING_DEQUEUE_POP_FRONT BUILDING_QUEUE_MAX
#define BUILDING_QUEUE_BLOCKED UINT32_MAX
#define BUILDING_QUEUE_EXIT_BLOCKED UINT32_MAX - 1
#define BUILDING_FADE_DURATION 300
#define GOLD_PATCH_ID_NULL UINT32_MAX
#define ENTITY_UNLOAD_ALL ID_NULL

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
    ENTITY_MINE,
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
    MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP,
    MODE_UNIT_MINE,
    MODE_UNIT_TINKER_THROW,
    MODE_UNIT_DEATH,
    MODE_UNIT_DEATH_FADE,
    MODE_BUILDING_IN_PROGRESS,
    MODE_BUILDING_FINISHED,
    MODE_BUILDING_DESTROYED,
    MODE_MINE_ARM,
    MODE_MINE_PRIME,
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
    TARGET_SMOKE,
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

struct remembered_entity_t {
    render_sprite_params_t sprite_params;
    xy cell;
    int cell_size;
};
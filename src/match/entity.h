#pragma once

#include "map.h"
#include "math/gmath.h"
#include "render/sprite.h"
#include "core/animation.h"
#include "core/sound.h"

#define BUILDING_QUEUE_MAX 5
#define BUILDING_DEQUEUE_POP_FRONT BUILDING_QUEUE_MAX
#define BUILDING_QUEUE_BLOCKED UINT32_MAX
#define BUILDING_QUEUE_EXIT_BLOCKED UINT32_MAX - 1

const uint32_t ENTITY_FLAG_HOLD_POSITION = 1;
const uint32_t ENTITY_FLAG_DAMAGE_FLICKER = 2;
const uint32_t ENTITY_FLAG_INVISIBLE = 1 << 2;
const uint32_t ENTITY_FLAG_CHARGED = 1 << 3;
const uint32_t ENTITY_FLAG_ON_FIRE = 1 << 4;
const uint32_t ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY = 1 << 5;

const uint32_t ENTITY_CANNOT_GARRISON = 0;

const int ENTITY_SKY_POSITION_Y_OFFSET = -16;
const uint32_t ENTITY_BALLOON_DEATH_DURATION = 50;
const uint32_t UNIT_TAKING_DAMAGE_FLICKER_DURATION = 10;
const uint32_t UNIT_HEALTH_REGEN_DURATION = 64;
const uint32_t UNIT_HEALTH_REGEN_DELAY = 600;

enum EntityType {
    ENTITY_GOLDMINE,
    ENTITY_MINER,
    ENTITY_COWBOY,
    ENTITY_BANDIT,
    ENTITY_WAGON,
    ENTITY_JOCKEY,
    ENTITY_SAPPER,
    ENTITY_PYRO,
    ENTITY_SOLDIER,
    ENTITY_CANNON,
    ENTITY_DETECTIVE,
    ENTITY_BALLOON,
    ENTITY_HALL,
    ENTITY_HOUSE,
    ENTITY_SALOON,
    ENTITY_BUNKER,
    ENTITY_WORKSHOP,
    ENTITY_SMITH,
    ENTITY_COOP,
    ENTITY_BARRACKS,
    ENTITY_SHERIFFS,
    ENTITY_LANDMINE,
    ENTITY_TYPE_COUNT
};

enum EntityMode {
    MODE_UNIT_IDLE,
    MODE_UNIT_BLOCKED,
    MODE_UNIT_MOVE,
    MODE_UNIT_MOVE_FINISHED,
    MODE_UNIT_BUILD,
    MODE_UNIT_BUILD_ASSIST,
    MODE_UNIT_REPAIR,
    MODE_UNIT_ATTACK_WINDUP,
    MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP,
    MODE_UNIT_SOLDIER_CHARGE,
    MODE_UNIT_IN_MINE,
    MODE_UNIT_PYRO_THROW,
    MODE_COWBOY_FAN_HAMMER,
    MODE_UNIT_DEATH,
    MODE_UNIT_DEATH_FADE,
    MODE_UNIT_BALLOON_DEATH_START,
    MODE_UNIT_BALLOON_DEATH,
    MODE_BUILDING_IN_PROGRESS,
    MODE_BUILDING_FINISHED,
    MODE_BUILDING_DESTROYED,
    MODE_MINE_ARM,
    MODE_MINE_PRIME,
    MODE_GOLDMINE,
    MODE_GOLDMINE_COLLAPSED
};

enum TargetType {
    TARGET_NONE,
    TARGET_CELL,
    TARGET_ENTITY,
    TARGET_ATTACK_CELL,
    TARGET_ATTACK_ENTITY,
    TARGET_REPAIR,
    TARGET_UNLOAD,
    TARGET_MOLOTOV,
    TARGET_BUILD,
    TARGET_BUILD_ASSIST
};

struct TargetBuild {
    ivec2 unit_cell;
    ivec2 building_cell;
    EntityType building_type;
};

struct Target {
    TargetType type;
    EntityId id;
    ivec2 cell;
    union {
        TargetBuild build;
    };
};

enum BuildingQueueItemType {
    BUILDING_QUEUE_ITEM_UNIT,
    BUILDING_QUEUE_ITEM_UPGRADE
};

struct BuildingQueueItem {
    BuildingQueueItemType type;
    union {
        EntityType unit_type;
        uint32_t upgrade;
    };
};

struct Entity {
    EntityType type;
    EntityMode mode;
    uint8_t player_id;
    uint32_t flags;

    ivec2 cell;
    fvec2 position;
    Direction direction;

    int health;
    uint32_t energy;
    Target target;
    std::vector<Target> target_queue;
    std::vector<ivec2> path;
    std::vector<BuildingQueueItem> queue;
    ivec2 rally_point;
    uint32_t pathfind_attempts;
    uint32_t timer;

    Animation animation;

    std::vector<EntityId> garrisoned_units;
    EntityId garrison_id;
    uint32_t cooldown_timer;

    uint32_t gold_held;
    EntityId goldmine_id;

    uint32_t taking_damage_counter;
    uint32_t taking_damage_timer;
    uint32_t health_regen_timer;
    uint32_t fire_damage_timer;
    uint32_t energy_regen_timer;
    uint32_t fan_hammer_timer;
    uint32_t bleed_timer;
    uint32_t bleed_damage_timer;
    Animation bleed_animation;
};

struct EntityDataUnit {
    uint32_t population_cost;
    fixed speed;
    uint32_t max_energy;

    SoundName attack_sound;
    int damage;
    int accuracy;
    int evasion;
    int attack_cooldown;
    int range_squared;
    int min_range_squared;
};

struct EntityDataBuilding {
    int builder_positions_x[3];
    int builder_positions_y[3];
    int builder_flip_h[3];
    uint32_t options;
};

const uint32_t BUILDING_CAN_RALLY = 1;
const uint32_t BUILDING_COSTS_ENERGY = 2;

struct EntityData {
    const char* name;
    SpriteName sprite;
    SpriteName icon;
    SoundName death_sound;
    CellLayer cell_layer;
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
        EntityDataUnit unit_data;
        EntityDataBuilding building_data;
    };
};

const EntityData& entity_get_data(EntityType type);
bool entity_is_unit(EntityType type);
bool entity_is_building(EntityType type);
bool entity_is_selectable(const Entity& entity);
uint32_t entity_get_elevation(const Entity& entity, const Map& map);
Rect entity_get_rect(const Entity& entity);
fvec2 entity_get_target_position(const Entity& entity);
void entity_set_target(Entity& entity, Target target);
bool entity_check_flag(const Entity& entity, uint32_t flag);
void entity_set_flag(Entity& entity, uint32_t flag, bool value);
AnimationName entity_get_expected_animation(const Entity& entity);
ivec2 entity_get_animation_frame(const Entity& entity);
Rect entity_goldmine_get_block_building_rect(ivec2 cell);
bool entity_should_die(const Entity& entity);
void entity_on_damage_taken(Entity& entity);
bool entity_is_target_within_min_range(const Entity& entity, const Entity& target);

// Building queue items

uint32_t building_queue_item_duration(const BuildingQueueItem& item);
uint32_t building_queue_item_cost(const BuildingQueueItem& item);
uint32_t building_queue_population_cost(const BuildingQueueItem& item);
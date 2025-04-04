#pragma once

#include "map.h"
#include "math/gmath.h"
#include "render/sprite.h"
#include "core/animation.h"
#include "core/sound.h"

const uint32_t ENTITY_FLAG_HOLD_POSITION = 1;
const uint32_t ENTITY_FLAG_DAMAGE_FLICKER = 2;
const uint32_t ENTITY_FLAG_INVISIBLE = 4;
const uint32_t ENTITY_FLAG_CHARGED = 8;

const uint32_t ENTITY_CANNOT_GARRISON = 0;

enum EntityType {
    ENTITY_GOLDMINE,
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
    ENTITY_DETECTIVE,
    ENTITY_HALL,
    ENTITY_HOUSE,
    ENTITY_SALOON,
    ENTITY_BUNKER,
    ENTITY_COOP,
    ENTITY_SMITH,
    ENTITY_BARRACKS,
    ENTITY_SHERIFFS,
    ENTITY_LANDMINE
};

enum EntityMode {
    MODE_UNIT_IDLE,
    MODE_UNIT_BLOCKED,
    MODE_UNIT_MOVE,
    MODE_UNIT_MOVE_FINISHED,
    MODE_UNIT_BUILD,
    MODE_UNIT_REPAIR,
    MODE_UNIT_ATTACK_WINDUP,
    MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP,
    MODE_SOLDIER_CHARGE,
    MODE_UNIT_IN_MINE,
    MODE_UNIT_TINKER_THROW,
    MODE_UNIT_DEATH,
    MODE_UNIT_DEATH_FADE,
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
    TARGET_SMOKE,
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
    EntityId gold_mine_id;

    uint32_t taking_damage_counter;
    uint32_t taking_damage_timer;
    uint32_t health_regen_timer;
};

struct EntityDataUnit {
    uint32_t population_cost;
    fixed speed;

    int damage;
    int attack_cooldown;
    int range_squared;
    int min_range_squared;
};

struct EntityDataBuilding {
    int builder_positions_x[3];
    int builder_positions_y[3];
    int builder_flip_h[3];
    bool can_rally;
};

struct EntityData {
    const char* name;
    SpriteName sprite;
    SpriteName icon;
    SoundName death_sound;
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
uint16_t entity_get_elevation(const Entity& entity, const Map& map);
Rect entity_get_rect(const Entity& entity);
fvec2 entity_get_target_position(const Entity& entity);
void entity_set_target(Entity& entity, Target target);
bool entity_check_flag(const Entity& entity, uint32_t flag);
void entity_set_flag(Entity& entity, uint32_t flag, bool value);
SpriteName entity_get_sprite(const Entity& entity);
AnimationName entity_get_expected_animation(const Entity& entity);
ivec2 entity_get_animation_frame(const Entity& entity);
Rect entity_goldmine_get_block_building_rect(ivec2 cell);
bool entity_should_die(const Entity& entity);

// Building queue items

uint32_t building_queue_item_duration(const BuildingQueueItem& item);
uint32_t building_queue_item_cost(const BuildingQueueItem& item);
uint32_t building_queue_population_cost(const BuildingQueueItem& item);
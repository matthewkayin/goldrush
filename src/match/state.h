#pragma once

#include "defines.h"
#include "match/entity.h"
#include "match/input.h"
#include "core/animation.h"
#include "core/input.h"
#include "container/id_array.h"
#include "container/fixed_queue.h"
#include "container/fixed_vector.h"
#include "container/circular_vector.h"
#include "container/pool.h"
#include <vector>
#include <unordered_map>
#include <functional>

#define BUILDING_QUEUE_MAX 5
#define BUILDING_DEQUEUE_POP_FRONT BUILDING_QUEUE_MAX
#define BUILDING_QUEUE_BLOCKED UINT32_MAX
#define BUILDING_QUEUE_EXIT_BLOCKED UINT32_MAX - 1

#define MATCH_UI_STATUS_CANT_BUILD "You can't build there."
#define MATCH_UI_STATUS_NOT_ENOUGH_GOLD "Not enough gold."
#define MATCH_UI_STATUS_NOT_ENOUGH_HOUSE "Not enough houses."
#define MATCH_UI_STATUS_BUILDING_QUEUE_FULL "Building queue is full."
#define MATCH_UI_STATUS_MINE_COLLAPSED "Your gold mine collapsed!"
#define MATCH_UI_STATUS_MINE_RUNNING_LOW "Your gold mine is running low."
#define MATCH_UI_STATUS_UNDER_ATTACK "You're under attack!"
#define MATCH_UI_STATUS_ALLY_UNDER_ATTACK "Your ally is under attack!"
#define MATCH_UI_STATUS_MINE_EXIT_BLOCKED "Mine exit is blocked."
#define MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED "Building exit is blocked."
#define MATCH_UI_STATUS_REPAIR_TARGET_INVALID "Must target an allied building."
#define MATCH_UI_STATUS_SMOKE_COOLDOWN "Smoke bomb is on cooldown."
#define MATCH_UI_STATUS_NOT_ENOUGH_ENERGY "Not enough energy."
#define MATCH_UI_STATUS_COMMAND_QUEUE_IS_FULL "Command queue is full."
#define MATCH_UI_STATUS_ENTITY_LIMIT_REACHED "Cannot create building. Entity limit reached."

#define MATCH_MAX_ENTITIES (200U * MAX_PLAYERS)
#define MATCH_MAX_REMEMBERED_ENTITIES 64
#define ENTITY_UNLOAD_ALL ID_NULL
#define MATCH_MAX_MINERS_ON_GOLD 8
#define MATCH_MAX_POPULATION 100U
#define MATCH_MAX_PARTICLES 256U
#define MATCH_MAX_PROJECTILES 32U
#define MATCH_MAX_FIRES 256U
#define MATCH_MAX_FOG_REVEALS 32U
#define MATCH_MAX_EVENTS 32U
#define MATCH_MAX_UNITS (MATCH_MAX_POPULATION * MAX_PLAYERS)
#define ENTITY_PATH_INDEX_NONE MATCH_MAX_UNITS
#define ENTITY_TARGET_QUEUE_INDEX_NONE MATCH_MAX_UNITS

#define MOLOTOV_ENERGY_COST 40
#define CAMO_ENERGY_COST 30
#define CAMO_ENERGY_PER_SECOND_COST 1

#define GARRISONED_UNITS_MAX 4
#define BUILDING_QUEUE_SIZE 5
#define TARGET_QUEUE_CAPACITY 32

#define MATCH_EVENT_STATUS_MESSAGE_BUFFER_SIZE 63

using EntityList = FixedVector<EntityId, MATCH_MAX_POPULATION>;

const int FOG_HIDDEN = -1;
const int FOG_EXPLORED = 0;

const int SOLDIER_BAYONET_DAMAGE = 5;
const uint32_t BLEED_DAMAGE_RATE = 30;
const uint32_t BLEED_DURATION = 60 * 5;

const int PROJECTILE_MOLOTOV_FIRE_SPREAD = 3;
const int MOLOTOV_RANGE_SQUARED = 9 * 9;
const uint32_t UNIT_BUILD_TICK_DURATION = 6;

const uint32_t ENTITY_FLAG_HOLD_POSITION = 1;
const uint32_t ENTITY_FLAG_DAMAGE_FLICKER = 2;
const uint32_t ENTITY_FLAG_INVISIBLE = 1 << 2;
const uint32_t ENTITY_FLAG_CHARGED = 1 << 3;
const uint32_t ENTITY_FLAG_ON_FIRE = 1 << 4;
const uint32_t ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY = 1 << 5;

const int ENTITY_SKY_POSITION_Y_OFFSET = -16;
const uint32_t ENTITY_BALLOON_DEATH_DURATION = 50;
const uint32_t UNIT_TAKING_DAMAGE_FLICKER_DURATION = 10;
const uint32_t UNIT_HEALTH_REGEN_DURATION = 64;
const uint32_t UNIT_HEALTH_REGEN_DELAY = 600;

const uint32_t MATCH_ENTITY_NOT_REMEMBERED = UINT32_MAX;

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
    MODE_GOLDMINE_COLLAPSED,
    MODE_COUNT
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
    TARGET_BUILD_ASSIST,
    TARGET_PATROL,
    TARGET_TYPE_COUNT
};

struct TargetBuild {
    ivec2 unit_cell;
    ivec2 building_cell;
    EntityType building_type;
};

struct TargetPatrol {
    ivec2 cell_a;
    ivec2 cell_b;
    uint32_t padding;
};

struct Target {
    TargetType type;
    EntityId id;
    uint8_t padding = 0;
    uint8_t padding2 = 0;
    ivec2 cell;
    union {
        TargetBuild build;
        TargetPatrol patrol;
    };
};
using TargetQueue = FixedQueue<Target, TARGET_QUEUE_CAPACITY>;

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
    uint8_t padding[3];
    uint32_t flags;

    ivec2 cell;
    fvec2 position;
    Direction direction;

    int health;
    uint32_t energy;
    uint32_t timer;
    uint32_t energy_regen_timer;
    uint32_t health_regen_timer;

    Animation animation;
    FixedVector<EntityId, GARRISONED_UNITS_MAX> garrisoned_units;
    EntityId garrison_id;

    EntityId goldmine_id;
    uint32_t gold_held;

    Target target;
    uint32_t target_queue_index;

    uint32_t path_index;
    uint32_t pathfind_attempts;

    // This is a FixedVector because players can remove items from the middle of a building queue
    FixedVector<BuildingQueueItem, BUILDING_QUEUE_SIZE> queue;
    ivec2 rally_point;

    uint32_t cooldown_timer;
    ivec2 attack_move_cell;

    uint32_t taking_damage_counter;
    uint32_t taking_damage_timer;
    uint32_t fire_damage_timer;

    uint32_t bleed_timer;
    uint32_t bleed_damage_timer;
    Animation bleed_animation;
};

// Match Player

struct MatchPlayer {
    bool active;
    char name[MAX_USERNAME_LENGTH + 1];
    uint8_t padding = 0;
    uint8_t padding2 = 0;
    uint32_t team;
    int recolor_id;
    uint32_t gold;
    uint32_t gold_mined_total;
    uint32_t upgrades;
    uint32_t upgrades_in_progress;
};

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
    MATCH_EVENT_STATUS,
    MATCH_EVENT_PLAYER_DEFEATED
};

struct MatchEventSound {
    ivec2 position;
    SoundName sound;
};

enum MatchAlertType {
    MATCH_ALERT_TYPE_BUILDING,
    MATCH_ALERT_TYPE_UNIT,
    MATCH_ALERT_TYPE_RESEARCH,
    MATCH_ALERT_TYPE_ATTACK,
    MATCH_ALERT_TYPE_MINE_COLLAPSE,
    MATCH_ALERT_TYPE_MINE_RUNNING_LOW
};

struct MatchEventAlert {
    MatchAlertType type;
    uint8_t player_id;
    ivec2 cell;
    int cell_size;
};

struct MatchEventSelectionHandoff {
    uint8_t player_id;
    EntityId to_deselect;
    EntityId to_select;
};

struct MatchEventResearchComplete {
    uint8_t player_id;
    uint32_t upgrade;
};

struct MatchEventStatus {
    uint8_t player_id;
    char message[MATCH_EVENT_STATUS_MESSAGE_BUFFER_SIZE];
};

struct MatchEventPlayerDefeated {
    uint8_t player_id;
};

struct MatchEvent {
    MatchEventType type;
    union {
        MatchEventSound sound;
        MatchEventAlert alert;
        MatchEventSelectionHandoff selection_handoff;
        MatchEventResearchComplete research_complete;
        MatchEventStatus status;
        MatchEventPlayerDefeated player_defeated;
    };
};

struct RememberedEntity {
    EntityId entity_id;
    uint16_t recolor_id;
    EntityType type;
    ivec2 frame;
    ivec2 cell;
};

// Particles

enum ParticleLayer {
    PARTICLE_LAYER_GROUND,
    PARTICLE_LAYER_SKY
};

struct Particle {
    ParticleLayer layer;
    SpriteName sprite;
    Animation animation;
    int vframe;
    ivec2 position;
};

struct Fire {
    ivec2 cell;
    ivec2 source;
    uint32_t time_to_live;
    Animation animation;
};

// Projectiles

enum ProjectileType {
    PROJECTILE_MOLOTOV
};

struct Projectile {
    ProjectileType type;
    fvec2 position;
    fvec2 target;
};

// Fog

struct FogReveal {
    uint32_t team;
    ivec2 cell;
    int cell_size;
    int sight;
    uint32_t timer;
};

enum MatchInitMapType {
    MATCH_INIT_MAP_FROM_NOISE,
    MATCH_INIT_MAP_FROM_COPY
};

struct MatchInitMapParamsNoise {
    MapType type;
    Noise* noise;
};

struct MatchInitMapParamsCopy {
    const Map* map;
};

struct MatchInitMapParams {
    MatchInitMapType type;
    union {
        MatchInitMapParamsNoise noise;
        MatchInitMapParamsCopy copy;
    };
};

struct MatchState {
    int lcg_seed;
    Map map;
    int fog[MAX_PLAYERS][MAP_SIZE_MAX * MAP_SIZE_MAX];
    int detection[MAX_PLAYERS][MAP_SIZE_MAX * MAP_SIZE_MAX];
    FixedVector<RememberedEntity, MATCH_MAX_REMEMBERED_ENTITIES> remembered_entities[MAX_PLAYERS];

    IdArray<Entity, MATCH_MAX_ENTITIES> entities;
    Pool<MapPath, MATCH_MAX_UNITS> entity_paths;
    Pool<TargetQueue, MATCH_MAX_UNITS> entity_target_queues;

    CircularVector<Particle, MATCH_MAX_PARTICLES> particles;
    CircularVector<Projectile, MATCH_MAX_PROJECTILES> projectiles;
    CircularVector<Fire, MATCH_MAX_FIRES> fires;
    int fire_cells[MAP_SIZE_MAX * MAP_SIZE_MAX];
    CircularVector<FogReveal, MATCH_MAX_FOG_REVEALS> fog_reveals;
    MatchPlayer players[MAX_PLAYERS];
    FixedQueue<MatchEvent, MATCH_MAX_EVENTS> events;
};

struct MatchFindBestEntityParams {
    std::function<bool(const Entity& entity, EntityId entity_id)> filter;
    std::function<bool(const Entity& a, const Entity& b)> compare;
};

void match_init(MatchState& state, int32_t lcg_seed, MatchPlayer players[MAX_PLAYERS], MatchInitMapParams map_params);

void match_spawn_players(MatchState& state, const std::vector<ivec2>& map_spawn_points);
uint32_t match_get_player_population(const MatchState& state, uint8_t player_id);
uint32_t match_get_player_max_population(const MatchState& state, uint8_t player_id);
bool match_player_has_upgrade(const MatchState& state, uint8_t player_id, uint32_t upgrade);
bool match_player_upgrade_is_available(const MatchState& state, uint8_t player_id, uint32_t upgrade);
void match_grant_player_upgrade(MatchState& state, uint8_t player_id, uint32_t upgrade);
uint32_t match_get_miners_on_gold(const MatchState& state, EntityId goldmine_id, uint8_t player_id);
void match_handle_input(MatchState& state, const MatchInput& input);
void match_update(MatchState& state);

EntityId match_find_entity(const MatchState& state, std::function<bool(const Entity& entity, EntityId entity_id)> filter);
EntityId match_find_best_entity(const MatchState& state, const MatchFindBestEntityParams& params);
std::function<bool(const Entity& a, const Entity& b)> match_compare_closest_manhattan_distance_to(ivec2 cell);
EntityList match_find_entities(const MatchState& state, std::function<bool(const Entity& entity, EntityId entity_id)> filter);
EntityId match_get_nearest_builder(const MatchState& state, const std::vector<EntityId>& builders, ivec2 cell);

bool match_is_target_invalid(const MatchState& state, const Target& target, uint8_t player_id);
bool match_player_has_buildings(const MatchState& state, uint8_t player_id);
bool match_player_has_entities(const MatchState& state, uint8_t player_id);

uint32_t match_team_find_remembered_entity_index(const MatchState& state, uint8_t team, EntityId entity_id);
bool match_team_remembers_entity(const MatchState& state, uint8_t team, EntityId entity_id);

// Entity

EntityId entity_create(MatchState& state, EntityType type, ivec2 cell, uint8_t player_id);
EntityId entity_goldmine_create(MatchState& state, ivec2 cell, uint32_t gold_left);
void entity_update(MatchState& state, uint32_t entity_index);

SpriteName entity_get_sprite(const MatchState& state, const Entity& entity);
SpriteName entity_get_icon(const MatchState& state, EntityType type, uint8_t player_id);
fixed entity_get_speed(const MatchState& state, const Entity& entity);
bool entity_has_detection(const MatchState& state, const Entity& entity);
uint32_t entity_get_energy_regen_duration(const MatchState& state, const Entity& entity);
int entity_get_armor(const MatchState& state, const Entity& entity);
int entity_get_range_squared(const MatchState& state, const Entity& entity);
bool entity_is_selectable(const Entity& entity);
bool entity_can_be_given_orders(const MatchState& state, const Entity& entity);
uint32_t entity_get_elevation(const Entity& entity, const Map& map);
Rect entity_get_rect(const Entity& entity);
fvec2 entity_get_target_position(const Entity& entity);

bool entity_check_flag(const Entity& entity, uint32_t flag);
void entity_set_flag(Entity& entity, uint32_t flag, bool value);
AnimationName entity_get_expected_animation(const Entity& entity);
ivec2 entity_get_animation_frame(const Entity& entity);
Rect entity_goldmine_get_block_building_rect(ivec2 cell);

bool entity_is_mining(const MatchState& state, const Entity& entity);
bool entity_is_in_mine(const MatchState& state, const Entity& entity);
bool entity_is_idle_miner(const Entity& entity);
void entity_get_mining_path_to_avoid(const MatchState& state, const Entity& entity, MapPath* mine_exit_path);
bool entity_is_blocker_walking_towards_entity(const MatchState& state, const Entity& entity);
bool entity_is_visible_to_player(const MatchState& state, const Entity& entity, uint8_t player_id);

bool entity_has_path(const Entity& entity);
void entity_pathfind(MatchState& state, Entity& entity, ivec2 to, uint32_t options, const MapPath* ignore_cells);
void entity_path_clear(MatchState& state, Entity& entity);
bool entity_is_path_end_too_far_from_target(const MatchState& state, const Entity& entity);

void entity_set_target(MatchState& state, Entity& entity, Target target);
void entity_target_queue_push(MatchState& state, Entity& entity, Target target);
void entity_target_queue_clear(MatchState& state, Entity& entity);
void entity_refund_target_build(MatchState& state, Entity& entity, const Target& target);
bool entity_is_target_invalid(const MatchState& state, const Entity& entity);
bool entity_has_reached_target(const MatchState& state, const Entity& entity);
bool entity_is_target_in_range(const MatchState& state, const Entity& entity, const Entity& target, TargetType target_type);
ivec2 entity_get_target_cell(const MatchState& state, const Entity& entity);
bool entity_is_target_within_min_range(const Entity& entity, const Entity& target);

Target entity_target_nearest_goldmine(const MatchState& state, const Entity& entity);
Target entity_target_nearest_hall(const MatchState& state, const Entity& entity);
uint32_t entity_get_target_attack_priority(const Entity& entity, const Entity& target);
Target entity_target_nearest_enemy(const MatchState& state, const Entity& entity);

void entity_attack_target(MatchState& state, EntityId attacker_id);
void entity_on_attack(MatchState& state, EntityId attacker_id, Entity& defender);
uint32_t entity_get_garrisoned_occupancy(const MatchState& state, const Entity& entity);
void entity_unload_unit(MatchState& state, Entity& carrier, EntityId garrisoned_unit_id);
void entity_release_garrisoned_units_on_death(MatchState& state, Entity& entity);
void entity_explode(MatchState& state, EntityId entity_id);
bool entity_should_die(const Entity& entity);
void entity_on_damage_taken(Entity& entity);

void entity_stop_building(MatchState& state, EntityId entity_id);
void entity_building_finish(MatchState& state, EntityId building_id);
void entity_building_enqueue(MatchState& state, Entity& building, BuildingQueueItem item);
void entity_building_dequeue(MatchState& state, Entity& building);
bool entity_building_is_supply_blocked(const MatchState& state, const Entity& building);

// Building Queue

uint32_t building_queue_item_duration(const BuildingQueueItem& item);
uint32_t building_queue_item_cost(const BuildingQueueItem& item);
uint32_t building_queue_population_cost(const BuildingQueueItem& item);

// Target

Target target_none();
Target target_cell(ivec2 cell);
Target target_entity(EntityId entity_id);
Target target_attack_cell(ivec2 cell);
Target target_attack_entity(EntityId entity_id);
Target target_repair(EntityId entity_id);
Target target_unload(ivec2 cell);
Target target_molotov(ivec2 cell);
Target target_build(TargetBuild target_build);
Target target_build_assist(EntityId entity_id);
Target target_patrol(ivec2 cell_a, ivec2 cell_b);

// Event

void match_event_play_sound(MatchState& state, SoundName sound, ivec2 position);
void match_event_alert(MatchState& state, MatchAlertType type, uint8_t player_id, ivec2 cell, int cell_size);
void match_event_research_complete(MatchState& state, uint8_t player_id, uint32_t upgrade);
void match_event_selection_handoff(MatchState& state, uint8_t player_id, EntityId to_deselect, EntityId to_select);
void match_event_show_status(MatchState& state, uint8_t player_id, const char* message);
void match_event_player_defeated(MatchState& state, uint8_t player_id);

// Fog

int match_get_fog(const MatchState& state, uint8_t team, ivec2 cell);
bool match_is_cell_rect_revealed(const MatchState& state, uint8_t team, ivec2 cell, int cell_size);
bool match_is_cell_rect_explored(const MatchState& state, uint8_t team, ivec2 cell, int cell_size);
void match_fog_update(MatchState& state, uint32_t team, ivec2 cell, int cell_size, int sight, bool has_detection, CellLayer cell_layer, bool increment);

// Fire

bool match_is_cell_on_fire(const MatchState& state, ivec2 cell);
bool match_is_cell_rect_on_fire(const MatchState& state, ivec2 cell, int cell_size);
void match_set_cell_on_fire(MatchState& state, ivec2 cell, ivec2 source);
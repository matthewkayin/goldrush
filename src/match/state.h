#pragma once

#include "map.h"
#include "noise.h"
#include "entity.h"
#include "container/id_array.h"
#include "match/input.h"
#include "core/input.h"
#include "core/sound.h"
#include <vector>
#include <unordered_map>

#define MATCH_UI_STATUS_CANT_BUILD "You can't build there."
#define MATCH_UI_STATUS_NOT_ENOUGH_GOLD "Not enough gold."
#define MATCH_UI_STATUS_NOT_ENOUGH_HOUSE "Not enough houses."
#define MATCH_UI_STATUS_BUILDING_QUEUE_FULL "Building queue is full."
#define MATCH_UI_STATUS_MINE_COLLAPSED "Your gold mine collapsed!"
#define MATCH_UI_STATUS_UNDER_ATTACK "You're under attack!"
#define MATCH_UI_STATUS_ALLY_UNDER_ATTACK "Your ally is under attack!"
#define MATCH_UI_STATUS_MINE_EXIT_BLOCKED "Mine exit is blocked."
#define MATCH_UI_STATUS_BUILDING_EXIT_BLOCKED "Building exit is blocked."
#define MATCH_UI_STATUS_REPAIR_TARGET_INVALID "Must target an allied building."
#define MATCH_UI_STATUS_SMOKE_COOLDOWN "Smoke bomb is on cooldown."
#define MATCH_UI_STATUS_NOT_ENOUGH_ENERGY "Not enough energy."

#define MATCH_MAX_ENTITIES (400 * MAX_PLAYERS)
#define MATCH_ENTITY_UNLOAD_ALL ID_NULL

#define MOLOTOV_ENERGY_COST 60
#define CAMO_ENERGY_COST 20
#define CAMO_ENERGY_PER_SECOND_COST 1

const int FOG_HIDDEN = -1;
const int FOG_EXPLORED = 0;

struct MatchPlayer {
    bool active;
    char name[MAX_USERNAME_LENGTH + 1];
    uint32_t team;
    int32_t recolor_id;
    uint32_t gold;
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
    MATCH_EVENT_STATUS
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
    MATCH_ALERT_TYPE_MINE_COLLAPSE
};

struct MatchEventAlert {
    MatchAlertType type;
    uint8_t player_id;
    ivec2 cell;
    int cell_size;
};

struct MatchEventResearchComplete {
    uint32_t upgrade;
    uint8_t player_id;
};

struct MatchEventSelectionHandoff {
    uint8_t player_id;
    EntityId to_deselect;
    EntityId to_select;
};

struct MatchEventStatus {
    uint8_t player_id;
    const char* message;
};

struct MatchEvent {
    MatchEventType type;
    union {
        MatchEventSound sound;
        MatchEventAlert alert;
        MatchEventSelectionHandoff selection_handoff;
        MatchEventResearchComplete research_complete;
        MatchEventStatus status;
    };
};

struct RememberedEntity {
    EntityType type;
    ivec2 frame;
    ivec2 cell;
    int cell_size;
    int recolor_id;
};

// Particles

struct Particle {
    SpriteName sprite;
    Animation animation;
    int vframe;
    ivec2 position;
};

enum ParticleLayer {
    PARTICLE_LAYER_GROUND,
    PARTICLE_LAYER_SKY,
    PARTICLE_LAYER_COUNT
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

struct MatchState {
    int32_t lcg_seed;
    Map map;
    std::vector<int> fog[MAX_PLAYERS];
    std::vector<int> detection[MAX_PLAYERS];
    std::unordered_map<EntityId, RememberedEntity> remembered_entities[MAX_PLAYERS];
    bool is_fog_dirty;

    IdArray<Entity, MATCH_MAX_ENTITIES> entities;
    std::vector<Particle> particles[PARTICLE_LAYER_COUNT];
    std::vector<Projectile> projectiles;
    std::vector<Fire> fires;
    std::vector<int> fire_cells;
    std::vector<FogReveal> fog_reveals;
    MatchPlayer players[MAX_PLAYERS];
    std::vector<MatchEvent> events;
};

MatchState match_init(int32_t lcg_seed, Noise& noise, MatchPlayer players[MAX_PLAYERS]);
uint32_t match_get_player_population(const MatchState& state, uint8_t player_id);
uint32_t match_get_player_max_population(const MatchState& state, uint8_t player_id);
bool match_player_has_upgrade(const MatchState& state, uint8_t player_id, uint32_t upgrade);
bool match_player_upgrade_is_available(const MatchState& state, uint8_t player_id, uint32_t upgrade);
void match_grant_player_upgrade(MatchState& state, uint8_t player_id, uint32_t upgrade);
bool match_does_player_meet_hotkey_requirements(const MatchState& state, uint8_t player_id, InputAction hotkey);
void match_handle_input(MatchState& state, const MatchInput& input);
void match_update(MatchState& state);

// Entity

EntityId match_create_entity(MatchState& state, EntityType type, ivec2 cell, uint8_t player_id);
EntityId match_create_goldmine(MatchState& state, ivec2 cell, uint32_t gold_left);
void match_entity_update(MatchState& state, uint32_t entity_index);
bool match_is_entity_visible_to_player(const MatchState& state, const Entity& entity, uint8_t player_id);
bool match_is_target_invalid(const MatchState& state, const Target& target, uint8_t player_id);
bool match_has_entity_reached_target(const MatchState& state, const Entity& entity);
ivec2 match_get_entity_target_cell(const MatchState& state, const Entity& entity);
bool match_is_entity_mining(const MatchState& state, const Entity& entity);
EntityId match_get_nearest_builder(const MatchState& state, const std::vector<EntityId>& builders, ivec2 cell);
void match_entity_stop_building(MatchState& state, EntityId entity_id);
void match_entity_building_finish(MatchState& state, EntityId building_id);
void match_building_enqueue(MatchState& state, Entity& building, BuildingQueueItem item);
void match_building_dequeue(MatchState& state, Entity& building);
bool match_is_building_supply_blocked(const MatchState& state, const Entity& building);
Target match_entity_target_nearest_gold_mine(const MatchState& state, const Entity& entity);
Target match_entity_target_nearest_hall(const MatchState& state, const Entity& entity);
Target match_entity_target_nearest_enemy(const MatchState& state, const Entity& entity);
void match_entity_attack_target(MatchState& state, EntityId attacker_id, Entity& defender);
void match_entity_on_attack(MatchState& state, EntityId attacker_id, Entity& defender);
uint32_t match_get_entity_garrisoned_occupancy(const MatchState& state, const Entity& entity);
void match_entity_unload_unit(MatchState& state, Entity& carrier, EntityId garrisoned_unit_id);
void match_entity_release_garrisoned_units_on_death(MatchState& state, Entity& entity);
void match_entity_explode(MatchState& state, EntityId entity_id);
uint32_t match_entity_get_max_energy(const MatchState& state, const Entity& entity);
bool match_entity_has_detection(const MatchState& state, const Entity& entity);
int match_entity_get_armor(const MatchState& state, const Entity& entity);
int match_entity_get_evasion(const MatchState& state, const Entity& entity);

// Event

void match_event_play_sound(MatchState& state, SoundName sound, ivec2 position);
void match_event_alert(MatchState& state, MatchAlertType type, uint8_t player_id, ivec2 cell, int cell_size);
void match_event_show_status(MatchState& state, uint8_t player_id, const char* message);

// Fog

int match_get_fog(const MatchState& state, uint8_t team, ivec2 cell);
bool match_is_cell_rect_revealed(const MatchState& state, uint8_t team, ivec2 cell, int cell_size);
bool match_is_cell_rect_explored(const MatchState& state, uint8_t team, ivec2 cell, int cell_size);
void match_fog_update(MatchState& state, uint8_t team, ivec2 cell, int cell_size, int sight, bool has_detection, CellLayer cell_layer, bool increment);

// Fire

bool match_is_cell_on_fire(const MatchState& state, ivec2 cell);
bool match_is_cell_rect_on_fire(const MatchState& state, ivec2 cell, int cell_size);
void match_set_cell_on_fire(MatchState& state, ivec2 cell, ivec2 source);
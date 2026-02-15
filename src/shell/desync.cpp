#include "desync.h"

#include "core/filesystem.h"
#include "core/logger.h"
#include "profile/profile.h"
#include "network/network.h"
#include <algorithm>
#include <cstdlib>

#ifdef GOLD_DEBUG
    #define DESYNC_FILEPATH_BUFFER_SIZE 256
#endif

struct DesyncState {
    uint32_t a;
    uint32_t b;

#ifdef GOLD_DEBUG
    bool desync_debug = false;
    char desync_folder_path[DESYNC_FILEPATH_BUFFER_SIZE];
#endif
};
static DesyncState state;

STATIC_ASSERT(sizeof(int) == 4ULL);
STATIC_ASSERT(sizeof(MapType) == 4ULL);
STATIC_ASSERT(sizeof(MapRegionConnection) == 260ULL);
STATIC_ASSERT(sizeof(RememberedEntity) == 28ULL);
STATIC_ASSERT(sizeof(Entity) == 1924ULL);
STATIC_ASSERT(sizeof(BuildingQueueItem) == 8ULL);
STATIC_ASSERT(sizeof(Particle) == 44ULL);
STATIC_ASSERT(sizeof(Projectile) == 20ULL);
STATIC_ASSERT(sizeof(Target) == 36ULL);
STATIC_ASSERT(sizeof(FogReveal) == 24ULL);
STATIC_ASSERT(sizeof(MatchPlayer) == 60ULL);
STATIC_ASSERT(sizeof(BotUnitComp) == 4ULL);
STATIC_ASSERT(sizeof(EntityCount) == 88ULL);
STATIC_ASSERT(sizeof(BotSquadType) == 4ULL);
STATIC_ASSERT(sizeof(BotDesiredSquad) == 92ULL);
STATIC_ASSERT(sizeof(BotBaseInfo) == 228ULL);
STATIC_ASSERT(sizeof(MatchState) == 2825556ULL);
STATIC_ASSERT(sizeof(Bot) == 16428ULL);

#ifdef GOLD_DEBUG


void desync_get_filepath(char* buffer, uint32_t frame) {
    sprintf(buffer, "%s/%u.desync", state.desync_folder_path, frame);
}

bool desync_init(const char* desync_foldername) {
    state.desync_debug = true;
    sprintf(state.desync_folder_path, "%s%s", filesystem_get_data_path().c_str(), desync_foldername);

    SDL_CreateDirectory(state.desync_folder_path);
    return true;
}

void desync_quit() {
    if (!state.desync_debug) {
        return;
    }
    SDL_RemovePath(state.desync_folder_path);
}

void desync_write_file(const MatchState* match_state, const Bot* bots, uint32_t frame) {
    char desync_filepath[DESYNC_FILEPATH_BUFFER_SIZE];
    desync_get_filepath(desync_filepath, frame);
    FILE* desync_file = fopen(desync_filepath, "wb");
    if (desync_file == NULL) {
        log_error("Could not open desync file %s for writing.", desync_filepath);
        return;
    }

    fwrite(match_state, sizeof(MatchState), 1, desync_file);
    fwrite(bots, sizeof(Bot) * MAX_PLAYERS, 1, desync_file);

    fclose(desync_file);
    log_debug("Wrote desync file %s", desync_filepath);
}

#endif

uint32_t desync_compute_match_checksum(const MatchState* match_state, const Bot* bots, uint32_t frame) {
    ZoneScoped;

    const uint32_t MOD_ADLER = 65521;
    uint32_t a = 1;
    uint32_t b = 0;

    const uint8_t* match_state_bytes = (uint8_t*)match_state;
    for (size_t index = 0; index < sizeof(MatchState); index++) {
        a = (a + match_state_bytes[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    const uint8_t* bot_bytes = (uint8_t*)bots;
    for (size_t index = 0; index < sizeof(Bot) * MAX_PLAYERS; index++) {
        a = (a + bot_bytes[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    // Write to desync file
#ifdef GOLD_DEBUG
    if (state.desync_debug) {
        desync_write_file(match_state, bots, frame);
    }
#endif

    return (b << 16) | a;
}

uint32_t desync_get_checksum_frequency() {
#ifdef GOLD_DEBUG
    if (state.desync_debug) {
        return 4U;
    }
#endif
    return 60U;
}

#ifdef GOLD_DEBUG

uint32_t desync_compute_buffer_checksum(uint8_t* data, size_t length) {
    const uint32_t MOD_ADLER = 65521;
    uint32_t a = 1;
    uint32_t b = 0;

    for (size_t index = 0; index < length; index++) {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}

void desync_send_serialized_frame(uint32_t frame) {
    size_t state_buffer_length;
    uint8_t* state_buffer = desync_read_serialized_frame(frame, &state_buffer_length);
    if (state_buffer == NULL) {
        return;
    }

    network_send_serialized_frame(state_buffer, state_buffer_length);
    free(state_buffer);

    log_info("DESYNC Sent serialized frame %u with size %llu", frame, state_buffer_length);
}

void desync_delete_serialized_frame(uint32_t frame) {
    if (!state.desync_debug) {
        return;
    }

    char desync_filepath[DESYNC_FILEPATH_BUFFER_SIZE];
    desync_get_filepath(desync_filepath, frame);
    SDL_RemovePath(desync_filepath);
    log_debug("DESYNC deleted serialized frame %s", desync_filepath);
}

uint8_t* desync_read_serialized_frame(uint32_t frame_number, size_t* state_buffer_length) {
    char desync_filepath[DESYNC_FILEPATH_BUFFER_SIZE];
    desync_get_filepath(desync_filepath, frame_number);
    FILE* desync_file = fopen(desync_filepath, "rb");
    if (desync_file == NULL) {
        log_error("desync file with path %s could not be opened.", desync_filepath);
        return NULL;
    }

    size_t frame_length = sizeof(MatchState) + (MAX_PLAYERS * sizeof(Bot));
    *state_buffer_length = frame_length + sizeof(uint8_t) + sizeof(uint32_t);

    // Malloc and read into buffer, but leave room for the frame number and message type
    uint8_t* state_buffer = (uint8_t*)malloc(*state_buffer_length);
    if (state_buffer == NULL) {
        fclose(desync_file);
        log_error("error mallocing desync state buffer.");
        return NULL;
    }
    memcpy(state_buffer + sizeof(uint8_t), &frame_number, sizeof(frame_number));
    fread(state_buffer + sizeof(uint8_t) + sizeof(uint32_t), frame_length, 1, desync_file);

    fclose(desync_file);
    return state_buffer;
}

// Use this only when T is trivially copiable
template <typename T, uint32_t _capacity>
void desync_assert_fixed_vectors_equal(const FixedVector<T, _capacity>& vector_a, const FixedVector<T, _capacity>& vector_b) {
    const typename FixedVector<T, _capacity>::InternalState* state_a = vector_a.get_internal_state();
    const typename FixedVector<T, _capacity>::InternalState* state_b = vector_b.get_internal_state();
    GOLD_ASSERT(state_a->size == state_b->size);
    for (size_t index = 0; index < _capacity; index++) {
        GOLD_ASSERT(state_a->data[index] == state_b->data[index]);
    }
}

// Use this only when T is trivially copiable
template <typename T, uint32_t _capacity>
void desync_assert_fixed_queues_equal(const FixedQueue<T, _capacity>& queue_a, const FixedQueue<T, _capacity>& queue_b) {
    const typename FixedQueue<T, _capacity>::InternalState* state_a = queue_a.get_internal_state();
    const typename FixedQueue<T, _capacity>::InternalState* state_b = queue_b.get_internal_state();
    GOLD_ASSERT(state_a->size == state_b->size);
    GOLD_ASSERT(state_a->tail == state_b->tail);
    for (size_t index = 0; index < _capacity; index++) {
        GOLD_ASSERT(state_a->data[index] == state_b->data[index]);
    }
}

void desync_assert_animations_equal(const Animation& animation_a, const Animation& animation_b) {
    GOLD_ASSERT(animation_a.name == animation_b.name);
    GOLD_ASSERT(animation_a.timer == animation_b.timer);
    GOLD_ASSERT(animation_a.frame_index == animation_b.frame_index);
    GOLD_ASSERT(animation_a.frame == animation_b.frame);
    GOLD_ASSERT(animation_a.loops_remaining == animation_b.loops_remaining);
}

void desync_assert_targets_equal(const Target& target_a, const Target& target_b) {
    GOLD_ASSERT(target_a.type == target_b.type);
    GOLD_ASSERT(target_a.id == target_b.id);
    GOLD_ASSERT(target_a.padding == target_b.padding);
    GOLD_ASSERT(target_a.padding2 == target_b.padding2);
    GOLD_ASSERT(target_a.cell == target_b.cell);
    // This also checks for the patrol target's memory
    GOLD_ASSERT(target_a.build.unit_cell == target_b.build.unit_cell);
    GOLD_ASSERT(target_a.build.building_cell == target_b.build.building_cell);
    GOLD_ASSERT(target_a.build.building_type == target_b.build.building_type);
}

void desync_compare_frames(uint8_t* state_buffer_a, uint8_t* state_buffer_b) {
    const MatchState* state_a = (MatchState*)state_buffer_a;
    const MatchState* state_b = (MatchState*)state_buffer_b;

    // LCG seed
    GOLD_ASSERT(state_a->lcg_seed == state_b->lcg_seed);

    // Map
    GOLD_ASSERT(state_a->map.type == state_b->map.type);
    GOLD_ASSERT(state_a->map.width == state_b->map.width);
    GOLD_ASSERT(state_a->map.height == state_b->map.height);

    // Map tiles
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        const Tile& tile_a = state_a->map.tiles[index];
        const Tile& tile_b = state_b->map.tiles[index];
        GOLD_ASSERT(tile_a.sprite == tile_b.sprite);
        GOLD_ASSERT(tile_a.frame_x == tile_b.frame_x);
        GOLD_ASSERT(tile_a.frame_y == tile_b.frame_y);
        GOLD_ASSERT(tile_a.elevation == tile_b.elevation);
    }

    // Map cells
    for (size_t cell_layer = 0; cell_layer < CELL_LAYER_COUNT; cell_layer++) {
        for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
            const Cell& cell_a = state_a->map.cells[cell_layer][index];
            const Cell& cell_b = state_b->map.cells[cell_layer][index];

            GOLD_ASSERT(cell_a.type == cell_b.type);
            GOLD_ASSERT(cell_a.id == cell_b.id);
        }
    }

    // Regions
    GOLD_ASSERT(state_a->map.region_count == state_b->map.region_count);
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        GOLD_ASSERT(state_a->map.regions[index] == state_b->map.regions[index]);
    }

    // Region connection indices
    for (size_t region = 0; region < MAP_REGION_MAX; region++) {
        for (size_t other_region = 0; other_region < MAP_REGION_MAX; other_region++) {
            GOLD_ASSERT(state_a->map.region_connection_indices[region][other_region] == state_b->map.region_connection_indices[region][other_region]);
        }
    }

    // Region connections
    GOLD_ASSERT(state_a->map.region_connection_count == state_b->map.region_connection_count);
    for (size_t index = 0; index < MAP_REGION_CONNECTION_MAX; index++) {
        const MapRegionConnection& connection_a = state_a->map.region_connections[index];
        const MapRegionConnection& connection_b = state_b->map.region_connections[index];

        GOLD_ASSERT(connection_a.cell_count == connection_b.cell_count);
        for (size_t cell_index = 0; cell_index < MAP_REGION_CHUNK_SIZE; cell_index++) {
            GOLD_ASSERT(connection_a.cells[cell_index] == connection_b.cells[cell_index]);
        }
    }

    // Region connection-to-connection cost
    for (size_t index = 0; index < MAP_REGION_CONNECTION_MAX; index++) {
        for (size_t other = 0; other < MAP_REGION_CONNECTION_MAX; other++) {
            GOLD_ASSERT(state_a->map.region_connection_to_connection_cost[index][other] == state_b->map.region_connection_to_connection_cost[index][other]);
        }
    }

    // Fog
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
            GOLD_ASSERT(state_a->fog[player_id][index] == state_b->fog[player_id][index]);
        }
    }

    // Detection
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
            GOLD_ASSERT(state_a->detection[player_id][index] == state_b->detection[player_id][index]);
        }
    }

    // Remembered entities
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        GOLD_ASSERT(state_a->remembered_entities[player_id].size() == state_b->remembered_entities[player_id].size());
        for (size_t index = 0; index < MATCH_MAX_REMEMBERED_ENTITIES; index++) {
            const RememberedEntity& entity_a = state_a->remembered_entities[player_id][index];
            const RememberedEntity& entity_b = state_b->remembered_entities[player_id][index];

            GOLD_ASSERT(entity_a.entity_id == entity_b.entity_id);
            GOLD_ASSERT(entity_a.type == entity_b.type);
            GOLD_ASSERT(entity_a.frame == entity_b.frame);
            GOLD_ASSERT(entity_a.cell == entity_b.cell);
            GOLD_ASSERT(entity_a.recolor_id == entity_b.recolor_id);
        }
    }

    // Entities
    const IdArray<Entity, MATCH_MAX_ENTITIES>::InternalState* entities_a = state_a->entities.get_internal_state();
    const IdArray<Entity, MATCH_MAX_ENTITIES>::InternalState* entities_b = state_b->entities.get_internal_state();
    GOLD_ASSERT(entities_a->size == entities_b->size);
    GOLD_ASSERT(entities_a->next_id == entities_b->next_id);
    for (size_t entity_index = 0; entity_index < MATCH_MAX_ENTITIES; entity_index++) {
        const Entity& entity_a = entities_a->data[entity_index];
        const Entity& entity_b = entities_b->data[entity_index];

        GOLD_ASSERT(entities_a->ids[entity_index] == entities_b->ids[entity_index]);

        GOLD_ASSERT(entity_a.type == entity_b.type);
        GOLD_ASSERT(entity_a.mode == entity_b.mode);
        GOLD_ASSERT(entity_a.player_id == entity_b.player_id);
        GOLD_ASSERT(entity_a.padding[0] == entity_b.padding[0]);
        GOLD_ASSERT(entity_a.padding[1] == entity_b.padding[1]);
        GOLD_ASSERT(entity_a.padding[2] == entity_b.padding[2]);
        GOLD_ASSERT(entity_a.flags == entity_b.flags);

        GOLD_ASSERT(entity_a.cell == entity_b.cell);
        GOLD_ASSERT(entity_a.position == entity_b.position);
        GOLD_ASSERT(entity_a.direction == entity_b.direction);

        GOLD_ASSERT(entity_a.health == entity_b.health);
        GOLD_ASSERT(entity_a.energy == entity_b.energy);
        GOLD_ASSERT(entity_a.timer == entity_b.timer);
        GOLD_ASSERT(entity_a.energy_regen_timer == entity_b.energy_regen_timer);
        GOLD_ASSERT(entity_a.health_regen_timer == entity_b.health_regen_timer);
        GOLD_ASSERT(entity_a.health_regen_timer == entity_b.health_regen_timer);

        desync_assert_animations_equal(entity_a.animation, entity_b.animation);

        desync_assert_fixed_vectors_equal(entity_a.garrisoned_units, entity_b.garrisoned_units);

        GOLD_ASSERT(entity_a.garrison_id == entity_b.garrison_id);

        GOLD_ASSERT(entity_a.goldmine_id == entity_b.goldmine_id);
        GOLD_ASSERT(entity_a.gold_held == entity_b.gold_held);

        desync_assert_targets_equal(entity_a.target, entity_b.target);

        // Target queue
        const FixedQueue<Target, TARGET_QUEUE_CAPACITY>::InternalState* target_queue_a = entity_a.target_queue.get_internal_state();
        const FixedQueue<Target, TARGET_QUEUE_CAPACITY>::InternalState* target_queue_b = entity_b.target_queue.get_internal_state();
        GOLD_ASSERT(target_queue_a->size == target_queue_b->size);
        GOLD_ASSERT(target_queue_a->tail == target_queue_b->tail);
        for (size_t index = 0; index < TARGET_QUEUE_CAPACITY; index++) {
            desync_assert_targets_equal(target_queue_a->data[index], target_queue_b->data[index]);
        }

        // Path
        desync_assert_fixed_vectors_equal(entity_a.path, entity_b.path);
        GOLD_ASSERT(entity_a.pathfind_attempts == entity_b.pathfind_attempts);

        // Building queue
        const FixedVector<BuildingQueueItem, BUILDING_QUEUE_SIZE>::InternalState* building_queue1 = entity_a.queue.get_internal_state();
        const FixedVector<BuildingQueueItem, BUILDING_QUEUE_SIZE>::InternalState* building_queue2 = entity_b.queue.get_internal_state();
        GOLD_ASSERT(building_queue1->size == building_queue2->size);
        for (size_t index = 0; index < BUILDING_QUEUE_SIZE; index++) {
            const BuildingQueueItem& item1 = building_queue1->data[index];
            const BuildingQueueItem& item2 = building_queue2->data[index];

            GOLD_ASSERT(item1.type == item2.type);
            // This assertion also covers item.upgrade
            GOLD_ASSERT(item1.unit_type == item2.unit_type);
        }

        GOLD_ASSERT(entity_a.rally_point == entity_b.rally_point);

        GOLD_ASSERT(entity_a.cooldown_timer == entity_b.cooldown_timer);
        GOLD_ASSERT(entity_a.attack_move_cell == entity_b.attack_move_cell);

        GOLD_ASSERT(entity_a.taking_damage_counter == entity_b.taking_damage_counter);
        GOLD_ASSERT(entity_a.taking_damage_timer == entity_b.taking_damage_timer);
        GOLD_ASSERT(entity_a.fire_damage_timer == entity_b.fire_damage_timer);

        GOLD_ASSERT(entity_a.bleed_timer == entity_b.bleed_timer);
        GOLD_ASSERT(entity_a.bleed_damage_timer == entity_b.bleed_damage_timer);
        desync_assert_animations_equal(entity_a.bleed_animation, entity_b.bleed_animation);
    }
    for (EntityId entity_id = 0; entity_id < ID_MAX; entity_id++) {
        GOLD_ASSERT(entities_a->id_to_index[entity_id] == entities_b->id_to_index[entity_id]);
    }

    // Particles
    {
        const CircularVector<Particle, MATCH_MAX_PARTICLES>::InternalState* particles_a = state_a->particles.get_internal_state();
        const CircularVector<Particle, MATCH_MAX_PARTICLES>::InternalState* particles_b = state_a->particles.get_internal_state();
        GOLD_ASSERT(particles_a->size == particles_b->size);
        for (size_t index = 0; index < MATCH_MAX_PARTICLES; index++) {
            const Particle& particle_a = particles_a->data[index];
            const Particle& particle_b = particles_b->data[index];

            GOLD_ASSERT(particle_a.layer == particle_b.layer);
            GOLD_ASSERT(particle_a.sprite == particle_b.sprite);
            desync_assert_animations_equal(particle_a.animation, particle_b.animation);
            GOLD_ASSERT(particle_a.vframe == particle_b.vframe);
            GOLD_ASSERT(particle_a.position == particle_b.position);
        }
    }

    // Projectiles
    {
        const CircularVector<Projectile, MATCH_MAX_PROJECTILES>::InternalState* projectiles_a = state_a->projectiles.get_internal_state();
        const CircularVector<Projectile, MATCH_MAX_PROJECTILES>::InternalState* projectiles_b = state_b->projectiles.get_internal_state();
        GOLD_ASSERT(projectiles_a->size == projectiles_b->size);
        for (size_t index = 0; index < MATCH_MAX_PROJECTILES; index++) {
            const Projectile& projectile_a = projectiles_a->data[index];
            const Projectile& projectile_b = projectiles_b->data[index];

            GOLD_ASSERT(projectile_a.type == projectile_b.type);
            GOLD_ASSERT(projectile_a.position == projectile_b.position);
            GOLD_ASSERT(projectile_a.target == projectile_b.target);
        }
    }

    // Fires
    {
        const CircularVector<Fire, MATCH_MAX_FIRES>::InternalState* fires_a = state_a->fires.get_internal_state();
        const CircularVector<Fire, MATCH_MAX_FIRES>::InternalState* fires_b = state_b->fires.get_internal_state();
        GOLD_ASSERT(fires_a->size == fires_b->size);
        for (size_t index = 0; index < MATCH_MAX_FIRES; index++) {
            const Fire& fire_a = fires_a->data[index];
            const Fire& fire_b = fires_b->data[index];

            GOLD_ASSERT(fire_a.cell == fire_b.cell);
            GOLD_ASSERT(fire_a.source == fire_b.source);
            GOLD_ASSERT(fire_a.time_to_live == fire_b.time_to_live);
            desync_assert_animations_equal(fire_a.animation, fire_b.animation);
        }
    }

    // Fire cells
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        GOLD_ASSERT(state_a->fire_cells[index] == state_b->fire_cells[index]);
    }

    // For reveals
    {
        const CircularVector<FogReveal, MATCH_MAX_FOG_REVEALS>::InternalState* fog_reveals_a = state_a->fog_reveals.get_internal_state();
        const CircularVector<FogReveal, MATCH_MAX_FOG_REVEALS>::InternalState* fog_reveals_b = state_b->fog_reveals.get_internal_state();
        GOLD_ASSERT(fog_reveals_a->size == fog_reveals_b->size);
        for (size_t index = 0; index < MATCH_MAX_FOG_REVEALS; index++) {
            const FogReveal& fog_reveal_a = fog_reveals_a->data[index];
            const FogReveal& fog_reveal_b = fog_reveals_b->data[index];

            GOLD_ASSERT(fog_reveal_a.team == fog_reveal_b.team);
            GOLD_ASSERT(fog_reveal_a.cell == fog_reveal_b.cell);
            GOLD_ASSERT(fog_reveal_a.cell_size == fog_reveal_b.cell_size);
            GOLD_ASSERT(fog_reveal_a.sight == fog_reveal_b.sight);
            GOLD_ASSERT(fog_reveal_a.timer == fog_reveal_b.timer);
        }
    }

    // Players
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const MatchPlayer& player_a = state_a->players[player_id];
        const MatchPlayer& player_b = state_b->players[player_id];

        GOLD_ASSERT(player_a.active == player_b.active);
        for (size_t name_index = 0; name_index < sizeof(player_a.name); name_index++) {
            GOLD_ASSERT(player_a.name[name_index] == player_b.name[name_index]);
        }
        GOLD_ASSERT(player_a.padding == player_b.padding);
        GOLD_ASSERT(player_a.padding2 == player_b.padding2);
        GOLD_ASSERT(player_a.team == player_b.team);
        GOLD_ASSERT(player_a.recolor_id == player_b.recolor_id);
        GOLD_ASSERT(player_a.gold == player_b.gold);
        GOLD_ASSERT(player_a.gold_mined_total == player_b.gold_mined_total);
        GOLD_ASSERT(player_a.upgrades == player_b.upgrades);
        GOLD_ASSERT(player_a.upgrades_in_progress == player_b.upgrades_in_progress);
    }

    // Events
    {
        const FixedQueue<MatchEvent, MATCH_MAX_EVENTS>::InternalState* events_a = state_a->events.get_internal_state();
        const FixedQueue<MatchEvent, MATCH_MAX_EVENTS>::InternalState* events_b = state_b->events.get_internal_state();
        GOLD_ASSERT(events_a->size == events_b->size);
        GOLD_ASSERT(events_a->tail == events_b->tail);
        for (size_t index = 0; index < MATCH_MAX_EVENTS; index++) {
            const MatchEvent& event_a = events_a->data[index];
            const MatchEvent& event_b = events_b->data[index];

            GOLD_ASSERT(memcmp(&event_a, &event_b, sizeof(MatchEvent)) == 0);
        }
    }

    // Bots
    const Bot* bots_a = (Bot*)(state_buffer_a + sizeof(MatchState));
    const Bot* bots_b = (Bot*)(state_buffer_b + sizeof(MatchState));
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const Bot& bot_a = bots_a[player_id];
        const Bot& bot_b = bots_b[player_id];

        // Config
        {
            GOLD_ASSERT(bot_a.config.flags == bot_b.config.flags);
            GOLD_ASSERT(bot_a.config.target_base_count == bot_b.config.target_base_count);
            GOLD_ASSERT(bot_a.config.macro_cycle_cooldown == bot_b.config.macro_cycle_cooldown);
            GOLD_ASSERT(bot_a.config.allowed_upgrades == bot_b.config.allowed_upgrades);
            for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
                GOLD_ASSERT(bot_a.config.is_entity_allowed[entity_type] == bot_b.config.is_entity_allowed[entity_type]);
            }
            GOLD_ASSERT(bot_a.config.padding2 == bot_b.config.padding2);
            GOLD_ASSERT(bot_a.config.padding2 == bot_b.config.padding2);
            GOLD_ASSERT(bot_a.config.opener == bot_b.config.opener);
            GOLD_ASSERT(bot_a.config.preferred_unit_comp == bot_b.config.preferred_unit_comp);
        }

        GOLD_ASSERT(bot_a.player_id == bot_b.player_id);
        GOLD_ASSERT(bot_a.padding == bot_b.padding);

        GOLD_ASSERT(bot_a.scout_id == bot_b.scout_id);
        GOLD_ASSERT(bot_a.scout_info == bot_b.scout_info);
        GOLD_ASSERT(bot_a.last_scout_time == bot_b.last_scout_time);
        desync_assert_fixed_vectors_equal(bot_a.entities_to_scout, bot_b.entities_to_scout);
        desync_assert_fixed_vectors_equal(bot_a.entities_assumed_to_be_scouted, bot_b.entities_assumed_to_be_scouted);

        // Reservation requests
        {
            const FixedQueue<BotReservationRequest, BOT_MAX_RESERVATION_REQUESTS>::InternalState* reservation_requests_a = bot_a.reservation_requests.get_internal_state();
            const FixedQueue<BotReservationRequest, BOT_MAX_RESERVATION_REQUESTS>::InternalState* reservation_requests_b = bot_b.reservation_requests.get_internal_state();
            GOLD_ASSERT(reservation_requests_a->size == reservation_requests_b->size);
            GOLD_ASSERT(reservation_requests_a->tail == reservation_requests_b->tail);
            for (size_t index = 0; index < BOT_MAX_RESERVATION_REQUESTS; index++) {
                const BotReservationRequest& request_a = reservation_requests_a->data[index];
                const BotReservationRequest& request_b = reservation_requests_b->data[index];

                GOLD_ASSERT(memcmp(&request_a, &request_b, sizeof(BotReservationRequest)) == 0);
            }
        }

        GOLD_ASSERT(bot_a.unit_comp == bot_b.unit_comp);
        GOLD_ASSERT(memcmp(&bot_a.desired_buildings, &bot_b.desired_buildings, sizeof(EntityCount)) == 0);
        GOLD_ASSERT(memcmp(&bot_a.desired_army_ratio, &bot_b.desired_army_ratio, sizeof(EntityCount)) == 0);

        // Desired squads
        {
            const FixedVector<BotDesiredSquad, BOT_MAX_DESIRED_SQUADS>::InternalState* desired_squads_a = bot_a.desired_squads.get_internal_state();
            const FixedVector<BotDesiredSquad, BOT_MAX_DESIRED_SQUADS>::InternalState* desired_squads_b = bot_b.desired_squads.get_internal_state();
            GOLD_ASSERT(desired_squads_a->size == desired_squads_b->size);
            for (size_t index = 0; index < BOT_MAX_DESIRED_SQUADS; index++) {
                const BotDesiredSquad& squad_a = desired_squads_a->data[index];
                const BotDesiredSquad& squad_b = desired_squads_b->data[index];

                GOLD_ASSERT(memcmp(&squad_a, &squad_b, sizeof(BotDesiredSquad)) == 0);
            }
        }

        desync_assert_fixed_queues_equal(bot_a.buildings_to_set_rally_points, bot_b.buildings_to_set_rally_points);

        GOLD_ASSERT(bot_a.macro_cycle_timer == bot_b.macro_cycle_timer);
        GOLD_ASSERT(bot_a.macro_cycle_count == bot_b.macro_cycle_count);

        GOLD_ASSERT(bot_a.next_squad_id == bot_b.next_squad_id);

        // Squads
        {
            const FixedVector<BotSquad, BOT_MAX_SQUADS>::InternalState* squads_a = bot_a.squads.get_internal_state();
            const FixedVector<BotSquad, BOT_MAX_SQUADS>::InternalState* squads_b = bot_b.squads.get_internal_state();
            GOLD_ASSERT(squads_a->size == squads_b->size);
            for (size_t index = 0; index < BOT_MAX_SQUADS; index++) {
                const BotSquad& squad_a = squads_a->data[index];
                const BotSquad& squad_b = squads_b->data[index];

                GOLD_ASSERT(memcmp(&squad_a, &squad_b, sizeof(BotSquad)) == 0);
            }
        }

        GOLD_ASSERT(bot_a.next_landmine_time == bot_b.next_landmine_time);

        // Base info
        {
            const FixedVector<BotBaseInfo, BOT_MAX_BASE_INFO>::InternalState* base_info_array_a = bot_a.base_info.get_internal_state();
            const FixedVector<BotBaseInfo, BOT_MAX_BASE_INFO>::InternalState* base_info_array_b = bot_b.base_info.get_internal_state();
            GOLD_ASSERT(base_info_array_a->size == base_info_array_b->size);
            for (size_t index = 0; index < BOT_MAX_BASE_INFO; index++) {
                const BotBaseInfo& base_info_a = base_info_array_a->data[index];
                const BotBaseInfo& base_info_b = base_info_array_b->data[index];

                GOLD_ASSERT(memcmp(&base_info_a, &base_info_b, sizeof(BotBaseInfo)) == 0);
            }
        }
    }
}

#endif
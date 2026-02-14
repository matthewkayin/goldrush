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
STATIC_ASSERT(sizeof(MatchState) == 2824400ULL);
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

template <typename T>
size_t desync_compare_value(uint8_t* data, uint8_t* data2, T* returned_value = NULL) {
    T value; 
    T value2; 
    memcpy(&value, data, sizeof(T));
    memcpy(&value2, data2, sizeof(T));

    for (size_t index = 0; index < sizeof(T); index++) {
        GOLD_ASSERT(data[index] == data2[index]);
    }

    if (returned_value != NULL) {
        *returned_value = value;
    }
    return sizeof(T);
}

void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2) {
    size_t state_buffer_offset = 0;

    // LCG seed
    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map: type, width, height
    state_buffer_offset += desync_compare_value<MapType>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map tiles
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        state_buffer_offset += desync_compare_value<Tile>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Map cells
    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
            state_buffer_offset += desync_compare_value<Cell>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        }
    }

    // Map region count
    state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map regions
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Map region connection indices
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Map region connection count
    state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map region connections
    for (size_t index = 0; index < MAP_REGION_CONNECTION_MAX; index++) {
        state_buffer_offset += desync_compare_value<MapRegionConnection>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Map region connection-to-connection cost
    for (size_t index = 0; index < MAP_REGION_CONNECTION_MAX * MAP_REGION_CONNECTION_MAX; index++) {
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Fog
    for (size_t index = 0; index < MAX_PLAYERS * MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Detection
    for (size_t index = 0; index < MAX_PLAYERS * MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Remembered entity count
    for (size_t index = 0; index < MAX_PLAYERS; index++) {
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Remembered entities
    for (size_t index = 0; index < MAX_PLAYERS * MATCH_MAX_REMEMBERED_ENTITIES; index++) {
        state_buffer_offset += desync_compare_value<RememberedEntity>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Is fog dirty
    state_buffer_offset += desync_compare_value<bool>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Entities
    for (size_t entity_index = 0; entity_index < MATCH_MAX_ENTITIES; entity_index++) {
        // Type
        state_buffer_offset += desync_compare_value<EntityType>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Mode
        state_buffer_offset += desync_compare_value<EntityMode>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Player ID
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Padding
        for (size_t index = 0; index < 3; index++) {
            state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        }

        // Flags
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Cell
        state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Position
        state_buffer_offset += desync_compare_value<fvec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Direction
        state_buffer_offset += desync_compare_value<Direction>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Health
        state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Energy
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Energy regen timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Health regen timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Animation
        state_buffer_offset += desync_compare_value<Animation>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Garrisoned units
        state_buffer_offset += desync_compare_value<FixedVector<EntityId, GARRISONED_UNITS_MAX>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Garrison ID
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Goldmine ID
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Gold held
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Target
        state_buffer_offset += desync_compare_value<Target>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Target queue
        state_buffer_offset += desync_compare_value<FixedQueue<Target, TARGET_QUEUE_CAPACITY>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Path
        state_buffer_offset += desync_compare_value<MapPath>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Pathfind attempts
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Building queue
        state_buffer_offset += desync_compare_value<FixedVector<BuildingQueueItem, BUILDING_QUEUE_SIZE>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Rally point
        state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Cooldown timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Attack move cell
        state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Taking damage counter
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Taking damage timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Fire damage timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Bleed timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Bleed damage timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Bleed animation
        state_buffer_offset += desync_compare_value<Animation>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Entities array ids
    for (size_t index = 0; index < MATCH_MAX_ENTITIES; index++) {
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Entities array id_to_index
    for (size_t index = 0; index < ID_MAX; index++) {
        state_buffer_offset += desync_compare_value<uint16_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Entities array next_id
    state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Entities array _size
    state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Particles
    state_buffer_offset += desync_compare_value<CircularVector<Particle, MATCH_MAX_PARTICLES>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Projectiles
    state_buffer_offset += desync_compare_value<CircularVector<Projectile, MATCH_MAX_PROJECTILES>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Fire
    state_buffer_offset += desync_compare_value<CircularVector<Fire, MATCH_MAX_FIRES>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Fire cells
    for (size_t index = 0; index < MAP_SIZE_MAX * MAP_SIZE_MAX; index++) {
        state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Fire
    state_buffer_offset += desync_compare_value<CircularVector<FogReveal, MATCH_MAX_FOG_REVEALS>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Players
    for (size_t index = 0; index < MAX_PLAYERS; index++) {
        state_buffer_offset += desync_compare_value<MatchPlayer>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Event queue
    state_buffer_offset += desync_compare_value<FixedQueue<MatchEvent, MATCH_MAX_EVENTS>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Bots
    for (size_t bot_index = 0; bot_index < MAX_PLAYERS; bot_index++) {
        // Bot config
        state_buffer_offset += desync_compare_value<BotConfig>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Player ID
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Padding
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Scout ID
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Scout info
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Last scout time
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Entities to scout
        state_buffer_offset += desync_compare_value<EntityList>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Entities assumed to be scouted
        state_buffer_offset += desync_compare_value<FixedVector<EntityId, BOT_MAX_ENTITIES_ASSUMED_TO_BE_SCOUTED>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Reservation requests
        state_buffer_offset += desync_compare_value<FixedQueue<BotReservationRequest, BOT_MAX_RESERVATION_REQUESTS>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Unit comp
        state_buffer_offset += desync_compare_value<BotUnitComp>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Desired buildings
        state_buffer_offset += desync_compare_value<EntityCount>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Desired army ratio
        state_buffer_offset += desync_compare_value<EntityCount>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Desired squads
        state_buffer_offset += desync_compare_value<FixedVector<BotDesiredSquad, BOT_MAX_DESIRED_SQUADS>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Buildings to set rally points
        state_buffer_offset += desync_compare_value<FixedQueue<EntityId, BOT_MAX_RALLY_REQUESTS>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Macro cycle timer
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Macro cycle count
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Next squad ID
        state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Squads
        state_buffer_offset += desync_compare_value<FixedQueue<EntityId, BOT_MAX_RALLY_REQUESTS>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Next landmine time
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Base info
        state_buffer_offset += desync_compare_value<FixedVector<BotBaseInfo, BOT_MAX_BASE_INFO>>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }
}

#endif
#include "desync.h"

STATIC_ASSERT(sizeof(Tile) == 16ULL);
STATIC_ASSERT(sizeof(Cell) == 8ULL);
STATIC_ASSERT(sizeof(Animation) == 24ULL);
STATIC_ASSERT(sizeof(RememberedEntity) == 28ULL);
STATIC_ASSERT(sizeof(Target) == 36ULL);
STATIC_ASSERT(sizeof(BuildingQueueItem) == 8ULL);
STATIC_ASSERT(sizeof(Particle) == 40ULL);
STATIC_ASSERT(sizeof(Projectile) == 20ULL);
STATIC_ASSERT(sizeof(FogReveal) == 24ULL);
STATIC_ASSERT(sizeof(MatchPlayer) == 56ULL);
STATIC_ASSERT(sizeof(MapType) == 4ULL);
STATIC_ASSERT(sizeof(Difficulty) == 4ULL);
STATIC_ASSERT(sizeof(BotUnitComp) == 4ULL);
STATIC_ASSERT(sizeof(EntityCount) == 88ULL);
STATIC_ASSERT(sizeof(BotSquadType) == 4ULL);
STATIC_ASSERT(sizeof(BotDesiredSquad) == 92ULL);
STATIC_ASSERT(sizeof(BotBaseInfo) == 12ULL);

#include "core/filesystem.h"
#include <algorithm>
#include <cstdlib>

struct DesyncState {
    uint32_t a;
    uint32_t b;

#ifdef GOLD_DEBUG_DESYNC
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_capacity;

    std::string desync_file_path;
    FILE* desync_file;
#endif
};
static DesyncState state;

#ifdef GOLD_DEBUG_DESYNC

bool desync_init(const char* desync_filename) {
        state.buffer_size = 0;
        state.buffer_capacity = 1024ULL * 1024ULL;
        state.buffer = (uint8_t*)malloc(state.buffer_capacity);
        if (state.buffer == NULL) {
            return false;
        }

        state.desync_file_path = filesystem_get_data_path() + FILESYSTEM_LOG_FOLDER_NAME + desync_filename + ".desync";
        state.desync_file = fopen(state.desync_file_path.c_str(), "wb");
        if (state.desync_file == NULL) {
            return false;
        }

    return true;
}

void desync_quit() {
    free(state.buffer);
    fclose(state.desync_file);
}

#endif

void desync_checksum_init(uint32_t* a, uint32_t* b) {
    *a = 1;
    *b = 0;
}

void desync_checksum_add_byte(uint32_t *a, uint32_t *b, uint8_t byte) {
    static const uint32_t MOD_ADLER = 65521;
    *a = (*a + byte) % MOD_ADLER;
    *b = (*b + *a) % MOD_ADLER;
}

uint32_t desync_checksum_compute_result(uint32_t a, uint32_t b) {
    return (b << 16) | a;
}

void desync_write(uint8_t* data, size_t length) {
    #ifdef GOLD_DEBUG_DESYNC
        if (state.buffer_size + length > state.buffer_capacity) {
            uint8_t* temp = state.buffer;
            state.buffer_capacity *= 2;
            state.buffer = (uint8_t*)malloc(state.buffer_capacity);
            memcpy(state.buffer, temp, state.buffer_size);
            free(temp);
        }

        memcpy(state.buffer + state.buffer_size, data, length);
        state.buffer_size += length;
    #endif

    for (size_t index = 0; index < length; index++) {
        desync_checksum_add_byte(&state.a, &state.b, data[index]);
    }
}

template <typename T>
void desync_write_value(const T& value) {
    desync_write((uint8_t*)&value, sizeof(T));
}

template <typename T>
void desync_write_vector(const std::vector<T>& vector_value) {
    desync_write_value<size_t>(vector_value.size());
    if (!vector_value.empty()) {
        desync_write((uint8_t*)&vector_value[0], vector_value.size() * sizeof(T));
    }
}

template <typename T>
void desync_write_queue(const std::queue<T>& queue_value) {
    std::queue<T> queue_copy = queue_value;
    desync_write_value<size_t>(queue_copy.size());
    while (!queue_copy.empty()) {
        desync_write_value<T>(queue_copy.front());
        queue_copy.pop();
    }
}

template <typename T, typename U>
void desync_write_unordered_map(const std::unordered_map<T, U>& map) {
    desync_write_value<size_t>(map.size());
    std::vector<T> keys;
    for (auto it : map) {
        keys.push_back(it.first);
    }
    std::sort(keys.begin(), keys.end());
    for (T key : keys) {
        desync_write_value<T>(key);
        desync_write_value<U>(map.at(key));
    }
}

uint32_t desync_compute_match_checksum(const MatchState& match_state, const Bot bots[MAX_PLAYERS]) {
    desync_checksum_init(&state.a, &state.b);

    #ifdef GOLD_DEBUG_DESYNC
        state.buffer_size = 0;
    #endif

    // LCG seed
    desync_write_value<int>(match_state.lcg_seed);

    // Map: type, width, and height
    desync_write_value<MapType>(match_state.map.type);
    desync_write_value<int>(match_state.map.width);
    desync_write_value<int>(match_state.map.height);

    // Map tiles
    desync_write_vector<Tile>(match_state.map.tiles);

    // Map cell layers
    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        desync_write_vector<Cell>(match_state.map.cells[layer]);
    }

    // Map regions
    desync_write_vector<int>(match_state.map.regions);

    // Map region connections
    desync_write_value<size_t>(match_state.map.region_connections.size());
    for (size_t region = 0; region < match_state.map.region_connections.size(); region++) {
        for (size_t other_region = 0; other_region < match_state.map.region_connections.size(); other_region++) {
            desync_write_vector<ivec2>(match_state.map.region_connections[region][other_region]);
        }
    }

    // Fog 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        desync_write_vector<int>(match_state.fog[player]);
    }

    // Detection 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        desync_write_vector<int>(match_state.detection[player]);
    }

    // Remembered entities
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        desync_write_unordered_map<EntityId, RememberedEntity>(match_state.remembered_entities[player]);
    }

    desync_write_value<bool>(match_state.is_fog_dirty);

    // Entities
    desync_write_value<size_t>(match_state.entities.size());
    for (uint32_t entity_index = 0; entity_index < match_state.entities.size(); entity_index++) {
        desync_write_value<EntityId>(match_state.entities.get_id_of(entity_index));

        const Entity& entity = match_state.entities[entity_index];
        desync_write_value<uint32_t>(entity.type);
        desync_write_value<uint32_t>(entity.mode);
        desync_write_value<uint8_t>(entity.player_id);
        desync_write_value<uint32_t>(entity.flags);
        desync_write_value<ivec2>(entity.cell);
        desync_write_value<fvec2>(entity.position);
        desync_write_value<uint32_t>(entity.direction);
        desync_write_value<int>(entity.health);
        desync_write_value<uint32_t>(entity.energy);
        desync_write_value<uint32_t>(entity.timer);
        desync_write_value<uint32_t>(entity.energy_regen_timer);
        desync_write_value<uint32_t>(entity.health_regen_timer);
        desync_write_value<Animation>(entity.animation);
        desync_write_vector<EntityId>(entity.garrisoned_units);
        desync_write_value<EntityId>(entity.garrison_id);
        desync_write_value<EntityId>(entity.goldmine_id);
        desync_write_value<uint32_t>(entity.gold_held);
        desync_write_value<Target>(entity.target);
        desync_write_vector<Target>(entity.target_queue);
        desync_write_vector<ivec2>(entity.path);
        desync_write_value<uint32_t>(entity.pathfind_attempts);
        desync_write_vector<BuildingQueueItem>(entity.queue);
        desync_write_value<ivec2>(entity.rally_point);
        desync_write_value<uint32_t>(entity.cooldown_timer);
        desync_write_value<ivec2>(entity.attack_move_cell);
        desync_write_value<uint32_t>(entity.taking_damage_counter);
        desync_write_value<uint32_t>(entity.taking_damage_timer);
        desync_write_value<uint32_t>(entity.fire_damage_timer);
        desync_write_value<uint32_t>(entity.bleed_timer);
        desync_write_value<uint32_t>(entity.bleed_damage_timer);
        desync_write_value<Animation>(entity.bleed_animation);
    }

    // Particles
    for (size_t layer = 0; layer < PARTICLE_LAYER_COUNT; layer++) {
        desync_write_vector<Particle>(match_state.particles[layer]);
    }

    // Projectiles, fire, fire cells, fog reveals
    desync_write_vector<Projectile>(match_state.projectiles);
    desync_write_vector<Fire>(match_state.fires);
    desync_write_vector<int>(match_state.fire_cells);
    desync_write_vector<FogReveal>(match_state.fog_reveals);

    // Players
    desync_write((uint8_t*)match_state.players, MAX_PLAYERS * sizeof(MatchPlayer));

    // Bots
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        const Bot& bot = bots[player_id];

        // Player ID
        desync_write_value<uint8_t>(bot.player_id);
        desync_write_value<Difficulty>(bot.difficulty);

        // Is entity reserved
        desync_write_unordered_map<EntityId, bool>(bot.is_entity_reserved);

        // Production
        desync_write_value<BotUnitComp>(bot.unit_comp);
        desync_write_value<BotUnitComp>(bot.preferred_unit_comp);
        desync_write_value<EntityCount>(bot.desired_buildings);
        desync_write_value<EntityCount>(bot.desired_army_ratio);
        desync_write_vector<BotDesiredSquad>(bot.desired_squads);
        desync_write_queue<EntityId>(bot.buildings_to_set_rally_points);;
        desync_write_value<uint32_t>(bot.macro_cycle_timer);
        desync_write_value<uint32_t>(bot.macro_cycle_count);

        // Squads
        desync_write_value<size_t>(bot.squads.size());
        for (size_t index = 0; index < bot.squads.size(); index++) {
            const BotSquad& squad = bot.squads[index];

            desync_write_value<BotSquadType>(squad.type);
            desync_write_value<ivec2>(squad.target_cell);
            desync_write_vector<EntityId>(squad.entities);
        }

        // Next landmine time
        desync_write_value<uint32_t>(bot.next_landmine_time);

        // Scouting
        desync_write_value<EntityId>(bot.scout_id);
        desync_write_value<uint32_t>(bot.scout_info);
        desync_write_value<uint32_t>(bot.last_scout_time);
        desync_write_vector<EntityId>(bot.entities_to_scout);
        desync_write_unordered_map<EntityId, bool>(bot.is_entity_assumed_to_be_scouted);

        // Base info
        desync_write_unordered_map<EntityId, BotBaseInfo>(bot.base_info);

        // Retreat memory
        desync_write_value<size_t>(bot.retreat_memory.size());
        std::vector<EntityId> keys;
        for (auto it : bot.retreat_memory) {
            keys.push_back(it.first);
        }
        std::sort(keys.begin(), keys.end());
        for (EntityId entity_id : keys) {
            const BotRetreatMemory& memory = bot.retreat_memory.at(entity_id);

            desync_write_value<EntityId>(entity_id);
            desync_write_vector<EntityId>(memory.enemy_list);
            desync_write_value<int>(memory.retreat_count);
            desync_write_value<uint32_t>(memory.retreat_time);
        }
    }

    // Write to desync file
    #ifdef GOLD_DEBUG_DESYNC
        GOLD_ASSERT(state.buffer_size != 0);
        fwrite(&state.buffer_size, sizeof(state.buffer_size), 1, state.desync_file);
        fwrite(state.buffer, state.buffer_size, 1, state.desync_file);
    #endif

    // Return checksum
    return desync_checksum_compute_result(state.a, state.b);
}

#ifdef GOLD_DEBUG_DESYNC

uint32_t desync_compute_buffer_checksum(uint8_t* data, size_t length) {
    uint32_t a, b;
    desync_checksum_init(&a, &b);

    for (size_t index = 0; index < length; index++) {
        desync_checksum_add_byte(&a, &b, data[index]);
    }

    return desync_checksum_compute_result(a, b);
}

uint8_t* desync_read_serialized_frame(uint32_t frame_number, size_t* state_buffer_length) {
    fclose(state.desync_file);
    state.desync_file = fopen(state.desync_file_path.c_str(), "rb");
    if (state.desync_file == NULL) {
        return NULL;
    }

    uint32_t current_frame_number = 0;
    while (current_frame_number != frame_number) {
        size_t block_size;
        fread(&block_size, sizeof(size_t), 1, state.desync_file);

        state.buffer_size = 0;
        if (state.buffer_capacity < block_size) {
            state.buffer_capacity = block_size;
            free(state.buffer);
            state.buffer = (uint8_t*)malloc(state.buffer_capacity);
            if (state.buffer == NULL) {
                fclose(state.desync_file);
                return NULL;
            }
        }
        fread(state.buffer, block_size, 1, state.desync_file);

        current_frame_number++;
    }

    size_t frame_length;
    fread(&frame_length, sizeof(size_t), 1, state.desync_file);
    *state_buffer_length = frame_length + sizeof(uint8_t) + sizeof(uint32_t);
    // Malloc and read into buffer, but leave room for the frame number and message type
    uint8_t* state_buffer = (uint8_t*)malloc(*state_buffer_length);
    if (state_buffer == NULL) {
        fclose(state.desync_file);
        return NULL;
    }
    memcpy(state_buffer + sizeof(uint8_t), &frame_number, sizeof(frame_number));
    fread(state_buffer + sizeof(uint8_t) + sizeof(uint32_t), frame_length, 1, state.desync_file);

    fclose(state.desync_file);
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

template <typename T>
size_t desync_compare_vector(uint8_t* data, uint8_t* data2) {
    size_t vector_size;
    size_t offset = desync_compare_value<size_t>(data, data2, &vector_size);

    for (size_t index = 0; index < vector_size; index++) {
        offset += desync_compare_value<T>(data + offset, data2 + offset);
    }

    return offset;
}

template <typename T, typename U>
size_t desync_compare_unordered_map(uint8_t* data, uint8_t* data2) {
    size_t map_size;
    size_t offset = desync_compare_value<size_t>(data, data2, &map_size);

    for (size_t index = 0; index < map_size; index++) {
        offset += desync_compare_value<T>(data, data2);
        offset += desync_compare_value<U>(data, data2);
    }

    return offset;
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
    state_buffer_offset += desync_compare_vector<Tile>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map cell layers
    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        state_buffer_offset += desync_compare_vector<Cell>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Map regions
    state_buffer_offset += desync_compare_vector<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map region connections
    size_t region_connections_size;
    state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &region_connections_size);
    for (size_t region = 0; region < region_connections_size; region++) {
        for (size_t other_region = 0; other_region < region_connections_size; other_region++) {
            state_buffer_offset += desync_compare_vector<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        }
    }

    // Fog 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        state_buffer_offset += desync_compare_vector<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Detection 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        state_buffer_offset += desync_compare_vector<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Remembered entities
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        state_buffer_offset += desync_compare_unordered_map<EntityId, RememberedEntity>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    state_buffer_offset += desync_compare_value<bool>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Entities
    size_t entities_size;
    state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &entities_size);
    for (uint32_t entity_index = 0; entity_index < entities_size; entity_index++) {
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<fvec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<Animation>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<Target>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<Target>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<BuildingQueueItem>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<Animation>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Particles
    for (size_t layer = 0; layer < PARTICLE_LAYER_COUNT; layer++) {
        state_buffer_offset += desync_compare_vector<Particle>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Projectiles, fire, fire cells, fog reveals
    state_buffer_offset += desync_compare_vector<Projectile>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_vector<Fire>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_vector<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_vector<FogReveal>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Players
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state_buffer_offset += desync_compare_value<MatchPlayer>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    // Bots
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        // Player ID
        state_buffer_offset += desync_compare_value<uint8_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<Difficulty>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Is entity reserved
        state_buffer_offset += desync_compare_unordered_map<EntityId, bool>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Production
        state_buffer_offset += desync_compare_value<BotUnitComp>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<BotUnitComp>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<EntityCount>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<EntityCount>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<BotDesiredSquad>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Squads
        size_t squad_size;
        state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &squad_size);
        for (size_t index = 0; index < squad_size; index++) {
            state_buffer_offset += desync_compare_value<BotSquadType>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
            state_buffer_offset += desync_compare_value<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
            state_buffer_offset += desync_compare_vector<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        }

        // Next landmine time
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Scouting
        state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_unordered_map<EntityId, bool>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Base info
        state_buffer_offset += desync_compare_unordered_map<EntityId, BotBaseInfo>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

        // Retreat memory
        size_t retreat_memory_size;
        state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &retreat_memory_size);
        for (size_t index = 0; index < retreat_memory_size; index++) {
            state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
            state_buffer_offset += desync_compare_vector<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
            state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
            state_buffer_offset += desync_compare_value<uint32_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        }
    }
}

#endif
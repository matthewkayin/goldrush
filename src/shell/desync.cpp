#include "desync.h"

#include "core/filesystem.h"
#include "core/logger.h"
#include <algorithm>
#include <cstdlib>

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

struct DynamicBuffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
};

DynamicBuffer* dynamic_buffer_init() {
    DynamicBuffer* buffer = (DynamicBuffer*)malloc(sizeof(DynamicBuffer));
    buffer->size = 0;
    buffer->capacity = 1024ULL * 1024ULL;
    buffer->data = (uint8_t*)malloc(buffer->capacity);

    return buffer;
}

void dynamic_buffer_write(DynamicBuffer* buffer, uint8_t* data, size_t length) {
    if (buffer->size + length > buffer->capacity) {
        uint8_t* temp = buffer->data;
        buffer->capacity *= 2;
        buffer->data = (uint8_t*)malloc(buffer->capacity);
        memcpy(buffer->data, temp, buffer->size);
        free(temp);
    }

    memcpy(buffer->data + buffer->size, data, length);
    buffer->size += length;
}

template <typename T>
void dynamic_buffer_write_value(DynamicBuffer* buffer, const T& value) {
    dynamic_buffer_write(buffer, (uint8_t*)&value, sizeof(T));
}

template <typename T>
void dynamic_buffer_write_vector(DynamicBuffer* buffer, const std::vector<T>& vector_value) {
    dynamic_buffer_write_value<size_t>(buffer, vector_value.size());
    if (!vector_value.empty()) {
        dynamic_buffer_write(buffer, (uint8_t*)&vector_value[0], vector_value.size() * sizeof(T));
    }
}

void dynamic_buffer_clear(DynamicBuffer* buffer) {
    buffer->size = 0;
}

void dynamic_buffer_free(DynamicBuffer* buffer) {
    free(buffer->data);
    free(buffer);
}

struct DesyncState {
    std::string desync_file_path;
    DynamicBuffer* buffer;
    FILE* desync_file;
};
static DesyncState state;

bool match_desync_init(const char* desync_filename) {
    state.buffer = dynamic_buffer_init();
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

void match_desync_quit() {
    dynamic_buffer_free(state.buffer);
    fclose(state.desync_file);
}

uint32_t compute_checksum(uint8_t* data, size_t length) {
    static const uint32_t MOD_ADLER = 65521;

    uint32_t a = 1;
    uint32_t b = 0;

    for (size_t index = 0; index < length; index++) {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}

uint32_t match_serialize(const MatchState& match_state) {
    // LCG seed
    dynamic_buffer_write_value<int>(state.buffer, match_state.lcg_seed);

    // Map
    dynamic_buffer_write_value<int>(state.buffer, match_state.map.width);
    dynamic_buffer_write_value<int>(state.buffer, match_state.map.height);

    dynamic_buffer_write_vector<Tile>(state.buffer, match_state.map.tiles);

    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        dynamic_buffer_write_vector<Cell>(state.buffer, match_state.map.cells[layer]);
    }

    dynamic_buffer_write_value<int>(state.buffer, match_state.map.pathing_region_count);
    dynamic_buffer_write_vector<int>(state.buffer, match_state.map.pathing_regions);

    dynamic_buffer_write_value<size_t>(state.buffer, match_state.map.pathing_region_connection_indices.size());
    for (size_t index = 0; index < match_state.map.pathing_region_connection_indices.size(); index++) {
        dynamic_buffer_write_vector<int>(state.buffer, match_state.map.pathing_region_connection_indices[index]);
    }

    dynamic_buffer_write_value<size_t>(state.buffer, match_state.map.pathing_region_connections.size());
    for (size_t index = 0; index < match_state.map.pathing_region_connections.size(); index++) {
        dynamic_buffer_write_vector<ivec2>(state.buffer, match_state.map.pathing_region_connections[index].left);
        dynamic_buffer_write_vector<ivec2>(state.buffer, match_state.map.pathing_region_connections[index].right);
    }

    // Fog 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        dynamic_buffer_write_vector<int>(state.buffer, match_state.fog[player]);
    }

    // Detection 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        dynamic_buffer_write_vector<int>(state.buffer, match_state.detection[player]);
    }

    // Remembered entities
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        dynamic_buffer_write_value<size_t>(state.buffer, match_state.remembered_entities[player].size());

        std::vector<EntityId> keys;
        for (auto it : match_state.remembered_entities[player]) {
            keys.push_back(it.first);
        }
        std::sort(keys.begin(), keys.end());

        for (const EntityId key : keys) {
            dynamic_buffer_write_value<EntityId>(state.buffer, key);
            dynamic_buffer_write_value<RememberedEntity>(state.buffer, match_state.remembered_entities[player].at(key));
        }
    }

    dynamic_buffer_write_value<bool>(state.buffer, match_state.is_fog_dirty);

    // Entities
    dynamic_buffer_write_value<size_t>(state.buffer, match_state.entities.size());
    for (uint32_t entity_index = 0; entity_index < match_state.entities.size(); entity_index++) {
        dynamic_buffer_write_value<EntityId>(state.buffer, match_state.entities.get_id_of(entity_index));

        const Entity& entity = match_state.entities[entity_index];
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.type);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.mode);
        dynamic_buffer_write_value<uint8_t>(state.buffer, entity.player_id);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.flags);
        dynamic_buffer_write_value<ivec2>(state.buffer, entity.cell);
        dynamic_buffer_write_value<fvec2>(state.buffer, entity.position);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.direction);
        dynamic_buffer_write_value<int>(state.buffer, entity.health);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.energy);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.timer);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.energy_regen_timer);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.health_regen_timer);
        dynamic_buffer_write_value<Animation>(state.buffer, entity.animation);
        dynamic_buffer_write_vector<EntityId>(state.buffer, entity.garrisoned_units);
        dynamic_buffer_write_value<EntityId>(state.buffer, entity.garrison_id);
        dynamic_buffer_write_value<EntityId>(state.buffer, entity.goldmine_id);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.gold_held);
        dynamic_buffer_write_value<Target>(state.buffer, entity.target);
        dynamic_buffer_write_vector<Target>(state.buffer, entity.target_queue);
        dynamic_buffer_write_vector<ivec2>(state.buffer, entity.path);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.pathfind_attempts);
        dynamic_buffer_write_vector<BuildingQueueItem>(state.buffer, entity.queue);
        dynamic_buffer_write_value<ivec2>(state.buffer, entity.rally_point);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.cooldown_timer);
        dynamic_buffer_write_value<ivec2>(state.buffer, entity.attack_move_cell);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.taking_damage_counter);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.taking_damage_timer);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.fire_damage_timer);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.bleed_timer);
        dynamic_buffer_write_value<uint32_t>(state.buffer, entity.bleed_damage_timer);
        dynamic_buffer_write_value<Animation>(state.buffer, entity.bleed_animation);
    }

    // Particles
    for (size_t layer = 0; layer < PARTICLE_LAYER_COUNT; layer++) {
        dynamic_buffer_write_vector<Particle>(state.buffer, match_state.particles[layer]);
    }

    // Projectiles, fire, fire cells, fog reveals
    dynamic_buffer_write_vector<Projectile>(state.buffer, match_state.projectiles);
    dynamic_buffer_write_vector<Fire>(state.buffer, match_state.fires);
    dynamic_buffer_write_vector<int>(state.buffer, match_state.fire_cells);
    dynamic_buffer_write_vector<FogReveal>(state.buffer, match_state.fog_reveals);

    // Players
    dynamic_buffer_write(state.buffer, (uint8_t*)match_state.players, MAX_PLAYERS * sizeof(MatchPlayer));

    // Write to desync file
    fwrite(&state.buffer->size, sizeof(state.buffer->size), 1, state.desync_file);
    fwrite(state.buffer->data, state.buffer->size, 1, state.desync_file);

    uint32_t checksum = compute_checksum(state.buffer->data, state.buffer->size);

    dynamic_buffer_clear(state.buffer);

    // Compute and return checksum
    return checksum;
}

uint8_t* match_read_serialized_frame(uint32_t frame_number, size_t* state_buffer_length) {
    fclose(state.desync_file);
    state.desync_file = fopen(state.desync_file_path.c_str(), "rb");
    if (state.desync_file == NULL) {
        return NULL;
    }

    uint32_t current_frame_number = 0;
    while (current_frame_number != frame_number) {
        size_t block_size;
        fread(&block_size, sizeof(size_t), 1, state.desync_file);

        dynamic_buffer_clear(state.buffer);
        if (state.buffer->capacity < block_size) {
            state.buffer->capacity = block_size;
            free(state.buffer->data);
            state.buffer->data = (uint8_t*)malloc(state.buffer->capacity);
            if (state.buffer->data == NULL) {
                fclose(state.desync_file);
                return NULL;
            }
        }
        fread(state.buffer->data, block_size, 1, state.desync_file);

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

void desync_compare_frames(uint8_t* state_buffer, uint8_t* state_buffer2) {
    size_t state_buffer_offset = 0;

    // LCG seed
    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    // Map
    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    state_buffer_offset += desync_compare_vector<Tile>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        state_buffer_offset += desync_compare_vector<Cell>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    state_buffer_offset += desync_compare_value<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    state_buffer_offset += desync_compare_vector<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);

    size_t pathing_region_connection_indices_size;
    state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &pathing_region_connection_indices_size);
    for (size_t index = 0; index < pathing_region_connection_indices_size; index++) {
        state_buffer_offset += desync_compare_vector<int>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
    }

    size_t pathing_region_connections_size;
    state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &pathing_region_connections_size);
    for (size_t index = 0; index < pathing_region_connections_size; index++) {
        state_buffer_offset += desync_compare_vector<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        state_buffer_offset += desync_compare_vector<ivec2>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
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
        size_t remembered_entities_size;
        state_buffer_offset += desync_compare_value<size_t>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset, &remembered_entities_size);

        for (size_t index = 0; index < remembered_entities_size; index++) {
            state_buffer_offset += desync_compare_value<EntityId>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
            state_buffer_offset += desync_compare_value<RememberedEntity>(state_buffer + state_buffer_offset, state_buffer2 + state_buffer_offset);
        }
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
}
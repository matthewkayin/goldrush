#include "desync.h"

#include "core/filesystem.h"
#include <algorithm>

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

    memcpy(buffer->data, data, length);
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
    DynamicBuffer* buffer;
    FILE* desync_file;
};
static DesyncState state;

bool match_desync_init(const char* desync_filename) {
    state.buffer = dynamic_buffer_init();
    if (state.buffer == NULL) {
        return false;
    }

    std::string desync_file_path = filesystem_get_data_path() + FILESYSTEM_LOG_FOLDER_NAME + desync_filename + ".desync";
    state.desync_file = fopen(desync_file_path.c_str(), "wb");
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

uint32_t match_checksum(uint32_t frame_number, const MatchState& match_state) {
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
    fwrite(&frame_number, sizeof(frame_number), 1, state.desync_file);
    fwrite(&state.buffer->size, sizeof(state.buffer->size), 1, state.desync_file);
    fwrite(state.buffer->data, state.buffer->size, 1, state.desync_file);

    uint32_t checksum = compute_checksum(state.buffer->data, state.buffer->size);

    dynamic_buffer_clear(state.buffer);

    // Compute and return checksum
    return checksum;
}
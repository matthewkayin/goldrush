#include "desync.h"

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

void match_checksum_write(uint32_t* a, uint32_t* b, uint8_t* data, size_t length) {
    static const uint32_t MOD_ADLER = 65521;
    for (size_t index = 0; index < length; index++) {
        *a = (*a + data[index]) % MOD_ADLER;
        *b = (*b + *a) % MOD_ADLER;
    }
}

template <typename T>
void match_checksum_write_value(uint32_t* a, uint32_t* b, const T& value) {
    match_checksum_write(a, b, (uint8_t*)&value, sizeof(T));
}

template <typename T>
void match_checksum_write_vector(uint32_t* a, uint32_t* b, const std::vector<T>& vector_value) {
    match_checksum_write_value<size_t>(a, b, vector_value.size());
    match_checksum_write(a, b, (uint8_t*)&vector_value[0], sizeof(T) * vector_value.size());
}

uint32_t match_checksum(const MatchState& state) {
    uint32_t a = 1;
    uint32_t b = 0;

    // LCG seed
    match_checksum_write_value<int>(&a, &b, state.lcg_seed);

    // Map
    match_checksum_write_value<int>(&a, &b, state.map.width);
    match_checksum_write_value<int>(&a, &b, state.map.height);

    match_checksum_write_vector<Tile>(&a, &b, state.map.tiles);

    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        match_checksum_write_vector<Cell>(&a, &b, state.map.cells[layer]);
    }

    match_checksum_write_value<int>(&a, &b, state.map.pathing_region_count);
    match_checksum_write_vector<int>(&a, &b, state.map.pathing_regions);

    match_checksum_write_value<size_t>(&a, &b, state.map.pathing_region_connection_indices.size());
    for (size_t index = 0; index < state.map.pathing_region_connection_indices.size(); index++) {
        match_checksum_write_vector<int>(&a, &b, state.map.pathing_region_connection_indices[index]);
    }

    match_checksum_write_value<size_t>(&a, &b, state.map.pathing_region_connections.size());
    for (size_t index = 0; index < state.map.pathing_region_connections.size(); index++) {
        match_checksum_write_vector<ivec2>(&a, &b, state.map.pathing_region_connections[index].left);
        match_checksum_write_vector<ivec2>(&a, &b, state.map.pathing_region_connections[index].right);
    }

    // Fog 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        match_checksum_write_vector<int>(&a, &b, state.fog[player]);
    }

    // Detection 
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        match_checksum_write_vector<int>(&a, &b, state.detection[player]);
    }

    // Remembered entities
    for (size_t player = 0; player < MAX_PLAYERS; player++) {
        match_checksum_write_value<size_t>(&a, &b, state.remembered_entities[player].size());

        std::vector<EntityId> keys;
        for (auto it : state.remembered_entities[player]) {
            keys.push_back(it.first);
        }
        std::sort(keys.begin(), keys.end());

        for (const EntityId key : keys) {
            match_checksum_write_value<EntityId>(&a, &b, key);
            match_checksum_write_value<RememberedEntity>(&a, &b, state.remembered_entities[player].at(key));
        }
    }

    match_checksum_write_value<bool>(&a, &b, state.is_fog_dirty);

    // Entities
    match_checksum_write_value<size_t>(&a, &b, state.entities.size());
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        match_checksum_write_value<EntityId>(&a, &b, state.entities.get_id_of(entity_index));

        const Entity& entity = state.entities[entity_index];
        match_checksum_write_value<uint32_t>(&a, &b, entity.type);
        match_checksum_write_value<uint32_t>(&a, &b, entity.mode);
        match_checksum_write_value<uint8_t>(&a, &b, entity.player_id);
        match_checksum_write_value<uint32_t>(&a, &b, entity.flags);
        match_checksum_write_value<ivec2>(&a, &b, entity.cell);
        match_checksum_write_value<fvec2>(&a, &b, entity.position);
        match_checksum_write_value<uint32_t>(&a, &b, entity.direction);
        match_checksum_write_value<int>(&a, &b, entity.health);
        match_checksum_write_value<uint32_t>(&a, &b, entity.energy);
        match_checksum_write_value<uint32_t>(&a, &b, entity.timer);
        match_checksum_write_value<uint32_t>(&a, &b, entity.energy_regen_timer);
        match_checksum_write_value<uint32_t>(&a, &b, entity.health_regen_timer);
        match_checksum_write_value<Animation>(&a, &b, entity.animation);
        match_checksum_write_vector<EntityId>(&a, &b, entity.garrisoned_units);
        match_checksum_write_value<EntityId>(&a, &b, entity.garrison_id);
        match_checksum_write_value<EntityId>(&a, &b, entity.goldmine_id);
        match_checksum_write_value<uint32_t>(&a, &b, entity.gold_held);
        match_checksum_write_value<Target>(&a, &b, entity.target);
        match_checksum_write_vector<Target>(&a, &b, entity.target_queue);
        match_checksum_write_vector<ivec2>(&a, &b, entity.path);
        match_checksum_write_value<uint32_t>(&a, &b, entity.pathfind_attempts);
        match_checksum_write_vector<BuildingQueueItem>(&a, &b, entity.queue);
        match_checksum_write_value<ivec2>(&a, &b, entity.rally_point);
        match_checksum_write_value<uint32_t>(&a, &b, entity.cooldown_timer);
        match_checksum_write_value<ivec2>(&a, &b, entity.attack_move_cell);
        match_checksum_write_value<uint32_t>(&a, &b, entity.taking_damage_counter);
        match_checksum_write_value<uint32_t>(&a, &b, entity.taking_damage_timer);
        match_checksum_write_value<uint32_t>(&a, &b, entity.fire_damage_timer);
        match_checksum_write_value<uint32_t>(&a, &b, entity.bleed_timer);
        match_checksum_write_value<uint32_t>(&a, &b, entity.bleed_damage_timer);
        match_checksum_write_value<Animation>(&a, &b, entity.bleed_animation);
    }

    // Particles
    for (size_t layer = 0; layer < PARTICLE_LAYER_COUNT; layer++) {
        match_checksum_write_vector<Particle>(&a, &b, state.particles[layer]);
    }

    // Projectiles, fire, fire cells, fog reveals
    match_checksum_write_vector<Projectile>(&a, &b, state.projectiles);
    match_checksum_write_vector<Fire>(&a, &b, state.fires);
    match_checksum_write_vector<int>(&a, &b, state.fire_cells);
    match_checksum_write_vector<FogReveal>(&a, &b, state.fog_reveals);

    // Players
    match_checksum_write(&a, &b, (uint8_t*)state.players, MAX_PLAYERS * sizeof(MatchPlayer));

    
    uint32_t checksum = (b << 16) | a;

    return checksum;
}
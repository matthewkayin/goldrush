#include "entity_count.h"

EntityCount::EntityCount() {
    entity_count.fill(0);
}

uint32_t EntityCount::size() const {
    uint32_t _size = 0;
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        _size += entity_count[entity_type];
    }

    return _size;
}

uint32_t EntityCount::size_units_only() const {
    uint32_t _size = 0;
    for (uint32_t entity_type = ENTITY_MINER; entity_type < ENTITY_HALL; entity_type++) {
        _size += entity_count[entity_type];
    }

    return _size;
}

uint32_t EntityCount::size_buildings_only() const {
    uint32_t _size = 0;
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_LANDMINE; entity_type++) {
        _size += entity_count[entity_type];
    }
    
    return _size;
}

uint32_t& EntityCount::operator[](uint32_t entity_type) {
    GOLD_ASSERT(entity_type < ENTITY_TYPE_COUNT);
    return entity_count[entity_type];
}

uint32_t EntityCount::operator[](uint32_t entity_type) const {
    GOLD_ASSERT(entity_type < ENTITY_TYPE_COUNT);
    return entity_count[entity_type];
}

bool EntityCount::is_empty() const {
    return size() == 0;
}

bool EntityCount::is_gte_to(const EntityCount& other) const {
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (entity_count[entity_type] < other[entity_type]) {
            return false;
        }
    }

    return true;
}

void EntityCount::clear() {
    memset(&entity_count[0], 0, sizeof(entity_count));
}

EntityCount EntityCount::add(const EntityCount& other) const {
    EntityCount result;
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        result[entity_type] = entity_count[entity_type] + other[entity_type];
    }

    return result;
}

EntityCount EntityCount::subtract(const EntityCount& other) const {
    EntityCount result;
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (entity_count[entity_type] > other[entity_type]) {
            result[entity_type] = entity_count[entity_type] - other[entity_type];
        } else {
            result[entity_type] = 0;
        }
    }

    return result;
}

EntityCount EntityCount::select(const std::vector<EntityType>& selected_types) const {
    EntityCount count;

    bool should_include[ENTITY_TYPE_COUNT];
    memset(should_include, 0, sizeof(should_include));
    for (EntityType type : selected_types) {
        should_include[type] = true;
    }

    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (should_include[entity_type]) {
            count[entity_type] = entity_count[entity_type];
        }
    }

    return count;
}

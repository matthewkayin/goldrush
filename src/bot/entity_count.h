#pragma once

#include "match/entity.h"
#include <array>

class EntityCount {
public:
    EntityCount();
    uint32_t& operator[](uint32_t entity_type);
    uint32_t operator[](uint32_t entity_type) const;
    uint32_t size() const;
    uint32_t size_units_only() const;
    uint32_t size_buildings_only() const;
    bool is_empty() const;
    bool is_gte_to(const EntityCount& other) const;
    void clear();
    EntityCount add(const EntityCount& other) const;
    EntityCount subtract(const EntityCount& other) const;
    EntityCount select(const std::vector<EntityType>& selected_types) const;
private:
    std::array<uint32_t, ENTITY_TYPE_COUNT> entity_count;
};
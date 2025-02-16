#pragma once

#include "asserts.h"
#include "defines.h"
#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>

template <typename T, size_t capacity>
struct id_array {
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    std::vector<T> data;
    std::vector<entity_id> ids;
    std::queue<entity_id> available_ids;
    std::unordered_map<entity_id, uint32_t> id_to_index;

    id_array() {
        for (entity_id id = 0; id < ID_MAX; id++) {
            available_ids.push(id);
        }
        data.reserve(capacity);
        ids.reserve(capacity);
    }

    T& operator[](uint32_t index) { 
        GOLD_ASSERT(index != INDEX_INVALID); 
        return data[index]; 
    }
    const T& operator[](uint32_t index) const { 
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index]; 
    }
    T& get_by_id(entity_id id) {
        uint32_t index = get_index_of(id);
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index];
    }
    const T& get_by_id(entity_id id) const {
        uint32_t index = get_index_of(id);
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index];
    }

    uint32_t get_index_of(entity_id id) const {
        auto index_it = id_to_index.find(id);
        if (index_it == id_to_index.end()) {
            return INDEX_INVALID;
        }
        return index_it->second;
    }
    entity_id get_id_of(uint32_t index) const {
        return ids[index];
    }

    iterator begin() { return data.begin(); }
    const_iterator begin() const { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator end() const { return data.end(); }

    size_t size() const { return data.size(); }

    entity_id push_back(const T& value) {
        GOLD_ASSERT(!available_ids.empty());
        GOLD_ASSERT(data.size() < capacity);

        entity_id id = available_ids.front();
        available_ids.pop();
        id_to_index[id] = data.size();
        ids.push_back(id);
        data.push_back(value);

        return id;
    }

    void remove_at(uint32_t index) {
        // remove the mapping for this id
        entity_id id = ids[index];
        id_to_index.erase(id);

        // update the mapping of elements whose index got shifted
        for (uint32_t other_index = index + 1; other_index < data.size(); other_index++) {
            entity_id other_id = ids[other_index];
            id_to_index[other_id] = other_index - 1;
        }

        // remove the actual element
        data.erase(data.begin() + index);
        ids.erase(ids.begin() + index);

        // add the id back into the free list
        available_ids.push(id);
    }
};
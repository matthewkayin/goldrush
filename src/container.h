#pragma once

#include "asserts.h"
#include "defines.h"
#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>

template <typename T>
struct id_array {
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    static const id_t ID_MAX = 512;
    static const uint32_t INDEX_INVALID = 65535;

    std::vector<T> data;
    std::vector<id_t> ids;
    std::queue<id_t> available_ids;
    std::unordered_map<id_t, uint32_t> id_to_index;

    id_array() {
        for (id_t id = 0; id < ID_MAX; id++) {
            available_ids.push(id);
        }
    }

    T& operator[](uint32_t index) { 
        GOLD_ASSERT(index != INDEX_INVALID); 
        return data[index]; 
    }
    const T& operator[](uint8_t index) const { 
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index]; 
    }

    uint32_t get_index_of(id_t id) {
        auto index_it = id_to_index.find(id);
        if (index_it == id_to_index.end()) {
            return INDEX_INVALID;
        }
        return index_it->second;
    }
    id_t get_id_of(uint32_t index) {
        return ids[index];
    }

    iterator begin() { return data.begin(); }
    const_iterator begin() const { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator end() const { return data.end(); }

    size_t size() const { return data.size(); }

    id_t push_back(const T& value) {
        GOLD_ASSERT(!available_ids.empty());

        id_t id = available_ids.front();
        available_ids.pop();
        id_to_index[id] = data.size();
        ids.push_back(id);
        data.push_back(value);

        return id;
    }

    void remove_by_id(id_t id) {
        auto index_it = id_to_index.find(id);
        GOLD_ASSERT(index_it != id_to_index.end());

        // remove the mapping for this id
        uint32_t index = index_it->second;
        id_to_index.erase(id);

        // update the mapping of elements whose index got shifted
        for (uint32_t other_index = index + 1; other_index < data.size(); other_index++) {
            id_t other_id = ids[other_index];
            id_to_index[other_id] = other_index - 1;
        }

        // remove the actual element
        data.erase(data.begin() + index);
        ids.erase(ids.begin() + index);

        // add the id back into the free list
        available_ids.push(id);
    }
};
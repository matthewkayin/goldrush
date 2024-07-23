#pragma once

#include "asserts.h"
#include "defines.h"
#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>

typedef uint16_t entity_id;

template <typename T>
struct id_array {
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    static const entity_id ID_MAX = 4096;
    static const uint32_t INVALID_INDEX = 65535;

    std::vector<T> data;
    std::vector<entity_id> ids;
    std::queue<entity_id> available_ids;
    std::unordered_map<entity_id, uint32_t> id_to_index;

    id_array() {
        for (entity_id id = 0; id < ID_MAX; id++) {
            available_ids.push(id);
        }
    }

    T& operator[](uint32_t index) { 
        GOLD_ASSERT(index != INVALID_INDEX); 
        return data[index]; 
    }
    const T& operator[](uint32_t index) const { 
        GOLD_ASSERT(index != INVALID_INDEX);
        return data[index]; 
    }

    uint32_t get_index_of(entity_id id) const {
        auto index_it = id_to_index.find(id);
        if (index_it == id_to_index.end()) {
            return INVALID_INDEX;
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

template <typename T, uint32_t capacity>
struct circular_vector {
    T data[capacity];
    uint32_t head = 0;
    uint32_t tail = 0;

    circular_vector(): head(0), tail(0) {}

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index < size());
        return data[(tail + index) % capacity];
    }

    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < size());
        return data[(tail + index) % capacity];
    }

    uint32_t size() const {
        if (head >= tail) {
            return head - tail;
        } else {
            return (capacity - tail) + head;
        }
    }

    void push_back(const T& value) {
        uint32_t next_index = (head + 1) % capacity;
        // If the array is full, delete the last element to make room for this new one
        if (next_index == tail) {
            tail = (tail + 1) % capacity;
        }
        data[head] = value;
        head = next_index;
    }

    void pop_front() {
        tail = (tail + 1) % capacity;
    }
};
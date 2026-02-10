#pragma once

#include "defines.h"
#include "core/asserts.h"
#include <cstdint>
#include <array>

template <typename T, size_t capacity>
class IdArray {
    using iterator = typename std::array<T, capacity>::iterator;
    using const_iterator = typename std::array<T, capacity>::const_iterator;

private:
    std::array<T, capacity> data;
    EntityId ids[capacity];
    uint16_t id_to_index[ID_MAX];
    uint32_t _size;
    EntityId next_id;
public:
    IdArray() {
        for (uint32_t id = 0; id < ID_MAX; id++) {
            id_to_index[id] = (uint16_t)INDEX_INVALID;
        }
        _size = 0;
        next_id = 0;
    }

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index];
    }
    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index];
    }
    T& get_by_id(EntityId id) {
        uint32_t index = get_index_of(id);
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index];
    }
    const T& get_by_id(EntityId id) const {
        uint32_t index = get_index_of(id);
        GOLD_ASSERT(index != INDEX_INVALID);
        return data[index];
    }

    uint32_t get_index_of(EntityId id) const {
        if (id == ID_NULL) {
            return INDEX_INVALID;
        }
        return id_to_index[id];
    }
    EntityId get_id_of(uint32_t index) const {
        return ids[index];
    }

    iterator begin() { return data.begin(); }
    const_iterator begin() const { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator end() const { return data.end(); }

    size_t size() const { return _size; }

    EntityId push_back(const T& value) {
        GOLD_ASSERT(_size < capacity);

        EntityId id = next_id;
        GOLD_ASSERT(next_id < ID_MAX - 1);
        next_id++;
        id_to_index[id] = _size;
        ids[_size] = id;
        data[_size] = value;
        _size++;

        return id;
    }

    void remove_at(uint32_t index) {
        // store the ID for later
        EntityId id = ids[index];

        // swap 
        data[index] = data[_size - 1];
        ids[index] = ids[_size - 1];
        id_to_index[ids[index]] = index;

        // and pop
        _size--;

        // remove the mapping for this id
        // this is done after so that if we end up "pop and swapping" with the last element, 
        // we still remove the mapping
        id_to_index[id] = INDEX_INVALID;
    }
};
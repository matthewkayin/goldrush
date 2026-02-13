#pragma once

#include "defines.h"
#include "core/asserts.h"
#include <cstdint>

template <typename T, size_t capacity>
class IdArray {

private:
    T data[capacity];
    EntityId ids[capacity];
    uint16_t id_to_index[ID_MAX];
    // id_to_index is not 4-byte aligned and the position of next_id serves as padding
    EntityId next_id;
    uint32_t _size;
public:
    static IdArray init() {
        IdArray array;
        memset(array.data, 0, sizeof(array.data));
        for (uint32_t index = 0; index < capacity; index++) {
            array.ids[index] = INDEX_INVALID;
        }
        for (uint32_t id = 0; id < ID_MAX; id++) {
            array.id_to_index[id] = (uint16_t)INDEX_INVALID;
        }
        array.next_id = 0;
        array._size = 0;

        return array;
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

    size_t size() const { 
        return _size; 
    }

    bool is_full() const {
        return _size == capacity;
    }

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
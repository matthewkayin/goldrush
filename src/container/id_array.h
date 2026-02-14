#pragma once

#include "defines.h"
#include "core/asserts.h"
#include <cstdint>

template <typename T, size_t capacity>
class IdArray {
public:
    struct InternalState {
        T data[capacity];
        EntityId ids[capacity];
        uint16_t id_to_index[ID_MAX];
        // id_to_index is not 4-byte aligned and the position of next_id serves as padding
        EntityId next_id;
        uint32_t size;
    };

    IdArray() {
        memset(state.data, 0, sizeof(state.data));
        for (uint32_t index = 0; index < capacity; index++) {
            state.ids[index] = INDEX_INVALID;
        }
        for (uint32_t id = 0; id < ID_MAX; id++) {
            state.id_to_index[id] = (uint16_t)INDEX_INVALID;
        }
        state.next_id = 0;
        state.size = 0;
    }

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index != INDEX_INVALID && index <= state.size);
        return state.data[index];
    }
    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index != INDEX_INVALID && index <= state.size);
        return state.data[index];
    }
    T& get_by_id(EntityId id) {
        uint32_t index = get_index_of(id);
        GOLD_ASSERT(index != INDEX_INVALID && index <= state.size);
        return state.data[index];
    }
    const T& get_by_id(EntityId id) const {
        uint32_t index = get_index_of(id);
        GOLD_ASSERT(index != INDEX_INVALID && index <= state.size);
        return state.data[index];
    }

    uint32_t get_index_of(EntityId id) const {
        if (id == ID_NULL) {
            return INDEX_INVALID;
        }
        return state.id_to_index[id];
    }
    EntityId get_id_of(uint32_t index) const {
        return state.ids[index];
    }

    size_t size() const { 
        return state.size; 
    }

    bool is_full() const {
        return state.size == capacity;
    }

    EntityId push_back(const T& value) {
        GOLD_ASSERT(state.size < capacity);

        EntityId id = state.next_id;
        GOLD_ASSERT(next_id < ID_MAX - 1);
        state.next_id++;
        state.id_to_index[id] = state.size;
        state.ids[state.size] = id;
        state.data[state.size] = value;
        state.size++;

        return id;
    }

    void remove_at(uint32_t index) {
        // store the ID for later
        EntityId id = state.ids[index];

        // swap 
        state.data[index] = state.data[state.size - 1];
        state.ids[index] = state.ids[state.size - 1];
        state.id_to_index[state.ids[index]] = index;

        // and pop
        state.size--;

        // remove the mapping for this id
        // this is done after so that if we end up "pop and swapping" with the last element, 
        // we still remove the mapping
        state.id_to_index[id] = INDEX_INVALID;
    }

    const InternalState* get_internal_state() const {
        return &state;
    }
private:
    InternalState state;
};
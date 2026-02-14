#pragma once

#include "core/asserts.h"
#include <cstdint>

template <typename T, uint32_t _capacity>
class FixedVector {
public:
    struct InternalState {
        T data[_capacity];
        uint32_t size;
    };

    FixedVector() {
        memset(state.data, 0, sizeof(state.data));
        state.size = 0;
    }

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index < state.size);
        return state.data[index];
    }
    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < state.size);
        return state.data[index];
    }

    const T& back() const {
        GOLD_ASSERT(state.size != 0);
        return state.data[state.size - 1];
    }

    const T& get_from_top(uint32_t index) {
        GOLD_ASSERT(index < state.size);
        return state.data[state.size - 1 - index];
    }

    uint32_t size() const {
        return state.size;
    }

    uint32_t capacity() const {
        return _capacity;
    }

    void push_back(T value) {
        GOLD_ASSERT(state.size < _capacity);
        state.data[state.size] = value;
        state.size++;
    }

    void pop_back() {
        GOLD_ASSERT(state.size != 0);
        state.size--;
    }

    // Removes an element with pop and swap idiom
    // This is fast, but does not preserve array order
    void remove_at_unordered(uint32_t index) {
        GOLD_ASSERT(state.size != 0 && index < state.size);
        state.data[index] = state.data[state.size - 1];
        state.size--;
    }

    // Removes an element while keeping array order intact
    // This is slower than remove_at_unordered()
    void remove_at_ordered(uint32_t index) {
        GOLD_ASSERT(state.size != 0 && index < state.size);
        for (uint32_t replace_index = index; replace_index < state.size - 1; replace_index++) {
            state.data[replace_index] = state.data[replace_index + 1];
        }
        state.size--;
    }

    void clear() {
        state.size = 0;
    }

    bool empty() const {
        return state.size == 0;
    }

    bool is_full() const {
        return state.size == _capacity;
    }

    bool contains(T value) const {
        for (uint32_t index = 0; index < state.size; index++) {
            if (state.data[index] == value) {
                return true;
            }
        }

        return false;
    }

    const InternalState* get_internal_state() const {
        return &state;
    }
private:
    InternalState state;
};
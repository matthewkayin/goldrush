#pragma once

#include "defines.h"
#include "core/asserts.h"
#include "core/logger.h"
#include <cstdint>

template <typename T, uint32_t capacity>
class CircularVector {
public:
    struct InternalState {
        T data[capacity];
        uint32_t tail;
        uint32_t size;
    };

    CircularVector() {
        memset(state.data, 0, sizeof(state.data));
        state.tail = 0;
        state.size = 0;
    }

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index < state.size);
        return state.data[wrap_index(index)];
    }
    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < state.size);
        return state.data[wrap_index(index)];
    }

    uint32_t size() const {
        return state.size;
    }

    bool empty() const {
        return state.size == 0;
    }

    void push_back(T value) {
    #ifdef GOLD_DEBUG
        if (state.size == capacity) {
            log_warn("Circular vector with capacity %u and element size %u is eating itself.", capacity, sizeof(T));
        }
    #endif
        state.data[wrap_index(state.size)] = value;
        if (state.size == capacity) {
            state.tail = state.tail == capacity - 1
                ? 0
                : state.tail + 1;
        } else {
            state.size++;
        }
    }

    void remove_at_unordered(uint32_t index) {
        GOLD_ASSERT(state.size != 0 && index < state.size);
        state.data[wrap_index(index)] = state.data[wrap_index(state.size - 1)];
        state.size--;
    }

    void clear() {
        state.size = 0;
    }

    const InternalState* get_internal_state() const {
        return &state;
    }
private:
    InternalState state;

    uint32_t wrap_index(uint32_t index) const {
        uint32_t wrapped_index = state.tail + index;
        if (wrapped_index >= capacity) {
            wrapped_index -= capacity;
        }
        return wrapped_index;
    }
};
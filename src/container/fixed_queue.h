#pragma once

#include "core/asserts.h"
#include <cstdint>

template <typename T, uint32_t capacity>
class FixedQueue {
public:
    struct InternalState {
        T data[capacity];
        uint32_t tail;
        uint32_t size;
    };

    FixedQueue() {
        memset(state.data, 0, sizeof(state.data));
        state.tail = 0;
        state.size = 0;
    }

    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < size());
        return state.data[wrap_index(index)];
    }

    const T& front() const {
        return state.data[0];
    }

    uint32_t size() const {
        return state.size;
    }

    void push(T value) {
        GOLD_ASSERT(state.size + 1 <= capacity);
        data[wrap_index(state.size)] = value;
        state.size++;
    }

    void pop() {
        GOLD_ASSERT(state.size > 0);
        state.tail = state.tail == capacity - 1
            ? 0
            : state.tail + 1;
        state.size--;
    }

    void clear() {
        state.size = 0;
    }

    bool empty() const {
        return state.size == 0;
    }

    bool is_full() const {
        return state.size == capacity;
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
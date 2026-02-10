#pragma once

#include "core/asserts.h"
#include <cstdint>

template <typename T, uint32_t capacity>
class FixedQueue {
private:
    T data[capacity];
    uint32_t tail;
    uint32_t _size;

    uint32_t wrap_index(uint32_t index) const {
        uint32_t wrapped_index = tail + index;
        if (wrapped_index >= capacity) {
            wrapped_index -= capacity;
        }
        return wrapped_index;
    }
public:
    FixedQueue() {
        tail = 0;
        _size = 0;
    }

    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < size());
        return data[wrap_index(index)];
    }

    uint32_t size() const {
        return _size;
    }

    void push(T value) {
        GOLD_ASSERT(_size + 1 <= capacity);
        data[wrap_index(_size)] = value;
        _size++;
    }

    void pop() {
        GOLD_ASSERT(_size > 0);
        tail = tail == capacity - 1
            ? 0
            : tail + 1;
        _size--;
    }

    void clear() {
        _size = 0;
    }

    bool empty() const {
        return _size == 0;
    }

    bool is_full() const {
        return _size == capacity;
    }
};
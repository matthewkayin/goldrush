#pragma once

#include "defines.h"
#include "core/asserts.h"
#include "core/logger.h"
#include <cstdint>

template <typename T, uint32_t capacity>
class CircularVector {
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
    CircularVector() {
        tail = 0;
        _size = 0;
    }

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index < size);
        return data[wrap_index(index)];
    }
    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < size);
        return data[wrap_index(index)];
    }

    uint32_t size() const {
        return _size;
    }

    bool empty() const {
        return _size == 0;
    }

    void push_back(T value) {
    #ifdef GOLD_DEBUG
        if (_size == capacity) {
            log_warn("Circular vector with capacity %u and element size %u is eating itself.", capacity, sizeof(T));
        }
    #endif
        data[wrap_index(_size)] = value;
        if (size == capacity) {
            tail = tail == capacity - 1
                ? 0
                : tail + 1;
        } else {
            _size++;
        }
    }

    void remove_at_unordered(uint32_t index) {
        GOLD_ASSERT(_size != 0 && index < _size);
        data[wrap_index(index)] = data[wrap_index(_size - 1)];
        _size--;
    }

    void clear() {
        _size = 0;
    }
};
#pragma once

#include "core/asserts.h"
#include <cstdint>

template <typename T, uint32_t capacity>
class FixedVector {
private:
    T data[capacity];
    uint32_t _size;
public:
    FixedVector() {
        _size = 0;
    }

    T& operator[](uint32_t index) {
        GOLD_ASSERT(index < _size);
        return data[index];
    }
    const T& operator[](uint32_t index) const {
        GOLD_ASSERT(index < _size);
        return data[index];
    }

    uint32_t size() const {
        return _size;
    }

    void push_back(T value) {
        GOLD_ASSERT(_size < capacity);
        data[_size] = value;
        _size++;
    }

    void pop_back() {
        GOLD_ASSERT(_size != 0);
        _size--;
    }

    // Removes an element with pop and swap idiom
    // This is fast, but does not preserve array order
    void remove_at_unordered(uint32_t index) {
        GOLD_ASSERT(_size != 0 && index < _size);
        data[index] = data[_size - 1];
        _size--;
    }

    // Removes an element while keeping array order intact
    // This is slower than remove_at_unordered()
    void remove_at_ordered(uint32_t index) {
        GOLD_ASSERT(_size != 0 && index < _size);
        for (uint32_t replace_index = index; replace_index < _size - 1; replace_index++) {
            data[replace_index] = data[replace_index + 1];
        }
        _size--;
    }

    void clear() {
        _size = 0;
    }

    bool empty() const {
        return _size == 0;
    }
};
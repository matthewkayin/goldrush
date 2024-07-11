#pragma once

#include "asserts.h"
#include <cstdint>

// Using uint8_t instead of size_t for the index because the indices need to be sent over the network during gameplay
template <typename T, uint8_t capacity>
struct swiss_array {
    uint8_t length;
    T data[capacity];
    bool filled[capacity];

    swiss_array() {
        memset(filled, 0, sizeof(filled));
        length = 0;
    }

    T& operator[](uint8_t index) {
        return data[index];
    }
    const T& operator[](uint8_t index) const {
        return data[index];
    }

    uint8_t first() const {
        uint8_t index = 0;
        while (index < capacity && !filled[index]) {
            index++;
        }
        return index;
    }
    uint8_t end() const {
        return capacity;
    }
    void next(uint8_t& index) const {
        index++;
        while (index < capacity && !filled[index]) {
            index++;
        }
    }

    uint8_t push_back(const T& value) {
        GOLD_ASSERT(length < capacity);

        uint8_t index = 0;
        while (filled[index]) {
            index++;
        }
        data[index] = value;
        filled[index] = true;
        length++;

        return index;
    }
    void remove_at(uint8_t index) {
        GOLD_ASSERT(filled[index]);

        filled[index] = false;
        length--;
    }
};
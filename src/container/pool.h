#pragma once

#include <cstdint>

template <typename T, uint32_t capacity>
class Pool {
private:
    T data[capacity];
    uint32_t free_list[capacity];
    uint32_t head;
public:
    Pool() {
        for (uint32_t index = 0; index < capacity - 1; index++) {
            free_list[index] = index + 1;
        }
        head = 0;
    }

    uint32_t reserve() {
        GOLD_ASSERT(head != capacity);
        uint32_t index = head;
        head = free_list[index];
        return index;
    }

    void release(uint32_t index) {
        free_list[index] = head;
        head = index;
    }

    T* get(uint32_t index) {
        GOLD_ASSERT(index < capacity);
        return index;
    }

    const T* get(uint32_t index) const {
        GOLD_ASSERT(index < capacity);
        return index;
    }
};
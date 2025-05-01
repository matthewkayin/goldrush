#pragma once

#include "core/asserts.h"
#include "stack_alloc.h"

template <typename T>
class DynamicArray {
public:
    DynamicArray() {
        data = NULL;
        capacity = 0;
        _size = 0;
    }

    void reserve(size_t amount) {
        // This function will not preserve existing data
        GOLD_ASSERT(size == 0);
        capacity = amount;
        data = (T*)stack_alloc(capacity * sizeof(T));
    }

    T& operator[](size_t index) {
        GOLD_ASSERT(index < _size);
        return data[index];
    }

    const T& operator[](size_t index) const {
        GOLD_ASSERT(index < _size);
        return data[index];
    }

    size_t size() const {
        return _size;
    }

    void push_back(const T& value) {
        if (_size == capacity) {
            T* old_data = data;
            capacity = capacity == 0 ? 1 : capacity * 2;
            data = (T*)stack_alloc(capacity * sizeof(T));
            memcpy(data, old_data, _size * sizeof(T));
        }

        data[_size] = value;
        _size++;
    }

    void pop_back() {
        _size--;
    }

    void swap_remove(size_t index) {
        GOLD_ASSERT(index < size);
        data[index] = data[_size - 1];
        _size--;
    }
private:
    T* data;
    size_t _size;
    size_t capacity;
};
#pragma once

#include <cstdint>

struct RingBuffer {
    uint8_t* data;
    size_t head;
    size_t tail;
    size_t length;
    size_t capacity;
};

RingBuffer* ring_buffer_init(size_t capacity);
void ring_buffer_free(RingBuffer* buffer);
void ring_buffer_write(RingBuffer* buffer, uint8_t* data, size_t data_length);
void ring_buffer_pop(RingBuffer* buffer, size_t length);
void ring_buffer_read(const RingBuffer* buffer, size_t advance, uint8_t* dest, size_t read_length);
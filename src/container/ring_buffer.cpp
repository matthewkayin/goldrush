#include "ring_buffer.h"

#include "core/asserts.h"
#include <cstdlib>
#include <algorithm>

RingBuffer* ring_buffer_init(size_t capacity) {
    RingBuffer* buffer = (RingBuffer*)malloc(sizeof(RingBuffer));
    if (buffer == NULL) {
        return NULL;
    }

    buffer->capacity = capacity;
    buffer->head = 0;
    buffer->tail = 0;
    buffer->length = 0;
    buffer->data = (uint8_t*)malloc(capacity);

    if (buffer->data == NULL) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

void ring_buffer_free(RingBuffer* buffer) {
    free(buffer->data);
    free(buffer);
}

void ring_buffer_write(RingBuffer* buffer, uint8_t* data, size_t data_length) {
    GOLD_ASSERT(buffer->length + data_length <= buffer->capacity);

    while (data_length != 0) {
        size_t space_left = buffer->capacity - buffer->head;
        size_t copy_length = std::min(data_length, space_left);
        memcpy(buffer->data + buffer->head, data, copy_length);
        data_length -= copy_length;
        buffer->length += copy_length;
        buffer->head += copy_length;
        if (buffer->head >= buffer->capacity) {
            buffer->head -= buffer->capacity;
        }
    }
}

void ring_buffer_pop(RingBuffer* buffer, size_t length) {
    GOLD_ASSERT(length <= buffer->length);

    buffer->tail += length;
    if (buffer->tail >= buffer->capacity) {
        buffer->tail -= buffer->capacity;
    }
    buffer->length -= length;
}

void ring_buffer_read(const RingBuffer* buffer, size_t advance, uint8_t* dest, size_t read_length) {
    GOLD_ASSERT(advance < buffer->length);

    size_t offset = buffer->tail + advance;
    if (offset >= buffer->capacity) {
        offset -= buffer->capacity;
    }

    size_t bytes_read = 0;
    while (bytes_read < read_length) {
        size_t copy_length = std::min(read_length - bytes_read, buffer->capacity - offset);
        memcpy(dest + bytes_read, buffer->data + offset, copy_length);

        bytes_read += copy_length;
        offset += copy_length;
        if (offset >= buffer->capacity) {
            offset -= buffer->capacity;
        }
    }
}
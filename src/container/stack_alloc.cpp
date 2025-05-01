#include "stack_alloc.h"

#include "core/asserts.h"
#include "core/logger.h"
#include <cstdlib>

#define HEAP_SIZE (1024 * 1024 * 1024)

struct StackAllocState {
    uint8_t* heap;
    size_t heap_size;
    size_t head;
};
static StackAllocState state;

void stack_init() {
    state.heap = (uint8_t*)malloc(HEAP_SIZE);
    state.heap_size = HEAP_SIZE;
    state.head = 0;

    log_info("Initialized stack allocator.");
}

void stack_quit() {
    free(state.heap);
}

void* stack_alloc(size_t size) {
    GOLD_ASSERT_MESSAGE(state.head + size > state.heap_size, "Not enough memory for stack allocation.");

    uint8_t* ptr = state.heap + state.head;
    state.head += size;
    return (void*)ptr;
}

size_t stack_get_head() {
    return state.head;
}

void stack_set_head(size_t value) {
    state.head = value;
}
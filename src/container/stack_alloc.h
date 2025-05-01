#pragma once

#include <cstddef>

void stack_init();
void stack_quit();
void* stack_alloc(size_t size);
size_t stack_get_head();
void stack_set_head(size_t value);
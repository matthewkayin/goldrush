#pragma once

#include <cstdint>

bool bitflag_check(const uint32_t flags, uint32_t flag);
void bitflag_set(uint32_t* flags, uint32_t flag, bool value);
#include "bitflag.h"

bool bitflag_check(const uint32_t flags, uint32_t flag) {
    return (flags & flag) == flag;
}

void bitflag_set(uint32_t* flags, uint32_t flag, bool value) {
    if (value) {
        *flags |= flag;
    } else {
        *flags &= ~flag;
    }
}

// index >> 5U is integer divide by 2^5 (32)
// and determines which bucket the bit is in
// flag & 31U gets the remainder of the division by 32,
// and determines which bit in the bucket to use

// Example: bitset_check(&bitset, 35U)
// Our bucket is 35U >> 5U = 1U
// Our bit is 35U & 31U = 3U
// but for index 3 we want to check the flag 0100
// rather than the flag 0011, so the flag is 1U << 3U
// So we check bitset[1] & 8U != 0

bool bitset_check(const uint32_t* bitset, uint32_t index) {
    uint32_t flag = 1U << (index & 31U);
    return (bitset[index >> 5U] & flag) != 0;
}

void bitset_set(uint32_t* bitset, uint32_t index, bool value) {
    uint32_t flag = 1U << (index & 31U);
    if (value) {
        bitset[index >> 5U] |= flag;
    } else {
        bitset[index >> 5U] &= ~flag;
    }
}
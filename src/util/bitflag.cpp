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
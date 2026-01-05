#pragma once

#include "core/asserts.h"
#include <cstdint>

enum ObjectiveType {
    OBJECTIVE_TYPE_CONQUEST,
    OBJECTIVE_TYPE_GOLD,
    OBJECTIVE_TYPE_COUNT
};

struct ObjectiveGold {
    uint32_t target;
};

struct Objective {
    ObjectiveType type;
    union {
        ObjectiveGold gold;
    };
};
STATIC_ASSERT(sizeof(Objective) == 8);

const char* objective_type_str(ObjectiveType objective_type);
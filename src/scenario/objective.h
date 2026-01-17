#pragma once

#include "defines.h"
#include "match/entity.h"

#define OBJECTIVE_TITLE_BUFFER_LENGTH 32

enum ObjectiveCounterType {
    OBJECTIVE_COUNTER_TYPE_NONE,
    OBJECTIVE_COUNTER_TYPE_VARIABLE,
    OBJECTIVE_COUNTER_TYPE_ENTITY,
    OBJECTIVE_COUNTER_TYPE_COUNT
};

struct Objective {
    bool is_finished;
    ObjectiveCounterType counter_type;
    uint32_t counter_value;
    uint32_t counter_target;
    char description[OBJECTIVE_TITLE_BUFFER_LENGTH];
};

Objective objective_init();
const char* objective_counter_type_str(ObjectiveCounterType type);
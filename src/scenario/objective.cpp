#include "objective.h"

Objective objective_init() {
    Objective objective;
    objective.is_complete = false;
    objective.counter_type = OBJECTIVE_COUNTER_TYPE_NONE;
    objective.counter_value = 0;
    objective.counter_target = 1;
    sprintf(objective.description, "");

    return objective;
}

const char* objective_counter_type_str(ObjectiveCounterType type) {
    switch (type) {
        case OBJECTIVE_COUNTER_TYPE_NONE: 
            return "None";
        case OBJECTIVE_COUNTER_TYPE_ENTITY:
            return "Entity";
        case OBJECTIVE_COUNTER_TYPE_VARIABLE:
            return "Variable";
        case OBJECTIVE_COUNTER_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}
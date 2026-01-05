#pragma once

#include "core/asserts.h"
#include <cstdint>

enum ScenarioObjectiveType {
    SCENARIO_OBJECTIVE_CONQUEST,
    SCENARIO_OBJECTIVE_GOLD,
    SCENARIO_OBJECTIVE_COUNT
};

struct ScenarioObjectiveGold {
    uint32_t target;
};

struct ScenarioObjective {
    ScenarioObjectiveType type;
    union {
        ScenarioObjectiveGold gold;
    };
};
STATIC_ASSERT(sizeof(ScenarioObjective) == 8);

const char* objective_type_str(ScenarioObjectiveType objective_type);
#include "objective.h"

const char* objective_type_str(ScenarioObjectiveType type) {
    switch (type) {
        case SCENARIO_OBJECTIVE_CONQUEST:
            return "Conquest";
        case SCENARIO_OBJECTIVE_GOLD:
            return "Gold";
        case SCENARIO_OBJECTIVE_COUNT:
            return "";
    }
}
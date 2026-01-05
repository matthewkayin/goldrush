#include "objective.h"

const char* objective_type_str(ObjectiveType type) {
    switch (type) {
        case OBJECTIVE_TYPE_CONQUEST:
            return "Conquest";
        case OBJECTIVE_TYPE_GOLD:
            return "Gold";
        case OBJECTIVE_TYPE_COUNT:
            return "";
    }
}
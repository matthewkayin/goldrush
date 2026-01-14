#include "trigger.h"

const char* trigger_condition_type_str(TriggerConditionType type) {
    switch (type) {
        case TRIGGER_CONDITION_ENTITY_COUNT: 
            return "Entity Count";
        case TRIGGER_CONDITION_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* trigger_effect_type_str(TriggerEffectType type) {
    switch (type) {
        case TRIGGER_EFFECT_MESSAGE:
            return "Message";
        case TRIGGER_EFFECT_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}
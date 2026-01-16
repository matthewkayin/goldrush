#include "trigger.h"

const char* trigger_condition_type_str(TriggerConditionType type) {
    switch (type) {
        case TRIGGER_CONDITION_TYPE_ENTITY_COUNT: 
            return "Entity Count";
        case TRIGGER_CONDITION_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* trigger_effect_type_str(TriggerEffectType type) {
    switch (type) {
        case TRIGGER_EFFECT_TYPE_HINT:
            return "Hint";
        case TRIGGER_EFFECT_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

int trigger_condition_sprintf(char* str_ptr, const TriggerCondition& condition) {
    switch (condition.type) {
        case TRIGGER_CONDITION_TYPE_ENTITY_COUNT: {
            const EntityData& entity_data = entity_get_data(condition.entity_count.entity_type);
            return sprintf(str_ptr, "%u %s", condition.entity_count.entity_count, entity_data.name);
        }
        case TRIGGER_CONDITION_TYPE_COUNT:
            GOLD_ASSERT(false);
            return 0;
    }
}
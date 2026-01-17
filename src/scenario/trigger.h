#pragma once

#include "scenario/objective.h"
#include "bot/entity_count.h"
#include <vector>

#define TRIGGER_NAME_BUFFER_LENGTH 32
#define TRIGGER_ACTION_HINT_MESSAGE_BUFFER_LENGTH 64
#define TRIGGER_EFFECT_COUNT_MAX 16

// Condition

enum TriggerConditionType {
    TRIGGER_CONDITION_TYPE_ENTITY_COUNT,
    TRIGGER_CONDITION_TYPE_COUNT
};

struct TriggerConditionEntityCount {
    EntityType entity_type;
    uint32_t entity_count;
};

struct TriggerCondition {
    TriggerConditionType type;
    union {
        TriggerConditionEntityCount entity_count;
    };
};
STATIC_ASSERT(sizeof(TriggerCondition) == 12ULL);

// Action

enum TriggerActionType {
    TRIGGER_ACTION_TYPE_HINT,
    TRIGGER_ACTION_TYPE_ADD_OBJECTIVE,
    TRIGGER_ACTION_TYPE_FINISH_OBJECTIVE,
    TRIGGER_ACTION_TYPE_WAIT,
    TRIGGER_ACTION_TYPE_COUNT
};

struct TriggerActionHint {
    char message[TRIGGER_ACTION_HINT_MESSAGE_BUFFER_LENGTH];
};

struct TriggerActionAddObjective {
    uint32_t objective_index;
};

struct TriggerActionFinishObjective {
    uint32_t objective_index;
    bool is_finished;
};

struct TriggerActionWait {
    uint32_t seconds;
};

struct TriggerAction {
    TriggerActionType type;
    union {
        TriggerActionHint hint;
        TriggerActionAddObjective add_objective;
        TriggerActionFinishObjective finish_objective;
        TriggerActionWait wait;
    };
};
STATIC_ASSERT(sizeof(TriggerAction) == 68ULL);

struct Trigger {
    bool is_active;
    char name[TRIGGER_NAME_BUFFER_LENGTH];
    std::vector<TriggerCondition> conditions;
    std::vector<TriggerAction> actions;
};

const char* trigger_condition_type_str(TriggerConditionType type);
const char* trigger_action_type_str(TriggerActionType type);
int trigger_condition_sprintf(char* str_ptr, const TriggerCondition& condition);
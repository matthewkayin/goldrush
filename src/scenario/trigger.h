#pragma once

#include "bot/entity_count.h"
#include <vector>

#define TRIGGER_NAME_MAX_LENGTH 32
#define TRIGGER_EFFECT_MESSAGE_MAX_LENGTH 64
#define TRIGGER_EFFECT_COUNT_MAX 16

// Condition

enum TriggerConditionType {
    TRIGGER_CONDITION_ACQUIRED_ENTITIES,
    TRIGGER_CONDITION_COUNT
};

struct TriggerConditionAcquiredEntities {
    uint32_t entity_count[ENTITY_TYPE_COUNT];
};

struct TriggerCondition {
    TriggerConditionType type;
    union {
        TriggerConditionAcquiredEntities acquired_entities;
    };
};

// Effect

enum TriggerEffectType {
    TRIGGER_EFFECT_MESSAGE,
    TRIGGER_EFFECT_COUNT
};

struct TriggerEffectMessage {
    char message[TRIGGER_EFFECT_MESSAGE_MAX_LENGTH];
    bool is_hint;
    uint8_t padding = 0;
    uint8_t padding1 = 0;
    uint8_t padding2 = 0;
};

struct TriggerEffect {
    TriggerEffectType type;
    union {
        TriggerEffectMessage message;
    };
};

struct Trigger {
    bool is_active;
    char name[TRIGGER_NAME_MAX_LENGTH];
    TriggerCondition condition;
    std::vector<TriggerEffect> effects;
};

const char* trigger_condition_type_str(TriggerConditionType type);
const char* trigger_effect_type_str(TriggerEffectType type);
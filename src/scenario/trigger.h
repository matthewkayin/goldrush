#pragma once

#include "scenario/objective.h"
#include "bot/entity_count.h"
#include <vector>

#define TRIGGER_NAME_BUFFER_LENGTH 32
#define TRIGGER_ACTION_CHAT_PREFIX_BUFFER_LENGTH MAX_USERNAME_LENGTH
#define TRIGGER_ACTION_CHAT_MESSAGE_BUFFER_LENGTH 64
#define TRIGGER_ACTION_SPAWN_UNITS_ENTITY_COUNT_MAX SELECTION_LIMIT

// Condition

enum TriggerConditionType {
    TRIGGER_CONDITION_TYPE_ENTITY_COUNT,
    TRIGGER_CONDITION_TYPE_OBJECTIVE_COMPLETE,
    TRIGGER_CONDITION_TYPE_AREA_DISCOVERED,
    TRIGGER_CONDITION_TYPE_COUNT
};

struct TriggerConditionEntityCount {
    EntityType entity_type;
    uint32_t entity_count;
};

struct TriggerConditionObjectiveComplete {
    uint32_t objective_index;
};

struct TriggerConditionAreaDiscovered {
    ivec2 cell;
    int cell_size;
};

struct TriggerCondition {
    TriggerConditionType type;
    union {
        TriggerConditionEntityCount entity_count;
        TriggerConditionObjectiveComplete objective_complete;
        TriggerConditionAreaDiscovered area_discovered;
    };
};
STATIC_ASSERT(sizeof(TriggerCondition) == 16ULL);

// Action

enum TriggerActionType {
    TRIGGER_ACTION_TYPE_CHAT,
    TRIGGER_ACTION_TYPE_ADD_OBJECTIVE,
    TRIGGER_ACTION_TYPE_COMPLETE_OBJECTIVE,
    TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES,
    TRIGGER_ACTION_TYPE_WAIT,
    TRIGGER_ACTION_TYPE_FOG_REVEAL,
    TRIGGER_ACTION_TYPE_ALERT,
    TRIGGER_ACTION_TYPE_SPAWN_UNITS,
    TRIGGER_ACTION_TYPE_SHOW_ENEMY_GOLD,
    TRIGGER_ACTION_TYPE_SET_LOSE_CONDITION,
    TRIGGER_ACTION_TYPE_COUNT
};

enum TriggerActionChatType {
    TRIGGER_ACTION_CHAT_TYPE_MESSAGE,
    TRIGGER_ACTION_CHAT_TYPE_NEW_OBJECTIVE,
    TRIGGER_ACTION_CHAT_TYPE_HINT,
    TRIGGER_ACTION_CHAT_TYPE_OBJECTIVES_COMPLETE,
    TRIGGER_ACTION_CHAT_TYPE_COUNT
};

struct TriggerActionChat {
    TriggerActionChatType type;
    char prefix[TRIGGER_ACTION_CHAT_PREFIX_BUFFER_LENGTH];
    char message[TRIGGER_ACTION_CHAT_MESSAGE_BUFFER_LENGTH];
};

struct TriggerActionAddObjective {
    uint32_t objective_index;
};

struct TriggerActionFinishObjective {
    uint32_t objective_index;
    bool is_complete;
};

struct TriggerActionWait {
    uint32_t seconds;
};

struct TriggerActionFogReveal {
    ivec2 cell;
    int cell_size;
    int sight;
    uint32_t duration_seconds;
};

struct TriggerActionAlert {
    ivec2 cell;
    int cell_size;
};

struct TriggerActionSpawnUnits {
    uint8_t player_id;
    ivec2 spawn_cell;
    ivec2 target_cell;
    uint32_t entity_count;
    EntityType entity_types[TRIGGER_ACTION_SPAWN_UNITS_ENTITY_COUNT_MAX];
};

struct TriggerActionSetLoseCondition {
    bool lose_on_buildings_destroyed;
};

struct TriggerAction {
    TriggerActionType type;
    union {
        TriggerActionChat chat;
        TriggerActionAddObjective add_objective;
        TriggerActionFinishObjective finish_objective;
        TriggerActionWait wait;
        TriggerActionFogReveal fog;
        TriggerActionAlert alert;
        TriggerActionSpawnUnits spawn_units;
        TriggerActionSetLoseCondition set_lose_condition;
    };
};
STATIC_ASSERT(sizeof(TriggerAction) == 108ULL);

struct Trigger {
    bool is_active;
    char name[TRIGGER_NAME_BUFFER_LENGTH];
    std::vector<TriggerCondition> conditions;
    std::vector<TriggerAction> actions;
};

const char* trigger_condition_type_str(TriggerConditionType type);
const char* trigger_action_type_str(TriggerActionType type);
const char* trigger_action_chat_type_str(TriggerActionChatType type);
int trigger_condition_sprintf(char* str_ptr, const TriggerCondition& condition);
int trigger_action_sprintf(char* str_ptr, const TriggerAction& action);

TriggerAction trigger_action_init();
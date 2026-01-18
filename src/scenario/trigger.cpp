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

const char* trigger_action_type_str(TriggerActionType type) {
    switch (type) {
        case TRIGGER_ACTION_TYPE_CHAT:
            return "Message";
        case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE:
            return "Add Objective";
        case TRIGGER_ACTION_TYPE_FINISH_OBJECTIVE:
            return "Finish Objective";
        case TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES:
            return "Clear Objectives";
        case TRIGGER_ACTION_TYPE_WAIT:
            return "Wait";
        case TRIGGER_ACTION_TYPE_FOG_REVEAL:
            return "Fog Reveal";
        case TRIGGER_ACTION_TYPE_ALERT:
            return "Alert";
        case TRIGGER_ACTION_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* trigger_action_chat_prefix_type_str(TriggerActionChatPrefixType type) {
    switch (type) {
        case TRIGGER_ACTION_CHAT_PREFIX_TYPE_NONE:
            return "None";
        case TRIGGER_ACTION_CHAT_PREFIX_TYPE_GOLD:
            return "Gold";
        case TRIGGER_ACTION_CHAT_PREFIX_TYPE_BLUE:
            return "Blue";
        case TRIGGER_ACTION_CHAT_PREFIX_TYPE_COUNT:
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

TriggerAction trigger_action_init() {
    TriggerAction new_action;
    new_action.type = TRIGGER_ACTION_TYPE_CHAT;
    new_action.chat.prefix_type = TRIGGER_ACTION_CHAT_PREFIX_TYPE_NONE;
    sprintf(new_action.chat.prefix, "");
    sprintf(new_action.chat.message, "");

    return new_action;
}
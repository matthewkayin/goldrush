#include "trigger.h"

const char* trigger_condition_type_str(TriggerConditionType type) {
    switch (type) {
        case TRIGGER_CONDITION_TYPE_ENTITY_COUNT: 
            return "Entity Count";
        case TRIGGER_CONDITION_TYPE_OBJECTIVE_COMPLETE:
            return "Obj Comp";
        case TRIGGER_CONDITION_TYPE_AREA_DISCOVERED:
            return "Area Discvrd";
        case TRIGGER_CONDITION_TYPE_FULL_BUNKER:
            return "Full Bunker";
        case TRIGGER_CONDITION_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* trigger_action_type_str(TriggerActionType type) {
    switch (type) {
        case TRIGGER_ACTION_TYPE_CHAT:
            return "Chat";
        case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE:
            return "Add Obj";
        case TRIGGER_ACTION_TYPE_COMPLETE_OBJECTIVE:
            return "Comp Obj";
        case TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES:
            return "Clear Objs";
        case TRIGGER_ACTION_TYPE_WAIT:
            return "Wait";
        case TRIGGER_ACTION_TYPE_FOG_REVEAL:
            return "Fog Reveal";
        case TRIGGER_ACTION_TYPE_ALERT:
            return "Alert";
        case TRIGGER_ACTION_TYPE_SPAWN_UNITS:
            return "Spawn Units";
        case TRIGGER_ACTION_TYPE_SHOW_ENEMY_GOLD:
            return "Show Opp Gold";
        case TRIGGER_ACTION_TYPE_SET_LOSE_CONDITION:
            return "Set Lose Cnd";
        case TRIGGER_ACTION_TYPE_HIGHLIGHT_ENTITY:
            return "Highlight";
        case TRIGGER_ACTION_TYPE_CAMERA_PAN:
            return "Cam Pan";
        case TRIGGER_ACTION_TYPE_CAMERA_RETURN:
            return "Cam Return";
        case TRIGGER_ACTION_TYPE_CAMERA_FREE:
            return "Cam Free";
        case TRIGGER_ACTION_TYPE_SOUND:
            return "Sound";
        case TRIGGER_ACTION_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* trigger_action_chat_type_str(TriggerActionChatType type) {
    switch (type) {
        case TRIGGER_ACTION_CHAT_TYPE_MESSAGE:
            return "Message";
        case TRIGGER_ACTION_CHAT_TYPE_NEW_OBJECTIVE:
            return "New Obj";
        case TRIGGER_ACTION_CHAT_TYPE_HINT:
            return "Hint";
        case TRIGGER_ACTION_CHAT_TYPE_OBJECTIVES_COMPLETE:
            return "Obj Comp";
        case TRIGGER_ACTION_CHAT_TYPE_COUNT:
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
        case TRIGGER_CONDITION_TYPE_OBJECTIVE_COMPLETE: {
            return sprintf(str_ptr, "Obj %u Complete", condition.objective_complete.objective_index);
        }
        case TRIGGER_CONDITION_TYPE_AREA_DISCOVERED: {
            return sprintf(str_ptr, "Area Discvrd");
        }
        case TRIGGER_CONDITION_TYPE_FULL_BUNKER: {
            return sprintf(str_ptr, "Full Bunker");
        }
        case TRIGGER_CONDITION_TYPE_COUNT:
            GOLD_ASSERT(false);
            return 0;
    }
}

int trigger_action_sprintf(char* str_ptr, const TriggerAction& action) {  
    switch (action.type) {
        case TRIGGER_ACTION_TYPE_CHAT: {
            return sprintf(str_ptr, "Chat: %s", trigger_action_chat_type_str(action.chat.type));
        }
        case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE: {
            return sprintf(str_ptr, "Add Obj %u", action.add_objective.objective_index);
        }
        case TRIGGER_ACTION_TYPE_COMPLETE_OBJECTIVE: {
            return sprintf(str_ptr, "Cmp Obj %u", action.finish_objective.objective_index);
        }
        case TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES: {
            return sprintf(str_ptr, "Clear Objs");
        }
        case TRIGGER_ACTION_TYPE_WAIT: {
            return sprintf(str_ptr, "Wait %us", action.wait.seconds);
        }
        case TRIGGER_ACTION_TYPE_FOG_REVEAL: {
            return sprintf(str_ptr, "Fog %us", action.fog.duration_seconds);
        }
        case TRIGGER_ACTION_TYPE_ALERT: {
            return sprintf(str_ptr, "Alert <%i,%i>", action.alert.cell.x, action.alert.cell.y);
        }
        case TRIGGER_ACTION_TYPE_SPAWN_UNITS: {
            return sprintf(str_ptr, "Spawn %u", action.spawn_units.entity_count);
        }
        case TRIGGER_ACTION_TYPE_SHOW_ENEMY_GOLD: {
            return sprintf(str_ptr, "Show Opp Gld");
        }
        case TRIGGER_ACTION_TYPE_SET_LOSE_CONDITION: {
            return sprintf(str_ptr, "Lose Cond");
        }
        case TRIGGER_ACTION_TYPE_HIGHLIGHT_ENTITY: {
            return sprintf(str_ptr, "Highlight %u", action.highlight_entity.entity_index);
        }
        case TRIGGER_ACTION_TYPE_CAMERA_PAN: {
            return sprintf(str_ptr, "Cam Pan %u", action.camera_pan.duration_seconds);
        }
        case TRIGGER_ACTION_TYPE_CAMERA_RETURN: {
            return sprintf(str_ptr, "Cam Return");
        }
        case TRIGGER_ACTION_TYPE_CAMERA_FREE: {
            return sprintf(str_ptr, "Cam Free");
        }
        case TRIGGER_ACTION_TYPE_SOUND: {
            return sprintf(str_ptr, "Sfx: %s", sound_get_name(action.sound.sound));
        }
        case TRIGGER_ACTION_TYPE_COUNT: {
            GOLD_ASSERT(false);
            return 0;
        }
    }
}

TriggerAction trigger_action_init() {
    TriggerAction new_action;
    new_action.type = TRIGGER_ACTION_TYPE_CHAT;
    new_action.chat.type = TRIGGER_ACTION_CHAT_TYPE_MESSAGE;
    sprintf(new_action.chat.prefix, "");
    sprintf(new_action.chat.message, "");

    return new_action;
}
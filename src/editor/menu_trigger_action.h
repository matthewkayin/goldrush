#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "editor/menu.h"
#include "scenario/trigger.h"
#include "scenario/scenario.h"
#include <vector>
#include <string>

enum EditorMenuTriggerActionRequest {
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_NONE,
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_FOG_REVEAL,
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_ALERT,
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_SPAWN_CELL,
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_TARGET_CELL,
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_ENTITY,
    EDITOR_MENU_TRIGGER_ACTION_REQUEST_CAMERA_PAN_CELL,
};

struct EditorMenuTriggerAction {
    EditorMenuTriggerActionRequest request;
    uint32_t action_index;
    TriggerAction action;
    std::vector<std::string> action_type_items;
    std::vector<std::string> objective_items;
    std::vector<std::string> message_type_items;
    std::vector<std::string> spawn_units_player_items;
    std::string chat_prefix_value;
    std::string chat_message_value;
};

EditorMenuTriggerAction editor_menu_trigger_action_open(const Scenario* scenario, const TriggerAction& action, uint32_t action_index);
void editor_menu_trigger_action_update(EditorMenuTriggerAction& menu, UI& ui, EditorMenuMode& mode);
void editor_menu_trigger_action_set_fog_cell(EditorMenuTriggerAction& menu, ivec2 cell, int cell_size, int sight);
void editor_menu_trigger_action_set_alert(EditorMenuTriggerAction& menu, ivec2 cell, int cell_size);
void editor_menu_trigger_action_set_request_cell(EditorMenuTriggerAction& menu, ivec2 cell);
void editor_menu_trigger_action_set_request_entity(EditorMenuTriggerAction& menu, uint32_t entity_index);

#endif
#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "scenario/scenario.h"
#include "scenario/trigger.h"
#include "math/gmath.h"
#include "match/noise.h"
#include <vector>
#include <variant>

enum EditorActionMode {
    EDITOR_ACTION_MODE_DO,
    EDITOR_ACTION_MODE_UNDO
};

enum EditorActionType {
    EDITOR_ACTION_BRUSH,
    EDITOR_ACTION_DECORATE,
    EDITOR_ACTION_DECORATE_BULK,
    EDITOR_ACTION_ADD_ENTITY,
    EDITOR_ACTION_EDIT_ENTITY,
    EDITOR_ACTION_DELETE_ENTITY,
    EDITOR_ACTION_ADD_SQUAD,
    EDITOR_ACTION_EDIT_SQUAD,
    EDITOR_ACTION_DELETE_SQUAD,
    EDITOR_ACTION_SET_PLAYER_SPAWN,
    EDITOR_ACTION_ADD_TRIGGER,
    EDITOR_ACTION_DELETE_TRIGGER,
    EDITOR_ACTION_RENAME_TRIGGER
};

struct EditorActionBrushStroke {
    int index;
    uint8_t previous_value;
    uint8_t new_value;
};

struct EditorActionBrush {
    std::vector<EditorActionBrushStroke> stroke;
};

static const int EDITOR_ACTION_DECORATE_REMOVE_DECORATION = -1;

struct EditorActionDecorate {
    int index;
    int previous_hframe;
    int new_hframe;
};

struct EditorActionDecorateBulk {
    std::vector<EditorActionDecorate> changes;
};

struct EditorActionAddEntity {
    EntityType type;
    uint8_t player_id;
    ivec2 cell;
};

struct EditorActionEditEntity {
    uint32_t index;
    ScenarioEntity previous_value;
    ScenarioEntity new_value;
};

struct EditorActionDeleteEntity {
    uint32_t index;
    ScenarioEntity value;
};

struct EditorActionEditSquad {
    uint32_t index;
    ScenarioSquad previous_value;
    ScenarioSquad new_value;
};

struct EditorActionDeleteSquad {
    uint32_t index;
    ScenarioSquad value;
};

struct EditorActionSetPlayerSpawn {
    ivec2 previous_value;
    ivec2 new_value;
};

struct EditorActionDeleteTrigger {
    uint32_t index;
    Trigger value;
};

struct EditorActionRenameTrigger {
    uint32_t index;
    char previous_name[TRIGGER_NAME_MAX_LENGTH];
    char new_name[TRIGGER_NAME_MAX_LENGTH];
};

struct EditorAction {
    EditorActionType type;
    std::variant<
        EditorActionBrush,
        EditorActionDecorate,
        EditorActionDecorateBulk,
        EditorActionAddEntity,
        EditorActionEditEntity,
        EditorActionDeleteEntity,
        EditorActionEditSquad,
        EditorActionDeleteSquad,
        EditorActionSetPlayerSpawn,
        EditorActionDeleteTrigger,
        EditorActionRenameTrigger> data;
};

// Action

void editor_action_execute(Scenario* scenario, const EditorAction& action, EditorActionMode mode);

#endif
#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "scenario/scenario.h"
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
    EDITOR_ACTION_REMOVE_ENTITY,
    EDITOR_ACTION_ADD_SQUAD,
    EDITOR_ACTION_EDIT_SQUAD,
    EDITOR_ACTION_REMOVE_SQUAD,
    EDITOR_ACTION_SET_PLAYER_SPAWN,
    EDITOR_ACTION_ADD_CONSTANT,
    EDITOR_ACTION_REMOVE_CONSTANT,
    EDITOR_ACTION_EDIT_CONSTANT
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

struct EditorActionRemoveEntity {
    uint32_t index;
    ScenarioEntity value;
    uint32_t squad_index;
    std::vector<uint32_t> constant_indices;
};

struct EditorActionEditSquad {
    uint32_t index;
    ScenarioSquad previous_value;
    ScenarioSquad new_value;
};

struct EditorActionRemoveSquad {
    uint32_t index;
    ScenarioSquad value;
};

struct EditorActionSetPlayerSpawn {
    ivec2 previous_value;
    ivec2 new_value;
};

struct EditorActionRemoveConstant {
    uint32_t index;
    ScenarioConstant value;
};

struct EditorActionEditConstant {
    uint32_t index;
    ScenarioConstant previous_value;
    ScenarioConstant new_value;
};

struct EditorAction {
    EditorActionType type;
    std::variant<
        EditorActionBrush,
        EditorActionDecorate,
        EditorActionDecorateBulk,
        EditorActionAddEntity,
        EditorActionEditEntity,
        EditorActionRemoveEntity,
        EditorActionEditSquad,
        EditorActionRemoveSquad,
        EditorActionSetPlayerSpawn,
        EditorActionRemoveConstant,
        EditorActionEditConstant
    > data;
};

// Action

void editor_action_execute(Scenario* scenario, const EditorAction& action, EditorActionMode mode);

#endif
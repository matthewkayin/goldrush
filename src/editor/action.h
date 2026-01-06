#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "scenario/scenario.h"
#include "math/gmath.h"
#include "match/noise.h"
#include <vector>

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
    EDITOR_ACTION_SET_PLAYER_SPAWN
};

struct EditorActionBrushStroke {
    int index;
    uint8_t previous_value;
    uint8_t new_value;
};

struct EditorActionBrush {
    uint32_t stroke_size;
    EditorActionBrushStroke* stroke;
};

static const int EDITOR_ACTION_DECORATE_REMOVE_DECORATION = -1;

struct EditorActionDecorate {
    int index;
    int previous_hframe;
    int new_hframe;
};

struct EditorActionDecorateBulk {
    uint32_t size;
    EditorActionDecorate* changes;
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

struct EditorAction {
    EditorActionType type;
    union {
        EditorActionBrush brush;
        EditorActionDecorate decorate;
        EditorActionDecorateBulk decorate_bulk;
        EditorActionAddEntity add_entity;
        EditorActionEditEntity edit_entity;
        EditorActionDeleteEntity delete_entity;
        EditorActionEditSquad edit_squad;
        EditorActionDeleteSquad delete_squad;
        EditorActionSetPlayerSpawn set_player_spawn;
    };
};

// Action

EditorAction editor_action_create_brush(const std::vector<EditorActionBrushStroke>& stroke);
EditorAction editor_action_create_decorate_bulk(const std::vector<EditorActionDecorate>& changes);
void editor_action_destroy(EditorAction& action);
void editor_action_execute(Scenario* scenario, const EditorAction& action, EditorActionMode mode);

#endif
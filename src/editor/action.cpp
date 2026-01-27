#include "action.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"

// Action

void editor_action_execute_decorate(Scenario* scenario, const EditorActionDecorate& data, EditorActionMode mode);

void editor_action_execute(Scenario* scenario, const EditorAction& action, EditorActionMode mode) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            const EditorActionBrush& action_data = std::get<EditorActionBrush>(action.data);
            for (uint32_t index = 0; index < action_data.stroke.size(); index++) {
                uint8_t value = mode == EDITOR_ACTION_MODE_UNDO 
                    ? action_data.stroke[index].previous_value
                    : action_data.stroke[index].new_value;
                scenario->noise->map[action_data.stroke[index].index] = value;
            }

            scenario_bake_map(scenario);
            break;
        }
        case EDITOR_ACTION_DECORATE: {
            const EditorActionDecorate& action_data = std::get<EditorActionDecorate>(action.data);
            editor_action_execute_decorate(scenario, action_data, mode);
            break;
        }
        case EDITOR_ACTION_DECORATE_BULK: {
            const EditorActionDecorateBulk& action_data = std::get<EditorActionDecorateBulk>(action.data);
            for (uint32_t index = 0; index < action_data.changes.size(); index++) {
                editor_action_execute_decorate(scenario, action_data.changes[index], mode);
            }
            break;
        }
        case EDITOR_ACTION_ADD_ENTITY: {
            const EditorActionAddEntity& action_data = std::get<EditorActionAddEntity>(action.data);

            Cell cell_value = mode == EDITOR_ACTION_MODE_DO
                ? (Cell) {
                    .type = CELL_UNIT,
                    .id = (uint16_t)scenario->entity_count
                }
                : (Cell) {
                    .type = CELL_EMPTY,
                    .id = ID_NULL
                };

            const EntityData& entity_data = entity_get_data(action_data.type);
            map_set_cell_rect(scenario->map, entity_data.cell_layer, action_data.cell, entity_data.cell_size, cell_value);

            if (mode == EDITOR_ACTION_MODE_DO) {
                ScenarioEntity entity;
                entity.type = action_data.type;
                entity.player_id = action_data.player_id;
                entity.cell = action_data.cell;
                entity.gold_held = 0;
                if (entity.type == ENTITY_GOLDMINE) {
                    entity.gold_held = 7500;
                } 
                scenario->entities[scenario->entity_count] = entity;
                scenario->entity_count++;
            } else {
                scenario->entity_count--;
            }

            break;
        }
        case EDITOR_ACTION_EDIT_ENTITY: {
            const EditorActionEditEntity& action_data = std::get<EditorActionEditEntity>(action.data);

            ivec2 previous_cell = scenario->entities[action_data.index].cell;
            const EntityData& entity_data = entity_get_data(scenario->entities[action_data.index].type);
            map_set_cell_rect(scenario->map, entity_data.cell_layer, previous_cell, entity_data.cell_size, (Cell) {
                .type = CELL_EMPTY,
                .id = ID_NULL
            });

            scenario->entities[action_data.index] = mode == EDITOR_ACTION_MODE_DO
                ? action_data.new_value
                : action_data.previous_value;

            map_set_cell_rect(scenario->map, entity_data.cell_layer, scenario->entities[action_data.index].cell, entity_data.cell_size, (Cell) {
                .type = CELL_UNIT,
                .id = (uint16_t)scenario->entity_count
            });
            break;
        }
        case EDITOR_ACTION_REMOVE_ENTITY: {
            const EditorActionRemoveEntity& action_data = std::get<EditorActionRemoveEntity>(action.data);

            if (mode == EDITOR_ACTION_MODE_DO) {
                scenario->entities[action_data.index] = scenario->entities[scenario->entity_count - 1];
                scenario->entity_count--;
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                scenario->entities[scenario->entity_count] = scenario->entities[action_data.index];
                scenario->entities[action_data.index] = action_data.value;
                scenario->entity_count++;
            }

            break;
        }
        case EDITOR_ACTION_ADD_SQUAD: {
            if (mode == EDITOR_ACTION_MODE_DO) {
                ScenarioSquad squad = scenario_squad_init();
                sprintf(squad.name, "Squad %u", (uint32_t)scenario->squads.size() + 1U);
                scenario->squads.push_back(squad);
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                scenario->squads.pop_back();
            }
            break;
        }
        case EDITOR_ACTION_EDIT_SQUAD: {
            const EditorActionEditSquad& action_data = std::get<EditorActionEditSquad>(action.data);

            scenario->squads[action_data.index] = mode == EDITOR_ACTION_MODE_DO
                ? action_data.new_value
                : action_data.previous_value;
            break;
        }
        case EDITOR_ACTION_REMOVE_SQUAD: {
            const EditorActionRemoveSquad& action_data = std::get<EditorActionRemoveSquad>(action.data);

            if (mode == EDITOR_ACTION_MODE_DO) {
                scenario->squads.erase(scenario->squads.begin() + action_data.index);
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                scenario->squads.insert(scenario->squads.begin() + action_data.index, action_data.value);
            }
            break;
        }
        case EDITOR_ACTION_SET_PLAYER_SPAWN: {
            const EditorActionSetPlayerSpawn& action_data = std::get<EditorActionSetPlayerSpawn>(action.data);

            scenario->player_spawn = mode == EDITOR_ACTION_MODE_DO
                ? action_data.new_value
                : action_data.previous_value;
            break;
        }
        case EDITOR_ACTION_ADD_CONSTANT: {
            if (mode == EDITOR_ACTION_MODE_DO) {
                ScenarioConstant new_constant;
                sprintf(new_constant.name, "Constant %u", (uint32_t)scenario->constants.size() + 1U);
                new_constant.type = SCENARIO_CONSTANT_TYPE_ENTITY;
                new_constant.entity_index = INDEX_INVALID;

                scenario->constants.push_back(new_constant);
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                scenario->constants.pop_back();
            }

            break;
        }
        case EDITOR_ACTION_REMOVE_CONSTANT: {
            const EditorActionRemoveConstant& action_data = std::get<EditorActionRemoveConstant>(action.data);

            if (mode == EDITOR_ACTION_MODE_DO) {
                scenario->constants.erase(scenario->constants.begin() + action_data.index);
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                scenario->constants.insert(scenario->constants.begin() + action_data.index, action_data.value);
            }

            break;
        }
        case EDITOR_ACTION_EDIT_CONSTANT: {
            const EditorActionEditConstant& action_data = std::get<EditorActionEditConstant>(action.data);

            scenario->constants[action_data.index] = mode == EDITOR_ACTION_MODE_DO
                ? action_data.new_value
                : action_data.previous_value;
            break;
        }
    }
}

void editor_action_execute_decorate(Scenario* scenario, const EditorActionDecorate& data, EditorActionMode mode) {
    int hframe = mode == EDITOR_ACTION_MODE_UNDO
        ? data.previous_hframe
        : data.new_hframe;
    scenario->map.cells[CELL_LAYER_GROUND][data.index] = hframe == EDITOR_ACTION_DECORATE_REMOVE_DECORATION
        ? (Cell) {
            .type = CELL_EMPTY,
            .id = ID_NULL
        }
        : (Cell) {
            .type = CELL_DECORATION,
            .decoration_hframe = (uint16_t)hframe
        };
}

#endif
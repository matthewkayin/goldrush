#include "action.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"

// Action

EditorAction editor_action_create_brush(const std::vector<EditorActionBrushStroke>& stroke) {
    EditorAction action;
    action.type = EDITOR_ACTION_BRUSH;
    action.brush.stroke_size = stroke.size();
    action.brush.stroke = (EditorActionBrushStroke*)malloc(action.brush.stroke_size * sizeof(EditorActionBrushStroke));
    memcpy(action.brush.stroke, &stroke[0], action.brush.stroke_size * sizeof(EditorActionBrushStroke));

    return action;
}

EditorAction editor_action_create_decorate_bulk(const std::vector<EditorActionDecorate>& changes) {
    EditorAction action;
    action.type = EDITOR_ACTION_DECORATE_BULK;
    action.decorate_bulk.size = changes.size();
    action.decorate_bulk.changes = (EditorActionDecorate*)malloc(action.decorate_bulk.size * sizeof(EditorActionDecorateBulk));
    memcpy(action.decorate_bulk.changes, &changes[0], action.decorate_bulk.size * sizeof(EditorActionDecorateBulk));

    return action;
}

void editor_action_destroy(EditorAction& action) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            free(action.brush.stroke);
            break;
        }
        case EDITOR_ACTION_DECORATE_BULK: {
            free(action.decorate_bulk.changes);
            break;
        }
        case EDITOR_ACTION_DECORATE:
        case EDITOR_ACTION_ADD_ENTITY: 
        case EDITOR_ACTION_EDIT_ENTITY: 
        case EDITOR_ACTION_DELETE_ENTITY: 
        case EDITOR_ACTION_ADD_SQUAD:
        case EDITOR_ACTION_EDIT_SQUAD:
        case EDITOR_ACTION_DELETE_SQUAD:
            break;
    }
}

void editor_action_execute(EditorDocument* document, const EditorAction& action, EditorActionMode mode) {
    switch (action.type) {
        case EDITOR_ACTION_BRUSH: {
            for (uint32_t index = 0; index < action.brush.stroke_size; index++) {
                uint8_t value = mode == EDITOR_ACTION_MODE_UNDO 
                    ? action.brush.stroke[index].previous_value
                    : action.brush.stroke[index].new_value;
                document->noise->map[action.brush.stroke[index].index] = value;
            }

            editor_document_bake_map(document);
            break;
        }
        case EDITOR_ACTION_DECORATE: 
        case EDITOR_ACTION_DECORATE_BULK: {
            const EditorActionDecorate* changes = action.type == EDITOR_ACTION_DECORATE
                ? &action.decorate
                : action.decorate_bulk.changes;
            const uint32_t size = action.type == EDITOR_ACTION_DECORATE ? 1 : action.decorate_bulk.size;
            for (uint32_t change_index = 0; change_index < size; change_index++) {
                int hframe = mode == EDITOR_ACTION_MODE_UNDO 
                    ? changes[change_index].previous_hframe
                    : changes[change_index].new_hframe;
                document->map->cells[CELL_LAYER_GROUND][changes[change_index].index] = hframe == EDITOR_ACTION_DECORATE_REMOVE_DECORATION
                    ? (Cell) {
                        .type = CELL_EMPTY,
                        .id = ID_NULL
                    }
                    : (Cell) {
                        .type = CELL_DECORATION,
                        .decoration_hframe = (uint16_t)hframe
                    };
            }
            break;
        }
        case EDITOR_ACTION_ADD_ENTITY: {
            Cell cell_value = mode == EDITOR_ACTION_MODE_DO
                ? (Cell) {
                    .type = CELL_UNIT,
                    .id = (uint16_t)document->entity_count
                }
                : (Cell) {
                    .type = CELL_EMPTY,
                    .id = ID_NULL
                };
            const EntityData& entity_data = entity_get_data(action.add_entity.type);
            map_set_cell_rect(*document->map, entity_data.cell_layer, action.add_entity.cell, entity_data.cell_size, cell_value);

            if (mode == EDITOR_ACTION_MODE_DO) {
                EditorEntity entity;
                entity.type = action.add_entity.type;
                entity.player_id = action.add_entity.player_id;
                entity.cell = action.add_entity.cell;
                entity.gold_held = 0;
                if (entity.type == ENTITY_GOLDMINE) {
                    entity.gold_held = 7500;
                } 
                document->entities[document->entity_count] = entity;
                document->entity_count++;
            } else {
                document->entity_count--;
            }

            break;
        }
        case EDITOR_ACTION_EDIT_ENTITY: {
            document->entities[action.edit_entity.index] = mode == EDITOR_ACTION_MODE_DO
                ? action.edit_entity.new_value
                : action.edit_entity.previous_value;
            break;
        }
        case EDITOR_ACTION_DELETE_ENTITY: {
            if (mode == EDITOR_ACTION_MODE_DO) {
                document->entities[action.delete_entity.index] = document->entities[document->entity_count - 1];
                document->entity_count--;
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                document->entities[document->entity_count] = document->entities[action.delete_entity.index];
                document->entities[action.delete_entity.index] = action.delete_entity.value;
                document->entity_count++;
            }

            break;
        }
        case EDITOR_ACTION_ADD_SQUAD: {
            if (mode == EDITOR_ACTION_MODE_DO) {
                document->squads[document->squad_count] = editor_document_squad_init();
                sprintf(document->squads[document->squad_count].name, "Squad %u", document->squad_count + 1);
                document->squad_count++;
            } else if (mode == EDITOR_ACTION_MODE_UNDO) {
                document->squad_count--;
            }
            break;
        }
        case EDITOR_ACTION_EDIT_SQUAD: {
            document->squads[action.edit_squad.index] = mode == EDITOR_ACTION_MODE_DO
                ? action.edit_squad.new_value
                : action.edit_squad.previous_value;
            break;
        }
    }
}

#endif
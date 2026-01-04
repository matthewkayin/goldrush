#include "document.h"

#ifdef GOLD_DEBUG

#include "core/logger.h"

EditorDocument* editor_document_base_init() {
    EditorDocument* document = (EditorDocument*)malloc(sizeof(EditorDocument));
    if (document == NULL) {
        return NULL;
    }

    document->tile_bake_seed = rand();
    document->entity_count = 0;

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == 0) {
            sprintf(document->players[player_id].name, "Player");
        } else {
            sprintf(document->players[player_id].name, "Enemy %u", player_id);
        }
        document->players[player_id].starting_gold = 50;
    }

    document->squad_count = 0;

    return document;
}

void editor_document_init_map(EditorDocument* document, MapType map_type) {
    document->map = new Map();
    map_init(*document->map, map_type, document->noise->width, document->noise->height);
    map_cleanup_noise(*document->map, document->noise);

    editor_document_bake_map(document, true);
}

void editor_document_bake_map(EditorDocument* document, bool remove_artifacts) {
    int lcg_seed = document->tile_bake_seed;
    if (remove_artifacts) {
        map_bake_map_tiles_and_remove_artifacts(*document->map, document->noise, &lcg_seed);
    } else {
        map_bake_tiles(*document->map, document->noise, &lcg_seed);
    }
    map_bake_front_walls(*document->map);
    map_bake_ramps(*document->map, document->noise);
    for (int index = 0; index < document->map->width * document->map->height; index++) {
        if (document->map->cells[CELL_LAYER_GROUND][index].type == CELL_BLOCKED) {
            document->map->cells[CELL_LAYER_GROUND][index].type = CELL_EMPTY;
        }
    }

    for (int y = 0; y < document->map->height; y++) {
        for (int x = 0; x < document->map->width; x++) {
            if (map_should_cell_be_blocked(*document->map, ivec2(x, y)) &&
                    map_get_cell(*document->map, CELL_LAYER_GROUND, ivec2(x,y)).type == CELL_EMPTY) {
                map_set_cell(*document->map, CELL_LAYER_GROUND, ivec2(x, y), (Cell) {
                    .type = CELL_BLOCKED,
                    .id = ID_NULL
                });
            }
        }
    }
}

EditorDocument* editor_document_init_blank(MapType map_type, MapSize map_size) {
    // Init base document
    EditorDocument* document = editor_document_base_init();
    if (document == NULL) {
        return NULL;
    }

    // Init noise
    int map_size_value = match_setting_get_map_size(map_size);
    document->noise = noise_init(map_size_value, map_size_value);
    if (document->noise == NULL) {
        free(document);
        return NULL;
    }

    // Clear out noise values to make a blank map
    for (int index = 0; index < document->noise->width * document->noise->height; index++) {
        document->noise->map[index] = NOISE_VALUE_LOWGROUND;
        document->noise->forest[index] = 0;
    }

    editor_document_init_map(document, map_type);

    return document;
}

EditorDocument* editor_document_init_generated(MapType map_type, const NoiseGenParams& params) {
    // Init base document
    EditorDocument* document = editor_document_base_init();
    if (document == NULL) {
        return NULL;
    }

    // Generate noise
    document->noise = noise_generate(params);
    if (document->noise == NULL) {
        free(document);
        return NULL;
    }

    editor_document_init_map(document, map_type);

    return document;
}

void editor_document_free(EditorDocument* document) {
    noise_free(document->noise);
    delete document->map;
    free(document);
}

EditorSquad editor_document_squad_init() {
    EditorSquad squad;
    sprintf(squad.name, "New Squad");
    squad.player_id = 1;
    squad.type = EDITOR_SQUAD_TYPE_DEFEND;
    squad.entity_count = 0;

    return squad;
}

bool editor_document_squads_are_equal(const EditorSquad& a, const EditorSquad& b) {
    if (strcmp(a.name, b.name) != 0) {
        return false;
    }
    if (a.player_id != b.player_id || a.type != b.type || a.entity_count != b.entity_count) {
        return false;
    }
    for (uint32_t index = 0; index < a.entity_count; index++) {
        if (a.entities[index] != b.entities[index]) {
            return false;
        }
    }
    return true;
}

const char* editor_document_squad_type_str(EditorSquadType type) {
    switch (type) {
        case EDITOR_SQUAD_TYPE_DEFEND:
            return "Defend";
        case EDITOR_SQUAD_TYPE_LANDMINES:
            return "Landmines";
        case EDITOR_SQUAD_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

uint8_t editor_document_get_noise_map_value(EditorDocument* document, ivec2 cell) {
    return document->noise->map[cell.x + (cell.y * document->noise->width)];
}

void editor_document_set_noise_map_value(EditorDocument* document, ivec2 cell, uint8_t value) {
    document->noise->map[cell.x + (cell.y * document->noise->width)] = value;

    editor_document_bake_map(document);
}

#endif
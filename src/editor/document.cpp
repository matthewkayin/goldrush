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

    memset(document->players, 0, sizeof(document->players));
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        sprintf(document->players[player_id].name, "Player %u", (player_id + 1));
        document->players[player_id].recolor_id = player_id;
        document->players[player_id].team = player_id;
    }

    return document;
}

void editor_document_init_map(EditorDocument* document, MapType map_type) {
    document->map = new Map();
    map_init(*document->map, map_type, document->noise->width, document->noise->height);
    map_cleanup_noise(*document->map, document->noise);

    int lcg_seed = document->tile_bake_seed;
    map_bake_map_tiles_and_remove_artifacts(*document->map, document->noise, &lcg_seed);
    map_bake_front_walls(*document->map);
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

uint8_t editor_document_get_noise_map_value(EditorDocument* document, ivec2 cell) {
    return document->noise->map[cell.x + (cell.y * document->noise->width)];
}

void editor_document_set_noise_map_value(EditorDocument* document, ivec2 cell, uint8_t value) {
    document->noise->map[cell.x + (cell.y * document->noise->width)] = value;

    int lcg_seed = document->tile_bake_seed;
    map_bake_tiles(*document->map, document->noise, &lcg_seed);
    map_bake_front_walls(*document->map);
}

#endif
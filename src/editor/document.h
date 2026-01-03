#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "match/noise.h"
#include "match/state.h"
#include "match/map.h"

struct EditorPlayer {
    std::string name;
    uint32_t starting_gold;
};

struct EditorDocument {
    Map* map;
    Noise* noise;
    int tile_bake_seed;
    uint32_t entity_count;
    Entity entities[MATCH_MAX_ENTITIES];
    EditorPlayer players[MAX_PLAYERS];
};

EditorDocument* editor_document_init_blank(MapType map_type, MapSize map_size);
EditorDocument* editor_document_init_generated(MapType map_type, const NoiseGenParams& params);
void editor_document_bake_map(EditorDocument* document, bool remove_artifacts = false);
void editor_document_free(EditorDocument* document);

uint8_t editor_document_get_noise_map_value(EditorDocument* document, ivec2 cell);
void editor_document_set_noise_map_value(EditorDocument* document, ivec2 cell, uint8_t value);

#endif
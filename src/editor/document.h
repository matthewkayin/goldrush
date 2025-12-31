#pragma once

#include "match/noise.h"
#include "match/state.h"
#include "match/map.h"

struct EditorDocument {
    Noise* noise;
    int tile_bake_seed;
    uint32_t entity_count;
    Entity entities[MATCH_MAX_ENTITIES];
    MatchPlayer players[MAX_PLAYERS];

    Map map;
};

EditorDocument* editor_document_init_blank(MapType map_type, MapSize map_size);
EditorDocument* editor_document_init_generated(MapType map_type, const NoiseGenParams& params);
void editor_document_free(EditorDocument* document);

uint8_t editor_document_get_noise_map_value(EditorDocument* document, ivec2 cell);
void editor_document_set_noise_map_value(EditorDocument* document, ivec2 cell, uint8_t value);
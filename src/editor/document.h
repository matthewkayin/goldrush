#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "match/noise.h"
#include "match/state.h"
#include "match/map.h"

#define EDITOR_SQUAD_MAX_ENTITIES SELECTION_LIMIT
#define EDITOR_MAX_SQUADS 32

struct EditorPlayer {
    char name[MAX_USERNAME_LENGTH + 1];
    uint32_t starting_gold;
};

struct EditorEntity {
    EntityType type;
    uint8_t player_id;
    uint32_t gold_held;
    ivec2 cell;
};

enum EditorSquadType {
    EDITOR_SQUAD_TYPE_DEFEND,
    EDITOR_SQUAD_TYPE_LANDMINES,
    EDITOR_SQUAD_TYPE_COUNT
};

struct EditorSquad {
    char name[MAX_USERNAME_LENGTH + 1];
    uint8_t player_id;
    EditorSquadType type;
    uint32_t entity_count;
    uint32_t entities[EDITOR_SQUAD_MAX_ENTITIES];
};

struct EditorDocument {
    Map* map;
    Noise* noise;
    int tile_bake_seed;
    EditorPlayer players[MAX_PLAYERS];
    uint32_t entity_count;
    EditorEntity entities[MATCH_MAX_ENTITIES];
    uint32_t squad_count;
    EditorSquad squads[EDITOR_MAX_SQUADS];
};

EditorDocument* editor_document_init_blank(MapType map_type, MapSize map_size);
EditorDocument* editor_document_init_generated(MapType map_type, const NoiseGenParams& params);
void editor_document_bake_map(EditorDocument* document, bool remove_artifacts = false);
void editor_document_free(EditorDocument* document);

EditorSquad editor_document_squad_init();
bool editor_document_squads_are_equal(const EditorSquad& a, const EditorSquad& b);
const char* editor_document_squad_type_str(EditorSquadType type);

uint8_t editor_document_get_noise_map_value(EditorDocument* document, ivec2 cell);
void editor_document_set_noise_map_value(EditorDocument* document, ivec2 cell, uint8_t value);

#endif
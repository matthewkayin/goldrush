#include "scenario.h"

#include "core/logger.h"
#include "core/filesystem.h"

static const uint32_t MAP_FILE_SIGNATURE = 0x46536877;

Scenario* scenario_base_init() {
    Scenario* scenario = new Scenario();

    scenario->noise = NULL;
    scenario->player_spawn = ivec2(0, 0);
    scenario->entity_count = 0;
    scenario->objective = (ScenarioObjective) {
        .type = SCENARIO_OBJECTIVE_CONQUEST
    };

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == 0) {
            sprintf(scenario->players[player_id].name, "Player");
        } else {
            sprintf(scenario->players[player_id].name, "Enemy %u", player_id);
        }
        scenario->players[player_id].starting_gold = 50;
    }

    scenario->squad_count = 0;

    return scenario;
}

void scenario_init_map(Scenario* scenario, MapType map_type) {
    map_init(scenario->map, map_type, scenario->noise->width, scenario->noise->height);
    map_cleanup_noise(scenario->map, scenario->noise);

    scenario_bake_map(scenario, true);
}

void scenario_bake_map(Scenario* scenario, bool remove_artifacts) {
    int lcg_seed = rand();
    if (remove_artifacts) {
        map_bake_map_tiles_and_remove_artifacts(scenario->map, scenario->noise, &lcg_seed);
    } else {
        map_bake_tiles(scenario->map, scenario->noise, &lcg_seed);
    }
    map_bake_front_walls(scenario->map);
    map_bake_ramps(scenario->map, scenario->noise);
    for (int index = 0; index < scenario->map.width * scenario->map.height; index++) {
        if (scenario->map.cells[CELL_LAYER_GROUND][index].type == CELL_BLOCKED) {
            scenario->map.cells[CELL_LAYER_GROUND][index].type = CELL_EMPTY;
        }
    }

    for (int y = 0; y < scenario->map.height; y++) {
        for (int x = 0; x < scenario->map.width; x++) {
            if (map_should_cell_be_blocked(scenario->map, ivec2(x, y)) &&
                    map_get_cell(scenario->map, CELL_LAYER_GROUND, ivec2(x,y)).type == CELL_EMPTY) {
                map_set_cell(scenario->map, CELL_LAYER_GROUND, ivec2(x, y), (Cell) {
                    .type = CELL_BLOCKED,
                    .id = ID_NULL
                });
            }
        }
    }
}

Scenario* scenario_init_blank(MapType map_type, MapSize map_size) {
    // Init base scenario
    Scenario* scenario = scenario_base_init();
    if (scenario == NULL) {
        return NULL;
    }

    // Init noise
    int map_size_value = match_setting_get_map_size(map_size);
    scenario->noise = noise_init(map_size_value, map_size_value);
    if (scenario->noise == NULL) {
        free(scenario);
        return NULL;
    }

    // Clear out noise values to make a blank map
    for (int index = 0; index < scenario->noise->width * scenario->noise->height; index++) {
        scenario->noise->map[index] = NOISE_VALUE_LOWGROUND;
        scenario->noise->forest[index] = 0;
    }

    scenario_init_map(scenario, map_type);

    return scenario;
}

Scenario* scenario_init_generated(MapType map_type, const NoiseGenParams& params) {
    // Init base scenario
    Scenario* scenario = scenario_base_init();
    if (scenario == NULL) {
        return NULL;
    }

    // Generate noise
    scenario->noise = noise_generate(params);
    if (scenario->noise == NULL) {
        free(scenario);
        return NULL;
    }

    scenario_init_map(scenario, map_type);

    return scenario;
}

void scenario_free(Scenario* scenario) {
    if (scenario->noise != NULL) {
        noise_free(scenario->noise);
    }
    delete scenario;
}

ScenarioSquad scenario_squad_init() {
    ScenarioSquad squad;
    sprintf(squad.name, "New Squad");
    squad.player_id = 1;
    squad.type = SCENARIO_SQUAD_TYPE_DEFEND;
    squad.entity_count = 0;

    return squad;
}

bool scenario_squads_are_equal(const ScenarioSquad& a, const ScenarioSquad& b) {
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

const char* scenario_squad_type_str(ScenarioSquadType type) {
    switch (type) {
        case SCENARIO_SQUAD_TYPE_DEFEND:
            return "Defend";
        case SCENARIO_SQUAD_TYPE_LANDMINES:
            return "Landmines";
        case SCENARIO_SQUAD_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

uint8_t scenario_get_noise_map_value(Scenario* scenario, ivec2 cell) {
    return scenario->noise->map[cell.x + (cell.y * scenario->noise->width)];
}

void scenario_set_noise_map_value(Scenario* scenario, ivec2 cell, uint8_t value) {
    scenario->noise->map[cell.x + (cell.y * scenario->noise->width)] = value;

    scenario_bake_map(scenario);
}

bool scenario_save_file(const Scenario* scenario, const char* path) {
    std::string full_path = filesystem_get_resource_path() + "scenario/" + path;
    FILE* file = fopen(full_path.c_str(), "wb");
    if (file == NULL) {
        log_error("Could not open editor file %s for writing.", full_path.c_str());
        return false;
    }

    // Signature
    fwrite(&MAP_FILE_SIGNATURE, 1, sizeof(uint32_t), file);

    // Map
    uint8_t map_type_byte = (uint8_t)scenario->map.type;
    fwrite(&map_type_byte, 1, sizeof(uint8_t), file);
    fwrite(&scenario->map.width, 1, sizeof(int), file);
    fwrite(&scenario->map.height, 1, sizeof(int), file);
    fwrite(&scenario->map.tiles[0], 1, scenario->map.width * scenario->map.height * sizeof(Tile), file);
    for (uint32_t cell_layer = 0; cell_layer < CELL_LAYER_COUNT; cell_layer++) {
        fwrite(&scenario->map.cells[cell_layer][0], 1, scenario->map.width * scenario->map.height * sizeof(Cell), file);
    }

    // Noise
    noise_fwrite(scenario->noise, file);

    // Player spawn
    fwrite(&scenario->player_spawn, 1, sizeof(ivec2), file);

    // Players
    fwrite(scenario->players, 1, sizeof(scenario->players), file);

    // Entities
    fwrite(&scenario->entity_count, 1, sizeof(uint32_t), file);
    fwrite(scenario->entities, 1, scenario->entity_count * sizeof(ScenarioEntity), file);

    // Squads
    fwrite(&scenario->squad_count, 1, sizeof(uint32_t), file);
    fwrite(scenario->squads, 1, scenario->squad_count * sizeof(ScenarioSquad), file);

    // Objective
    fwrite(&scenario->objective, 1, sizeof(ScenarioObjective), file);

    fclose(file);
    log_info("Map file saved successfully.");
    return true;
}

Scenario* scenario_open_file(const char* path) {
    std::string full_path = filesystem_get_resource_path() + "scenario/" + path;
    FILE* file = fopen(full_path.c_str(), "rb");
    if (file == NULL) {
        log_error("Could not open map file for reading with path %s.", full_path.c_str());
        return NULL;
    }

    // Signature
    uint32_t signature;
    fread(&signature, 1, sizeof(uint32_t), file);
    if (signature != MAP_FILE_SIGNATURE) {
        log_error("File %s is not a valid map file.", full_path.c_str());
        fclose(file);
        return NULL;
    }

    // Init scenario
    Scenario* scenario = scenario_base_init();
    if (scenario == NULL) {
        log_error("Error creating editor scenario.");
        fclose(file);
        return NULL;
    }

    // Map type
    uint8_t map_type_byte;
    fread(&map_type_byte, 1, sizeof(uint8_t), file);
    MapType map_type = (MapType)map_type_byte;

    // Map width, height
    int map_width, map_height;
    fread(&map_width, 1, sizeof(int), file);
    fread(&map_height, 1, sizeof(int), file);

    // Map init
    map_init(scenario->map, map_type, map_width, map_height);

    // Map tiles
    fread(&scenario->map.tiles[0], 1, map_width * map_height * sizeof(Tile), file);

    // Map cells
    for (uint32_t cell_layer = 0; cell_layer < CELL_LAYER_COUNT; cell_layer++) {
        fread(&scenario->map.cells[cell_layer][0], 1, map_width * map_height * sizeof(Cell), file);
    }

    // Noise
    scenario->noise = noise_fread(file);
    if (scenario->noise == NULL) {
        log_error("Error reading noise.");
        scenario_free(scenario);
        fclose(file);
        return NULL;
    }

    // Player spawn
    fread(&scenario->player_spawn, 1, sizeof(ivec2), file);

    // Players
    fread(scenario->players, 1, sizeof(scenario->players), file);

    // Entities
    fread(&scenario->entity_count, 1, sizeof(uint32_t), file);
    fread(scenario->entities, 1, scenario->entity_count * sizeof(ScenarioEntity), file);

    // Squads
    fread(&scenario->squad_count, 1, sizeof(uint32_t), file);
    fread(scenario->squads, 1, scenario->squad_count * sizeof(ScenarioSquad), file);

    // Objective
    fread(&scenario->objective, 1, sizeof(ScenarioObjective), file);

    fclose(file);
    log_info("Loaded map file %s.", full_path.c_str());

    return scenario;
}
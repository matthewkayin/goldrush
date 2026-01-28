#include "scenario.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "util/bitflag.h"
#include "util/json.h"
#include "util/util.h"

static const uint32_t MAP_FILE_SIGNATURE = 0x46536877;
static const uint32_t MAP_FILE_VERSION = 0;

Scenario* scenario_base_init() {
    Scenario* scenario = new Scenario();

    scenario->noise = NULL;
    scenario->player_spawn = ivec2(0, 0);
    scenario->entity_count = 0;

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id == 0) {
            sprintf(scenario->players[player_id].name, "Player");
        } else {
            sprintf(scenario->players[player_id].name, "Enemy %u", player_id);
        }
        scenario->players[player_id].starting_gold = 50;
    }

    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        scenario->player_allowed_entities[entity_type] = true;
    }

    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        scenario->player_allowed_upgrades |= 1U << upgrade_index;
    }

    // Init bot configs
    for (uint32_t bot_index = 0; bot_index < MAX_PLAYERS - 1; bot_index++) {
        scenario->bot_config[bot_index] = bot_config_init_from_difficulty(DIFFICULTY_HARD);
        scenario->bot_config[bot_index].allowed_upgrades = 0;
        scenario->bot_config[bot_index].flags = 0;
        scenario->bot_config[bot_index].target_base_count = 0;
        memset(scenario->bot_config[bot_index].is_entity_allowed, 0, sizeof(scenario->bot_config[bot_index].is_entity_allowed));
    }

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

ScenarioSquadType scenario_squad_type_from_str(const char* str) {
    return (ScenarioSquadType)enum_from_str(str, (EnumToStrFn)scenario_squad_type_str, SCENARIO_CONSTANT_TYPE_COUNT);
}

const char* scenario_constant_type_str(ScenarioConstantType type) {
    switch (type) {
        case SCENARIO_CONSTANT_TYPE_ENTITY:
            return "Entity";
        case SCENARIO_CONSTANT_TYPE_CELL:
            return "Cell";
        case SCENARIO_CONSTANT_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

ScenarioConstantType scenario_constant_type_from_str(const char* str) {
    return (ScenarioConstantType)enum_from_str(str, (EnumToStrFn)scenario_constant_type_str, SCENARIO_CONSTANT_TYPE_COUNT);
}

void scenario_constant_set_type(ScenarioConstant& constant, ScenarioConstantType type) {
    constant.type = type;
    switch (type) {
        case SCENARIO_CONSTANT_TYPE_ENTITY: {
            constant.entity_index = INDEX_INVALID;
            break;
        }
        case SCENARIO_CONSTANT_TYPE_CELL: {
            constant.cell = ivec2(0, 0);
            break;
        }
        case SCENARIO_CONSTANT_TYPE_COUNT: {
            break;
        }
    }
}

uint8_t scenario_get_noise_map_value(Scenario* scenario, ivec2 cell) {
    return scenario->noise->map[cell.x + (cell.y * scenario->noise->width)];
}

void scenario_set_noise_map_value(Scenario* scenario, ivec2 cell, uint8_t value) {
    scenario->noise->map[cell.x + (cell.y * scenario->noise->width)] = value;

    scenario_bake_map(scenario);
}

bool scenario_save_file(const Scenario* scenario, const char* json_full_path, const char* map_short_path, const char* script_short_path) {
    const std::string folder_path = filesystem_get_path_folder(json_full_path);

    // Write map file
    {
        std::string map_full_path = std::string(folder_path) + std::string(map_short_path);
        FILE* map_file = fopen(map_full_path.c_str(), "wb");
        if (map_file == NULL) {
            log_error("Could not open map file %s for writing.", map_full_path.c_str());
            return false;
        }

        // Signature
        fwrite(&MAP_FILE_SIGNATURE, 1, sizeof(uint32_t), map_file);

        // Version
        fwrite(&MAP_FILE_VERSION, 1, sizeof(uint32_t), map_file);

        // Map
        uint8_t map_type_byte = (uint8_t)scenario->map.type;
        fwrite(&map_type_byte, 1, sizeof(uint8_t), map_file);
        fwrite(&scenario->map.width, 1, sizeof(int), map_file);
        fwrite(&scenario->map.height, 1, sizeof(int), map_file);
        fwrite(&scenario->map.tiles[0], 1, scenario->map.width * scenario->map.height * sizeof(Tile), map_file);
        for (uint32_t cell_layer = 0; cell_layer < CELL_LAYER_COUNT; cell_layer++) {
            fwrite(&scenario->map.cells[cell_layer][0], 1, scenario->map.width * scenario->map.height * sizeof(Cell), map_file);
        }

        // Noise
        noise_fwrite(scenario->noise, map_file);

        fclose(map_file);
    }

    // Build scenario json
    Json* scenario_json = json_object();
    {
        json_object_set_string(scenario_json, "map", map_short_path);
        json_object_set_string(scenario_json, "script", script_short_path);

        // Player spawn
        json_object_set(scenario_json, "player_spawn", json_from_ivec2(scenario->player_spawn));

        // Players
        Json* players_json = json_array();
        for (size_t index = 0; index < MAX_PLAYERS; index++) {
            // Base player properties
            Json* player_json = json_object();
            json_object_set_string(player_json, "name", scenario->players[index].name);
            json_object_set_number(player_json, "starting_gold", scenario->players[index].starting_gold);

            // Player config
            if (index != 0) {
                const BotConfig& bot_config = scenario->bot_config[index - 1];

                // Opener
                json_object_set(player_json, "opener", json_string(bot_config_opener_str(bot_config.opener)));

                // Unit comp
                json_object_set(player_json, "preferred_unit_comp", json_string(bot_config_unit_comp_str(bot_config.preferred_unit_comp)));

                // Bot flags
                Json* flags_json = json_array();
                for (uint32_t flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
                    uint32_t flag = 1U << flag_index;
                    if (!bitflag_check(bot_config.flags, flag)) {
                        continue;
                    }

                    json_array_push_string(flags_json, bot_config_flag_str(flag));
                }
                json_object_set(player_json, "flags", flags_json);

                // Bot config variables
                json_object_set_number(player_json, "target_base_count", bot_config.target_base_count);
                json_object_set_number(player_json, "macro_cycle_cooldown", bot_config.macro_cycle_cooldown);
            }

            // Allowed upgrades
            Json* upgrades_json = json_array();
            uint32_t allowed_upgrades = index == 0 
                ? scenario->player_allowed_upgrades 
                : scenario->bot_config[index - 1].allowed_upgrades;
            for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
                uint32_t upgrade = 1U << upgrade_index;
                if (!bitflag_check(allowed_upgrades, upgrade)) {
                    continue;
                }

                json_array_push_string(upgrades_json, upgrade_get_data(upgrade).name);
            }
            json_object_set(player_json, "allowed_upgrades", upgrades_json);

            // Allowed entities
            Json* allowed_entities_json = json_array();
            const bool* allowed_entities = index == 0
                ? scenario->player_allowed_entities
                : scenario->bot_config[index - 1].is_entity_allowed;
            for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
                if (!allowed_entities[entity_type]) {
                    continue;
                }

                json_array_push_string(allowed_entities_json, entity_get_data((EntityType)entity_type).name);
            }
            json_object_set(player_json, "allowed_entities", allowed_entities_json);

            json_array_push(players_json, player_json);
        }
        json_object_set(scenario_json, "players", players_json);

        // Entities
        Json* entities_json = json_array();
        for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
            const ScenarioEntity& entity = scenario->entities[entity_index];

            Json* entity_json = json_object();
            json_object_set_string(entity_json, "type", entity_get_data(entity.type).name);
            json_object_set_number(entity_json, "player_id", entity.player_id);
            json_object_set_number(entity_json, "gold_held", entity.gold_held);
            json_object_set(entity_json, "cell", json_from_ivec2(entity.cell));

            json_array_push(entities_json, entity_json);
        }
        json_object_set(scenario_json, "entities", entities_json);

        // Squads
        Json* squads_json = json_array();
        for (const ScenarioSquad& squad : scenario->squads) {
            Json* squad_json = json_object();
            json_object_set_string(squad_json, "name", squad.name);
            json_object_set_number(squad_json, "player_id", squad.player_id);
            json_object_set_string(squad_json, "type", scenario_squad_type_str(squad.type));
            
            Json* squad_entities_json = json_array();
            for (uint32_t entity_index = 0; entity_index < squad.entity_count; entity_index++) {
                json_array_push_number(squad_entities_json, squad.entities[entity_index]);
            }
            json_object_set(squad_json, "entities", squad_entities_json);

            json_array_push(squads_json, squad_json);
        }
        json_object_set(scenario_json, "squads", squads_json);

        // Constants
        Json* constants_json = json_array();
        for (const ScenarioConstant& constant : scenario->constants) {
            Json* constant_json = json_object();
            json_object_set_string(constant_json, "name", constant.name);
            json_object_set_string(constant_json, "type", scenario_constant_type_str(constant.type));
            switch (constant.type) {
                case SCENARIO_CONSTANT_TYPE_ENTITY: {
                    json_object_set_number(constant_json, "entity_index", constant.entity_index);
                    break;
                }
                case SCENARIO_CONSTANT_TYPE_CELL: {
                    json_object_set(constant_json, "cell", json_from_ivec2(constant.cell));
                    break;
                }
                case SCENARIO_CONSTANT_TYPE_COUNT: {
                    GOLD_ASSERT(false);
                    break;
                }
            }

            json_array_push(constants_json, constant_json);
        }
        json_object_set(scenario_json, "constants", constants_json);
    }

    bool json_save_success = json_write(scenario_json, json_full_path);
    if (json_save_success) {
        log_info("Scenario %s saved successfully.", json_full_path);
    } else {
        log_error("Unable to save scenario json at path %s", json_full_path);
    }

    json_free(scenario_json);

    return json_save_success;
}

Scenario* scenario_open_file(const char* json_path, std::string* map_short_path, std::string* script_short_path) {
    const std::string folder_path = filesystem_get_path_folder(json_path);

    Json* scenario_json = NULL;
    Scenario* scenario = NULL;
    FILE* map_file = NULL;

    {
        scenario_json = json_read(json_path);
        if (scenario_json == NULL) {
            log_error("Unable to open scenario json at path %s", json_path);
            goto error;
        }

        // Init scenario
        scenario = scenario_base_init();
        if (scenario == NULL) {
            log_error("Error creating editor scenario.");
            goto error;
        }

        // Read map file 
        *map_short_path = std::string(json_object_get_string(scenario_json, "map"));
        {
            std::string map_full_path = std::string(folder_path) + *map_short_path;
            map_file = fopen(map_full_path.c_str(), "rb");
            if (map_file == NULL) {
                log_error("Could not open map file for reading with path %s.", map_full_path.c_str());
                goto error;
            }

            // Signature
            uint32_t signature;
            fread(&signature, 1, sizeof(uint32_t), map_file);
            if (signature != MAP_FILE_SIGNATURE) {
                log_error("File %s is not a valid map file.", map_full_path.c_str());
                goto error;
            }

            // Version
            uint32_t version;
            fread(&version, 1, sizeof(uint32_t), map_file);
            if (version != MAP_FILE_VERSION) {
                log_error("Scenario version %u not handled", version);
                goto error;
            }

            // Map type
            uint8_t map_type_byte;
            fread(&map_type_byte, 1, sizeof(uint8_t), map_file);
            MapType map_type = (MapType)map_type_byte;

            // Map width, height
            int map_width, map_height;
            fread(&map_width, 1, sizeof(int), map_file);
            fread(&map_height, 1, sizeof(int), map_file);

            // Map init
            map_init(scenario->map, map_type, map_width, map_height);

            // Map tiles
            fread(&scenario->map.tiles[0], 1, map_width * map_height * sizeof(Tile), map_file);

            // Map cells
            for (uint32_t cell_layer = 0; cell_layer < CELL_LAYER_COUNT; cell_layer++) {
                fread(&scenario->map.cells[cell_layer][0], 1, map_width * map_height * sizeof(Cell), map_file);
            }

            // Noise
            scenario->noise = noise_fread(map_file);
            if (scenario->noise == NULL) {
                log_error("Error reading noise.");
                goto error;
            }

            fclose(map_file);
            map_file = NULL;
        }

        // Grab script short path
        *script_short_path = std::string(json_object_get_string(scenario_json, "script"));

        // Player spawn
        scenario->player_spawn = json_to_ivec2(json_object_get(scenario_json, "player_spawn"));

        // Players
        Json* players_json = json_object_get(scenario_json, "players");
        for (size_t index = 0; index < MAX_PLAYERS; index++) {
            // Base player properties
            Json* player_json = json_array_get(players_json, index);
            strncpy(scenario->players[index].name, json_object_get_string(player_json, "name"), MAX_USERNAME_LENGTH);
            scenario->players[index].starting_gold = (uint32_t)json_object_get_number(player_json, "starting_gold");

            // Player config
            if (index != 0) {
                scenario->bot_config[index - 1] = bot_config_init();
                BotConfig& bot_config = scenario->bot_config[index - 1];

                // Opener
                Json* opener_json = json_object_get(player_json, "opener");
                if (opener_json != NULL) {
                    bot_config.opener = bot_config_opener_from_str(opener_json->string.value);
                }

                // Unit comp
                Json* preferred_unit_comp_json = json_object_get(player_json, "preferred_unit_comp");
                if (preferred_unit_comp_json != NULL) {
                    bot_config.preferred_unit_comp = bot_config_unit_comp_from_str(preferred_unit_comp_json->string.value);
                }

                // Bot flags
                Json* flags_json = json_object_get(player_json, "flags");
                for (size_t flag_index = 0; flag_index < flags_json->array.length; flag_index++) {
                    const char* flag_str = json_array_get_string(flags_json, flag_index);
                    uint32_t flag = bot_config_flag_from_str(flag_str);
                    if (flag == BOT_CONFIG_FLAG_COUNT) {
                        log_warn("Bot config flag %s for player %u not recognized.", flag_str, index);
                        continue;
                    }
                    bot_config.flags |= flag;
                }

                // Bot config variables
                bot_config.target_base_count = (uint32_t)json_object_get_number(player_json, "target_base_count");
                bot_config.macro_cycle_cooldown = (uint32_t)json_object_get_number(player_json, "macro_cycle_cooldown");
            }

            // Allowed upgrades
            uint32_t* allowed_upgrades = index == 0
                ? &scenario->player_allowed_upgrades
                : &scenario->bot_config[index - 1].allowed_upgrades;
            *allowed_upgrades = 0;
            Json* upgrades_json = json_object_get(player_json, "allowed_upgrades");
            for (size_t upgrades_array_index = 0; upgrades_array_index < upgrades_json->array.length; upgrades_array_index++) {
                std::string upgrade_str = std::string(json_array_get_string(upgrades_json, upgrades_array_index));
                uint32_t upgrade_index;
                for (upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
                    uint32_t upgrade = 1U << upgrade_index;
                    if (strcmp(upgrade_str.c_str(), upgrade_get_data(upgrade).name) == 0) {
                        break;
                    }
                }

                if (upgrade_index < UPGRADE_COUNT) {
                    uint32_t upgrade = 1U << upgrade_index;
                    *allowed_upgrades |= upgrade;
                } else {
                    log_warn("Allowed upgrade %s for player %u not recognized.", upgrade_str.c_str(), index);
                    continue;
                }
            }

            // Allowed entities
            bool* allowed_entities = index == 0
                ? scenario->player_allowed_entities
                : scenario->bot_config[index - 1].is_entity_allowed;
            memset(allowed_entities, 0, ENTITY_TYPE_COUNT * sizeof(bool));
            Json* allowed_entities_json = json_object_get(player_json, "allowed_entities");
            for (size_t allowed_entities_index = 0; allowed_entities_index < allowed_entities_json->array.length; allowed_entities_index++) {
                const char* entity_str = json_array_get_string(allowed_entities_json, allowed_entities_index);
                EntityType entity_type = entity_type_from_str(entity_str);
                if (entity_type < ENTITY_TYPE_COUNT) {
                    allowed_entities[entity_type] = true;
                } else {
                    log_warn("Allowed entity %s for player %u not recognized.", entity_str, index);
                    continue;
                }
            }
        } // End for each player

        // Entities
        Json* entities_json = json_object_get(scenario_json, "entities");
        for (size_t entity_index = 0; entity_index < entities_json->array.length; entity_index++) {
            Json* entity_json = json_array_get(entities_json, entity_index);

            const char* entity_str = json_object_get_string(entity_json, "type");
            EntityType entity_type = entity_type_from_str(entity_str);
            if (entity_type == ENTITY_TYPE_COUNT) {
                log_warn("Entity type %s for entity index %u not recognized.", entity_str, entity_index);
                continue;
            }

            ScenarioEntity entity;
            entity.type = entity_type;
            entity.player_id = (uint8_t)json_object_get_number(entity_json, "player_id");
            entity.gold_held = (uint32_t)json_object_get_number(entity_json, "gold_held");
            entity.cell = json_to_ivec2(json_object_get(entity_json, "cell"));

            scenario->entities[scenario->entity_count] = entity;
            scenario->entity_count++;
        }

        // Squads
        Json* squads_json = json_object_get(scenario_json, "squads");
        for (size_t squad_index = 0; squad_index < squads_json->array.length; squad_index++) {
            Json* squad_json = json_array_get(squads_json, squad_index);

            const char* squad_type_str = json_object_get_string(squad_json, "type");
            ScenarioSquadType squad_type = scenario_squad_type_from_str(squad_type_str);
            if (squad_type == SCENARIO_SQUAD_TYPE_COUNT) {
                log_warn("Squad type %s for squad %u not recognized.", squad_type_str, squad_index);
                continue;
            }

            ScenarioSquad squad;
            strncpy(squad.name, json_object_get_string(squad_json, "name"), MAX_USERNAME_LENGTH);
            squad.type = squad_type;
            squad.player_id = (uint8_t)json_object_get_number(squad_json, "player_id");

            Json* squad_entities_json = json_object_get(squad_json, "entities");
            squad.entity_count = 0;
            for (size_t squad_entities_index = 0; squad_entities_index < squad_entities_json->array.length; squad_entities_index++) {
                uint32_t entity_index = (uint32_t)json_array_get_number(squad_entities_json, squad_entities_index);
                if (entity_index >= scenario->entity_count) {
                    log_warn("Entity index %u for squad %u (%s) is out of range.", entity_index, squad_index, squad.name);
                    continue;
                }
                squad.entities[squad.entity_count] = entity_index;
                squad.entity_count++;
            }

            scenario->squads.push_back(squad);
        }

        // Constants
        Json* constants_json = json_object_get(scenario_json, "constants");
        for (size_t constant_index = 0; constant_index < constants_json->array.length; constant_index++) {
            Json* constant_json = json_array_get(constants_json, constant_index);

            ScenarioConstant constant;
            strncpy(constant.name, json_object_get_string(constant_json, "name"), SCENARIO_CONSTANT_NAME_BUFFER_LENGTH);

            const char* constant_type_str = json_object_get_string(constant_json, "type");
            constant.type = scenario_constant_type_from_str(constant_type_str);
            if (constant.type == SCENARIO_CONSTANT_TYPE_COUNT) {
                log_warn("Constant type %s for constant %u:%s not recognized.", constant_type_str, constant_index, constant.name);
                continue;
            }

            switch (constant.type) {
                case SCENARIO_CONSTANT_TYPE_ENTITY: {
                    constant.entity_index = (uint32_t)json_object_get_number(constant_json, "entity_index");
                    break;
                }
                case SCENARIO_CONSTANT_TYPE_CELL: {
                    constant.cell = json_to_ivec2(json_object_get(constant_json, "cell"));
                    break;
                }
                case SCENARIO_CONSTANT_TYPE_COUNT: {
                    GOLD_ASSERT(false);
                    break;
                }
            }

            scenario->constants.push_back(constant);
        }   

        json_free(scenario_json);
        log_info("Loaded scenario %s.", json_path);

        return scenario;
    }
error:
    if (scenario_json != NULL) {
        json_free(scenario_json);
    }
    if (scenario != NULL) {
        scenario_free(scenario);
    }
    if (map_file != NULL) {
        fclose(map_file);
    }

    return NULL;
}
#include "scenario.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "util/json.h"
#include "util/bitflag.h"

static const uint32_t MAP_FILE_SIGNATURE = 0x46536877;

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

uint8_t scenario_get_noise_map_value(Scenario* scenario, ivec2 cell) {
    return scenario->noise->map[cell.x + (cell.y * scenario->noise->width)];
}

void scenario_set_noise_map_value(Scenario* scenario, ivec2 cell, uint8_t value) {
    scenario->noise->map[cell.x + (cell.y * scenario->noise->width)] = value;

    scenario_bake_map(scenario);
}

bool scenario_save_json(const Scenario* scenario, const char* path) {
    Json* json = json_object();

    // Map
    Json* map = json_object();
    json_object_set(json, "map", map);

    json_object_set_string(map, "type", match_setting_data(MATCH_SETTING_MAP_TYPE).values.at(scenario->map.type).c_str());
    json_object_set_number(map, "width", scenario->map.width);
    json_object_set_number(map, "height", scenario->map.height);

    // Map tiles
    Json* map_tiles = json_array();
    json_object_set(map, "tiles", map_tiles);
    for (size_t index = 0; index < scenario->map.tiles.size(); index++) {
        Json* tile = json_object();
        json_object_set_number(tile, "sprite", scenario->map.tiles[index].sprite);
        json_object_set_number(tile, "frame_x", scenario->map.tiles[index].frame.x);
        json_object_set_number(tile, "frame_y", scenario->map.tiles[index].frame.y);
        json_object_set_number(tile, "elevation", scenario->map.tiles[index].elevation);

        json_array_push(map_tiles, tile);
    }

    // Map cells
    Json* map_cell_layers = json_array();
    json_object_set(map, "cells", map_cell_layers);
    for (size_t layer = 0; layer < CELL_LAYER_COUNT; layer++) {
        Json* map_cells = json_array();
        for (size_t index = 0; index < scenario->map.cells[layer].size(); index++) {
            Json* cell = json_object();
            json_object_set_number(cell, "type", scenario->map.cells[layer][index].type);
            if (scenario->map.cells[layer][index].type == CELL_DECORATION) {
                json_object_set_number(cell, "decoration_hframes", scenario->map.cells[layer][index].decoration_hframe);
            } else {
                json_object_set_number(cell, "id", scenario->map.cells[layer][index].id);
            }

            json_array_push(map_cells, cell);
        }

        json_array_push(map_cell_layers, map_cells);
    }

    // Noise
    Json* noise = json_object();
    json_object_set(json, "noise", noise);
    json_object_set_number(noise, "width", scenario->noise->width);
    json_object_set_number(noise, "height", scenario->noise->height);
    Json* noise_map = json_array();
    json_object_set(noise, "map", noise_map);
    Json* noise_forest = json_array();
    json_object_set(noise, "forest", noise_forest);
    for (size_t index = 0; index < (size_t)(scenario->noise->width * scenario->noise->height); index++) {
        json_array_push_number(noise_map, scenario->noise->map[index]);
        json_array_push_number(noise_forest, scenario->noise->forest[index]);
    }

    // Player spawn
    Json* player_spawn = json_object();
    json_object_set(json, "player_spawn", player_spawn);
    json_object_set_number(player_spawn, "x", scenario->player_spawn.x);
    json_object_set_number(player_spawn, "y", scenario->player_spawn.y);

    // Players
    Json* players = json_array();
    json_object_set(json, "players", players);
    for (size_t index = 0; index < MAX_PLAYERS; index++) {
        Json* player = json_object();
        json_object_set_string(player, "name", scenario->players[index].name);
        json_object_set_number(player, "starting_gold", scenario->players[index].starting_gold);

        Json* config = json_object();
        if (index != 0) {
            const BotConfig& bot_config = scenario->bot_config[index - 1];

            Json* flags = json_array();
            for (uint32_t flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
                uint32_t flag = 1U << flag_index;
                if (!bitflag_check(bot_config.flags, flag)) {
                    continue;
                }

                json_array_push_string(flags, bot_config_flag_str(flag));
            }
            json_object_set(config, "flags", flags);

            json_object_set_number(config, "target_base_count", bot_config.target_base_count);
            json_object_set_number(config, "macro_cycle_cooldown", bot_config.macro_cycle_cooldown);
        }

        Json* upgrades = json_array();
        uint32_t allowed_upgrades = index == 0 ? scenario->player_allowed_upgrades : scenario->bot_config[index - 1].allowed_upgrades;
        for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
            uint32_t upgrade = 1U << upgrade_index;
            if (!bitflag_check(allowed_upgrades, upgrade)) {
                continue;
            }

            json_array_push_string(upgrades, upgrade_get_data(upgrade).name);
        }
        json_object_set(config, "allowed_upgrades", upgrades);

        Json* json_allowed_entities = json_array();
        const bool* allowed_entities = index == 0 ? scenario->player_allowed_entities : scenario->bot_config[index - 1].is_entity_allowed;
        for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
            if (!allowed_entities[entity_type]) {
                continue;
            }

            json_array_push_string(json_allowed_entities, entity_get_data((EntityType)entity_type).name);
        }
        json_object_set(config, "allowed_entities", json_allowed_entities);

        json_object_set(player, "config", config);
        json_array_push(players, player);
    }

    // Entities
    Json* entities = json_array();
    for (uint32_t entity_index = 0; entity_index < scenario->entity_count; entity_index++) {
        const ScenarioEntity& entity = scenario->entities[entity_index];

        Json* json_entity = json_object();
        json_object_set_string(json_entity, "type", entity_get_data(entity.type).name);
        json_object_set_number(json_entity, "player_id", entity.player_id);
        json_object_set_number(json_entity, "gold_held", entity.gold_held);
        json_object_set_number(json_entity, "cell_x", entity.cell.x);
        json_object_set_number(json_entity, "cell_y", entity.cell.y);

        json_array_push(entities, json_entity);
    }
    json_object_set(json, "entities", entities);

    // Squads
    Json* squads = json_array();
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

        json_array_push(squads, squad_json);
    }
    json_object_set(json, "squads", squads);

    // Triggers
    Json* triggers = json_array();
    for (const Trigger& trigger : scenario->triggers) {
        Json* json_trigger = json_object();
        json_object_set_boolean(json_trigger, "is_active", trigger.is_active);
        json_object_set_string(json_trigger, "name", trigger.name);

        Json* json_trigger_conditions = json_array();
        for (const TriggerCondition& condition : trigger.conditions) {
            Json* json_condition = json_object();
            json_object_set_string(json_condition, "type", trigger_condition_type_str(condition.type));

            switch (condition.type) {
                case TRIGGER_CONDITION_TYPE_ENTITY_COUNT: {
                    json_object_set_string(json_condition, "entity_type", entity_get_data(condition.entity_count.entity_type).name);
                    json_object_set_number(json_condition, "entity_count", condition.entity_count.entity_count);
                    break;
                }
                case TRIGGER_CONDITION_TYPE_OBJECTIVE_COMPLETE: {
                    json_object_set_number(json_condition, "objective_index", condition.objective_complete.objective_index);
                    break;
                }
                case TRIGGER_CONDITION_TYPE_AREA_DISCOVERED: {
                    json_object_set_number(json_condition, "cell_x", condition.area_discovered.cell.x);
                    json_object_set_number(json_condition, "cell_y", condition.area_discovered.cell.y);
                    json_object_set_number(json_condition, "cell_size", condition.area_discovered.cell_size);
                    break;
                }
                case TRIGGER_CONDITION_TYPE_FULL_BUNKER:
                    break;
                case TRIGGER_CONDITION_TYPE_COUNT:
                    GOLD_ASSERT(false);
            }

            json_array_push(json_trigger_conditions, json_condition);
        }
        json_object_set(json_trigger, "conditions", json_trigger_conditions);

        Json* json_trigger_actions = json_array();
        for (const TriggerAction& action : trigger.actions) {
            Json* json_action = json_object();
            json_object_set_string(json_action, "type", trigger_action_type_str(action.type));

            switch (action.type) {
                case TRIGGER_ACTION_TYPE_CHAT: {
                    json_object_set_string(json_action, "chat_type", trigger_action_chat_type_str(action.chat.type));
                    json_object_set_string(json_action, "prefix", action.chat.prefix);
                    json_object_set_string(json_action, "message", action.chat.message);
                    break;
                }
                case TRIGGER_ACTION_TYPE_ADD_OBJECTIVE: {
                    json_object_set_number(json_action, "objective_index", action.add_objective.objective_index);
                    break;
                }
                case TRIGGER_ACTION_TYPE_COMPLETE_OBJECTIVE: {
                    json_object_set_number(json_action, "objective_index", action.finish_objective.objective_index);
                    json_object_set_boolean(json_action, "is_complete", action.finish_objective.is_complete);
                    break;
                }
                case TRIGGER_ACTION_TYPE_WAIT: {
                    json_object_set_number(json_action, "seconds", action.wait.seconds);
                    break;
                }
                case TRIGGER_ACTION_TYPE_FOG_REVEAL: {
                    json_object_set_number(json_action, "cell_x", action.fog.cell.x);
                    json_object_set_number(json_action, "cell_y", action.fog.cell.y);
                    json_object_set_number(json_action, "cell_size", action.fog.cell_size);
                    json_object_set_number(json_action, "sight", action.fog.sight);
                    json_object_set_number(json_action, "duration_seconds", action.fog.duration_seconds);
                    break;
                }
                case TRIGGER_ACTION_TYPE_ALERT: {
                    json_object_set_number(json_action, "cell_x", action.alert.cell.x);
                    json_object_set_number(json_action, "cell_y", action.alert.cell.y);
                    json_object_set_number(json_action, "cell_size", action.alert.cell_size);
                    break;
                }
                case TRIGGER_ACTION_TYPE_SPAWN_UNITS: {
                    json_object_set_number(json_action, "player_id", action.spawn_units.player_id);
                    json_object_set_number(json_action, "spawn_cell_x", action.spawn_units.spawn_cell.x);
                    json_object_set_number(json_action, "spawn_cell_y", action.spawn_units.spawn_cell.y);
                    json_object_set_number(json_action, "target_cell_x", action.spawn_units.target_cell.x);
                    json_object_set_number(json_action, "target_cell_y", action.spawn_units.target_cell.y);

                    Json* action_entities = json_array();
                    for (uint32_t index = 0; index < action.spawn_units.entity_count; index++) {
                        json_array_push_string(action_entities, entity_get_data(action.spawn_units.entity_types[index]).name);
                    }
                    json_object_set(json_action, "entities", action_entities);
                    break;
                }
                case TRIGGER_ACTION_TYPE_SET_LOSE_CONDITION: {
                    json_object_set_boolean(json_action, "lose_on_buildings_destroyed", action.set_lose_condition.lose_on_buildings_destroyed);
                    break;
                }
                case TRIGGER_ACTION_TYPE_HIGHLIGHT_ENTITY: {
                    json_object_set_number(json_action, "entity_index", action.highlight_entity.entity_index);
                    break;
                }
                case TRIGGER_ACTION_TYPE_CAMERA_PAN: {
                    json_object_set_number(json_action, "cell_x", action.camera_pan.cell.x);
                    json_object_set_number(json_action, "cell_y", action.camera_pan.cell.y);
                    json_object_set_number(json_action, "duration", action.camera_pan.duration);
                    break;
                }
                case TRIGGER_ACTION_TYPE_SOUND: {
                    json_object_set_number(json_action, "sound", action.sound.sound);
                    break;
                }
                case TRIGGER_ACTION_TYPE_CAMERA_RETURN:
                case TRIGGER_ACTION_TYPE_CAMERA_FREE:
                case TRIGGER_ACTION_TYPE_CLEAR_OBJECTIVES:
                case TRIGGER_ACTION_TYPE_SHOW_ENEMY_GOLD:
                    break;
                case TRIGGER_ACTION_TYPE_COUNT:
                    GOLD_ASSERT(false);
                    break;
            }

            json_array_push(json_trigger_actions, json_action);
        }
        json_object_set(json_trigger, "actions", json_trigger_actions);

        json_array_push(triggers, json_trigger);
    }
    json_object_set(json, "triggers", triggers);

    // Objectives
    Json* objectives = json_array();
    for (const Objective& objective : scenario->objectives) {
        Json* json_objective = json_object();
        json_object_set_boolean(json_objective, "is_complete", objective.is_complete);
        json_object_set_string(json_objective, "counter_type", objective_counter_type_str(objective.counter_type));
        json_object_set_number(json_objective, "counter_value", objective.counter_value);
        json_object_set_number(json_objective, "counter_target", objective.counter_target);
        json_object_set_string(json_objective, "description", objective.description);

        json_array_push(objectives, json_objective);
    }
    json_object_set(json, "objectives", objectives);

    bool success = json_write(json, path);

    json_free(json);

    if (success) {
        log_info("Scenario json saved successfully to %s.", path);
    }
    return success;
}

bool scenario_save_file(const Scenario* scenario, const char* path) {
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        log_error("Could not open editor file %s for writing.", path);
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

    // Player allowed entities
    fwrite(&scenario->player_allowed_entities, 1, sizeof(bool) * ENTITY_TYPE_COUNT, file);

    // Player allowed upgrades
    fwrite(&scenario->player_allowed_upgrades, 1, sizeof(uint32_t), file);

    // Bot config
    fwrite(scenario->bot_config, 1, sizeof(scenario->bot_config), file);

    // Entities
    fwrite(&scenario->entity_count, 1, sizeof(uint32_t), file);
    fwrite(scenario->entities, 1, scenario->entity_count * sizeof(ScenarioEntity), file);

    // Squads
    size_t squads_size = scenario->squads.size();
    fwrite(&squads_size, 1, sizeof(size_t), file);
    fwrite(&scenario->squads[0], 1, scenario->squads.size() * sizeof(ScenarioSquad), file);

    // Triggers
    size_t triggers_size = scenario->triggers.size();
    fwrite(&triggers_size, 1, sizeof(size_t), file);
    for (size_t index = 0; index < triggers_size; index++) {
        const Trigger& trigger = scenario->triggers[index];

        fwrite(trigger.name, 1, sizeof(trigger.name), file);
        fwrite(&trigger.is_active, 1, sizeof(bool), file);

        // Conditions
        size_t conditions_size = trigger.conditions.size();
        fwrite(&conditions_size, 1, sizeof(size_t), file);
        fwrite(&trigger.conditions[0], 1, conditions_size * sizeof(TriggerCondition), file);

        // Actions
        size_t actions_size = trigger.actions.size();
        fwrite(&actions_size, 1, sizeof(size_t), file);
        fwrite(&trigger.actions[0], 1, actions_size * sizeof(TriggerAction), file);
    }

    // Objectives
    size_t objectives_size = scenario->objectives.size();
    fwrite(&objectives_size, 1, sizeof(size_t), file);
    fwrite(&scenario->objectives[0], 1, objectives_size * sizeof(Objective), file);

    fclose(file);
    log_info("Map file %s saved successfully.", path);
    return true;
}

Scenario* scenario_open_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        log_error("Could not open map file for reading with path %s.", path);
        return NULL;
    }

    // Signature
    uint32_t signature;
    fread(&signature, 1, sizeof(uint32_t), file);
    if (signature != MAP_FILE_SIGNATURE) {
        log_error("File %s is not a valid map file.", path);
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

    // Allowed tech
    fread(&scenario->player_allowed_entities, 1, sizeof(bool) * ENTITY_TYPE_COUNT, file);

    // Allowed upgrades
    fread(&scenario->player_allowed_upgrades, 1, sizeof(uint32_t), file);

    // Bot config
    fread(scenario->bot_config, 1, sizeof(scenario->bot_config), file);

    // Entities
    fread(&scenario->entity_count, 1, sizeof(uint32_t), file);
    fread(scenario->entities, 1, scenario->entity_count * sizeof(ScenarioEntity), file);

    // Squads
    size_t squads_size;
    fread(&squads_size, 1, sizeof(size_t), file);
    scenario->squads = std::vector<ScenarioSquad>(squads_size);
    fread(&scenario->squads[0], 1, squads_size * sizeof(ScenarioSquad), file);
    
    // Triggers
    size_t triggers_size;
    fread(&triggers_size, 1, sizeof(size_t), file);
    scenario->triggers = std::vector<Trigger>(triggers_size);
    for (size_t index = 0; index < triggers_size; index++) {
        Trigger& trigger = scenario->triggers[index];

        fread(trigger.name, 1, sizeof(trigger.name), file);
        fread(&trigger.is_active, 1, sizeof(bool), file);

        // Conditions
        size_t conditions_size;
        fread(&conditions_size, 1, sizeof(size_t), file);
        trigger.conditions = std::vector<TriggerCondition>(conditions_size);
        fread(&trigger.conditions[0], 1, conditions_size * sizeof(TriggerCondition), file);

        // Actions
        size_t actions_size;
        fread(&actions_size, 1, sizeof(size_t), file);
        trigger.actions = std::vector<TriggerAction>(actions_size);
        fread(&trigger.actions[0], 1, actions_size * sizeof(TriggerAction), file);
    }

    // Objectives
    size_t objectives_size;
    fread(&objectives_size, 1, sizeof(size_t), file);
    scenario->objectives = std::vector<Objective>(objectives_size);
    fread(&scenario->objectives[0], 1, objectives_size * sizeof(Objective), file);

    fclose(file);
    log_info("Loaded map file %s.", path);

    return scenario;
}
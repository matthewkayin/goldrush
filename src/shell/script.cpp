#include "shell/shell.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "network/network.h"
#include "util/util.h"
#include "util/bitflag.h"
#include <algorithm>

#ifdef GOLD_DEBUG
    #include <fstream>
#endif

enum ScriptChatColor {
    SCRIPT_CHAT_COLOR_WHITE,
    SCRIPT_CHAT_COLOR_GOLD,
    SCRIPT_CHAT_COLOR_BLUE
};

struct ScriptConstant {
    const char* name;
    int value;
};

// Lua function predeclares

// General
static int script_log(lua_State* lua_state);
static int script_play_sound(lua_State* lua_state);
static int script_get_time(lua_State* lua_state);

// Match over
static int script_set_match_over_victory(lua_State* lua_state);
static int script_set_match_over_defeat(lua_State* lua_state);

// Player
static int script_player_is_defeated(lua_State* lua_state);
static int script_player_get_gold(lua_State* lua_state);
static int script_player_get_gold_mined_total(lua_State* lua_state);

// Chat
static int script_chat(lua_State* lua_state);
static int script_hint(lua_State* lua_state);

// Camera
static int script_fog_reveal(lua_State* lua_state);
static int script_get_camera_centered_cell(lua_State* lua_state);
static int script_begin_camera_pan(lua_State* lua_state);
static int script_hold_camera(lua_State* lua_state);
static int script_release_camera(lua_State* lua_state);
static int script_get_camera_mode(lua_State* lua_state);

// Objectives
static int script_add_objective(lua_State* lua_state);
static int script_complete_objective(lua_State* lua_state);
static int script_is_objective_complete(lua_State* lua_state);
static int script_are_objectives_complete(lua_State* lua_state);
static int script_clear_objectives(lua_State* lua_state);
static int script_set_global_objective_counter(lua_State* lua_state);
static int script_entity_find_spawn_cell(lua_State* lua_state);
static int script_entity_create(lua_State* lua_state);
static int script_entity_get_player_who_controls_goldmine(lua_State* lua_state);

// Entities
static int script_entity_is_visible_to_player(lua_State* lua_state);
static int script_highlight_entity(lua_State* lua_state);
static int script_player_get_entity_count(lua_State* lua_state);
static int script_player_get_full_bunker_count(lua_State* lua_state);
static int script_player_has_entity_near_cell(lua_State* lua_state);
static int script_entity_get_info(lua_State* lua_state);
static int script_entity_get_gold_cost(lua_State* lua_state);

// Bot
static int script_bot_add_squad(lua_State* lua_state);
static int script_bot_squad_exists(lua_State* lua_state);
static int script_bot_set_config_flag(lua_State* lua_state);
static int script_bot_reserve_entity(lua_State* lua_state);
static int script_bot_release_entity(lua_State* lua_state);

// Match input
static int script_queue_match_input(lua_State* lua_state);

static const char* MODULE_NAME = "scenario";

static const luaL_reg GOLD_FUNCS[] = {
    // General
    { "log", script_log },
    { "play_sound", script_play_sound },
    { "get_time", script_get_time },

    // Match over
    { "set_match_over_victory", script_set_match_over_victory },
    { "set_match_over_defeat", script_set_match_over_defeat },

    // Player
    { "player_is_defeated", script_player_is_defeated },
    { "player_get_gold", script_player_get_gold },
    { "player_get_gold_mined_total", script_player_get_gold_mined_total },
    { "player_get_entity_count", script_player_get_entity_count },
    { "player_get_full_bunker_count", script_player_get_full_bunker_count },
    { "player_has_entity_near_cell", script_player_has_entity_near_cell },

    // Chat
    { "chat", script_chat },
    { "hint", script_hint },

    // Camera
    { "fog_reveal", script_fog_reveal },
    { "get_camera_centered_cell", script_get_camera_centered_cell },
    { "begin_camera_pan", script_begin_camera_pan },
    { "hold_camera", script_hold_camera },
    { "release_camera", script_release_camera },
    { "get_camera_mode", script_get_camera_mode },

    // Objectives
    { "add_objective", script_add_objective },
    { "complete_objective", script_complete_objective },
    { "is_objective_complete", script_is_objective_complete },
    { "are_objectives_complete", script_are_objectives_complete },
    { "clear_objectives", script_clear_objectives },
    { "set_global_objective_counter", script_set_global_objective_counter },

    // Entities
    { "entity_is_visible_to_player", script_entity_is_visible_to_player },
    { "highlight_entity", script_highlight_entity },
    { "entity_get_info", script_entity_get_info },
    { "entity_get_gold_cost", script_entity_get_gold_cost },
    { "entity_find_spawn_cell", script_entity_find_spawn_cell },
    { "entity_create", script_entity_create },
    { "entity_get_player_who_controls_goldmine", script_entity_get_player_who_controls_goldmine },

    // Bot
    { "bot_add_squad", script_bot_add_squad },
    { "bot_squad_exists", script_bot_squad_exists },
    { "bot_set_config_flag", script_bot_set_config_flag },
    { "bot_reserve_entity", script_bot_reserve_entity },
    { "bot_release_entity", script_bot_release_entity },

    // Match input
    { "queue_match_input", script_queue_match_input },

    { NULL, NULL }
};

static const ScriptConstant GOLD_CONSTANTS[] = {
    { "CHAT_COLOR_WHITE", SCRIPT_CHAT_COLOR_WHITE },
    { "CHAT_COLOR_GOLD", SCRIPT_CHAT_COLOR_GOLD },
    { "CHAT_COLOR_BLUE", SCRIPT_CHAT_COLOR_GOLD },
    { "SQUAD_ID_NULL", BOT_SQUAD_ID_NULL },
    { "CAMERA_MODE_FREE", CAMERA_MODE_FREE },
    { "CAMERA_MODE_MINIMAP_DRAG", CAMERA_MODE_MINIMAP_DRAG },
    { "CAMERA_MODE_PAN", CAMERA_MODE_PAN },
    { "CAMERA_MODE_HELD", CAMERA_MODE_HELD },
    { "PLAYER_NONE", PLAYER_NONE },
    { NULL, 0 }
};

void match_shell_script_register_scenario_constants(lua_State* lua_state);
const char* match_shell_script_get_entity_type_str(EntityType type);
const char* match_shell_script_get_entity_mode_str(EntityMode mode);
const char* match_shell_script_get_target_type_str(TargetType type);
const char* match_shell_script_get_global_objective_counter_type_str(GlobalObjectiveCounterType type);
void match_shell_script_call(MatchShellState* state, const char* func_name);

bool match_shell_script_init(MatchShellState* state, const Scenario* scenario, const char* script_path) {
    // Check for script existance
    {
        FILE* script_file = fopen(script_path, "r");
        if (script_file == NULL) {
            log_error("Could not open script file %s.", script_path);
            return false;
        }
        fclose(script_file);
    }

    // Init lua state
    state->scenario_lua_state = luaL_newstate();
    luaL_openlibs(state->scenario_lua_state);

    // Register scenario library
    luaL_register(state->scenario_lua_state, MODULE_NAME, GOLD_FUNCS);

    // Set module path
    lua_getglobal(state->scenario_lua_state, "package");
    lua_getfield(state->scenario_lua_state, -1, "path");
    const char* current_package_path = lua_tostring(state->scenario_lua_state, -1);
    std::string new_package_path = std::string(current_package_path) + ";" + filesystem_get_resource_path() + "scenario/modules/?.lua";
    lua_pushstring(state->scenario_lua_state, new_package_path.c_str());
    lua_setfield(state->scenario_lua_state, -3, "path");
    lua_pop(state->scenario_lua_state, 2);

    // Give lua a pointer to the shell state
    lua_pushlightuserdata(state->scenario_lua_state, state);
    lua_setfield(state->scenario_lua_state, LUA_REGISTRYINDEX, "__match_shell_state");

    // Register scenario constants
    match_shell_script_register_scenario_constants(state->scenario_lua_state);

    // Scenario file constants
    lua_getglobal(state->scenario_lua_state, MODULE_NAME);
    lua_newtable(state->scenario_lua_state);
    for (const ScenarioConstant& constant : scenario->constants) {
        switch (constant.type) {
            case SCENARIO_CONSTANT_TYPE_ENTITY: {
                lua_pushinteger(state->scenario_lua_state, constant.entity_index);
                break;
            }
            case SCENARIO_CONSTANT_TYPE_CELL: {
                lua_newtable(state->scenario_lua_state);
                lua_pushinteger(state->scenario_lua_state, constant.cell.x);
                lua_setfield(state->scenario_lua_state, -2, "x");
                lua_pushinteger(state->scenario_lua_state, constant.cell.y);
                lua_setfield(state->scenario_lua_state, -2, "y");
                break;
            }
            case SCENARIO_CONSTANT_TYPE_COUNT: {
                GOLD_ASSERT(false);
                break;
            }
        }
        lua_setfield(state->scenario_lua_state, -2, constant.name);
    }
    lua_setfield(state->scenario_lua_state, -2, "constants");

    // End scenario module constants
    lua_pop(state->scenario_lua_state, 1);

    int dofile_error = luaL_dofile(state->scenario_lua_state, script_path);
    if (dofile_error) {
        log_error("Error loading script file %s. Code %u: %s", script_path, dofile_error, lua_tostring(state->scenario_lua_state, -1));
        lua_close(state->scenario_lua_state);
        return false;
    }

    // Check to make sure that scenario_update() exists
    lua_getglobal(state->scenario_lua_state, "scenario_update");
    if (!lua_isfunction(state->scenario_lua_state, -1)) {
        log_error("Script %s is missing scenario_update() function.", script_path);
        lua_pop(state->scenario_lua_state, 1);
        lua_close(state->scenario_lua_state);
        return false;
    }
    // Pop scenario_update() off the stack because we are not calling it
    lua_pop(state->scenario_lua_state, 1);

    // Get the scenario_init() function
    lua_getglobal(state->scenario_lua_state, "scenario_init");
    if (!lua_isfunction(state->scenario_lua_state, -1)) {
        log_error("Script %s is missing scenario_init() function.", script_path);
        lua_pop(state->scenario_lua_state, 1);
        lua_close(state->scenario_lua_state);
        return false;
    }
    lua_pop(state->scenario_lua_state, 1);

    // Call scenario_init() 
    match_shell_script_call(state, "scenario_init");

    return true;
}

void match_shell_script_register_scenario_constants(lua_State* lua_state) {
    // Push scenario table onto the stack
    lua_getglobal(lua_state, MODULE_NAME);

    // The scenario table should always exist when this function is called
    GOLD_ASSERT(!lua_isnil(lua_state, -1));

    // Script constants
    size_t const_index = 0;
    while (GOLD_CONSTANTS[const_index].name != NULL) {
        lua_pushinteger(lua_state, GOLD_CONSTANTS[const_index].value);
        lua_setfield(lua_state, -2, GOLD_CONSTANTS[const_index].name);

        const_index++;
    }

    // Player ID constant
    lua_pushinteger(lua_state, 0);
    lua_setfield(lua_state, -2, "PLAYER_ID");

    // Entity constants
    lua_createtable(lua_state, 0, ENTITY_TYPE_COUNT);
    for (int entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        lua_pushinteger(lua_state, entity_type);
        lua_setfield(lua_state, -2, match_shell_script_get_entity_type_str((EntityType)entity_type));
    }
    lua_setfield(lua_state, -2, "entity_type");

    // Entity mode
    lua_createtable(lua_state, 0, MODE_COUNT);
    for (int entity_mode = 0; entity_mode < MODE_COUNT; entity_mode++) {
        lua_pushinteger(lua_state, entity_mode);
        lua_setfield(lua_state, -2, match_shell_script_get_entity_mode_str((EntityMode)entity_mode));
    }
    lua_setfield(lua_state, -2, "entity_mode");

    // Target type
    lua_createtable(lua_state, 0, TARGET_TYPE_COUNT);
    for (int target_type = 0; target_type < TARGET_TYPE_COUNT; target_type++) {
        lua_pushinteger(lua_state, target_type);
        lua_setfield(lua_state, -2, match_shell_script_get_target_type_str((TargetType)target_type));
    }
    lua_setfield(lua_state, -2, "target_type");

    // Sound constants
    lua_createtable(lua_state, 0, SOUND_COUNT);
    for (int sound_name = 0; sound_name < SOUND_COUNT; sound_name++) {
        char const_name[64];
        strcpy_to_upper(const_name, sound_get_name((SoundName)sound_name));

        lua_pushinteger(lua_state, sound_name);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "sound");

    // Bot squad type constants
    lua_createtable(lua_state, 0, BOT_SQUAD_TYPE_COUNT);
    for (int squad_type = 0; squad_type < BOT_SQUAD_TYPE_COUNT; squad_type++) {
        char const_name[64];
        strcpy_to_upper(const_name, bot_squad_type_str((BotSquadType)squad_type));

        lua_pushinteger(lua_state, squad_type);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "bot_squad_type");

    // Bot config constants
    lua_createtable(lua_state, 0, BOT_CONFIG_FLAG_COUNT);
    for (uint32_t flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
        uint32_t flag_value = 1U << flag_index;

        char const_name[64];
        char* const_name_ptr = const_name;
        const_name_ptr += sprintf(const_name_ptr, "SHOULD_");
        strcpy_to_upper(const_name_ptr, bot_config_flag_str(flag_value));

        lua_pushinteger(lua_state, flag_value);
        lua_setfield(lua_state, -2, const_name);
    }
    lua_setfield(lua_state, -2, "bot_config_flag");

    // Match input constants
    lua_createtable(lua_state, 0, MATCH_INPUT_TYPE_COUNT);
    for (uint32_t match_input_type = 0; match_input_type < MATCH_INPUT_TYPE_COUNT; match_input_type++) {
        lua_pushinteger(lua_state, match_input_type);
        lua_setfield(lua_state, -2, match_input_type_str((MatchInputType)match_input_type));
    }
    lua_setfield(lua_state, -2, "match_input_type");

    // Objective counter type constants
    lua_createtable(lua_state, 0, GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT);
    for (uint32_t counter_type = 0; counter_type < GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT; counter_type++) {
        lua_pushnumber(lua_state, counter_type);
        lua_setfield(lua_state, -2, match_shell_script_get_global_objective_counter_type_str((GlobalObjectiveCounterType)counter_type));
    }
    lua_setfield(lua_state, -2, "global_objective_counter_type");

    // Pops scenario table off the stack
    lua_pop(lua_state, 1);
}

void match_shell_script_update(MatchShellState* state) {
    match_shell_script_call(state, "scenario_update");
}

void match_shell_script_call(MatchShellState* state, const char* func_name) {
    lua_getglobal(state->scenario_lua_state, "debug");
    lua_getfield(state->scenario_lua_state, -1, "traceback");
    lua_remove(state->scenario_lua_state, -2);
    lua_getglobal(state->scenario_lua_state, func_name);
    if (lua_pcall(state->scenario_lua_state, 0, 0, -2)) {
        const char* error_str = lua_tostring(state->scenario_lua_state, -1);

        // Lua is not giving us the short_src so we will
        // manually look through the string to find the tail end 
        // (past the path separators)

        size_t index = 0;
        size_t path_sep_index = 0;
        while (error_str[index] != ':') {
            if (error_str[index] == GOLD_PATH_SEPARATOR) {
                path_sep_index = index;
            }
            index++;
        }
        if (path_sep_index != 0) {
            error_str += path_sep_index + 1;
        }

        log_error("%s", error_str);
        match_shell_leave_match(state, false);
    }
}

const char* match_shell_script_get_entity_type_str(EntityType type) {
    switch (type) {
        case ENTITY_GOLDMINE:
            return "GOLDMINE";
        case ENTITY_MINER:
            return "MINER";
        case ENTITY_COWBOY:
            return "COWBOY";
        case ENTITY_BANDIT:
            return "BANDIT"; 
        case ENTITY_WAGON:
            return "WAGON";
        case ENTITY_JOCKEY:
            return "JOCKEY";
        case ENTITY_SAPPER:
            return "SAPPER";
        case ENTITY_PYRO:
            return "PYRO";
        case ENTITY_SOLDIER:
            return "SOLDIER";
        case ENTITY_CANNON:
            return "CANNON";
        case ENTITY_DETECTIVE:
            return "DETECTIVE";
        case ENTITY_BALLOON:
            return "BALLOON";
        case ENTITY_HALL:
            return "HALL";
        case ENTITY_HOUSE:
            return "HOUSE";
        case ENTITY_SALOON:
            return "SALOON";
        case ENTITY_BUNKER:
            return "BUNKER";
        case ENTITY_WORKSHOP:
            return "WORKSHOP";
        case ENTITY_SMITH:
            return "SMITH";
        case ENTITY_COOP:
            return "COOP";
        case ENTITY_BARRACKS:
            return "BARRACKS";
        case ENTITY_SHERIFFS:
            return "SHERIFFS";
        case ENTITY_LANDMINE:
            return "LANDMINE";
        case ENTITY_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* match_shell_script_get_entity_mode_str(EntityMode mode) {
    switch (mode) {
        case MODE_UNIT_IDLE:
            return "UNIT_IDLE";
        case MODE_UNIT_BLOCKED:
            return "UNIT_BLOCKED";
        case MODE_UNIT_MOVE:
            return "UNIT_MOVE";
        case MODE_UNIT_MOVE_FINISHED:
            return "UNIT_MOVE_FINISHED";
        case MODE_UNIT_BUILD:
            return "UNIT_BUILD";
        case MODE_UNIT_BUILD_ASSIST:
            return "UNIT_BUILD_ASSIST";
        case MODE_UNIT_REPAIR:
            return "UNIT_REPAIR";
        case MODE_UNIT_ATTACK_WINDUP:
            return "UNIT_ATTACK_WINDUP";
        case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP:
            return "UNIT_SOLDIER_RANGED_ATTACK_WINDUP";
        case MODE_UNIT_SOLDIER_CHARGE:
            return "UNIT_SOLDIER_CHARGE";
        case MODE_UNIT_IN_MINE:
            return "UNIT_IN_MINE";
        case MODE_UNIT_PYRO_THROW:
            return "UNIT_PYRO_THROW";
        case MODE_UNIT_DEATH:
            return "UNIT_DEATH";
        case MODE_UNIT_DEATH_FADE:
            return "UNIT_DEATH_FADE";
        case MODE_UNIT_BALLOON_DEATH_START:
            return "UNIT_BALLOON_DEATH_START";
        case MODE_UNIT_BALLOON_DEATH:
            return "UNIT_BALLOON_DEATH";
        case MODE_BUILDING_IN_PROGRESS:
            return "BUILDING_IN_PROGRESS";
        case MODE_BUILDING_FINISHED:
            return "BUILDING_FINISHED";
        case MODE_BUILDING_DESTROYED:
            return "BUILDING_DESTROYED";
        case MODE_MINE_ARM:
            return "MINE_ARM";
        case MODE_MINE_PRIME:
            return "MINE_PRIME";
        case MODE_GOLDMINE:
            return "GOLDMINE";
        case MODE_GOLDMINE_COLLAPSED:
            return "GOLDMINE_COLLAPSED";
        case MODE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* match_shell_script_get_target_type_str(TargetType type) {
    switch (type) {
        case TARGET_NONE:
            return "NONE";
        case TARGET_CELL:
            return "CELL";
        case TARGET_ENTITY:
            return "ENTITY";
        case TARGET_ATTACK_CELL:
            return "ATTACK_CELL";
        case TARGET_ATTACK_ENTITY:
            return "ATTACK_ENTITY";
        case TARGET_REPAIR:
            return "REPAIR";
        case TARGET_UNLOAD:
            return "UNLOAD";
        case TARGET_MOLOTOV:
            return "MOLOTOV";
        case TARGET_BUILD:
            return "BUILD";
        case TARGET_BUILD_ASSIST:
            return "BUILD_ASSIST";
        case TARGET_PATROL:
            return "PATROL";
        case TARGET_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

const char* match_shell_script_get_global_objective_counter_type_str(GlobalObjectiveCounterType type) {
    switch (type) {
        case GLOBAL_OBJECTIVE_COUNTER_OFF:
            return "OFF";
        case GLOBAL_OBJECTIVE_COUNTER_GOLD:
            return "GOLD";
        case GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

#ifdef GOLD_DEBUG
void match_shell_script_generate_doc() {
    std::string doc_path = filesystem_get_resource_path() + "scenario" + GOLD_PATH_SEPARATOR + "modules" + GOLD_PATH_SEPARATOR + "scenario.d.lua";
    FILE* file = fopen(doc_path.c_str(), "w");
    if (!file) {
        printf("Error: could not open %s.\n", doc_path.c_str());
        return;
    }

    fprintf(file, "--- @meta\n\n");

    fprintf(file, "--- @class scenario\n");
    fprintf(file, "--- @field constants table\n");
    fprintf(file, "%s = {}\n\n", MODULE_NAME);

    fprintf(file, "--- @class ivec2\n");
    fprintf(file, "--- @field x number\n");
    fprintf(file, "--- @field y number\n\n");

    // Create and populate the scenario table with all of the constants
    lua_State* lua_state = luaL_newstate();
    lua_newtable(lua_state);
    lua_setglobal(lua_state, MODULE_NAME);
    match_shell_script_register_scenario_constants(lua_state);

    // Now that we have the constants, we can read them to create documentation
    lua_getglobal(lua_state, MODULE_NAME);
    lua_pushnil(lua_state);
    while (lua_next(lua_state, -2) != 0) {
        const char* const_name = lua_tostring(lua_state, -2);
        int type = lua_type(lua_state, -1);

        if (type == LUA_TTABLE) {
            fprintf(file, "%s.%s = {}\n", MODULE_NAME, const_name);
            lua_pushnil(lua_state);
            while (lua_next(lua_state, -2) != 0) {
                const char* table_const_name = lua_tostring(lua_state, -2);
                int table_const_value = lua_tonumber(lua_state, -1);

                fprintf(file, "%s.%s.%s = %i\n", MODULE_NAME, const_name, table_const_name, table_const_value);

                lua_pop(lua_state, 1);
            }
            fprintf(file, "\n");
        } else if (type == LUA_TNUMBER) {
            fprintf(file, "%s.%s = %i\n", MODULE_NAME, const_name, (int)lua_tonumber(lua_state, -1));
        }
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);
    fprintf(file, "\n");

    lua_close(lua_state);

    std::ifstream script_cpp_ifstream("../src/shell/script.cpp");
    std::vector<std::string> comments;
    std::unordered_map<std::string, std::vector<std::string>> function_comments;

    std::string line;
    while (std::getline(script_cpp_ifstream, line)) {
        if (line.rfind("// ", 0) == 0) {
            comments.push_back(line.substr(3));
            continue;
        }

        if (line.rfind("static int script_", 0) == 0 && line.rfind("{") != std::string::npos) {
            size_t function_name_start = strlen("static int script_");
            size_t parameter_list_start = line.find('(');
            size_t function_name_length = parameter_list_start - function_name_start;
            std::string function_name = line.substr(function_name_start, function_name_length);
            function_comments[function_name] = comments;
        }

        // If line not a comment, clear the comments list
        comments.clear();
    }

    size_t index = 0;
    while (GOLD_FUNCS[index].name != NULL) {
        std::vector<std::string> param_names;
        for (const std::string& comment : function_comments[GOLD_FUNCS[index].name]) {
            fprintf(file, "--- %s\n", comment.c_str());
            if (comment.rfind("@param", 0) == 0) {
                size_t space_index = comment.find(' ');
                if (space_index == std::string::npos) {
                    printf("ERROR: Invalid @param for function %s\n", GOLD_FUNCS[index].name);
                    continue;
                } 

                std::string param_name = comment.substr(space_index + 1);
                space_index = param_name.find(' ');
                if (space_index != std::string::npos) {
                    param_name = param_name.substr(0, space_index);
                }

                param_names.push_back(param_name);
            }
        }

        std::string param_string = "";
        for (size_t param_index = 0; param_index < param_names.size(); param_index++) {
            param_string += param_names[param_index];
            if (param_index != param_names.size() - 1) {
                param_string += ", ";
            }
        }

        fprintf(file, "function %s.%s(%s) end\n\n", MODULE_NAME, GOLD_FUNCS[index].name, param_string.c_str());

        index++;
    }

    fclose(file);
    printf("Generated Lua doc at %s\n", doc_path.c_str());
}
#endif

MatchShellState* script_get_match_shell_state(lua_State* lua_state) {
    lua_getfield(lua_state, LUA_REGISTRYINDEX, "__match_shell_state");
    MatchShellState* state = (MatchShellState*)lua_touserdata(lua_state, -1);
    lua_pop(lua_state, 1);
    return state;
}

const char* script_lua_type_str(int lua_type) {
    switch (lua_type) {
        case LUA_TNIL: 
            return "<nil>";
        case LUA_TNUMBER: 
            return "<number>";
        case LUA_TBOOLEAN: 
            return "<boolean>";
        case LUA_TSTRING:
            return "<string>";
        case LUA_TTABLE: 
            return "<table>";
        case LUA_TFUNCTION: 
            return "<function>";
        case LUA_TUSERDATA: 
            return "<userdata>";
        case LUA_TTHREAD: 
            return "<thread>";
        case LUA_TLIGHTUSERDATA: 
            return "<lightuserdata>";
        default:
            GOLD_ASSERT(false);
            return "";
    }
}

void script_error(lua_State* lua_state, const char* message, ...) {
    const size_t ERROR_BUFFER_LENGTH = 1024;
    char error_str[ERROR_BUFFER_LENGTH];
    char* error_str_ptr = error_str;

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(error_str_ptr, ERROR_BUFFER_LENGTH, message, arg_ptr);
    va_end(arg_ptr);

    lua_pushstring(lua_state, error_str);
    lua_error(lua_state);
}

void script_validate_type(lua_State* lua_state, int stack_index, const char* name, int expected_type) {
    int received_type = lua_type(lua_state, stack_index);
    if (received_type != expected_type) {
        script_error(lua_state, "Invalid type for %s. Received %s. Expected %s.",
            name,
            script_lua_type_str(received_type),
            script_lua_type_str(expected_type));
    }
}

void script_validate_arguments(lua_State* lua_state, const int* arg_types, int arg_count) {
    int arg_number = lua_gettop(lua_state);
    if (arg_number != arg_count) {
        script_error(lua_state, "Invalid arg count. Received %u. Expected %u.", arg_number, arg_count);
    }

    for (int arg_index = 1; arg_index <= arg_count; arg_index++) {
        char field_name[16];
        sprintf(field_name, "arg %u", arg_index);
        script_validate_type(lua_state, arg_index, field_name, arg_types[arg_index - 1]);
    }
}

void script_validate_entity_type(lua_State* lua_state, int entity_type) {
    if (entity_type < 0 || entity_type >= ENTITY_TYPE_COUNT) {
        script_error(lua_state, "Entity type %i not recognized.", entity_type);
    }
}

uint32_t script_validate_entity_id(lua_State* lua_state, const MatchShellState* state, EntityId entity_id) {
    uint32_t entity_index = state->match_state->entities.get_index_of(entity_id);
    if (entity_index == INDEX_INVALID) {
        script_error(lua_state, "Entity ID %u does not exist.", entity_id);
    }

    return entity_index;
}

ivec2 script_lua_to_ivec2(lua_State* lua_state, int stack_index, const char* name) {
    ivec2 value;

    lua_getfield(lua_state, stack_index, "x");
    if (lua_type(lua_state, -1) != LUA_TNUMBER) {
        script_error(lua_state, "Invalid ivec2 %s: x is not a number.", name);
    }
    value.x = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    lua_getfield(lua_state, stack_index, "y");
    if (lua_type(lua_state, -1) != LUA_TNUMBER) {
        script_error(lua_state, "Invalid ivec2 %s: y is not a number.", name);
    }
    value.y = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    return value;
}

void script_lua_push_ivec2(lua_State* lua_state, ivec2 cell) {
    lua_createtable(lua_state, 0, 2);

    lua_pushnumber(lua_state, cell.x);
    lua_setfield(lua_state, -2, "x");

    lua_pushnumber(lua_state, cell.y);
    lua_setfield(lua_state, -2, "y");
}

static const uint32_t SCRIPT_VALIDATE_PLAYER_IS_BOT = 1U << 0U;
static const uint32_t SCRIPT_VALIDATE_PLAYER_IS_ACTIVE = 1U << 1U;
void script_validate_player_id(lua_State* lua_state, const MatchShellState* state, uint8_t player_id, uint32_t options = 0) {
    if (player_id >= MAX_PLAYERS) {
        script_error(lua_state, "player_id %u is out of range.", player_id);
    }
    if (bitflag_check(options, SCRIPT_VALIDATE_PLAYER_IS_BOT) && player_id == 0) {
        script_error(lua_state, "player_id is 0, but it must be a bot.");
    }
    if (bitflag_check(options, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE) && !state->match_state->players[player_id].active) {
        script_error(lua_state, "player %u is inactive.");
    }
}

int script_sprintf(char* str_ptr, lua_State* lua_state, int stack_index) {
    int arg_type = lua_type(lua_state, stack_index);
    switch (arg_type) {
        case LUA_TNIL: {
            return sprintf(str_ptr, "nil");
        }
        case LUA_TNUMBER: {
            double value = lua_tonumber(lua_state, stack_index);
            return sprintf(str_ptr, "%f", value);
        }
        case LUA_TBOOLEAN: {
            bool value = lua_toboolean(lua_state, stack_index);
            return sprintf(str_ptr, "%s", value ? "true" : "false");
        }
        case LUA_TSTRING: {
            const char* value = lua_tostring(lua_state, stack_index);
            return sprintf(str_ptr, "%s", value);
        }
        case LUA_TTABLE: {
            size_t offset = 0;
            offset += sprintf(str_ptr + offset, "{ ");
            lua_pushnil(lua_state);
            while (lua_next(lua_state, stack_index) != 0) {
                int key_type = lua_type(lua_state, -2);
                if (key_type == LUA_TSTRING) {
                    offset += sprintf(str_ptr + offset, "%s = ", lua_tostring(lua_state, -2));
                }
                offset += script_sprintf(str_ptr + offset, lua_state, lua_gettop(lua_state));
                offset += sprintf(str_ptr + offset, ", ");
                lua_pop(lua_state, 1);
            }
            offset += sprintf(str_ptr + offset, "}");
            return offset;
        }
        default: {
            return sprintf(str_ptr, "%s", script_lua_type_str(arg_type));
        }
    }
}

// GENERAL

// Send a debug log. If debug logging is disabled, this function does nothing.
// @param ... any Values to print
static int script_log(lua_State* lua_state) {
#if GOLD_LOG_LEVEL >= LOG_LEVEL_DEBUG
    int nargs = lua_gettop(lua_state);
    char buffer[4096];
    char* buffer_ptr = buffer;

    for (int arg_index = 1; arg_index <= nargs; arg_index++) {
        buffer_ptr += script_sprintf(buffer_ptr, lua_state, arg_index);
        if (arg_index < nargs) {
            buffer_ptr += sprintf(buffer_ptr, " ");
        }
    }

    log_debug("%s", buffer);
#endif

    return 0;
}

// Plays a sound effect.
// @param sound number
static int script_play_sound(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    int sound = (int)lua_tonumber(lua_state, 1);

    if (sound < 0 || sound >= SOUND_COUNT) {
        script_error(lua_state, "Invalid sound %i", sound);
    }

    sound_play((SoundName)sound);

    return 0;
}

// Returns the time in seconds since the scenario started.
// @return number
static int script_get_time(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    lua_pushnumber(lua_state, (double)state->match_timer / (double)UPDATES_PER_SECOND);

    return 1;
}

// MATCH OVER

// End the match in victory.
static int script_set_match_over_victory(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    // This validation is to prevent the player from losing and then winning
    if (state->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY || state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT) {
        log_warn("script set_match_over_victory(): the match is already over.");
        return 0;
    }

    state->mode = MATCH_SHELL_MODE_MATCH_OVER_VICTORY;

    return 0;
}

// End the match in defeat.
static int script_set_match_over_defeat(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    // This validation is to prevent the player from winning and then losing
    if (state->mode == MATCH_SHELL_MODE_MATCH_OVER_VICTORY || state->mode == MATCH_SHELL_MODE_MATCH_OVER_DEFEAT) {
        log_warn("script set_match_over_defeat(): the match is already over.");
        return 0;
    }

    state->mode = MATCH_SHELL_MODE_MATCH_OVER_DEFEAT;

    return 0;
}

// PLAYERS

// Checks if the player has been defeated.
// @param player_id number
// @return boolean 
static int script_player_is_defeated(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id);

    lua_pushboolean(lua_state, !state->match_state->players[player_id].active);

    return 1;
}

// Returns the specified player's gold count
// @param player_id number
// @return number
static int script_player_get_gold(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id);

    lua_pushnumber(lua_state, state->match_state->players[player_id].gold);

    return 1;
}

// Returns the total number of gold mined by the specified player this match
// @param player_id number
// @return number
static int script_player_get_gold_mined_total(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id);

    lua_pushnumber(lua_state, state->match_state->players[player_id].gold_mined_total);

    return 1;
}

// CHAT

FontName script_chat_get_font_from_chat_color(int chat_color) {
    switch (chat_color) {
        case SCRIPT_CHAT_COLOR_WHITE:
            return FONT_HACK_WHITE;
        case SCRIPT_CHAT_COLOR_GOLD:
            return FONT_HACK_GOLD;
        case SCRIPT_CHAT_COLOR_BLUE:
            return FONT_HACK_PLAYER0;
        default: 
            return FONT_COUNT;
    }
}

// Sends a chat message.
// @param color number
// @param prefix string
// @param message string
static int script_chat(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TSTRING, LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    int chat_color = (int)lua_tonumber(lua_state, 1);
    const char* chat_prefix = lua_tostring(lua_state, 2);
    const char* chat_message = lua_tostring(lua_state, 3);

    FontName font = script_chat_get_font_from_chat_color(chat_color);
    if (font == FONT_COUNT) {
        script_error(lua_state, "Unregonized chat color %i", chat_color);
    }

    match_shell_add_chat_message(state, font, chat_prefix, chat_message, CHAT_MESSAGE_DURATION);

    return 0;
}

// Sends a hint message.
// @param message string
static int script_hint(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    const char* message = lua_tostring(lua_state, 1);

    sound_play(SOUND_UI_CLICK);
    match_shell_add_chat_message(state, FONT_HACK_PLAYER0, "Hint:", message, CHAT_MESSAGE_HINT_DURATION);

    return 0;
}

// CAMERA / FOG

// Reveals fog at the specified cell.
// @param params { cell: ivec2, cell_size: number, sight: number, duration: number }
static int script_fog_reveal(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Cell
    lua_getfield(lua_state, 1, "cell");
    ivec2 cell = script_lua_to_ivec2(lua_state, -1, "cell");
    lua_pop(lua_state, 1);

    // Cell size
    lua_getfield(lua_state, 1, "cell_size");
    script_validate_type(lua_state, -1, "cell_size", LUA_TNUMBER);
    int cell_size = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Sight
    lua_getfield(lua_state, 1, "sight");
    script_validate_type(lua_state, -1, "sight", LUA_TNUMBER);
    int sight = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Duration
    lua_getfield(lua_state, 1, "duration");
    script_validate_type(lua_state, -1, "duration", LUA_TNUMBER);
    double duration = lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    uint8_t team = state->match_state->players[network_get_player_id()].team;
    match_fog_update(state->match_state, team, cell, cell_size, sight, false, CELL_LAYER_GROUND, true);
    state->match_state->fog_reveals.push_back((FogReveal) {
        .team = team,
        .cell = cell,
        .cell_size = cell_size,
        .sight = sight,
        .timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND)
    });

    return 0;
}

// Returns the cell the camera is currently centered on
// @return ivec2
static int script_get_camera_centered_cell(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    ivec2 camera_center = state->camera_offset + ivec2(SCREEN_WIDTH / 2, ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2));
    ivec2 cell = camera_center / TILE_SIZE;

    lua_newtable(lua_state);

    lua_pushnumber(lua_state, cell.x);
    lua_setfield(lua_state, -2, "x");

    lua_pushnumber(lua_state, cell.y);
    lua_setfield(lua_state, -2, "y");

    return 1;
}

// Gradually pans the camera to center on the specified cell. 
// @param cell ivec2 The cell to pan the camera to
// @param duration number The duration in seconds of the camera pan
static int script_begin_camera_pan(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    ivec2 cell = script_lua_to_ivec2(lua_state, 1, "arg 1");
    double duration = lua_tonumber(lua_state, 2);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    // Convert to cell into camera offset
    state->camera_pan_end_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state->camera_pan_end_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2);
    state->camera_pan_end_offset.x = std::clamp(state->camera_pan_end_offset.x, 0, (state->match_state->map.width * TILE_SIZE) - SCREEN_WIDTH);
    state->camera_pan_end_offset.y = std::clamp(state->camera_pan_end_offset.y, 0, (state->match_state->map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);

    state->camera_pan_start_offset = state->camera_offset;
    state->camera_pan_timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND);
    state->camera_pan_duration = state->camera_pan_timer;

    state->camera_mode = CAMERA_MODE_PAN;

    return 0;
}

// Removes camera movement from the player and holds the camera in place
static int script_hold_camera(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->camera_mode = CAMERA_MODE_HELD;

    return 0;
}

// Returns camera movement to the player
static int script_release_camera(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->camera_mode = CAMERA_MODE_FREE;

    return 0;
}

// Returns the current camera mode.
// @return number
static int script_get_camera_mode(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    lua_pushnumber(lua_state, state->camera_mode);

    return 1;
}

// OBJECTIVES

// Adds an objective to the objectives list.
// 
// If entity_type is provided, then counter_target is also required and 
// the objective counter will be based on the player's entities of that type.
// 
// If counter_target is provided but entity_type is not, then the objective counter
// will be a variable counter that must be manually updated.
// 
// Returns the index of the created objective.
// @param params { description: string, entity_type: number|nil, counter_target: number|nil }
static int script_add_objective(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Initialize objective
    Objective objective = (Objective) {
        .description = std::string(),
        .is_complete = false,
        .counter_type = OBJECTIVE_COUNTER_TYPE_NONE,
        .counter_value = 0,
        .counter_target = 0
    };

    // Get description
    lua_getfield(lua_state, 1, "description");
    script_validate_type(lua_state, -1, "description", LUA_TSTRING);
    objective.description = std::string(lua_tostring(lua_state, -1));
    lua_pop(lua_state, 1);

    // Get entity type (optional)
    lua_getfield(lua_state, 1, "entity_type");
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "entity_type", LUA_TNUMBER);
        int entity_type = (int)lua_tonumber(lua_state, -1);
        script_validate_entity_type(lua_state, entity_type);

        objective.counter_type = OBJECTIVE_COUNTER_TYPE_ENTITY;
        objective.counter_value = (uint32_t)entity_type;
    }
    lua_pop(lua_state, 1);

    // Get counter target (optional, but required if counter_type_entity)
    lua_getfield(lua_state, 1, "counter_target");
    if (objective.counter_type == OBJECTIVE_COUNTER_TYPE_ENTITY && lua_isnil(lua_state, -1)) {
        script_error(lua_state, "Objective field counter_target is required when entity_type is defined.");
    }
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "counter_target", LUA_TNUMBER);
        objective.counter_target = (uint32_t)lua_tonumber(lua_state, -1);
    }
    lua_pop(lua_state, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    state->scenario_objectives.push_back(objective);
    lua_pushnumber(lua_state, (int)state->scenario_objectives.size() - 1);

    return 1;
}

// Marks the specified objective as complete.
// @param objective_index number
static int script_complete_objective(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    uint32_t objective_index = lua_tonumber(lua_state, 1);
    if (objective_index >= state->scenario_objectives.size()) {
        script_error(lua_state, "Objective index %u out of bounds.", objective_index);
    }

    state->scenario_objectives[objective_index].is_complete = true;

    return 0;
}

// Returns true if the specified objective is complete.
// @param objective_index number
// @return boolean
static int script_is_objective_complete(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    uint32_t objective_index = lua_tonumber(lua_state, 1);
    if (objective_index >= state->scenario_objectives.size()) {
        script_error(lua_state, "Objective index %u out of bounds.", objective_index);
    }

    lua_pushboolean(lua_state, state->scenario_objectives[objective_index].is_complete);

    return 1;
}

// Returns true if all objectives are complete.
// @return boolean
static int script_are_objectives_complete(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    bool result = true;

    const MatchShellState* state = script_get_match_shell_state(lua_state);
    if (state->scenario_objectives.empty()) {
        result = false;
    }
    for (uint32_t index = 0; index < state->scenario_objectives.size(); index++) {
        if (!state->scenario_objectives[index].is_complete) {
            result = false;
            break;
        }
    }

    lua_pushboolean(lua_state, result);

    return 1;
}

// Clears the objectives list.
static int script_clear_objectives(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->scenario_objectives.clear();

    return 0;
}

// Sets the global objective counter
// @param value number
// @param max_value number
static int script_set_global_objective_counter(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    uint32_t value = (uint32_t)lua_tonumber(lua_state, 1);
    if (value >= GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT) {
        script_error(lua_state, "Global objective counter value of %u is invalid.", value);
    }

    uint32_t max_value = (uint32_t)lua_tonumber(lua_state, 2);

    GlobalObjectiveCounter counter;
    counter.type = (GlobalObjectiveCounterType)value;
    switch (counter.type) {
        case GLOBAL_OBJECTIVE_COUNTER_GOLD: {
            memset(&counter.gold, 0, sizeof(counter.gold));
            counter.gold.max_value = max_value;
            break;
        }
        case GLOBAL_OBJECTIVE_COUNTER_OFF:
            break;
        case GLOBAL_OBJECTIVE_COUNTER_TYPE_COUNT:
            GOLD_ASSERT(false);
    }

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->scenario_global_objective_counter = counter;

    return 0;
}

// ENTITIES

// Returns true if the specified entity is visible to the player.
// @param entity_id number
// @return boolean
static int script_entity_is_visible_to_player(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    EntityId entity_id = lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, state, entity_id);

    bool result = entity_is_visible_to_player(state->match_state, state->match_state->entities[entity_index], network_get_player_id());
    lua_pushboolean(lua_state, result);

    return 1;
}

// Highlights the specified entity.
// @param entity_id number
static int script_highlight_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    EntityId entity_id = lua_tonumber(lua_state, 1);
    script_validate_entity_id(lua_state, state, entity_id);

    state->highlight_animation = animation_create(ANIMATION_UI_HIGHLIGHT_ENTITY);
    state->highlight_entity_id = entity_id;

    // Do a fog reveal on the highlighted entity
    const Entity& entity = state->match_state->entities.get_by_id(entity_id);
    FogReveal fog_reveal = (FogReveal) {
        .team = state->match_state->players[network_get_player_id()].team,
        .cell = entity.cell,
        .cell_size = entity_get_data(entity.type).cell_size,
        .sight = 3,
        .timer = 160U // this is the duration of animation_ui_highlight_entity
    };

    match_fog_update(state->match_state, fog_reveal.team, fog_reveal.cell, fog_reveal.cell_size, fog_reveal.sight, false, CELL_LAYER_GROUND, true);
    state->match_state->fog_reveals.push_back(fog_reveal);

    return 0;
}

// Returns the number of entities controlled by the player of a given type.
// @param player_id number
// @param entity_type number
// @return number
static int script_player_get_entity_count(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    int entity_type = (int)lua_tonumber(lua_state, 2);
    script_validate_entity_type(lua_state, entity_type);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint32_t result = match_shell_get_player_entity_count(state, player_id, (EntityType)entity_type);
    lua_pushnumber(lua_state, result);

    return 1;
}

// Returns the number of bunkers controlled by the player that have 4 cowboys in them.
// @param player_id number
// @return number
static int script_player_get_full_bunker_count(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    uint8_t player_id = lua_tonumber(lua_state, 1);
    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint32_t count = 0;
    for (uint32_t entity_index = 0; entity_index < state->match_state->entities.size(); entity_index++) {
        const Entity& entity = state->match_state->entities[entity_index];
        if (entity.player_id != player_id ||
                entity.type != ENTITY_BUNKER ||
                entity.garrisoned_units.size() != 4) {
            continue;
        }
        bool is_all_cowboys = true;
        for (uint32_t garrisoned_units_index = 0; garrisoned_units_index < entity.garrisoned_units.size(); garrisoned_units_index++) {
            EntityId garrisoned_unit_id = entity.garrisoned_units[garrisoned_units_index];
            if (state->match_state->entities.get_by_id(garrisoned_unit_id).type != ENTITY_COWBOY) {
                is_all_cowboys = false;
            }
        }
        if (!is_all_cowboys) {
            continue;
        }
        count++;
    }

    lua_pushnumber(lua_state, count);

    return 1;
}

// Returns true if the specified player has an entity near the cell within the given distance
// @param player_id number
// @param cell ivec2
// @param distance number
// @return boolean
static int script_player_has_entity_near_cell(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 3);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    ivec2 cell = script_lua_to_ivec2(lua_state, 2, "arg 2");
    int distance = (int)lua_tonumber(lua_state, 3);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    bool result = false;
    for (uint32_t entity_index = 0; entity_index < state->match_state->entities.size(); entity_index++) {
        const Entity& entity = state->match_state->entities[entity_index];
        if (entity.player_id == player_id && 
                entity.health != 0 && 
                ivec2::manhattan_distance(entity.cell, cell) < distance) {
            result = true;
            break;
        }
    }

    lua_pushboolean(lua_state, result);

    return 1;
}

// Returns a table of information about the specified entity, or nil if the entity does not exist
// @param entity_id number
// @return table
static int script_entity_get_info(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 1);
    uint32_t entity_index = state->match_state->entities.get_index_of(entity_id);

    if (entity_index == INDEX_INVALID) {
        lua_pushnil(lua_state);
        return 1;
    }

    const Entity& entity = state->match_state->entities[entity_index];
    lua_newtable(lua_state);

    // Type
    lua_pushnumber(lua_state, entity.type);
    lua_setfield(lua_state, -2, "type");

    // Mode
    lua_pushnumber(lua_state, entity.mode);
    lua_setfield(lua_state, -2, "mode");

    // Player
    lua_pushnumber(lua_state, entity.player_id);
    lua_setfield(lua_state, -2, "player_id");

    // Cell
    script_lua_push_ivec2(lua_state, entity.cell);
    lua_setfield(lua_state, -2, "cell");

    // Target
    lua_newtable(lua_state);
        // Type
        lua_pushnumber(lua_state, entity.target.type);
        lua_setfield(lua_state, -2, "type");

        // Id
        lua_pushnumber(lua_state, entity.target.id);
        lua_setfield(lua_state, -2, "id");

        // Cell
        script_lua_push_ivec2(lua_state, entity.cell);
        lua_setfield(lua_state, -2, "cell");
    lua_setfield(lua_state, -2, "target");

    // Health
    lua_pushnumber(lua_state, entity.health);
    lua_setfield(lua_state, -2, "health");

    return 1;
}

// Returns the gold cost of the specified entity type
// @param entity_type number
// @return number
static int script_entity_get_gold_cost(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    int entity_type = (uint32_t)lua_tonumber(lua_state, 1);
    script_validate_entity_type(lua_state, entity_type);

    lua_pushnumber(lua_state, entity_get_data((EntityType)entity_type).gold_cost);

    return 1;
}

// Finds an empty cell for the specified entity to spawn in. The manhattan distance of the cell to the provided spawn_cell will be less than 16
//
// If no cell is found, returns nil, but this should be rare.
// @param entity_type number
// @param spawn_cell ivec2
// @return ivec2|nil
static int script_entity_find_spawn_cell(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 2);

    // Entity type
    int entity_type = lua_tonumber(lua_state, 1);
    script_validate_entity_type(lua_state, entity_type);

    // Spawn cell
    ivec2 spawn_cell = script_lua_to_ivec2(lua_state, 2, "spawn_cell");

    const EntityData& entity_data = entity_get_data((EntityType)entity_type);
    const MatchShellState* state = script_get_match_shell_state(lua_state);

    std::vector<ivec2> frontier;
    std::vector<bool> explored = std::vector<bool>(state->match_state->map.width * state->match_state->map.height, false);
    ivec2 result = ivec2(-1, -1);

    frontier.push_back(spawn_cell);

    while (!frontier.empty()) {
        uint32_t nearest_index = 0;
        for (uint32_t index = 1; index < frontier.size(); index++) {
            if (ivec2::manhattan_distance(frontier[index], spawn_cell) < 
                    ivec2::manhattan_distance(frontier[nearest_index], spawn_cell)) {
                nearest_index = index;
            }
        }
        ivec2 next = frontier[nearest_index];
        frontier[nearest_index] = frontier.back();
        frontier.pop_back();

        if (!map_is_cell_rect_occupied(state->match_state->map, entity_data.cell_layer, next, entity_data.cell_size)) {
            result = next;
            break;
        }

        explored[next.x + (next.y * state->match_state->map.width)] = true;

        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_rect_in_bounds(state->match_state->map, child, entity_data.cell_size)) {
                continue;
            }
            if (explored[child.x + (child.y * state->match_state->map.width)]) {
                continue;
            }
            if (ivec2::manhattan_distance(child, spawn_cell) > 16) {
                continue;
            }

            bool is_in_frontier = false;
            for (ivec2 cell : frontier) {
                if (cell == child) {
                    is_in_frontier = true;
                    break;
                }
            }
            if (!is_in_frontier) {
                frontier.push_back(child);
            }
        }
    }

    if (result.x == -1) {
        log_warn("No entity spawn cell was found for entity type %u spawn_cell <%i, %i>", entity_type, spawn_cell.x, spawn_cell.y);
        lua_pushnil(lua_state);
    } else {
        script_lua_push_ivec2(lua_state, result);
    }

    return 1;
}

// Creates a new entity. Returns the ID of the newly created entity
// @param entity_type number
// @param cell ivec2
// @param player_id number
// @return number
static int script_entity_create(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    int entity_type = (int)lua_tonumber(lua_state, 1);
    script_validate_entity_type(lua_state, entity_type);

    ivec2 cell = script_lua_to_ivec2(lua_state, 2, "cell");

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 3);
    script_validate_player_id(lua_state, state, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE);

    EntityId entity_id = entity_create(state->match_state, (EntityType)entity_type, cell, player_id);
    lua_pushnumber(lua_state, entity_id);

    return 1;
}

// Returns the ID of the player who controls the specified goldmine
// @param goldmine_id number
// @return number
static int script_entity_get_player_who_controls_goldmine(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, state, entity_id);

    const Entity& goldmine = state->match_state->entities[entity_index];

    EntityId hall_surrounding_goldmine_id = match_find_entity(state->match_state, [&goldmine](const Entity& hall, EntityId /*hall_id*/) {
        return hall.type == ENTITY_HALL &&
            entity_is_selectable(hall) &&
            bot_does_entity_surround_goldmine(hall, goldmine.cell);
    });

    uint8_t controlling_player_id = hall_surrounding_goldmine_id != ID_NULL
        ? state->match_state->entities.get_by_id(hall_surrounding_goldmine_id).player_id
        : PLAYER_NONE;
    lua_pushnumber(lua_state, controlling_player_id);

    return 1;
}

// BOT

// Spawns an enemy squad. The entities table should be an array of entity types.
// Returns the squad ID of the created squad, or SQUAD_ID_NULL if no squad was created.
// @param params { player_id: number, type: number, target_cell: ivec2, entities: table }
// @return number 
static int script_bot_add_squad(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Player
    lua_getfield(lua_state, 1, "player_id");
    script_validate_type(lua_state, -1, "player_id", LUA_TNUMBER);
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Type
    lua_getfield(lua_state, 1, "type");
    uint32_t squad_type = (uint32_t)lua_tonumber(lua_state, -1);
    if (squad_type >= BOT_SQUAD_TYPE_COUNT) {
        script_error(lua_state, "Squad type %u is invalid.", squad_type);
    }
    lua_pop(lua_state, 1);

    // Target cell
    lua_getfield(lua_state, 1, "target_cell");
    script_validate_type(lua_state, -1, "target_cell", LUA_TTABLE);
    ivec2 target_cell = script_lua_to_ivec2(lua_state, -1, "target_cell");
    lua_pop(lua_state, 1);

    // Entities
    lua_getfield(lua_state, 1, "entity_list");
    script_validate_type(lua_state, -1, "entity_list", LUA_TTABLE);

    EntityList entity_list;
    lua_pushnil(lua_state);
    while (lua_next(lua_state, -2)) {
        script_validate_type(lua_state, -1, "entities.element", LUA_TNUMBER);
        EntityId entity_id = (EntityId)lua_tonumber(lua_state, -1);
        if (entity_id == ID_NULL) {
            log_warn("bot_add_squad: provided entity_id is ID_NULL.");
            lua_pop(lua_state, 1);
            continue;
        } 
        
        if (entity_list.is_full()) {
            log_warn("bot_add_squad: entity_list is full.");
            lua_pop(lua_state, 1);
            break;
        }

        entity_list.push_back(entity_id);

        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1); 

    // Create the squad
    MatchShellState* state = script_get_match_shell_state(lua_state);

    int squad_id = BOT_SQUAD_ID_NULL;
    if (!entity_list.empty()) {
        squad_id = bot_add_squad(state->bots[player_id], {
            .type = (BotSquadType)squad_type,
            .target_cell = target_cell,
            .entity_list = entity_list
        });
    }

    if (squad_id == BOT_SQUAD_ID_NULL) {
        log_warn("script_bot_add_squad() did not create a squad.");
    }

    lua_pushnumber(lua_state, squad_id);

    return 1;
}

// Checks if the squad exists.
// @param player_id number
// @param squad_id number
// @return boolean 
static int script_bot_squad_exists(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    int squad_id = (int)lua_tonumber(lua_state, 2);
    if (squad_id == BOT_SQUAD_ID_NULL) {
        script_error(lua_state, "Squad ID NULL is not valid.");
    }

    bool result = false;
    for (uint32_t squad_index = 0; squad_index < state->bots[player_id].squads.size(); squad_index++) {
        const BotSquad& squad = state->bots[player_id].squads[squad_index];
        if (squad.id == squad_id) {
            result = true;
            break;
        }
    }

    lua_pushboolean(lua_state, result);

    return 1;
}

// Sets a bot config flag to the specified value
// @param player_id number
// @param flag number
// @param value boolean
static int script_bot_set_config_flag(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER, LUA_TBOOLEAN };
    script_validate_arguments(lua_state, arg_types, 3);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    uint32_t flag = (uint32_t)lua_tonumber(lua_state, 2);
    uint32_t flag_index;
    for (flag_index = 0; flag_index < BOT_CONFIG_FLAG_COUNT; flag_index++) {
        uint32_t flag_value = 1U << flag_index;
        if (flag == flag_value) {
            break;
        }
    }
    if (flag_index == BOT_CONFIG_FLAG_COUNT) {
        script_error(lua_state, "%u is not a valid bot config flag.", flag);
    }

    bool value = lua_toboolean(lua_state, 3);

    bitflag_set(&state->bots[player_id].config.flags, flag, value);

    return 0;
}

// Tells the specified bot to reserve the specified entity
// @param player_id number
// @param entity_id number
static int script_bot_reserve_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 2);
    uint32_t entity_index = script_validate_entity_id(lua_state, state, entity_id);

    if (state->match_state->entities[entity_index].player_id != player_id) {
        script_error(lua_state, "Bot tried to reserve an entity (%u) that it didn't own.", entity_id);
    }

    entity_set_flag(state->match_state->entities[entity_index], ENTITY_FLAG_IS_RESERVED, true);

    return 0;
}

// Tells the specified bot o release the specified entity
// @param player_id number
// @param entity_id number
static int script_bot_release_entity(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    script_validate_player_id(lua_state, state, player_id, SCRIPT_VALIDATE_PLAYER_IS_BOT);

    EntityId entity_id = (EntityId)lua_tonumber(lua_state, 2);
    uint32_t entity_index = script_validate_entity_id(lua_state, state, entity_id);

    if (state->match_state->entities[entity_index].player_id != player_id) {
        script_error(lua_state, "Bot tried to reserve an entity (%u) that it didn't own.", entity_id);
    }

    entity_set_flag(state->match_state->entities[entity_index], ENTITY_FLAG_IS_RESERVED, false);

    return 0;
}

// MATCH INPUT

void script_queue_match_input_get_entity_ids(lua_State* lua_state, uint8_t* entity_count, EntityId entity_ids[SELECTION_LIMIT]) {
    *entity_count = 0;

    lua_getfield(lua_state, 1, "entity_id");
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "entity_id", LUA_TNUMBER);
        *entity_count = 1;
        entity_ids[0] = (EntityId)lua_tonumber(lua_state, -1);
    }
    lua_pop(lua_state, 1);

    if (*entity_count != 0) {
        return;
    }

    lua_getfield(lua_state, 1, "entity_ids");
    if (!lua_isnil(lua_state, -1)) {
        script_validate_type(lua_state, -1, "entity_ids", LUA_TTABLE);
        lua_pushnil(lua_state);
        while (lua_next(lua_state, -2) != 0) {
            script_validate_type(lua_state, -1, "entity_ids.element", LUA_TNUMBER);
            if (*entity_count == SELECTION_LIMIT) {
                script_error(lua_state, "Too many entities provided for match input.");
            }

            entity_ids[*entity_count] = (EntityId)lua_tonumber(lua_state, -1);
            (*entity_count)++;
            lua_pop(lua_state, 1);
        }
        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 1);

    if (*entity_count == 0) {
        script_error(lua_state, "No entities provided for match input.");
    }
}

// Queues a match input
// 
// MOVE { target_cell: ivec2|nil, target_id: number|nil }
// BUILD { building_type: number, building_cell: ivec2 }
// @param params { player_id: number, type: number, entity_id: number|nil, entity_ids: table|nil, ... }
static int script_queue_match_input(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    // Input type
    lua_getfield(lua_state, 1, "type");
    uint8_t input_type = (uint8_t)lua_tonumber(lua_state, -1);
    if (input_type >= MATCH_INPUT_TYPE_COUNT) {
        script_error(lua_state, "Invalid match input type %u", input_type);
    }
    lua_pop(lua_state, 1);

    // Player ID
    lua_getfield(lua_state, 1, "player_id");
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, -1);
    script_validate_player_id(lua_state, state, player_id, SCRIPT_VALIDATE_PLAYER_IS_ACTIVE);
    lua_pop(lua_state, 1);

    MatchInput input;
    input.type = input_type;

    switch (input.type) {
        case MATCH_INPUT_NONE:
            break;
        case MATCH_INPUT_MOVE_CELL:
        case MATCH_INPUT_MOVE_ATTACK_CELL:
        case MATCH_INPUT_MOVE_UNLOAD: 
        case MATCH_INPUT_MOVE_MOLOTOV: {
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;

            // Target cell
            lua_getfield(lua_state, 1, "target_cell");
            script_validate_type(lua_state, -1, "target_cell", LUA_TTABLE);
            input.move.target_cell = script_lua_to_ivec2(lua_state, -1, "target_cell");
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.move.entity_count, input.move.entity_ids);
            break;
        }
        case MATCH_INPUT_MOVE_ENTITY:
        case MATCH_INPUT_MOVE_ATTACK_ENTITY:
        case MATCH_INPUT_MOVE_REPAIR: {
            input.move.shift_command = 0;
            input.move.target_cell = ivec2(-1, -1);

            // Target ID
            lua_getfield(lua_state, 1, "target_id");
            script_validate_type(lua_state, -1, "target_id", LUA_TNUMBER);
            input.move.target_id = (EntityId)lua_tonumber(lua_state, -1);
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.move.entity_count, input.move.entity_ids);
            break;
        }
        case MATCH_INPUT_BUILD: {
            input.build.shift_command = 0;

            // Building type
            lua_getfield(lua_state, 1, "building_type");
            int building_type = (int)lua_tonumber(lua_state, -1);
            script_validate_entity_type(lua_state, building_type);
            input.build.building_type = (EntityType)building_type;
            lua_pop(lua_state, 1);

            // Cell
            lua_getfield(lua_state, 1, "building_cell");
            script_validate_type(lua_state, -1, "building_cell", LUA_TTABLE);
            input.build.target_cell = script_lua_to_ivec2(lua_state, -1, "building_cell");
            lua_pop(lua_state, 1);

            script_queue_match_input_get_entity_ids(lua_state, &input.build.entity_count, input.build.entity_ids);
            break;
        }
        default: {
            script_error(lua_state, "Match input type %s is not yet supported.", match_input_type_str((MatchInputType)input_type));
        }
    }

    state->inputs[player_id].push({ input });

    return 0;
}
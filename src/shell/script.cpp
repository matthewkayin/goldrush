#include "shell/shell.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "network/network.h"
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
static int script_log(lua_State* lua_state);
static int script_set_match_over_victory(lua_State* lua_state);
static int script_set_match_over_defeat(lua_State* lua_state);
static int script_chat(lua_State* lua_state);
static int script_hint(lua_State* lua_state);
static int script_play_sound(lua_State* lua_state);
static int script_get_time(lua_State* lua_state);
static int script_add_objective(lua_State* lua_state);
static int script_complete_objective(lua_State* lua_state);
static int script_is_objective_complete(lua_State* lua_state);
static int script_are_objectives_complete(lua_State* lua_state);
static int script_clear_objectives(lua_State* lua_state);
static int script_entity_is_visible_to_player(lua_State* lua_state);
static int script_highlight_entity(lua_State* lua_state);
static int script_get_player_entity_count(lua_State* lua_state);
static int script_get_player_full_bunker_count(lua_State* lua_state);
static int script_fog_reveal(lua_State* lua_state);
static int script_begin_camera_pan(lua_State* lua_state);
static int script_begin_camera_return(lua_State* lua_state);
static int script_free_camera(lua_State* lua_state);
static int script_get_camera_mode(lua_State* lua_state);
static int script_spawn_enemy_squad(lua_State* lua_state);
static int script_does_squad_exist(lua_State* lua_state);
static int script_is_player_defeated(lua_State* lua_state);

static const char* MODULE_NAME = "scenario";

static const luaL_reg GOLD_FUNCS[] = {
    { "log", script_log },
    { "set_match_over_victory", script_set_match_over_victory },
    { "set_match_over_defeat", script_set_match_over_defeat },
    { "chat", script_chat },
    { "hint", script_hint },
    { "play_sound", script_play_sound },
    { "get_time", script_get_time },
    { "add_objective", script_add_objective },
    { "complete_objective", script_complete_objective },
    { "is_objective_complete", script_is_objective_complete },
    { "are_objectives_complete", script_are_objectives_complete },
    { "clear_objectives", script_clear_objectives },
    { "entity_is_visible_to_player", script_entity_is_visible_to_player },
    { "highlight_entity", script_highlight_entity },
    { "get_player_entity_count", script_get_player_entity_count },
    { "get_player_full_bunker_count", script_get_player_full_bunker_count },
    { "fog_reveal", script_fog_reveal },
    { "begin_camera_pan", script_begin_camera_pan },
    { "begin_camera_return", script_begin_camera_return },
    { "free_camera", script_free_camera },
    { "get_camera_mode", script_get_camera_mode },
    { "spawn_enemy_squad", script_spawn_enemy_squad },
    { "does_squad_exist", script_does_squad_exist },
    { "is_player_defeated", script_is_player_defeated },
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
    { "CAMERA_MODE_PAN_HOLD", CAMERA_MODE_PAN_HOLD },
    { "CAMERA_MODE_PAN_RETURN", CAMERA_MODE_PAN_RETURN },
    { NULL, 0 }
};

const char* match_shell_script_get_entity_type_str(EntityType type);
void match_shell_script_handle_error(MatchShellState* state);

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

    // Scenario module constants

    // Script constants
    lua_getglobal(state->scenario_lua_state, MODULE_NAME);
    size_t const_index = 0;
    while (GOLD_CONSTANTS[const_index].name != NULL) {
        lua_pushinteger(state->scenario_lua_state, GOLD_CONSTANTS[const_index].value);
        lua_setfield(state->scenario_lua_state, -2, GOLD_CONSTANTS[const_index].name);

        const_index++;
    }

    // Player ID constant
    // It's gonna be 0 anyways, but we may as well
    lua_pushinteger(state->scenario_lua_state, network_get_player_id());
    lua_setfield(state->scenario_lua_state, -2, "PLAYER_ID");

    // Entity constants
    lua_newtable(state->scenario_lua_state);
    for (int entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        lua_pushinteger(state->scenario_lua_state, entity_type);
        lua_setfield(state->scenario_lua_state, -2, match_shell_script_get_entity_type_str((EntityType)entity_type));
    }
    lua_setfield(state->scenario_lua_state, -2, "entity_type");

    // Sound constants
    lua_newtable(state->scenario_lua_state);
    for (int sound_name = 0; sound_name < SOUND_COUNT; sound_name++) {
        const char* sound_name_str = sound_get_name((SoundName)sound_name);
        char const_name[64];

        size_t index = 0;
        while (sound_name_str[index] != '\0') {
            const_name[index] = toupper(sound_name_str[index]);
            index++;
        }
        const_name[index] = '\0';

        lua_pushinteger(state->scenario_lua_state, sound_name);
        lua_setfield(state->scenario_lua_state, -2, const_name);
    }
    lua_setfield(state->scenario_lua_state, -2, "sound");

    // Scenario file constants
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

    // Call scenario_init() 
    if (lua_pcall(state->scenario_lua_state, 0, 0, 0)) {
        match_shell_script_handle_error(state);
    }

    return true;
}

void match_shell_script_update(MatchShellState* state) {
    lua_getglobal(state->scenario_lua_state, "scenario_update");
    if (lua_pcall(state->scenario_lua_state, 0, 0, 0)) {
        match_shell_script_handle_error(state);
    }
}

void match_shell_script_handle_error(MatchShellState* state) {
    const char* error_str = lua_tostring(state->scenario_lua_state, -1);

    // Lua is not giving us the short_src so we will
    // manually look through the string to find the tail end 
    // (past the path separators)

    size_t index = 0;
    size_t path_sep_index = 0;
    while (error_str[index] != '\0') {
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

#ifdef GOLD_DEBUG
void match_shell_script_generate_doc() {
    std::string doc_path = filesystem_get_resource_path() + "scenario" + GOLD_PATH_SEPARATOR + "modules" + GOLD_PATH_SEPARATOR + "scenario.d.lua";
    FILE* file = fopen(doc_path.c_str(), "w");
    if (!file) {
        printf("Error: could not open %s.\n", doc_path.c_str());
        return;
    }

    fprintf(file, "--- @meta\n");
    fprintf(file, "%s = {}\n\n", MODULE_NAME);

    fprintf(file, "--- @class ivec2\n");
    fprintf(file, "--- @field x number\n");
    fprintf(file, "--- @field y number\n\n");

    fprintf(file, "--- Script constants\n");
    size_t index = 0;
    while (GOLD_CONSTANTS[index].name != NULL) {
        fprintf(file, "%s.%s = %i\n", MODULE_NAME, GOLD_CONSTANTS[index].name, GOLD_CONSTANTS[index].value);
        index++;
    }
    fprintf(file, "\n");

    fprintf(file, "--- Scenario constants\n");
    fprintf(file, "%s.constants = {}\n\n", MODULE_NAME);

    fprintf(file, "--- Entity constants\n");
    fprintf(file, "%s.entity_type = {}\n", MODULE_NAME);
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        fprintf(file, "%s.entity_type.%s = %u\n", MODULE_NAME, match_shell_script_get_entity_type_str((EntityType)entity_type), entity_type);
    }
    fprintf(file, "\n");

    fprintf(file, "--- Sound constants\n");
    fprintf(file, "%s.sound = {}\n", MODULE_NAME);
    for (uint32_t sound = 0; sound < SOUND_COUNT; sound++) {
        const char* sound_name = sound_get_name((SoundName)sound);
        char const_name[64];
        sprintf(const_name, "%s", sound_name);
        char* sound_name_ptr = const_name;
        while (*sound_name_ptr != '\0') {
            (*sound_name_ptr) = toupper(*sound_name_ptr);
            sound_name_ptr++;
        }

        fprintf(file, "%s.sound.%s = %u\n", MODULE_NAME, const_name, sound);
    }
    fprintf(file, "\n");

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

    index = 0;
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

void script_error(lua_State* lua_state, const char* message) {
    lua_Debug debug;
    lua_getstack(lua_state, 1, &debug);
    lua_getinfo(lua_state, "Sl", &debug);

    char error_str[512];
    sprintf(error_str, "%s:%i %s", debug.source, debug.currentline, message);
    lua_pushstring(lua_state, error_str);
    lua_error(lua_state);
}

void script_validate_type(lua_State* lua_state, int stack_index, const char* name, int expected_type) {
    int received_type = lua_type(lua_state, stack_index);
    if (received_type != expected_type) {
        char error_message[256];
        sprintf(error_message, 
            "Invalid type for %s. Received %s. Expected %s.",
            name,
            script_lua_type_str(received_type),
            script_lua_type_str(expected_type));
        script_error(lua_state, error_message);
    }
}

void script_validate_arguments(lua_State* lua_state, const int* arg_types, int arg_count) {
    int arg_number = lua_gettop(lua_state);
    if (arg_number != arg_count) {
        char error_message[256];
        sprintf(error_message, "Invalid arg count. Received %u. Expected %u.", arg_number, arg_count);
        script_error(lua_state, error_message);
    }

    for (int arg_index = 1; arg_index <= arg_count; arg_index++) {
        char field_name[16];
        sprintf(field_name, "arg %u", arg_index);
        script_validate_type(lua_state, arg_index, field_name, arg_types[arg_index - 1]);
    }
}

void script_validate_entity_type(lua_State* lua_state, int entity_type) {
    if (entity_type < 0 || entity_type >= ENTITY_TYPE_COUNT) {
        char error_str[128];
        sprintf(error_str, "Entity type %i not recognized.", entity_type);
        script_error(lua_state, error_str);
    }
}

uint32_t script_validate_entity_id(lua_State* lua_state, const MatchShellState* state, EntityId entity_id) {
    uint32_t entity_index = state->match_state.entities.get_index_of(entity_id);
    if (entity_index == INDEX_INVALID) {
        char error_message[128];
        sprintf(error_message, "Entity ID %u does not exist.", entity_id);
        script_error(lua_state, error_message);
    }

    return entity_index;
}

ivec2 script_validate_ivec2(lua_State* lua_state, int stack_index, const char* name) {
    char field_name[32];
    ivec2 value;

    lua_getfield(lua_state, stack_index, "x");
    sprintf(field_name, "%s.x", name);
    script_validate_type(lua_state, -1, field_name, LUA_TNUMBER);
    value.x = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    lua_getfield(lua_state, stack_index, "y");
    sprintf(field_name, "%s.y", name);
    script_validate_type(lua_state, -1, field_name, LUA_TNUMBER);
    value.y = (int)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    return value;
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
                const char* key = lua_tostring(lua_state, -2);
                offset += sprintf(str_ptr + offset, "%s = ", key);
                offset += script_sprintf(str_ptr + offset, lua_state, -1);
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

// End the match in victory.
static int script_set_match_over_victory(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->mode = MATCH_SHELL_MODE_MATCH_OVER_VICTORY;

    return 0;
}

// End the match in defeat.
static int script_set_match_over_defeat(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->mode = MATCH_SHELL_MODE_MATCH_OVER_DEFEAT;

    return 0;
}

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
        char error_message[128];
        sprintf(error_message, "Unregonized chat color %i", chat_color);
        script_error(lua_state, error_message);
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

// Plays a sound effect.
// @param sound number
static int script_play_sound(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    int sound = (int)lua_tonumber(lua_state, 1);

    if (sound < 0 || sound >= SOUND_COUNT) {
        char error_message[128];
        sprintf(error_message, "Invalid sound %i", sound);
        script_error(lua_state, error_message);
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
        char error_message[128];
        sprintf(error_message, "Objective index %u out of bounds.", objective_index);
        script_error(lua_state, error_message);
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
        char error_message[128];
        sprintf(error_message, "Objective index %u out of bounds.", objective_index);
        script_error(lua_state, error_message);
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

// Returns true if the specified entity is visible to the player.
// @param entity_id number
// @return boolean
static int script_entity_is_visible_to_player(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    EntityId entity_id = lua_tonumber(lua_state, 1);
    uint32_t entity_index = script_validate_entity_id(lua_state, state, entity_id);

    bool result = entity_is_visible_to_player(state->match_state, state->match_state.entities[entity_index], network_get_player_id());
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

    return 0;
}

// Returns the number of entities controlled by the player of a given type.
// @param player_id number
// @param entity_type number
// @return number
static int script_get_player_entity_count(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    uint8_t player_id = lua_tonumber(lua_state, 1);
    int entity_type = lua_tonumber(lua_state, 2);
    script_validate_entity_type(lua_state, entity_type);

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint32_t result = match_shell_get_player_entity_count(state, player_id, (EntityType)entity_type);
    lua_pushnumber(lua_state, result);

    return 1;
}

// Returns the number of bunkers controlled by the player that have 4 cowboys in them.
// @param player_id number
// @return number
static int script_get_player_full_bunker_count(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    uint8_t player_id = lua_tonumber(lua_state, 1);
    const MatchShellState* state = script_get_match_shell_state(lua_state);

    uint32_t count = 0;
    for (const Entity& entity : state->match_state.entities) {
        if (entity.player_id != player_id ||
                entity.type != ENTITY_BUNKER ||
                entity.garrisoned_units.size() != 4) {
            continue;
        }
        bool is_all_cowboys = true;
        for (EntityId garrisoned_unit_id : entity.garrisoned_units) {
            if (state->match_state.entities.get_by_id(garrisoned_unit_id).type != ENTITY_COWBOY) {
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

// Reveals fog at the specified cell.
// @param params { player_id: number, cell: ivec2, cell_size: number, sight: number, duration: number }
static int script_fog_reveal(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Team
    lua_getfield(lua_state, 1, "player_id");
    script_validate_type(lua_state, -1, "player_id", LUA_TNUMBER);
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Cell
    lua_getfield(lua_state, 1, "cell");
    ivec2 cell = script_validate_ivec2(lua_state, -1, "cell");
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
    uint8_t team = state->match_state.players[player_id].team;
    match_fog_update(state->match_state, team, cell, cell_size, sight, false, CELL_LAYER_SKY, true);
    state->match_state.fog_reveals.push_back((FogReveal) {
        .team = team,
        .cell = cell,
        .cell_size = cell_size,
        .sight = sight,
        .timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND)
    });

    return 0;
}

// Gradually pans the camera to center on the specified cell. 
// @param cell ivec2 The cell to pan the camera to
// @param duration number The duration in seconds of the camera pan
static int script_begin_camera_pan(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    ivec2 cell = script_validate_ivec2(lua_state, 1, "arg 1");
    double duration = lua_tonumber(lua_state, 2);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    // Convert to cell into camera offset
    state->camera_pan_offset.x = (cell.x * TILE_SIZE) + (TILE_SIZE / 2) - (SCREEN_WIDTH / 2);
    state->camera_pan_offset.y = (cell.y * TILE_SIZE) + (TILE_SIZE / 2) - ((SCREEN_HEIGHT - MATCH_SHELL_UI_HEIGHT) / 2);
    state->camera_pan_offset.x = std::clamp(state->camera_pan_offset.x, 0, (state->match_state.map.width * TILE_SIZE) - SCREEN_WIDTH);
    state->camera_pan_offset.y = std::clamp(state->camera_pan_offset.y, 0, (state->match_state.map.height * TILE_SIZE) - SCREEN_HEIGHT + MATCH_SHELL_UI_HEIGHT);

    state->camera_pan_return_offset = state->camera_offset;
    state->camera_pan_timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND);
    state->camera_pan_duration = state->camera_pan_timer;

    state->camera_mode = CAMERA_MODE_PAN;

    return 0;
}

// Gradually returns the camera to the position it was at before the most recent camera pan started.
// @param duration number The duration in seconds of the camera return
static int script_begin_camera_return(lua_State* lua_state) {
    const int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    double duration = lua_tonumber(lua_state, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);

    state->camera_pan_timer = (uint32_t)(duration * (double)UPDATES_PER_SECOND);
    state->camera_pan_duration = state->camera_pan_timer;
    state->camera_mode = CAMERA_MODE_PAN_RETURN;

    return 0;
}

// Ends the current camera pan and returns free camera movement to the player.
static int script_free_camera(lua_State* lua_state) {
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

// Spawns an enemy squad. The entities table should be an array of entity types.
// Returns the squad ID of the created squad, or SQUAD_ID_NULL if no squad was created.
// @param params { player_id: number, target_cell: ivec2, spawn_cell: ivec2, entities: table }
// @return number 
static int script_spawn_enemy_squad(lua_State* lua_state) {
    int arg_types[] = { LUA_TTABLE };
    script_validate_arguments(lua_state, arg_types, 1);

    // Player
    lua_getfield(lua_state, 1, "player_id");
    script_validate_type(lua_state, -1, "player_id", LUA_TNUMBER);
    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, -1);
    lua_pop(lua_state, 1);

    // Target cell
    lua_getfield(lua_state, 1, "target_cell");
    ivec2 target_cell = script_validate_ivec2(lua_state, -1, "target_cell");
    lua_pop(lua_state, 1);

    // Spawn cell
    lua_getfield(lua_state, 1, "spawn_cell");
    ivec2 spawn_cell = script_validate_ivec2(lua_state, -1, "spawn_cell");
    lua_pop(lua_state, 1);

    // Entities
    lua_getfield(lua_state, 1, "entities");
    script_validate_type(lua_state, -1, "entities", LUA_TTABLE);

    std::vector<EntityType> entity_types;
    lua_pushnil(lua_state);
    while (lua_next(lua_state, -2)) {
        script_validate_type(lua_state, -1, "entity_type", LUA_TNUMBER);
        int value = lua_tonumber(lua_state, -1);
        script_validate_entity_type(lua_state, value);
        entity_types.push_back((EntityType)value);

        lua_pop(lua_state, 1);
    }

    lua_pop(lua_state, 1); 

    if (entity_types.empty()) {
        script_error(lua_state, "No entities provided for spawn_enemy_squad()");
    }

    // Create the squad
    MatchShellState* state = script_get_match_shell_state(lua_state);

    std::vector<EntityId> squad_entity_list;
    for (EntityType entity_type : entity_types) {
        ivec2 entity_cell = match_shell_spawn_enemy_squad_find_spawn_cell(state, entity_type, spawn_cell, target_cell);
        if (entity_cell.x == -1) {
            log_warn("Couldn't find a place to spawn unit with type %u spawn cell <%i, %i>", entity_type, spawn_cell.x, spawn_cell.y);
        }

        EntityId entity_id = entity_create(state->match_state, entity_type, entity_cell, player_id);
        squad_entity_list.push_back(entity_id);
    }

    int squad_id = BOT_SQUAD_ID_NULL;
    if (!squad_entity_list.empty()) {
        squad_id = bot_add_squad(state->bots[player_id], {
            .type = BOT_SQUAD_TYPE_ATTACK,
            .target_cell = target_cell,
            .entities = squad_entity_list
        });
    }

    lua_pushnumber(lua_state, squad_id);

    return 1;
}

// Checks if the squad exists.
// @param player_id number
// @param squad_id number
// @return boolean 
static int script_does_squad_exist(lua_State* lua_state) {
    int arg_types[] = { LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    if (player_id == 0 || player_id >= MAX_PLAYERS) {
        char error_message[128];
        sprintf(error_message, "player_id %u is out of bounds", player_id);
        script_error(lua_state, error_message);
    }

    int squad_id = (int)lua_tonumber(lua_state, 2);
    if (squad_id == BOT_SQUAD_ID_NULL) {
        script_error(lua_state, "Squad ID NULL is not valid.");
    }

    const MatchShellState* state = script_get_match_shell_state(lua_state);

    bool result = false;
    for (const BotSquad& squad : state->bots[player_id].squads) {
        if (squad.id == squad_id) {
            result = true;
            break;
        }
    }

    lua_pushboolean(lua_state, result);

    return 1;
}

// Checks if the player has been defeated.
// @param player_id number
// @return boolean 
static int script_is_player_defeated(lua_State* lua_state) {
    int arg_types[] = { LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 1);

    uint8_t player_id = (uint8_t)lua_tonumber(lua_state, 1);
    if (player_id == 0 || player_id >= MAX_PLAYERS) {
        char error_message[128];
        sprintf(error_message, "player_id %u is out of bounds", player_id);
        script_error(lua_state, error_message);
    }

    const MatchShellState* state = script_get_match_shell_state(lua_state);
    lua_pushboolean(lua_state, !state->match_state.players[player_id].active);

    return 1;
}
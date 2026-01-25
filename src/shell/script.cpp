#include "shell/shell.h"

#include "core/logger.h"

enum ScriptChatColor {
    SCRIPT_CHAT_COLOR_WHITE,
    SCRIPT_CHAT_COLOR_GOLD,
    SCRIPT_CHAT_COLOR_BLUE
};

struct ScriptConstant {
    const char* name;
    int value;
};

void match_shell_script_handle_error(MatchShellState* state);

// Lua function predeclares
static int script_log(lua_State* lua_state);
static int script_set_lose_on_buildings_destroyed(lua_State* lua_state);
static int script_chat(lua_State* lua_state);
static int script_chat_hint(lua_State* lua_state);
static int script_play_sound(lua_State* lua_state);
static int script_get_time(lua_State* lua_state);
static int script_add_objective(lua_State* lua_state);
static int script_add_objective_with_variable_counter(lua_State* lua_state);
static int script_add_objective_with_entity_counter(lua_State* lua_state);

static const luaL_reg GOLD_FUNCS[] = {
    { "log", script_log },
    { "set_lose_on_buildings_destroyed", script_set_lose_on_buildings_destroyed },
    { "chat", script_chat },
    { "chat_hint", script_chat_hint },
    { "play_sound", script_play_sound },
    { "get_time", script_get_time },
    { "add_objective", script_add_objective },
    { "add_objective_with_variable_counter", script_add_objective_with_variable_counter },
    { "add_objective_with_entity_counter", script_add_objective_with_entity_counter },
    { NULL, NULL }
};

static const ScriptConstant GOLD_CONSTANTS[] = {
    { "CHAT_COLOR_WHITE", SCRIPT_CHAT_COLOR_WHITE },
    { "CHAT_COLOR_GOLD", SCRIPT_CHAT_COLOR_GOLD },
    { "CHAT_COLOR_BLUE", SCRIPT_CHAT_COLOR_GOLD },
    { NULL, NULL }
};

bool match_shell_script_init(MatchShellState* state, const char* script_path) {
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

    // Register gold library
    luaL_register(state->scenario_lua_state, "scenario", GOLD_FUNCS);

    // Give lua a pointer to the shell state
    lua_pushlightuserdata(state->scenario_lua_state, state);
    lua_setfield(state->scenario_lua_state, LUA_REGISTRYINDEX, "__match_shell_state");

    // Constants
    lua_getglobal(state->scenario_lua_state, "scenario");
    size_t const_index = 0;
    while (GOLD_CONSTANTS[const_index].name != NULL) {
        lua_pushinteger(state->scenario_lua_state, GOLD_CONSTANTS[const_index].value);
        lua_setfield(state->scenario_lua_state, -2, GOLD_CONSTANTS[const_index].name);

        const_index++;
    }

    for (int entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        const char* entity_name = entity_get_data((EntityType)entity_type).name;

        char const_name[32];
        char* const_name_ptr = const_name;
        const_name_ptr += sprintf(const_name_ptr, "ENTITY_");

        size_t index = 0;
        while (entity_name[index] != '\0') {
            if (entity_name[index] == ' ' && !(entity_type == ENTITY_GOLDMINE || entity_type == ENTITY_LANDMINE)) {
                const_name_ptr[index] = '_';
            } else if (entity_name[index] != ' ' || entity_name[index] != '\'') {
                const_name_ptr[index] = toupper(entity_name[index]);
            }
            index++;
        }
        const_name_ptr[index] = '\0';

        log_debug("Setting const %s=%i", const_name, entity_type);

        lua_pushinteger(state->scenario_lua_state, entity_type);
        lua_setfield(state->scenario_lua_state, -2, const_name);
    }
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
        log_error("%s", lua_tostring(state->scenario_lua_state, -1));
        match_shell_leave_match(state, false);
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

void script_validate_arguments(lua_State* lua_state, const int* arg_types, int arg_count) {
    int arg_number = lua_gettop(lua_state);
    if (arg_number != arg_count) {
        char error_message[256];
        sprintf(error_message, "Invalid arg count. Expected %u. Received %u.", arg_count, arg_number);
        script_error(lua_state, error_message);
    }

    for (int arg_index = 1; arg_index <= arg_count; arg_index++) {
        int received_type = lua_type(lua_state, arg_index);
        if (received_type != arg_types[arg_index - 1]) {
            char error_message[256];
            sprintf(error_message,
                "Invalid type for arg %u. Received %s. Expected %s.", 
                arg_index, 
                script_lua_type_str(received_type), 
                script_lua_type_str(arg_types[arg_index - 1])
            );
            script_error(lua_state, error_message);
        }
    }
}

void script_validate_entity_type(lua_State* lua_state, int entity_type) {
    if (entity_type < 0 || entity_type >= ENTITY_TYPE_COUNT) {
        char error_str[128];
        sprintf(error_str, "Entity type %i not recognized.", entity_type);
        script_error(lua_state, error_str);
    }
}

static int script_log(lua_State* lua_state) {
#if GOLD_LOG_LEVEL >= LOG_LEVEL_DEBUG
    int nargs = lua_gettop(lua_state);
    char buffer[2048];
    char* buffer_ptr = buffer;

    for (int arg_index = 1; arg_index <= nargs; arg_index++) {
        int arg_type = lua_type(lua_state, arg_index);
        switch (arg_type) {
            case LUA_TNIL: {
                buffer_ptr += sprintf(buffer_ptr, "nil ");
                break;
            }
            case LUA_TNUMBER: {
                double value = lua_tonumber(lua_state, arg_index);
                buffer_ptr += sprintf(buffer_ptr, "%f ", value);
                break;
            }
            case LUA_TBOOLEAN: {
                bool value = lua_toboolean(lua_state, arg_index);
                buffer_ptr += sprintf(buffer_ptr, "%s ", value ? "true" : "false");
                break;
            }
            case LUA_TSTRING: {
                const char* value = lua_tostring(lua_state, arg_index);
                buffer_ptr += sprintf(buffer_ptr, "%s ", value);
                break;
            }
            default: {
                buffer_ptr += sprintf(buffer_ptr, "%s ", script_lua_type_str(arg_type));
                break;
            }
        }
    }

    log_debug("%s", buffer);
#endif

    return 0;
}

static int script_set_lose_on_buildings_destroyed(lua_State* lua_state) {
    const int arg_types[] = { LUA_TBOOLEAN };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->scenario_lose_on_buildings_destroyed = lua_toboolean(lua_state, 1);
    
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

static int script_chat_hint(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    const char* message = lua_tostring(lua_state, 1);

    sound_play(SOUND_UI_CLICK);
    match_shell_add_chat_message(state, FONT_HACK_PLAYER0, "Hint:", message, CHAT_MESSAGE_HINT_DURATION);

    return 0;
}

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

static int script_get_time(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    lua_pushnumber(lua_state, (double)state->match_timer / (double)UPDATES_PER_SECOND);

    return 1;
}

int script_add_objective_helper(MatchShellState* state, Objective objective) {
    state->scenario_objectives.push_back(objective);
    lua_pushnumber(state->scenario_lua_state, state->scenario_objectives.size() - 1);

    return 1;
}

static int script_add_objective(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING };
    script_validate_arguments(lua_state, arg_types, 1);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    Objective objective = (Objective) {
        .description = std::string(lua_tostring(lua_state, 1)),
        .is_complete = false,
        .counter_type = OBJECTIVE_COUNTER_TYPE_NONE,
        .counter_value = 0,
        .counter_target = 0
    };

    return script_add_objective_helper(state, objective);
}

static int script_add_objective_with_variable_counter(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    Objective objective = (Objective) {
        .description = std::string(lua_tostring(lua_state, 1)),
        .is_complete = false,
        .counter_type = OBJECTIVE_COUNTER_TYPE_VARIABLE,
        .counter_value = 0,
        .counter_target = (uint32_t)lua_tonumber(lua_state, 2)
    };

    return script_add_objective_helper(state, objective);
}

static int script_add_objective_with_entity_counter(lua_State* lua_state) {
    const int arg_types[] = { LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 3);

    int counter_value = (int)lua_tonumber(lua_state, 2);
    script_validate_entity_type(lua_state, counter_value);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    Objective objective = (Objective) {
        .description = std::string(lua_tostring(lua_state, 1)),
        .is_complete = false,
        .counter_type = OBJECTIVE_COUNTER_TYPE_ENTITY,
        .counter_value = (uint32_t)counter_value,
        .counter_target = (uint32_t)lua_tonumber(lua_state, 3)
    };

    return script_add_objective_helper(state, objective);
}
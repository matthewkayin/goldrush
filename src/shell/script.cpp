#include "shell/shell.h"

#include "core/logger.h"
#include "core/filesystem.h"
#include "network/network.h"

enum ScriptChatColor {
    SCRIPT_CHAT_COLOR_WHITE,
    SCRIPT_CHAT_COLOR_GOLD,
    SCRIPT_CHAT_COLOR_BLUE
};

static const int SQUAD_ID_NULL = -1;

struct ScriptConstant {
    const char* name;
    int value;
};

// Lua function predeclares
static int script_log(lua_State* lua_state);
static int script_set_lose_on_buildings_destroyed(lua_State* lua_state);
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
static int script_camera_pan(lua_State* lua_state);
static int script_camera_return(lua_State* lua_state);
static int script_camera_free(lua_State* lua_state);
static int script_spawn_enemy_squad(lua_State* lua_state);

static const luaL_reg GOLD_FUNCS[] = {
    { "log", script_log },
    { "set_lose_on_buildings_destroyed", script_set_lose_on_buildings_destroyed },
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
    { "camera_pan", script_camera_pan },
    { "camera_return", script_camera_return },
    { "camera_free", script_camera_free },
    { "spawn_enemy_squad", script_spawn_enemy_squad },
    { NULL, NULL }
};

static const ScriptConstant GOLD_CONSTANTS[] = {
    { "CHAT_COLOR_WHITE", SCRIPT_CHAT_COLOR_WHITE },
    { "CHAT_COLOR_GOLD", SCRIPT_CHAT_COLOR_GOLD },
    { "CHAT_COLOR_BLUE", SCRIPT_CHAT_COLOR_GOLD },
    { "SQUAD_ID_NULL", SQUAD_ID_NULL },
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
    luaL_register(state->scenario_lua_state, "scenario", GOLD_FUNCS);

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
    lua_getglobal(state->scenario_lua_state, "scenario");
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

static int script_hint(lua_State* lua_state) {
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

static int script_clear_objectives(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    state->scenario_objectives.clear();

    return 0;
}

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
        .timer = (uint32_t)(60.0 * duration)
    });

    return 0;
}

static int script_camera_pan(lua_State* lua_state) {
    const int arg_types[] = { LUA_TTABLE, LUA_TNUMBER };
    script_validate_arguments(lua_state, arg_types, 2);

    ivec2 cell = script_validate_ivec2(lua_state, 1, "arg 1");
    double duration = lua_tonumber(lua_state, 2);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    match_shell_begin_camera_pan(state, cell, (uint32_t)(60.0 * duration));

    return 0;
}

static int script_camera_return(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    match_shell_begin_camera_return(state);

    return 0;
}

static int script_camera_free(lua_State* lua_state) {
    script_validate_arguments(lua_state, NULL, 0);

    MatchShellState* state = script_get_match_shell_state(lua_state);
    match_shell_end_camera_pan(state);

    return 0;
}

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

    int squad_id = SQUAD_ID_NULL;
    if (!squad_entity_list.empty()) {
        squad_id = bot_squad_create(state->bots[player_id], BOT_SQUAD_TYPE_ATTACK, target_cell, squad_entity_list);
    }

    lua_pushnumber(lua_state, squad_id);

    return 1;
}
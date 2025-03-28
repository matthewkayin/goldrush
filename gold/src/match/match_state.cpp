#include "match_state.h"

#include "core/logger.h"
#include "lcg.h"

static const uint32_t MATCH_PLAYER_STARTING_GOLD = 50;
static const uint32_t MATCH_GOLDMINE_STARTING_GOLD = 5000;

MatchState match_init(int32_t lcg_seed, Noise& noise, MatchPlayer players[MAX_PLAYERS]) {
    MatchState state;
    #ifdef GOLD_RAND_SEED
        lcg_seed = GOLD_RAND_SEED;
    #endif
    lcg_srand(lcg_seed);
    log_info("Set random seed to %i", lcg_seed);
    std::vector<ivec2> player_spawns;
    std::vector<ivec2> goldmine_cells;
    map_init(state.map, noise, player_spawns, goldmine_cells);
    memcpy(state.players, players, sizeof(state.players));

    // Generate goldmines
    for (ivec2 cell : goldmine_cells) {
        match_create_goldmine(state, cell, MATCH_GOLDMINE_STARTING_GOLD);
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (!state.players[player_id].active) {
            continue;
        }
        state.players[player_id].gold = MATCH_PLAYER_STARTING_GOLD;
        state.players[player_id].upgrades = 0;
        state.players[player_id].upgrades_in_progress = 0;

        ivec2 town_hall_cell = map_get_player_town_hall_cell(state.map, player_spawns[player_id]);
        match_create_entity(state, ENTITY_MINER, town_hall_cell, player_id);
    }

    return state;
}

void match_update(MatchState& world) {

}

EntityId match_create_entity(MatchState& state, EntityType type, ivec2 cell, uint8_t player_id) {
    const EntityData& entity_data = entity_get_data(type);

    Entity entity;
    entity.type = type;
    entity.mode = ENTITY_MODE_IDLE;
    entity.player_id = player_id;
    entity.flags = 0;

    entity.cell = cell;
    entity.position = entity_is_unit(type) 
                        ? entity_get_target_position(entity)
                        : fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = entity_is_unit(type)
                        ? entity_data.max_health
                        : (entity_data.max_health / 10);
    entity.target = (EntityTarget) { .type = ENTITY_TARGET_NONE };
    entity.pathfind_attempts = 0;
    entity.timer = 0;
    entity.rally_point = ivec2(-1, -1);

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = 0;
    entity.gold_mine_id = ID_NULL;

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;

    EntityId id = state.entities.push_back(entity);
    map_set_cell_rect(state.map, entity.cell, entity_data.cell_size, id);
    // map fog update

    return id;
}

EntityId match_create_goldmine(MatchState& state, ivec2 cell, uint32_t gold_left) {
    Entity entity;
    entity.type = ENTITY_GOLDMINE;
    entity.player_id = PLAYER_NONE;
    entity.flags = 0;
    entity.mode = ENTITY_MODE_GOLDMINE;

    entity.cell = cell;
    entity.position = fvec2(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.garrison_id = ID_NULL;
    entity.gold_held = gold_left;

    EntityId id = state.entities.push_back(entity);
    map_set_cell_rect(state.map, entity.cell, entity_get_data(entity.type).cell_size, id);

    return id;
}

bool match_is_entity_visible_to_player(const MatchState& state, const Entity& entity, uint8_t player_id) {
    return true;
}
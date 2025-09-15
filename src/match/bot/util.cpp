#include "util.h"

#include "core/logger.h"

EntityId bot_find_entity(BotFindEntityParams params) {
    for (uint32_t entity_index = 0; entity_index < params.state.entities.size(); entity_index++) {
        const Entity& entity = params.state.entities[entity_index];
        EntityId entity_id = params.state.entities.get_id_of(entity_index);
        if (params.filter(entity, entity_id)) {
            return entity_id;
        }
    }

    return ID_NULL;
}

EntityId bot_find_best_entity(BotFindBestEntityParams params) {
    uint32_t best_entity_index = INDEX_INVALID;
    for (uint32_t entity_index = 0; entity_index < params.state.entities.size(); entity_index++) {
        const Entity& entity = params.state.entities[entity_index];
        EntityId entity_id = params.state.entities.get_id_of(entity_index);

        if (!params.filter(entity, entity_id)) {
            continue;
        }
        if (best_entity_index == INDEX_INVALID || params.compare(entity, params.state.entities[best_entity_index])) {
            best_entity_index = entity_index;
        }
    }

    if (best_entity_index == INDEX_INVALID) {
        return ID_NULL;
    }
    return params.state.entities.get_id_of(best_entity_index);
}

std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell) {
    return [cell](const Entity& a, const Entity& b) {
        return ivec2::manhattan_distance(a.cell, cell) < ivec2::manhattan_distance(b.cell, cell);
    };
}

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id) {
    return bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end();
}

void bot_reserve_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: reserve entity %u", entity_id);
    bot.is_entity_reserved[entity_id] = true;
}

void bot_release_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: release entity %u", entity_id);
    GOLD_ASSERT(bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end());
    bot.is_entity_reserved.erase(entity_id);
}

void bot_count_entities(const MatchState& state, const Bot& bot, uint32_t* entity_count, uint32_t options) {
    bool include_in_progress = (options & BOT_INCLUDE_IN_PROGRESS) == BOT_INCLUDE_IN_PROGRESS;
    bool include_reserved = (options & BOT_INCLUDE_RESERVED) == BOT_INCLUDE_RESERVED;
    
    memset(entity_count, 0, sizeof(uint32_t) * ENTITY_TYPE_COUNT);
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);

        if (entity.player_id != bot.player_id || entity.health == 0) {
            continue;
        }
        if (!include_reserved && bot_is_entity_reserved(bot, entity_id)) {
            continue;
        }

        entity_count[entity.type]++;
        if (include_in_progress && 
                entity.mode == MODE_BUILDING_FINISHED && 
                !entity.queue.empty() && 
                entity.queue.front().type == BUILDING_QUEUE_ITEM_UNIT) {
            entity_count[entity.queue.front().unit_type]++;
        }
        if (include_in_progress && 
                entity.type == ENTITY_MINER &&
                entity.target.type == TARGET_BUILD) {
            entity_count[entity.target.build.building_type]++;
        }
    }
}

bool bot_has_scouted_entity(const MatchState& state, const Bot& bot, const Entity& entity, EntityId entity_id) {
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    uint32_t bot_team = state.players[bot.player_id].team;
    // If fog is revealed, then they can see it
    if (match_is_cell_rect_revealed(state, bot_team, entity.cell, entity_cell_size)) {
        return true;
    }
    // If for is only explored but they have seen the entity before, then they can see it
    if (match_is_cell_rect_explored(state, bot_team, entity.cell, entity_cell_size) &&
            state.remembered_entities[bot_team].find(entity_id) != state.remembered_entities[bot_team].end()) {
        return true;
    }
    if (bot.scout_assumed_entities.find(entity_id) != bot.scout_assumed_entities.end()) {
        return true;
    }
    // Otherwise, they have not scouted it
    return false;
}

MatchInput bot_return_entity_to_nearest_hall(const MatchState& state, const Bot& bot, EntityId entity_id) {
    const Entity& entity = state.entities.get_by_id(entity_id);

    EntityId nearest_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& hall, EntityId hall_id) {
            return hall.type == ENTITY_HALL &&
                entity_is_selectable(hall) &&
                hall.player_id == bot.player_id;
        },
        .compare = bot_closest_manhattan_distance_to(entity.cell)
    });

    if (nearest_hall_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    int entity_size = entity_get_data(entity.type).cell_size;
    ivec2 target_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_size, state.entities.get_by_id(nearest_hall_id).cell, entity_get_data(ENTITY_HALL).cell_size, MAP_IGNORE_UNITS);

    std::vector<ivec2> path_to_hall;
    map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, target_cell, entity_size, &path_to_hall, MAP_IGNORE_UNITS);

    static const int DISTANCE_FROM_HALL = 8;
    if (path_to_hall.size() > DISTANCE_FROM_HALL) {
        target_cell = path_to_hall[path_to_hall.size() - DISTANCE_FROM_HALL];
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = target_cell;
    input.move.target_id = ID_NULL;
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

MatchInput bot_unit_flee(const MatchState& state, const Bot& bot, EntityId entity_id) {
    const Entity& entity = state.entities.get_by_id(entity_id);

    EntityId nearest_hall_id = bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& hall, EntityId hall_id) {
            return hall.type == ENTITY_HALL &&
                entity_is_selectable(hall) &&
                hall.player_id == bot.player_id;
        },
        .compare = bot_closest_manhattan_distance_to(entity.cell)
    });

    if (nearest_hall_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    int entity_size = entity_get_data(entity.type).cell_size;
    ivec2 target_cell = map_get_nearest_cell_around_rect(state.map, CELL_LAYER_GROUND, entity.cell, entity_size, state.entities.get_by_id(nearest_hall_id).cell, entity_get_data(ENTITY_HALL).cell_size, MAP_IGNORE_UNITS);

    std::vector<ivec2> path_to_hall;
    map_pathfind(state.map, CELL_LAYER_GROUND, entity.cell, target_cell, entity_size, &path_to_hall, MAP_IGNORE_UNITS);

    static const int FLEE_DISTANCE = 16;
    if (path_to_hall.size() < FLEE_DISTANCE) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = path_to_hall[FLEE_DISTANCE];
    input.move.target_id = ID_NULL;
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

std::unordered_map<uint32_t, uint32_t> bot_get_enemy_hall_defense_scores(const MatchState& state, const Bot& bot) {
    std::unordered_map<uint32_t, uint32_t> enemy_hall_defense_score;
    // First pass, find all enemy halls
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (state.players[entity.player_id].team == 
                state.players[bot.player_id].team ||
                entity.type != ENTITY_HALL ||
                !entity_is_selectable(entity) ||
                !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }

        enemy_hall_defense_score[entity_index] = 0;
    }

    // Second pass, find all hall defenses
    // (this way allows us to avoid O(n^2) because we don't have to search
    // all entities for every defense just to find the nearest hall)
    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        const Entity& entity = state.entities[entity_index];
        EntityId entity_id = state.entities.get_id_of(entity_index);
        if (state.players[entity.player_id].team == 
                state.players[bot.player_id].team ||
                entity.type == ENTITY_HALL ||
                !entity_is_building(entity.type) ||
                !entity_is_selectable(entity) ||
                !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }

        uint32_t nearest_hall_index = INDEX_INVALID;
        for (auto it : enemy_hall_defense_score) {
            if (nearest_hall_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(entity.cell, state.entities[it.first].cell) <
                    ivec2::manhattan_distance(entity.cell, state.entities[nearest_hall_index].cell)) {
                nearest_hall_index = it.first;
            }
        }

        GOLD_ASSERT(nearest_hall_index != INDEX_INVALID);
        uint32_t entity_defense_value = entity.type == ENTITY_BUNKER ? 4 : 1;
        enemy_hall_defense_score[nearest_hall_index] += entity_defense_value;
    }

     return enemy_hall_defense_score;
}

uint32_t bot_score_entity(const Entity& entity) {
    if (entity_is_building(entity.type) && entity.type != ENTITY_BUNKER) {
        return 0;
    }
    switch (entity.type) {
        case ENTITY_MINER:
            return 0;
        case ENTITY_BUNKER:
            return entity.garrisoned_units.size() * 2;
        case ENTITY_WAGON:
            return entity.garrisoned_units.size();
        case ENTITY_CANNON:
            return 3;
        default:
            return 1;
    }
}